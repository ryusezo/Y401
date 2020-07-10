#include <string.h>
#include "stm8s.h"
#include "stm8s_clk.h"
#include "stm8s_gpio.h"

#define LED_PORT GPIOD
#define LED_G GPIO_PIN_3
#define LED_Y GPIO_PIN_4//state
#define LED_R GPIO_PIN_1//alarm

#define ALARM_ON GPIO_WriteHigh(GPIOA,LED_R)
#define ALARM_OFF GPIO_WriteLow(GPIOA,LED_R)

#define STATE_ON GPIO_WriteHigh(LED_PORT,LED_Y)
#define STATE_OFF GPIO_WriteLow(LED_PORT,LED_Y)

#define POWER_ON GPIO_WriteHigh(LED_PORT,LED_G)
void Delay_us(uint32_t time_delay);                                            
void Delay_ms(uint32_t time_delay);                                           
void CLK_Cofiguration(void);                                                    
void GPIO_Configuration(void);      
void ADC_Configuration(void);
void SEG_show(uint8_t i);
void beep(void);
void count_down(void);
float get_gas_value(void);
uint16_t getADCValue(uint8_t channel);
void AD_Start(void);
void warm_up(void);
void InitAD(uint8_t channel);
void beep_warning();
void power_on();
void calibration();
void write2eeprom(float v0, float v1);
uint8_t sensor_error=0;
uint8_t key_press()
{
  return (0==GPIO_ReadInputPin(GPIOD, GPIO_PIN_5));
}
void calibration()
{
  uint8_t time=0;
  SEG_show(0);
  ALARM_OFF;
  //wait 120s 
  while(time<240)
  {
    time++;
    GPIO_WriteReverse(LED_PORT,LED_Y);
    Delay_ms(500);    
  }
  
  //get v0 5 times and use average value as result
  uint8_t i=5;
  static float v0=0.0;
  while(i--)
  {
    v0+=(getADCValue(6)/204.6);
    Delay_ms(20);
  }
  v0/=5;
  if((v0>=0.3) && (v0<=0.8))// success to get v0
  {
    STATE_OFF;
  }
  else                          //fail to get v0
  {
    while(1)
    {
      GPIO_WriteReverse(LED_PORT,LED_Y);
      Delay_ms(2000); 
    }
  }
  
  //wait until voltage above 1.1v
  while((getADCValue(6)/204.6)<1.1)
  {
    Delay_ms(500);
  }
  time=0;
  //wait 120s
  while(time<60)
  {
    time++;
    GPIO_WriteReverse(GPIOA,LED_R);
    Delay_ms(2000);    
  }
  i=5;
  float v1=0.0;
  //get v0 5 times and use average value as result
  while(i--)
  {
    v1+=(getADCValue(6)/204.6);
    Delay_ms(20);
  }
  v1/=5;
  ALARM_OFF;
  write2eeprom(v0,v1);
}
void write2eeprom(float v0, float v1)
{
  uint8_t buffer[8];
  memcpy(buffer,&v0,4);
  memcpy(&buffer[4],&v1,4);
  FLASH_DeInit();
  FLASH_SetProgrammingTime(FLASH_PROGRAMTIME_STANDARD);
  FLASH_Unlock(FLASH_MEMTYPE_DATA);
  uint8_t *p;
  p = (u8*)0x4000; 
  uint8_t *data;
  data=buffer;
  for(uint8_t Len=8 ; Len > 0; Len--)
  {
      *p++ = *data++;
      //等待写完成
      while(!(FLASH_GetFlagStatus(FLASH_FLAG_EOP))); 
  } 
  FLASH_Lock(FLASH_MEMTYPE_DATA);
  
}
void main(void)
{
  
  CLK_Cofiguration();                                                           
  GPIO_Configuration();                                                    
  ADC_Configuration();
  
  enableInterrupts();
  //step1
  power_on();  
  //step2 warm up

  warm_up();
  //count_down();
  /* Infinite loop */
  float gas_value=0.0;
  while (1)
  {      
      gas_value=get_gas_value();
      if(sensor_error==1)
      {
        sensor_error=0;
        SEG_show(11);//show R
        Delay_ms(1000);
        continue;
      }
      //SEG show
      if(gas_value<10)
      {
        SEG_show((int)(gas_value));
      }
         
      else
      {
        SEG_show(11);//show R
      }
      
      //beep control
      if(gas_value<3)
      {
	Delay_ms(1000);
        continue;
      }
      else if(gas_value>=3 && gas_value<5)
      {
        //STATE_ON;
        ALARM_ON;
        beep();
        //STATE_OFF;
        ALARM_OFF;
        Delay_ms(1000);
      }
      else if(gas_value>=5 && gas_value<10)
      {
        
        int k=3;
        while(k--)
        {
         // STATE_ON;
          ALARM_ON;
          beep();
          //STATE_OFF;
          ALARM_OFF;
          Delay_ms(50);
        }
        Delay_ms(1000);
      }      
      else if(gas_value>10)
      {
        beep_warning();
      }
      
  }
}
void beep_warning()
{
  uint16_t i=100;
  uint16_t j;
  while(i++!=500)
  {
    GPIO_WriteReverse(GPIOA,GPIO_PIN_3);    
    j=i;
    ALARM_ON;
    while(j--);
    ALARM_OFF;
  }
  while(i--!=100)
  {
    GPIO_WriteReverse(GPIOA,GPIO_PIN_3);   
    j=i;
    ALARM_ON;
    while(j--);
    ALARM_OFF;
  }
  GPIO_WriteLow(GPIOA,GPIO_PIN_3); 
}
void warm_up(void)
{
  uint8_t count=0;
  uint8_t i=90;
  while(i--)
  {
    STATE_ON;
    ALARM_ON;
    SEG_show(10);//show -

    for(uint8_t jj=0;jj<10;jj++)
    {
      if(key_press())
      {
        count++;
      }
      Delay_ms(100);
    }    
    ALARM_OFF;
    STATE_OFF;
    SEG_show(12);//SEG all dark
    for(uint8_t jj=0;jj<10;jj++)
    {
      if(key_press())
      {
        count++;
      }
      Delay_ms(100);
    }    
    if(count>=4)
    {
      calibration();
      WWDG_SWReset();
    }
  }
  beep();
}
void count_down(void)
{
  int i=9;
  while(i>=0)
  {
    SEG_show(i);
    Delay_ms(1000);
    i--;
  }
}
//bool key_press(void)
//{
//  if(GPIO_ReadInputPin(GPIOD,GPIO_PIN_5)==RESET)
//  {
//    Delay_ms(500);
//    if(GPIO_ReadInputPin(GPIOD,GPIO_PIN_5)==RESET)
//    {
//      return 1;
//    }
//  }  
//  return 0;
//}

