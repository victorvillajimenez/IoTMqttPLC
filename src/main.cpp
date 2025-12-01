#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "esp_system.h"

#define LED_GREEN 2
#define LED_RED 14
#define LED_BLUE 12
#define BUTTON 4

//-------- Red Wi-Fi ---------//
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

//-------- Broker MQTT ---------//
const char* MQTT_SERVER = "broker.hivemq.com";
const uint16_t MQTT_PORT = 8883; // TLS port
const char* MQTT_USER = "";
const char* MQTT_PASS = "";

WiFiClientSecure net; // TLS socket
PubSubClient client(net);
// WiFiClient espClient;
// PubSubClient client(espClient);

//-------- TOPICS ---------//
const char *channelTopic_heartbeat = "acme/commands/heartbeat"; // SUB
const char *channelTopic_heartbeat_callback = "acme/commands/heartbeat/callback"; // PUB
const char *channelTopic_sensor1 = "acme/cell35/ac1/temp1"; // PUB
const char *channelTopic_sensor1_set = "acme/cell35/ac1/temp1/commands/set"; // SUB

int randomInt () {
  srand(time(NULL)); // Seed the random number generator
  // int min = -55; // MIN ARMY TEMP
  // int max = 125; // MAX ARMY TEMP
  // int dice_roll = (rand() % (max - min + 1)) + min; // Seed does not work on SPE32
  int min = -30;
  int max = 100;
  int dice_roll = (esp_random() % (max - min + 1)) + min;
  return dice_roll;
}

String builtSensorMessage (const char *channelTopic, int desirable, int value, boolean alive, String status) {
  // create a string message with different vars joined by commas
  String sensorMsg = channelTopic;
  sensorMsg += ","; // ",desirable=";
  sensorMsg += String(desirable);
  sensorMsg += ","; // ",value=";
  sensorMsg += String(value);
  sensorMsg += ","; // ",alive=";
  sensorMsg += alive ? "true" : "false";
  sensorMsg += ","; // ",status=";
  sensorMsg += status;
  return sensorMsg;
}

long PastTime = 0;
char msg[50];
int slideSwitchValue = 0;

int value = randomInt();
int desirable = randomInt();
boolean alive = true;
String status = "idle";
int threshold = 2;

/* ============= SETUP WIFI ============= */
void setup_wifi () {
  delay(100);
  // Determinar MAC address
  Serial.println();
  Serial.print("La direccion MAC del dispositivo de red es: ");
  Serial.println(WiFi.macAddress());
  // Conectar a WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Se ha aceptado el wifi con la direccion IP: ");
  Serial.println(WiFi.localIP());
}

/* ============= CALLBACK SUBSCRIPTION ============= */
void callback (char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");;
  Serial.print(topic);
  Serial.print("] ");
  String sPayload_digits;
  String sPayload;
  for(unsigned int i = 0; i < length; i++) {
    sPayload += (char)payload[i];
    if ((char)payload[i]) {
      sPayload_digits += (char)payload[i];
    }
  }
  Serial.println("payload(string): [" + sPayload + "]: " + sPayload_digits);
  if (strcmp(topic, channelTopic_heartbeat) == 0) {
    // Responder al latido de corazón
    Serial.println("Respuesta al latido de corazón recibida.");

    // create a string message with differentent content joined by commas
    String heartbeatMsg = builtSensorMessage(channelTopic_sensor1, desirable, value, alive, status);
    client.publish(channelTopic_heartbeat_callback, heartbeatMsg.c_str());
    return;
  }
  if (strcmp(topic, channelTopic_sensor1_set) == 0) {
    // Update desirable temperature
    // convert string to int
    desirable = sPayload.toInt();
    Serial.println("Update desirable temperature.");
    return;
  }
  // int p = (char)payload[0] - '0';
  // if (p == 1) { // Si el broker MQTT envía un "1", el LED_RED en GPI012 se enciende
  //   digitalWrite(LED_RED, HIGH);
  //   Serial.println("*** El LED ROJO fue encendido de manera remota ***");
  // }
  // if (p == 0) { // Si el broker MQTT envía un "0", el LED_RED en GPI012 se apaga
  //   digitalWrite(LED_RED, LOW);
  //   Serial.println("*** El LED ROJO fue apagado de manera remota ***");
  // }
  Serial.println();
}

