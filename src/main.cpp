#include <Arduino.h>

#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "LittleFS.h"

uint8_t ledNodeMacAddress[] = {0xD4, 0x8A, 0xFC, 0xC7, 0xBB, 0xB0};

const int PIN_TO_SENSOR = 12;

int channel = -1;

int pinStateCurrent = LOW;
int pinStatePrevious = LOW;

bool isSent = false;
bool isFinished = false;

typedef struct outcome_message
{
  bool motionDetected;
} outcome_message;

outcome_message myData;

typedef struct income_message
{
  int32_t wifiChannel;
} income_message;

income_message incomingData;

esp_now_peer_info_t peerInfo;

void initLittleFS()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}

String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- write failed");
  }
}

void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\r\n", path);
  if (fs.remove(path))
  {
    Serial.println("- file deleted");
  }
  else
  {
    Serial.println("- delete failed");
  }
}

void loadValue()
{
  String channelstr = readFile(LittleFS, "/channel.txt");
  channel = channelstr.toInt();
  Serial.print("channel on load: ");
  Serial.println(channel);
}

void sendCallback(const uint8_t *macAddr, esp_now_send_status_t status)
{
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  if (status == ESP_NOW_SEND_SUCCESS)
  {
    isSent = true;
  }
  isFinished = true;
}

void onReceive(const uint8_t *macAddr, const uint8_t *data, int len)
{
  memcpy(&incomingData, data, sizeof(incomingData));
  String channelstr = (String)incomingData.wifiChannel;
  deleteFile(LittleFS, "/channel.txt");
  writeFile(LittleFS, "/channel.txt", channelstr.c_str());

  ESP.restart();
}

void sendMessage()
{
  for (int i = 0; i < 10; i++)
  {
    esp_err_t result = esp_now_send(ledNodeMacAddress, (uint8_t *)&myData, sizeof(myData));
    if (result == ESP_OK)
    {
      Serial.println("Sent with success");
    }
    else
    {
      Serial.print("Error sending the data: ");
      Serial.println(result);
    }

    while (!isFinished)
    {
      delay(10);
    }
    isFinished = false;

    if (isSent)
    {
      break;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(PIN_TO_SENSOR, INPUT);
  WiFi.mode(WIFI_STA);
  initLittleFS();
  loadValue();

  if (channel > -1 && channel < 14)
  {
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    Serial.println("Channel changed");
    delay(1000);
  }

  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(sendCallback);
  esp_now_register_recv_cb(onReceive);

  memcpy(peerInfo.peer_addr, ledNodeMacAddress, 6);
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK)
  {
    Serial.println("Failed to add peer");
    return;
  }
  Serial.print("WiFi channel: ");
  Serial.println(WiFi.channel());
}

void loop()
{

  pinStatePrevious = pinStateCurrent;
  pinStateCurrent = digitalRead(PIN_TO_SENSOR);

  if (pinStatePrevious == LOW && pinStateCurrent == HIGH)
  { // pin state change: LOW -> HIGH
    Serial.println("Motion detected!");
    myData.motionDetected = true;
  }
  else if (pinStatePrevious == HIGH && pinStateCurrent == LOW)
  { // pin state change: HIGH -> LOW
    Serial.println("Motion stopped!");
    myData.motionDetected = false;
  }
  if (pinStatePrevious != pinStateCurrent)
  {
    isSent = false;
    sendMessage();
  }

  delay(1000);
}
