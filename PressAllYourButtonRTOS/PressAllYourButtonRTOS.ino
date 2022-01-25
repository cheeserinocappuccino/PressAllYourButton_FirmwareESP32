#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#ifndef LED_BUILTIN
#define LED_BUILTIN 5
#endif

// -----Neopixels-----
#include <FastLED.h>
#define NUM_LEDS 16
#define Led_DataPin 32
CRGBArray<NUM_LEDS + 1> leds;
CRGB ledsColor = CRGB(100,0,0);
int ledcycle = 0;

// -----Touches-----
#define touchOutput_pin 19
int laststate = 1;
// ----------------------Conectivity---------------------------------
#include <WiFi.h>
#include "AzureIotHub.h"
#include "Esp32MQTTClient.h"

#define INTERVAL 10000
#define DEVICE_ID "esp32"
#define MESSAGE_MAX_LEN 256

#define WIFI_BYPASS_PIN 25
bool isWifiOn = false;

// Please input the SSID and password of WiFi
const char *ssid = "7H4F";
const char *password = "16881688";

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
static const char *connectionString = "HostName=FirstTimeIOTHub.azure-devices.net;DeviceId=Esp32;SharedAccessKey=b/YWnluBMuReCFZWQxETHL42wYw3vJoqCx/e9H78PZs=";

const char *messageData = "{\"deviceId\":\"%s\", \"messageId\":%d, \"Temperature\":%f, \"Humidity\":%f}";

int messageCount = 1;
static bool hasWifi = false;
static bool messageSending = true;
static uint64_t send_interval_ms;
//|||||||||||||||||||||||||||||||Conectivity|||||||||||||||||||||||||||||||

// --------------------------Queues Declaration------------------------------
QueueHandle_t xq_to_xTaskLED;

// --------------------------Task Declaration------------------------------
void xTaskAzureExample(void *pvParameters);
void xTaskLED(void *pvParameters);
void xTaskTouchButton(void *pvParameters);
// |||||||||||||||||||||||||||||||Task Declaration|||||||||||||||||||||||||||||||

// the setup function runs once when you press reset or power the board
void setup()
{
  // --------------------------Manual Setup------------------------------
  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  Serial.println("ESP32 Device");
  Serial.println("Initializing...");
  // Neopixels Init
  FastLED.addLeds<NEOPIXEL, Led_DataPin>(leds, NUM_LEDS);
	FastLED.clear();
	FastLED.show();
  
  // touchbutton
  pinMode(touchOutput_pin, INPUT);
  
  delay(3000);

  // Check Wifi bypass pin
  pinMode(WIFI_BYPASS_PIN, INPUT);
  if(digitalRead(WIFI_BYPASS_PIN) == 1)
  {
    isWifiOn = true;
  }

  if(isWifiOn == true)
  {
      // Initialize the WiFi module
      Serial.println(" > WiFi");
      hasWifi = false;
      InitWifi();
      if (!hasWifi)
      {
        return;
      }
      randomSeed(analogRead(0));
      
      Serial.println(" > IoT Hub");
      Esp32MQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "GetStarted");
      Esp32MQTTClient_Init((const uint8_t *)connectionString, true);

      Esp32MQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
      Esp32MQTTClient_SetMessageCallback(MessageCallback);
      Esp32MQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
      Esp32MQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);

      send_interval_ms = millis();
  }
  // ||||||||||||||||||||||||||Manual Setup|||||||||||||||||||||||||||||||
  // --------------------------Queues Defination----------------------------
  xq_to_xTaskLED = xQueueCreate(1, 8);
  // --------------------------Task Handle----------------------------------
  TaskHandle_t xAzureExampleHandle;
  // --------------------------Task Defination------------------------------
  xTaskCreatePinnedToCore(
    xTaskTouchButton
    ,  "TaskTouchButton"   // A name just for humans
    ,  1024  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    xTaskLED
    ,  "TaskLED"   // A name just for humans
    ,  1024  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
      xTaskAzureExample, "AzureExample", 8096 // Stack size
      ,
      NULL, 3 // Priority
      ,
      &xAzureExampleHandle, ARDUINO_RUNNING_CORE);

    // --------------------------Suspend task by conditions------------------------------
    if(isWifiOn == false)
    {
      vTaskSuspend(xAzureExampleHandle);
    }
}
// ||||||||||||||||||||||||||||||||Task Defination||||||||||||||||||||||||||||||||||||