/* ============= MQTT BROKER RE-CONNECTION ============= */
void reconnect () {
  // Conectar al broker MQTT
  while (!client.connected()) {
    Serial.print("Conectando al broker MQTT");
    Serial.println("");
    // Crear una ID de cliente aleatoria
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    //Intenta volver a conectarse ...
    // ... en caso de que el "broker" tenga ID de cliente, nombre de usuario y contraseña
    // ... cambiar la siguiente línea a --> if (client.connect(clientId, MQTT_USER, MQTT_PASS))
    Serial.println(clientId);
    // if (client.connect(clientId.c_str())) {
    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
      Serial.println("El broker MQTT esta conectado");
      // Una vez conectado al broker MQTT, suscribirse a los temas
      client.subscribe(channelTopic_heartbeat);
      client.subscribe(channelTopic_sensor1_set);
      String heartbeatMsg = builtSensorMessage(channelTopic_sensor1, desirable, value, alive, status);
      client.publish(channelTopic_heartbeat_callback, heartbeatMsg.c_str());
    } else {
      Serial.print("Error de conexion: ");
      Serial.print(client.state());
      Serial.println(" ... proximo intento en 5 segundos.");
      // Espera de 5 segundos para el próximo intento de conexión
      delay(5000);
    }
  }
}
/* ============= SETUP | INIT, TO RUN ONCE ============= */
void setup () {
  Serial.begin(115200);
  value = randomInt();
  desirable = randomInt();
  alive = true;
  status = "idle";
  setup_wifi();
  // Quick test only: skip certificate validation
  // (Use setCACert(root_ca) later for production)
  net.setInsecure();
  client.setServer(MQTT_SERVER, MQTT_PORT); // Puerto TCP 1883, estándar en MQTT
  client.setCallback(callback);
  pinMode(BUTTON, INPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  digitalWrite(LED_BLUE, LOW);
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
}

/* ============= MAIN, TO RUN REPEATEDLY ============= */
void loop () {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  long now = millis(); // Lectura del reloj del sistema en milisegundos
  // Validar (por control de tiempo) si han pasado 1 segundos desde el informe anterior
  if (now - PastTime > 1000) {
    PastTime = now;

    // Simular cambio de temperatura
    if (status == "idle") {
      if (value < desirable - threshold) {
        status = "heating";
      } else if (value > desirable + threshold) {
        status = "cooling";
      }
    } else if (status == "heating") {
      value += 1;
      if (value >= desirable) {
        status = "idle";
      }
    } else if (status == "cooling") {
      value -= 1;
      if (value <= desirable) {
        status = "idle";
      }
    }
    // Actualizar estado de los LEDs o Actuadores según el estado del sistema
    if (status == "heating") {
      digitalWrite(LED_BLUE, LOW);
      digitalWrite(LED_RED, HIGH);
    } else if (status == "cooling") {
      digitalWrite(LED_RED, LOW); 
      digitalWrite(LED_BLUE, HIGH);
    } else if (status == "idle") {
      digitalWrite(LED_BLUE, LOW);
      digitalWrite(LED_RED, LOW);
    }
    // Publicar datos del sensor de temperatura
    String sensorMsg = builtSensorMessage(channelTopic_sensor1, desirable, value, alive, status);
    sensorMsg.toCharArray(msg, 50);
    Serial.print("Publicando mensaje: ");
    Serial.println(msg);
    client.publish(channelTopic_sensor1, msg);

    int slideSwitchValue = digitalRead(BUTTON);
    digitalWrite(LED_GREEN, slideSwitchValue);
    String msg = String("* AC PLC is: ") + (slideSwitchValue ? "ON *" : "OFF *");
    Serial.println(msg);
    char message[58];
    msg.toCharArray(message, 58);
  }
}
