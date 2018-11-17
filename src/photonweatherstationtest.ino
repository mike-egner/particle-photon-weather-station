/* To do:
    1. make a cloud function to change external_publishing variable (change from const to variable) + serial input at start?
    2. make a cloud function to change low_power_mode and change that from const + serial input at start?
*/

//import libraries
#include <SparkFun_Photon_Weather_Shield_Library.h>

//define values
#define PI 3.1416
#define SECS_IN_AN_HOUR 3600 //useful
#define CM_IN_A_KM 100000
#define ANEMOM_FACTOR 1.18

//declare constants
const bool external_publishing = true; //set whether to publish to Thingspeak - can change to serial
const bool low_power_mode = true; //set whether to go into sleep between loops rather than just wait
const unsigned sync_interval = 24 * 60 * 60 * 1000; //clock to sync to Particle cloud once a day
const String channel_id = "373128";
const double rain_bucket_size = 0.2794;
const double anemom_radius_cm = 9;
const int wind_pin = D3;
const int rain_pin = D2;
const int vane_pin = A0;
const int soil_pin = A1;

//declare variables
int loop_counter = 0;
int publish_counter = 0;
unsigned long wind_counter = 0;
int wind_dir = 0;
double rainfall = 0;
int soil_moist = 0;
//double battery_level = 0;

unsigned long wind_millis = 0;
unsigned program_speed = 5 * 60 * 1000; //must be greater than 5000 for the system to work in low power mode
unsigned window_period = 30 * 1000; // window period i.e. delay before going offline
unsigned http_wait_time = 1000;
unsigned long last_sync = 0;
double temp = 0;
float humidity = 0;
float pascals = 0;
float baro_temp = 0;
String payload = "";

Weather sensor; //Create Instance of HTU21D or SI7021 temp and humidity sensor and MPL3115A2 barometric sensor
//FuelGauge fuel;

//if (external_publishing) {
    TCPClient client; // Initialize the TCP client library
    String json_buffer = "["; // initialise the JSON buffer to hold data
    char server[] = "api.thingspeak.com"; // ThingSpeak Server

//}

//---------------------------------------------------------------

void setup() {
    Serial.println("Initializing the device...");
    Particle.publish("Initializing device.", PRIVATE);

    //initialise here
    Particle.variable("loop_counter", loop_counter);
    Particle.variable("pub_counter", publish_counter);
    Particle.variable("wind_counter", wind_counter);
    Particle.variable("rain", rainfall);
    //Particle.variable("batt_level", battery_level);
    Particle.function("getAnalog", getAnalog);
    Particle.function("changeSpeed", changeSpeedSeconds);
    Particle.function("chngHttpWait", changeHttpWait);

    Particle.syncTime();

    sensor.begin(); //Initialize the sensors

    /*You can only receive accurate barometric readings or accurate altitude
    readings at a given time, not both at the same time. The following two lines
    tell the sensor what mode to use. You could easily write a function that
    takes a reading in one made and then switches to the other mode to grab that
    reading, resulting in data that contains both accurate altitude and barometric
    readings. Be sure to only uncomment one line at a time. */
    sensor.setModeBarometer();//Set to Barometer Mode
    //baro.setModeAltimeter();//Set to altimeter Mode

    //These are additional MPL3115A2 functions that MUST be called for the sensor to work.
    sensor.setOversampleRate(7); // Set Oversample rate
    /*Call with a rate from 0 to 7. See page 33 for table of ratios.
    Sets the over sample rate. Datasheet calls for 128 but you can set it
    from 1 to 128 samples. The higher the oversample rate the greater
    the time between data samples. */

    sensor.enableEventFlags(); //Necessary register calls to enble temp, baro and alt

    if (external_publishing) {
        Serial.println("Publishing to external site.");
    } else {
        Serial.println("Not publishing to external site.");
    }

    Serial.println("Success.");
    Particle.publish("Initializing successful.", PRIVATE);

    //Wind Sensor setup
    pinMode(wind_pin, INPUT_PULLUP); //set up the rain sensor pin as pulled-up digital pin
    attachInterrupt(wind_pin, windHandler, FALLING); //set up the interrupt for the pin falling

    //Rain Sensor setup
    pinMode(rain_pin, INPUT_PULLUP); //set up the rain sensor pin as pulled-up digital pin
    attachInterrupt(rain_pin, rainHandler, FALLING); //set up the interrupt for the pin falling

}

//---------------------------------------------------------------