float get_gas_value(void)
{  
  uint16_t ADC_result=getADCValue(6);
  float voltage=ADC_result/204.6;
  static uint16_t count;
  static float offset;
  if(voltage<0.1)//incorrect voltage
  {
    STATE_ON;
    Delay_ms(1000);
    STATE_OFF;
    Delay_ms(1000);
    sensor_error=1;
    return 0;
  }
  
  //get calibration data
  float v0,v1,result;
  uint8_t *pp;
  pp = (u8*)0x4000; 
  memcpy(&v0,pp,4);
  pp = (u8*)0x4004; 
  memcpy(&v1,pp,4);
  if(((uint32_t)v0==0)&&((uint32_t)v1==0))//no calibration data
  {
    result=voltage*5.0-2.25;
  }
  else
  {
    float k,b;
    k=8.0/(v1-v0);
    b=-1*k*v0;
    result=voltage*k+b;
  }
  //for auto recalibration
  if(( (int)result)==1)
  {
    count++;
  }
  else
  {
    count=0;
  }
  
  if(count==600)//6 hours later
  {
    count=0;
    offset=result;
  }
  result-=offset;
  return result>0?result:0;
}
void beep(void)
{
  uint16_t i=0xff;
  while(i--)
  {
    GPIO_WriteReverse(GPIOA,GPIO_PIN_3);
    int j=200;
    while(j--);
  }
  GPIO_WriteLow(GPIOA,GPIO_PIN_3);
}
//step1
void power_on()
{
  POWER_ON;
  int i=3;
  ALARM_ON;
  while(i--)
  {
    beep();
  }
  ALARM_OFF;
  Delay_ms(500);
  
  ALARM_ON;
  beep();
  ALARM_OFF;
}
void CLK_Cofiguration(void)
{
  CLK_DeInit();
  CLK_SYSCLKConfig(CLK_PRESCALER_CPUDIV1);
  CLK_SYSCLKConfig(CLK_PRESCALER_HSIDIV1);
  CLK_ClockSwitchConfig(CLK_SWITCHMODE_AUTO, CLK_SOURCE_HSI, DISABLE, CLK_CURRENTCLOCKSTATE_DISABLE);
}

