#include <Arduino.h>
#include <WiFiS3.h>
#include <ArduinoJson.h>
#include "ecolumina.h"

const char *ssid = WIFI_SSID;
const char *pass = WIFI_PASS;

const char *serverIP = SERVER_IP;
int serverPort = SERVER_PORT;
const char *path = "/api/arduinos/";

int arduinoId = ARDUINO_ID;

WiFiClient client;
StaticJsonDocument<2048> responseDoc;
DeserializationError responseError;

int lightsNumber = LIGHTS_NUMBER;
int lightsPin[] = {LIGHT1_PIN, LIGHT2_PIN, LIGHT3_PIN};

int lightsDim[] = {0, 0, 0};

bool lightsUseSensor[] = {true, true, true};

bool lightsUseLightSensor[] = {true, true, true};

bool lightsUseMotionSensor[] = {true, true, true};

bool lightsUseButton[] = {true, true, true};

int lightsMotionThreshold[] = {15, 10, 5, 0};

int lightSensorReading = 0;
float motionSensorReading = 0;
int buttonReading = 0;

void setup()
{
  Serial.begin(9600);

  Serial.println("Conectando à rede WIFI...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Conectado à rede WIFI");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  pinMode(LIGHT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(DISTANCE_TRIGGER_PIN, OUTPUT);
  pinMode(DISTANCE_ECHO_PIN, INPUT);

  for (int i = 0; i < lightsNumber; i++)
  {
    pinMode(lightsPin[i], OUTPUT);
  }
}

void loop()
{
  readServerData();

  parseResponse();

  doReadings();

  updateLights();

  updateServerData();
  // readServerData and updateServerData probably add enough delay, if you need more just add it below
}

void readServerData()
{
  Serial.println("-------------------------");
  Serial.println("Lendo dados do servidor.");

  if (client.connect(serverIP, serverPort))
  {
    client.println("GET " + String(path) + String(ARDUINO_ID) + " HTTP/1.1");
    client.println("Host: " + String(serverIP) + ":" + String(serverPort));
    client.println("Connection: close");
    client.println();

    while (client.connected() && !client.available())
    {
      delay(100);
    }

    bool isBody = false;
    String response;
    while (client.available())
    {
      String line = client.readStringUntil('\n');

      if (line == "\r")
      {
        isBody = true;
      }
      else if (isBody)
      {
        response += line;
      }
    }
    Serial.println(response);

    responseError = deserializeJson(responseDoc, response);

    client.stop();
  }
  else
  {
    Serial.println("Erro ao conectar ao servidor.");
  }

  Serial.println("-------------------------");
}

void parseResponse()
{
  Serial.println("-------------------------");
  Serial.println("Fazendo parse da resposta.");

  for (int i = 0; i < lightsNumber; i++)
  {
    lightsUseSensor[i] = responseDoc["lights"][i]["useSensor"];
  }

  for (int i = 0; i < lightsNumber; i++)
  {
    lightsUseLightSensor[i] = responseDoc["lights"][i]["useLightSensor"];
  }

  for (int i = 0; i < lightsNumber; i++)
  {
    lightsUseMotionSensor[i] = responseDoc["lights"][i]["useMotionSensor"];
  }

  for (int i = 0; i < lightsNumber; i++)
  {
    lightsUseButton[i] = responseDoc["lights"][i]["useButton"];
  }

  for (int i = 0; i < lightsNumber; i++)
  {
    if (!lightsUseLightSensor[i])
    {
      lightsDim[i] = responseDoc["lights"][i]["dim"];
    }
  }

  Serial.println("-------------------------");
}

void doReadings()
{
  buttonReading = readButton();
  lightSensorReading = readLight();
  motionSensorReading = readDistance();

  for (int i = 0; i < lightsNumber; i++)
  {
    if (!lightsUseSensor[i])
    {
      continue;
    }

    if (lightsUseButton[i] && buttonReading)
    {
      lightsDim[i] = 255;
      continue;
    }

    if (lightsUseLightSensor[i])
    {
      int dimValue = 255 - lightSensorReading;

      if (dimValue <= LIGHT_FRACTION)
      {
        dimValue = 0;
      }
      else if (dimValue >= 255 - LIGHT_FRACTION)
      {
        dimValue = 255;
      }

      lightsDim[i] = dimValue;

      if (!lightsUseMotionSensor[i])
      {
        continue;
      }

      if (motionSensorReading > lightsMotionThreshold[i] || motionSensorReading < lightsMotionThreshold[i + 1])
      {
        lightsDim[i] = 0;
      }
    }
  }
}

void updateLights()
{
  for (int i = 0; i < lightsNumber; i++)
  {
    analogWrite(lightsPin[i], lightsDim[i]);
  }
}

void updateServerData()
{
  Serial.println("-------------------------");
  Serial.println("Atualizando dados do servidor.");

  DynamicJsonDocument jsonDoc(1024);

  for (int i = 0; i < lightsNumber; i++)
  {
    jsonDoc["dims"][i] = lightsDim[i];
  }

  jsonDoc["light"] = lightSensorReading;
  jsonDoc["motion"] = motionSensorReading;
  jsonDoc["button"] = buttonReading * 255;

  String jsonData;
  serializeJson(jsonDoc, jsonData);

  if (client.connect(serverIP, serverPort))
  {
    client.println("POST " + String(path) + String(arduinoId) + " HTTP/1.1");
    client.println("Host: " + String(serverIP) + ":" + String(serverPort));
    client.println("Content-Type: application/json");
    client.println("Content-Length: " + String(jsonData.length()));
    client.println("Connection: close");
    client.println();
    client.println(jsonData);

    Serial.println(jsonData);

    while (client.connected() && !client.available())
    {
      delay(100);
    }

    while (client.available())
    {
      client.flush();
    }

    client.stop();
  }
  else
  {
    Serial.println("Erro ao conectar ao servidor.");
  }

  Serial.println("-------------------------");
}

int readButton()
{
  Serial.println("-------------------------");
  Serial.println("Lendo valor do botão.");

  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState)
  {
    Serial.println("Pressed");
  }
  else
  {
    Serial.println("Not pressed");
  }

  Serial.println("-------------------------");
  return buttonState;
}

int readLight()
{
  Serial.println("-------------------------");
  Serial.println("Lendo valor do sensor da luz.");

  int analogValue = analogRead(LIGHT_PIN);
  analogValue = map(analogValue, 0, 1023, 0, 255);

  Serial.println(analogValue);
  Serial.println("-------------------------");
  return analogValue;
}

float readDistance()
{
  Serial.println("-------------------------");
  Serial.println("Lendo valor do sensor de distância.");

  digitalWrite(DISTANCE_TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(DISTANCE_TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(DISTANCE_TRIGGER_PIN, LOW);

  float duration = pulseIn(DISTANCE_ECHO_PIN, HIGH);
  float distance = (duration * 0.0343) / 2;

  if (distance < 0)
  {
    distance = 0;
  }

  if (distance > 400)
  {
    distance = 400;
  }

  Serial.println(distance);
  Serial.println("-------------------------");
  return distance;
}