// --------------------------Normal Function------------------------------
static void InitWifi()
{
  Serial.println("Connecting...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  hasWifi = true;
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
// |||||||||||||||||||||||||||Normal Function|||||||||||||||||||||||||
// --------------------------Callback Function------------------------------
static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    Serial.println("Send Confirmation Callback finished.");
  }
}

static void MessageCallback(const char *payLoad, int size)
{
  Serial.println("Message callback:");
  Serial.println(payLoad);
}

static int DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending temperature and humidity data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending temperature and humidity data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  // Display Twin message.
  Serial.println(temp);
  free(temp);
}

// |||||||||||||||||||||||||||Callback Function|||||||||||||||||||||||||
// --------------------------Tasks Function------------------------------
void loop()
{
  // Empty. Things are done in Tasks.
}

/*--------------------------------------------------*/

void xTaskDealCommand(void *pvParameters)
{
  for (;;)
  {
   
    vTaskDelay(20);
  }


}




void xTaskTouchButton(void *pvParameters)
{
  uint8_t send_led_color[3] = {0};

  for (;;)
  {
    if(digitalRead(touchOutput_pin) == LOW && laststate == 1)
    {
      ledcycle ++;
      if(ledcycle >= 3)
        ledcycle = 0;
      if(ledcycle == 0)
      {
        send_led_color[0] = 100;
        send_led_color[1] = 0;
        send_led_color[2] = 0;
      }
      else if(ledcycle == 1)
      {
        send_led_color[0] = 0;
        send_led_color[1] = 100;
        send_led_color[2] = 0;
      }
      else if(ledcycle == 2)
      {
        send_led_color[0] = 0;
        send_led_color[1] = 0;
        send_led_color[2] = 100;
      }
      


      if(xQueueSend(xq_to_xTaskLED, send_led_color, 0) == pdTRUE)
      {
        Serial.println("sent color to leds");
      }
      
      

      laststate = 0;
    }else if(digitalRead(touchOutput_pin) == HIGH && laststate == 0)
    {
      laststate = 1;
    }
    
    vTaskDelay(20);
  }


}


void xTaskLED(void *pvParameters) // This is a task.
{
  (void)pvParameters;
  uint8_t color[3] = {0};
  
  for (;;)
  {
    if (xQueueReceive(xq_to_xTaskLED, color, portMAX_DELAY) == pdTRUE)
    {
      for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB(color[0], color[1], color[2]);
    }
    vTaskDelay(200);
    FastLED.show();
    vTaskDelay(20);

    }
    
  }
}

void xTaskAzureExample(void *pvParameters)
{
  for (;;)
  {
    if (hasWifi)
    {
      if (messageSending && (int)(millis() - send_interval_ms) >= INTERVAL)
      {
        char messagePayload[MESSAGE_MAX_LEN];
        float temperature = (float)random(0, 50);
        float humidity = (float)random(0, 1000) / 10;
        snprintf(messagePayload, MESSAGE_MAX_LEN, messageData, DEVICE_ID, messageCount++, temperature, humidity);
        Serial.println(messagePayload);
        EVENT_INSTANCE *message = Esp32MQTTClient_Event_Generate(messagePayload, MESSAGE);
        //Esp32MQTTClient_Event_AddProp(message, "temperatureAlert", "true");
        //Esp32MQTTClient_SendEventInstance(message);

        send_interval_ms = millis();
        Serial.println("message sent!");
      }
      else
      {
        Esp32MQTTClient_Check();
      }
    }
    vTaskDelay(10);
  }
}



