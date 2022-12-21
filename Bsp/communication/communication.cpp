
/* Includes ------------------------------------------------------------------*/

#include "communication.hpp"
#include "common_inc.h"

/* Private defines -----------------------------------------------------------*/
/* Private macros ------------------------------------------------------------*/
/* Private typedef -----------------------------------------------------------*/
/* Global constant data ------------------------------------------------------*/
/* Global variables ----------------------------------------------------------*/
/* Private constant data -----------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
volatile bool endpointListValid = false;
/* Private function prototypes -----------------------------------------------*/
/* Function implementations --------------------------------------------------*/
// @brief Sends a line on the specified output.

osThreadId_t commTaskHandle;                        // 线程id 通行线程
const osThreadAttr_t commTask_attributes = {
    .name = "commTask",                             // 线程名字
    .stack_size = 45000,                            //堆栈大小
    .priority = (osPriority_t) osPriorityNormal,    //初始线程优先级(默认值:osPriorityNormal) 没有优先级
};

void InitCommunication(void)
{
    // Start command handling thread
    /**
     * 开始一个新线程
     * CommunicationTask：这是一个函数指针，指向执行任务的函数
     * nullptr：用于传递给任务的参数，不用的话可以设为Null
     * &commTask_attributes：包含了.name，.stack_size，.priority等属性，分别覆盖了xTaskCreate()和xTaskCreateStatic()中
     */
    commTaskHandle = osThreadNew(CommunicationTask, nullptr, &commTask_attributes);   //

    while (!endpointListValid)
        osDelay(1);
}

extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
osThreadId_t usbIrqTaskHandle;

void UsbDeferredInterruptTask(void* ctx)  //Usb延迟中断任务
{
    (void) ctx; // unused parameter

    for (;;)
    {
        // Wait for signalling from USB interrupt (OTG_FS_IRQHandler)
        osStatus semaphore_status = osSemaphoreAcquire(sem_usb_irq, osWaitForever);
        if (semaphore_status == osOK)
        {
            // We have a new incoming USB transmission: handle it
            HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
            // Let the irq (OTG_FS_IRQHandler) fire again.
            HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
        }
    }
}

// Thread to handle deffered processing of USB interrupt, and
// read commands out of the UART DMA circular buffer
void CommunicationTask(void* ctx)
{
    (void) ctx; // unused parameter

    CommitProtocol();

    // Allow main init to continue
    endpointListValid = true;

    StartUartServer();
    StartUsbServer();
    StartCanServer(CAN1);
    StartCanServer(CAN2);

    for (;;)
    {
        osDelay(1000); // nothing to do
    }
}

extern "C" {
int _write(int file, const char* data, int len);
}

// @brief This is what printf calls internally
int _write(int file, const char* data, int len)
{
    usbStreamOutputPtr->process_bytes((const uint8_t*) data, len, nullptr);
    uart4StreamOutputPtr->process_bytes((const uint8_t*) data, len, nullptr);

    return len;
}