void loop() {

    /* This is not required if publishing appears to be working.
    //Test publishing
    if (Particle.publish("Publish", PRIVATE)) {
        publish_counter++;
        Serial.println("Successful publish. Loop counter = "+String(loop_counter)+", Publish counter = "+String(publish_counter));
    } else {
        Serial.println("Unsuccessful publish! Loop counter = "+String(loop_counter)+", Publish counter = "+String(publish_counter));
    }
    */

    //Get and print weather data.
    //updateBatteryStatus;
    getWeather();
    printInfo();
    updateBuffer(json_buffer);

    //Publish weather data - requires string conversion
    payload = "Rain: " + String(rainfall, 2) + "mm, " + "Temp: " + String(temp, 2) + "C, " + "Humidity: " + String(humidity, 2) + "%, " + "Baro_Temp: " + String(baro_temp, 2) +
            "C, " + "Pressure: " + String(pascals/100, 2) + "hPa";
    Serial.print("Uploading weather data: "+payload+"...");
    if (Particle.publish("Temperature", payload, PRIVATE)) {
        Serial.println("Success.");
    } else {
        Serial.println("Failure.");
    }

    Particle.publish("Now entering window period for " + String(window_period / 1000) + " seconds.");
    delay(window_period);

    if (low_power_mode) {
        Particle.publish("Entering low power state.");
        powerDown();
        delay(program_speed);
        powerUp();
    } else {
        Serial.println("Now pausing program for " + String(program_speed / 1000) + " seconds.");
        delay(program_speed);
        Serial.println("Now continuing program.");
    }

    loop_counter++;

}

//---------------------------------------------------------------

void getWeather()
{
    // Measure Relative Humidity from the HTU21D or Si7021
    humidity = sensor.getRH();

    // Measure Temperature from the HTU21D or Si7021
    temp = sensor.getTemp();
    // Temperature is measured every time RH is requested.
    // It is faster, therefore, to read it from previous RH
    // measurement with getTemp() instead with readTemp()

    //Measure the Barometer temperature in F from the MPL3115A2
    baro_temp = sensor.readBaroTemp();

    //Measure Pressure from the MPL3115A2
    pascals = sensor.readPressure();

    //If in altitude mode, you can get a reading in feet with this line:
    //float altf = sensor.readAltitudeFt();
}

//---------------------------------------------------------------

void printInfo() {
    //This function prints the weather data out to the default Serial Port

    Serial.print("Temp:");
    Serial.print(temp);
    Serial.print("C, ");

    Serial.print("Humidity:");
    Serial.print(humidity);
    Serial.print("%, ");

    Serial.print("Baro_Temp:");
    Serial.print(baro_temp);
    Serial.print("C, ");

    Serial.print("Pressure:");
    Serial.print(pascals/100);
    Serial.print("hPa, ");
    Serial.print((pascals/100) * 0.0295300);
    Serial.println("in.Hg");

    /*The MPL3115A2 outputs the pressure in Pascals. However, most weather stations
    report pressure in hectopascals or millibars thus dividing by 100 to get a reading
    more closely resembling what online weather reports may say in hPa or mb.
    Another common unit for pressure is Inches of Mercury (in.Hg). To convert
    from mb to in.Hg, use the following formula. P(inHg) = 0.0295300 * P(mb)
    More info on conversion can be found here:
    www.srh.noaa.gov/images/epz/wxcalc/pressureConversion.pdf */

    //If in altitude mode, print with these lines
    //Serial.print("Altitude:");
    //Serial.print(altf);
    //Serial.println("ft.");

}

//---------------------------------------------------------------

void updateBuffer(String json_buffer){
    /*  This function updates the JSON buffer with data
     *  JSON format for updates parameter in the API:
     *  Using the absolute timestamp i.e. the "created_at" parameter.
     *  You can also provide the relative timestamp using "delta_t"
     *  instead of "created_at".
     *   "[{\"created_at\":0,\"field1\":-70},{\"delta_t\":3,\"field1\":-66}]"
    */

    double wind_speed_avg = windSpeed(wind_counter, (millis() - wind_millis) / 1000);

    // Format the JSON buffer as above
    json_buffer = json_buffer + "{\"created_at\":\"" + String(Time.format("%Y-%m-%d %H:%M:%S %z")) + "\",\"field1\":" + String(rainfall, 2) + ",\"field2\":" + String(temp, 2) + ",\"field3\":" + String(humidity, 2) + ",\"field4\":" + String(wind_speed_avg, 2) + "},";
    //"%Y-%m-%d %H:%M:%S %z" as alternative to TIME_FORMAT_ISO8601_FULL

    // If some condition has been reached, you might want to close the buffer with square brackets
    if (true) {
        size_t len = strlen(json_buffer);
        json_buffer[len-1] = ']';
        Serial.println("JSON Buffer: "+json_buffer);
        //Particle.publish("JSON", json_buffer, PRIVATE);
        httpRequest(json_buffer);
        resetVariables();
    }
}