void GPIO_Configuration(void)
{
  GPIO_DeInit(GPIOD);                                                         
  GPIO_DeInit(GPIOA);      
  GPIO_DeInit(GPIOB); 
  GPIO_DeInit(GPIOC); 
  //LED                   
  GPIO_Init(LED_PORT,LED_G, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(LED_PORT,LED_Y, GPIO_MODE_OUT_PP_HIGH_FAST); 
  GPIO_Init(GPIOA,LED_R, GPIO_MODE_OUT_PP_HIGH_FAST); 
  //SEG
  GPIO_Init(GPIOD,GPIO_PIN_2, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(GPIOC,GPIO_PIN_3, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(GPIOC,GPIO_PIN_4, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(GPIOC,GPIO_PIN_5, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(GPIOC,GPIO_PIN_6, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(GPIOC,GPIO_PIN_7, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(GPIOB,GPIO_PIN_4, GPIO_MODE_OUT_PP_HIGH_FAST);
  GPIO_Init(GPIOB,GPIO_PIN_5, GPIO_MODE_OUT_PP_HIGH_FAST);
  
  //KEY
  GPIO_Init(GPIOD,GPIO_PIN_5, GPIO_MODE_IN_PU_NO_IT);

  //BEEP
  GPIO_Init(GPIOA,GPIO_PIN_3, GPIO_MODE_OUT_PP_HIGH_FAST);
  
  //ADC
  GPIO_Init(GPIOD,GPIO_PIN_6, GPIO_MODE_IN_FL_NO_IT);
}

void ADC_Configuration(void)
{
//  ADC1_DeInit();
//  ADC1_Init(ADC1_CONVERSIONMODE_CONTINUOUS,                                   
//            ADC1_CHANNEL_6,                                                   
//            ADC1_PRESSEL_FCPU_D18,                                            
//            ADC1_EXTTRIG_TIM,
//            DISABLE,                                     
//            ADC1_ALIGN_RIGHT,                                                
//            ADC1_SCHMITTTRIG_CHANNEL6,
//            DISABLE);  
//  //ADC1_ScanModeCmd(ENABLE);
//  ADC1_DataBufferCmd(ENABLE);
//  ADC1_Cmd(ENABLE);
//  ADC1_ITConfig(ADC1_IT_EOCIE,DISABLE);  
//  ADC1_StartConversion(); 
}
void InitAD(uint8_t channel)
{
  /* De-Init ADC peripheral*/
  ADC1_DeInit();
  //通道初始化
  ADC1_Init(ADC1_CONVERSIONMODE_SINGLE,
            (ADC1_Channel_TypeDef)channel,
            ADC1_PRESSEL_FCPU_D18,
            ADC1_EXTTRIG_TIM,
            DISABLE,
            ADC1_ALIGN_RIGHT,
            (ADC1_SchmittTrigg_TypeDef)channel,
            DISABLE);
}
 
void AD_Start(void)
{
  ADC1_ScanModeCmd(ENABLE);//启用扫描模式
  ADC1_DataBufferCmd(ENABLE);//启用缓存寄存器存储数据
  //ADC1_ITConfig(ADC1_IT_EOCIE,DISABLE);//关闭中断功能 
  ADC1_Cmd(ENABLE);//启用ADC1
  ADC1_StartConversion();//开始转换*/
}
 
uint16_t getADCValue(uint8_t channel)
{
  InitAD(channel);
  AD_Start();
  while(ADC1_GetFlagStatus(ADC1_FLAG_EOC) == RESET);
  return ADC1_GetConversionValue();
}

void Delay_us(uint32_t time_delay)
{
  while(time_delay--)
  {
  }
}

void Delay_ms(uint32_t time_delay)
{
  while(time_delay--)
  {
    Delay_us(200);
  }
}


// SEG function part
void SEG_show(uint8_t i)
{
  switch (i)
  {
  
  case 0:
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteHigh(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteHigh(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteHigh(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteLow(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 1:
    GPIO_WriteLow(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteLow(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteLow(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteLow(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 2:
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteLow(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteHigh(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteHigh(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteLow(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 3:
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteHigh(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteLow(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 4:
    GPIO_WriteLow(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteLow(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteHigh(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 5:
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteLow(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteHigh(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteHigh(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 6:
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteLow(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteHigh(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteHigh(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteHigh(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 7:
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteLow(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteLow(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteLow(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 8:
    GPIO_WriteHigh(GPIOD,GPIO_PIN_2);
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);
    GPIO_WriteHigh(GPIOC,GPIO_PIN_7);
    GPIO_WriteHigh(GPIOB,GPIO_PIN_4);
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);
    break;
  case 9:
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteHigh(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteHigh(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 10://show -
    GPIO_WriteLow(GPIOC,GPIO_PIN_3);//a
    GPIO_WriteLow(GPIOC,GPIO_PIN_4);//b
    GPIO_WriteLow(GPIOC,GPIO_PIN_6);//c
    GPIO_WriteLow(GPIOC,GPIO_PIN_7);//d
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);//e
    GPIO_WriteLow(GPIOB,GPIO_PIN_4);//f
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);//g
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);//dp
    break;
  case 11://show R
    GPIO_WriteHigh(GPIOD,GPIO_PIN_2);
    GPIO_WriteHigh(GPIOC,GPIO_PIN_3);
    GPIO_WriteHigh(GPIOC,GPIO_PIN_4);
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);
    GPIO_WriteHigh(GPIOC,GPIO_PIN_6);
    GPIO_WriteLow(GPIOC,GPIO_PIN_7);
    GPIO_WriteHigh(GPIOB,GPIO_PIN_4);
    GPIO_WriteHigh(GPIOB,GPIO_PIN_5);
    break;
  default:// all dark
    GPIO_WriteLow(GPIOD,GPIO_PIN_2);
    GPIO_WriteLow(GPIOC,GPIO_PIN_3);
    GPIO_WriteLow(GPIOC,GPIO_PIN_4);
    GPIO_WriteLow(GPIOC,GPIO_PIN_5);
    GPIO_WriteLow(GPIOC,GPIO_PIN_6);
    GPIO_WriteLow(GPIOC,GPIO_PIN_7);
    GPIO_WriteLow(GPIOB,GPIO_PIN_4);
    GPIO_WriteLow(GPIOB,GPIO_PIN_5);
    break;
  }
    
}














#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */
void assert_failed(u8* file, u32 line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
