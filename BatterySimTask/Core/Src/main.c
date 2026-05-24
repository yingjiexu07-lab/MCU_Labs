/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <string.h>   // 解決 strlen 編譯錯誤的元凶！
#include "freeRTOS.h" // 確保編譯器認識原生 FreeRTOS 的 API
#include "queue.h"    // 確保編譯器認識 xQueue 相關機制
#include "task.h"     // 確保編譯器認識 xTaskCreate 相關機制

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

//強迫編譯器「打包（Packed）」
typedef struct __attribute__((packed)){

	float voltage; 		//電池電壓，伏特(V)
	float current; 		//放電電流，安培(A)
	float temperature; 	//電池溫度，攝氏
	uint8_t soc;  		//剩餘電量百分比，範圍0-100(%)
	uint8_t status_flag; 	//狀態旗標，0x00 正常，0x01 過溫，0x02 低電壓
}Battery_State_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart5;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for BatterySimTask */
osThreadId_t BatterySimTaskHandle;
const osThreadAttr_t BatterySimTask_attributes = {
  .name = "BatterySimTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* USER CODE BEGIN PV */

Battery_State_t my_battery;

QueueHandle_t otaQueueHandle; // 專門用來安全傳遞 OTA 字元的隊列
uint8_t rx_byte;              // 存放中斷接收到的單一字元

void Battery_Init(void);

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART5_Init(void);
void StartDefaultTask(void *argument);
void StartBatterySimTask(void *argument);

/* USER CODE BEGIN PFP */

void vOTARecvTask(void *argument);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  // 關鍵：允許在睡眠、停止、待機模式下繼續進行模擬除錯
  // 0x07 代表同時將 DBG_SLEEP, DBG_STOP, DBG_STANDBY 三個位元設為 1
  DBGMCU->CR |= 0x00000007U;

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_UART5_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */

  otaQueueHandle = xQueueCreate(128, sizeof(uint8_t));

  //長度給 128，確保 Linux 塞過來的同步碼不會爆掉
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of BatterySimTask */
  BatterySimTaskHandle = osThreadNew(StartBatterySimTask, NULL, &BatterySimTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  xTaskCreate(vOTARecvTask, "OTARecvTask", 256, NULL, 2, NULL);

  //我們第三個 OTA 接收任務（優先權給低一點，保證 BatterySimTask 電力監控最高優先）

  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PD12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void Battery_Init(void){

	my_battery.voltage=4.2f;		//滿電電壓 4.2V
	my_battery.current=1.5f;		//假設穩定放電電流 1.5A
	my_battery.temperature=25.0f;	//室溫25度
	my_battery.soc=100;				//電量100%
	my_battery.status_flag=0x00;
}









void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == UART5)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        // 將收到的這 1 Byte 塞進 FreeRTOS 隊列中，通知後台任務處理
        xQueueSendFromISR(otaQueueHandle, &rx_byte, &xHigherPriorityTaskWoken);

        // 重啟 UART5 中斷監聽，準備接下一個 Byte
        HAL_UART_Receive_IT(&huart5, &rx_byte, 1);

        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}








// 2. 論文核心：OTA 接收狀態機任務 (不阻塞電力監控)
void vOTARecvTask(void *argument)
{
    uint8_t data;
    uint8_t state = 0; // 狀態機：0=等0x55, 1=等0xAA, 2=等Cmd, 3=等Len
    uint8_t cmd = 0;
    uint8_t len = 0;

    // 晶片一開機，先啟動第一次的中斷接球準備
    HAL_UART_Receive_IT(&huart5, &rx_byte, 1);

    for(;;)
    {
        // 這裡會進入休眠，直到中斷塞資料進隊列才會醒來，完全不吃 CPU
        if(xQueueReceive(otaQueueHandle, &data, portMAX_DELAY) == pdTRUE)
        {
            // 輕量化二進位協議解析狀態機
            switch(state)
            {
                case 0:
                    if(data == 0x55) state = 1; // 抓到第一個包頭
                    break;
                case 1:
                    if(data == 0xAA) state = 2; // 抓到第二個包頭
                    else state = 0;             // 歪掉，重來
                    break;
                case 2:
                    cmd = data;                  // 記下指令碼 (0x01 代表開始 OTA)
                    state = 3;
                    break;
                case 3:
                    len = data;                  // 記下後續長度

                    // 核心判斷：成功解析出一個完整的控制命令！
                    if(cmd == 0x01 && len == 0x00)
                    {
                        // 這是你的論文高光時刻！未來這裡就要去操作 Flash 切換 Bank 2
                        // 這裡我們先用 printf 或是特定標記來驗證
                        #define OTA_LOG "【STM32 晶片系統回報】成功解鎖並切換至雙區(Dual-bank) OTA 接收狀態！電力監控不中斷！\n"
                        HAL_UART_Transmit(&huart5, (uint8_t*)OTA_LOG, strlen(OTA_LOG), 100);
                    }

                    state = 0; // 處理完命令，狀態機重置，等待下一組
                    break;
                default:
                    state = 0;
                    break;
            }
        }
    }
}









/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {

	  HAL_UART_Transmit(&huart5, (uint8_t*)&my_battery, sizeof(my_battery), 100);

    osDelay(1000);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartBatterySimTask */
/**
* @brief Function implementing the BatterySimTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartBatterySimTask */
void StartBatterySimTask(void *argument)
{
  /* USER CODE BEGIN StartBatterySimTask */

	Battery_Init(); //初始化電池(滿電狀態)

  /* Infinite loop */
  for(;;)
  {
	  if(my_battery.soc>0){
		  my_battery.soc--;

		  // 模擬電壓：隨 SOC 線性下滑 (從 4.2V 掉到 3.0V)
		  my_battery.voltage = 3.0f + ((float)my_battery.soc / 100.0f) * 1.2f;


	  //模擬發熱：只要還在放電，溫度就每秒上升 0.2 度，最高到 45 度
		  if(my_battery.temperature <45.0f){
			  my_battery.temperature += 0.2f;
	   }
	  }else{

		  my_battery.current = 0.0f;
		  my_battery.voltage = 3.0f;   // 沒電時固定在截止電壓
	  }

	  // --- 狀態旗標判斷 (優先權：過溫 > 低電壓 > 正常) ---

	  if(my_battery.temperature > 40.0f)
	  {
	     my_battery.status_flag = 0x01; // 0x01: 過溫警告
	  }
	     else if(my_battery.soc < 20)
	  {
	     my_battery.status_flag = 0x02; // 0x02: 低電壓警告
	  }
	  else
	  {
	    my_battery.status_flag = 0x00; // 0x00: 正常
	   }


    osDelay(1000);   // 讓任務休息 1000 毫秒（1秒），把 CPU 讓給別人
  }
  /* USER CODE END StartBatterySimTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