//---------------------------------------------------------------

void httpRequest(String json_buffer) {
    /* JSON format for data buffer in the API
    *  This examples uses the relative timestamp as it uses the "delta_t". You can also provide the absolute timestamp using the "created_at" parameter
    *  instead of "delta_t".
    *   "{\"write_api_key\":\"YOUR-CHANNEL-WRITEAPIKEY\",\"updates\":[{\"delta_t\":0,\"field1\":-60},{\"delta_t\":15,\"field1\":200},{\"delta_t\":15,\"field1\":-66}]
    */
    // Format the data buffer as noted above
    String data = "{\"write_api_key\":\"38VJGXJCL7487DRY\",\"updates\":" + json_buffer + "}"; // Consider making the API key a constant
    //strcat(data,json_buffer);
    //strcat(data,"}");
    // Close any connection before sending a new request
    client.stop();
    String data_length = String(strlen(data)); //Compute the data buffer length
    Serial.println(data);
    Particle.publish("UploadData", data, PRIVATE);
    // POST data to ThingSpeak
    if (client.connect(server, 80)) {
        client.println("POST /channels/"+channel_id+"/bulk_update.json HTTP/1.1"); // Consider making the channel ID a constant
        client.println("Host: "+String(server));
        client.println("User-Agent: mw.doc.bulk-update (Particle Photon)");
        client.println("Connection: close");
        client.println("Content-Type: application/json");
        client.println("Content-Length: "+data_length);
        client.println();
        client.println(data);
    }
    else {
        Serial.println("Failure: Failed to connect to ThingSpeak");
    }
    delay(http_wait_time); //Wait to receive the response
    client.parseFloat();
    String resp = String(client.parseInt());
    Serial.println("Response code:"+resp); // Print the response code. 202 indicates that the server has accepted the response
    Particle.publish("HttpResponse", resp, PRIVATE);
    //lastConnectionTime = millis(); //Update the last connection time
}

void windHandler() {
    wind_counter++;
}

void rainHandler() {
    rainfall = rainfall + rain_bucket_size;
}

//cloud function - requires: an int to be returned and a String to be passed
int getAnalog(String command) {
    //update all analog-related variables anyway
    wind_dir = analogRead(vane_pin);
    soil_moist = analogRead(soil_pin);

    if (command == "wind") {
        return wind_dir;
    } else if (command == "soil") {
        return soil_moist;
    } else {
        return -1;
    }
}

int changeSpeedSeconds(String speed_string) {
    int speed_int = 0;
    speed_int = speed_string.toInt();
    if (speed_int < 10) {
        return -1;
    } else {
        program_speed = speed_int * 1000;
        return speed_int;
    }
}

int changeHttpWait(String wait_string) {
    int wait_int = 0;
    wait_int = wait_string.toInt();
    if (wait_int < 10) {
        return -1;
    } else {
        program_speed = wait_int * 1000;
        return wait_int;
    }
}

void resetVariables() {
    rainfall = 0;
    json_buffer = "[";
    wind_counter = 0;
    wind_millis = millis();
}

//void updateBatteryStatus () {
//    battery_level = fuel.getSoC();
//}

double windSpeed (int sensor_clicks, float seconds_elapsed) {
    String audit = "";
    double x = 2 * PI * anemom_radius_cm ; //calculate the anemometer circumference
    x = x * sensor_clicks / 2; //multiply by half of the clicks to get wind "distance"
    x = x / seconds_elapsed; //divide by seconds elapsed to get wind speed in cm/sec
    x = x * SECS_IN_AN_HOUR / CM_IN_A_KM; //convert to km/h
    x = x * ANEMOM_FACTOR;
    return x;
}

void powerDown() {
    Particle.publish("Now disconnecting for low power.");
    Particle.disconnect();
    WiFi.disconnect();
    WiFi.off();
}

void powerUp() {
    bool power_up_complete = false;
    WiFi.on();
    WiFi.connect();
    delay(1000);
    while (WiFi.ready() == false) {
        Serial.println("Waiting for WiFi Ready...");
        delay(1000);
        continue;
    }
    Particle.connect();
    while (Particle.connected() == false) {
        // if time window recall connect function
        Serial.println("Waiting for Particle connection...");
        delay(1000);
        continue;
    }
    Particle.publish("Successfully powered up.");

    /*
    while (power_up_complete == false) {
        if (WiFi.connecting() == false) {
            Particle.connect();
            if (Particle.connected()) {
                power_up_complete = true;
            }
        }
    }*/
}
