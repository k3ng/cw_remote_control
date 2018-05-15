/*


 K3NG Arduino CW Remote Control

 Copyright 1340 BC, 2010, 2011, 2012, 2013, 2014, 2015, 2016 Anthony Good, K3NG
 All trademarks referred to in source code and documentation are copyright their respective owners.

    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


Version History

  1.0.2016021301
    First version!  Yeahhhhhhh!

*/

#define CODE_VERSION "1.0.2016021301"

// Pins - define your hardware pins here

#define pin_cw_decoder 2             // input pin for cw decoding, LOW = active
#define pin_cw_decoder_indicator 0       // goes high when pin_cw_decoder goes active or Goertzel detector detects CW tone
#define pin_cw_decoder_audio_input 0  // audio input for use with OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR (must be analog pin)
#define pin_unlocked 13               // goes high when unit is in unlocked state

// Options and Features - uncomment lines to add functionality

// #define OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR   // uncomment this to use Goertzel audio detector and pin_cw_decoder_audio_input as audio input

#define INITIAL_WPM 20
#define PRIMARY_SERIAL_PORT &Serial
#define PRIMARY_SERIAL_PORT_BAUD 115200 
#define DEBUG_SERIAL_PORT &Serial
#define DEBUG_SERIAL_PORT_BAUD 115200 
#define CW_DECODER_SCREEN_COLUMNS 120        // word wrap at this many columns
#define CW_DECODER_SPACE_PRINT_THRESH 4.5   // print space if no tone for this many element lengths
#define CW_DECODER_SPACE_DECODE_THRESH 2.0  // invoke character decode if no tone for this many element lengths
#define CW_DECODER_NOISE_FILTER 20          // ignore elements shorter than this (mS)
#define UNKNOWN_CW_CHARACTER '*'
#define UNLOCK_INACTIVITY_TIMEOUT_MS 15000
#define UNLOCK_HARD_TIMEOUT_SECS 60
#define BUFFER_INACTIVITY_TIME_MS 4000
#define UNLOCK_PHRASE "VVVDEK3NG"


// Debug Stuff - turn these on to see various debug messages on DEBUG_SERIAL_PORT defined above

// #define DEBUG_CW_DECODER_WPM
// #define DEBUG_CW_DECODER
// #define DEBUG_CW_DECODER_WITH_TONE
// #define DEBUG_PROCESS_CW



#if defined(OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR)
  #include <goertzel.h>
#endif



HardwareSerial * primary_serial_port;
HardwareSerial * debug_serial_port;

// CW Commands - define commands to turn pins on and off
//
// You can define as many pins as you like.  You must have the same number of elements in each array

String control_phrases_pin_high[] = {"PIN4ON", "PIN5ON",  "A"};  // ON commands 
String control_phrases_pin_low[] =  {"PIN4OFF","PIN5OFF", "B"};  // OFF commands
uint8_t control_phrases_pins[] =    {     4,      5,       A0};  // pins


#if defined(OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR)
  Goertzdetector cwtonedetector;
#endif

//============================================================================================================================

void setup()
{

  initialize_pins_and_ports();


}

void loop(){

  service_cw_decoder();
  process_cw(32);

}

//============================================================================================================================

void initialize_pins_and_ports(){

  primary_serial_port = PRIMARY_SERIAL_PORT;
  debug_serial_port = DEBUG_SERIAL_PORT;

  primary_serial_port->begin(PRIMARY_SERIAL_PORT_BAUD);
  if (primary_serial_port != debug_serial_port){
    debug_serial_port->begin(DEBUG_SERIAL_PORT_BAUD);
  }

  pinMode (pin_cw_decoder, INPUT);
  digitalWrite (pin_cw_decoder, HIGH);

  if (pin_unlocked){
    pinMode (pin_unlocked, OUTPUT);
    digitalWrite (pin_unlocked, LOW);  	
  }

  #if defined(OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR)
	  digitalWrite (pin_cw_decoder_audio_input, LOW);
	  cwtonedetector.init(pin_cw_decoder_audio_input);
  #endif //OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR

  if (pin_cw_decoder_indicator){
  	pinMode(pin_cw_decoder_indicator,OUTPUT);
  	digitalWrite(pin_cw_decoder_indicator, LOW);
  }

  for (int x = 0;x < sizeof(control_phrases_pins);x++){
    pinMode (control_phrases_pins[x], OUTPUT);
    digitalWrite (control_phrases_pins[x], LOW);
  }

}


//---------------------------------------------------------------------------------------------------------------------------

void service_cw_decoder() {

  static unsigned long last_transition_time = 0;
  static unsigned long last_decode_time = 0;
  static uint8_t last_state = HIGH;
  static int decode_elements[16];                  // this stores received element lengths in mS (positive = tone, minus = no tone)
  static uint8_t decode_element_pointer = 0;
  static float decode_element_tone_average = 0;
  static float decode_element_no_tone_average = 0;
  static int no_tone_count = 0;
  static int tone_count = 0;
  uint8_t decode_it_flag = 0;
  uint8_t cd_decoder_pin_state = HIGH;
  
  int element_duration = 0;
  static float decoder_wpm = INITIAL_WPM;
  long decode_character = 0;
  static uint8_t space_sent = 0;

  static uint8_t screen_column = 0;
  static int last_printed_decoder_wpm = 0;


  cd_decoder_pin_state = digitalRead(pin_cw_decoder);

  #if defined(OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR)
    if (cwtonedetector.detecttone() == HIGH){  // invert states
      cd_decoder_pin_state = LOW;
    } else {
      cd_decoder_pin_state = HIGH;
    }  
  #endif  
 
  #if defined(DEBUG_CW_DECODER_WITH_TONE)
    if (cd_decoder_pin_state == LOW){
      #if defined(GOERTZ_TARGET_FREQ)
        tone(sidetone_line, GOERTZ_TARGET_FREQ);
      #else
        tone(sidetone_line, hz_sidetone);
      #endif //defined(GOERTZ_TARGET_FREQ)
    } else {
     noTone(sidetone_line);
    }
  #endif  //DEBUG_CW_DECODER 
 
  if ((pin_cw_decoder_indicator) && (cd_decoder_pin_state == LOW)){ 
   digitalWrite(pin_cw_decoder_indicator,HIGH);
  } else {
   digitalWrite(pin_cw_decoder_indicator,LOW);      
  }
 
  #ifdef DEBUG_OPTION_CW_DECODER_GOERTZEL_AUDIO_DETECTOR
    static unsigned long last_magnitude_debug_print = 0;
    if ((millis() - last_magnitude_debug_print) > 250){
      //debug_serial_port->print("service_cw_decoder: cwtonedetector magnitude: ");
      //debug_serial_port->print(cwtonedetector.magnitudelimit_low);
      //debug_serial_port->print("\t");
      debug_serial_port->print(cwtonedetector.magnitudelimit);
      debug_serial_port->print("\t");
      debug_serial_port->println(cwtonedetector.magnitude);
      last_magnitude_debug_print = millis();
    }
  #endif
 
  if  (last_transition_time == 0) { 
    if (cd_decoder_pin_state == LOW) {  // is this our first tone?
      last_transition_time = millis();
      last_state = LOW;
      
      #ifdef FEATURE_SLEEP
        last_activity_time = millis(); 
      #endif //FEATURE_SLEEP
      
    } else {
      if ((last_decode_time > 0) && (!space_sent) && ((millis() - last_decode_time) > ((1200/decoder_wpm)*CW_DECODER_SPACE_PRINT_THRESH))) { // should we send a space?
         primary_serial_port->write(32);
         screen_column++;
         space_sent = 1;
      }
    }
  } else {
    if (cd_decoder_pin_state != last_state) {
      // we have a transition 
      element_duration = millis() - last_transition_time;
      if (element_duration > CW_DECODER_NOISE_FILTER) {                                    // filter out noise
        if (cd_decoder_pin_state == LOW) {  // we have a tone
          decode_elements[decode_element_pointer] = (-1 * element_duration);  // the last element was a space, so make it negative
          no_tone_count++;
          if (decode_element_no_tone_average == 0) {
            decode_element_no_tone_average = element_duration;
          } else {
            decode_element_no_tone_average = (element_duration + decode_element_no_tone_average) / 2;
          }
          decode_element_pointer++;
          last_state = LOW;
        } else {  // we have no tone
          decode_elements[decode_element_pointer] = element_duration;  // the last element was a tone, so make it positive 
          tone_count++;       
          if (decode_element_tone_average == 0) {
            decode_element_tone_average = element_duration;
          } else {
            decode_element_tone_average = (element_duration + decode_element_tone_average) / 2;
          }
          last_state = HIGH;
          decode_element_pointer++;
        }
        last_transition_time = millis();
        if (decode_element_pointer == 16) { decode_it_flag = 1; }  // if we've filled up the array, go ahead and decode it
      }
      
      
    } else {
      // no transition
      element_duration = millis() - last_transition_time;
      if (last_state == HIGH)  {
        // we're still high (no tone) - have we reached character space yet?        
        //if ((element_duration > (decode_element_no_tone_average * 2.5)) || (element_duration > (decode_element_tone_average * 2.5))) {
        if (element_duration > (float(1200/decoder_wpm)*CW_DECODER_SPACE_DECODE_THRESH)) {
          decode_it_flag = 1;
        }
      } else {
        // have we had tone for an outrageous amount of time?  
      }
    }
   }
 
 
 
 
  if (decode_it_flag) {                      // are we ready to decode the element array?

    // adjust the decoder wpm based on what we got
    
    if ((no_tone_count > 0) && (tone_count > 1)){ // NEW
    
      if (decode_element_no_tone_average > 0) {
        if (abs((1200/decode_element_no_tone_average) - decoder_wpm) < 5) {
          decoder_wpm = (decoder_wpm + (1200/decode_element_no_tone_average))/2;
        } else {
          if (abs((1200/decode_element_no_tone_average) - decoder_wpm) < 10) {
            decoder_wpm = (decoder_wpm + decoder_wpm + (1200/decode_element_no_tone_average))/3;
          } else {
            if (abs((1200/decode_element_no_tone_average) - decoder_wpm) < 20) {
              decoder_wpm = (decoder_wpm + decoder_wpm + decoder_wpm + (1200/decode_element_no_tone_average))/4;    
            }      
          }
        }
      }
    
    
    } // NEW
    
    #ifdef DEBUG_CW_DECODER_WPM
      if (abs(decoder_wpm - last_printed_decoder_wpm) > 0.9) {
        debug_serial_port->print("<");
        debug_serial_port->print(int(decoder_wpm));
        debug_serial_port->print(">");
        last_printed_decoder_wpm = decoder_wpm;
      }
    #endif //DEBUG_CW_DECODER_WPM
    
    for (uint8_t x = 0;x < decode_element_pointer; x++) {
      if (decode_elements[x] > 0) {  // is this a tone element?          
        // we have no spaces to time from, use the current wpm
        if ((decode_elements[x]/(1200/decoder_wpm)) < 2.1 /*1.3*/) {  // changed from 1.3 to 2.1 2015-05-12
          decode_character = (decode_character * 10) + 1; // we have a dit
        } else {
          decode_character = (decode_character * 10) + 2; // we have a dah
        }  
      }
      #ifdef DEBUG_CW_DECODER
        debug_serial_port->print(F("service_cw_decoder: decode_elements["));
        debug_serial_port->print(x);
        debug_serial_port->print(F("]: "));
        debug_serial_port->println(decode_elements[x]);
      #endif //DEBUG_CW_DECODER
    }

    #ifdef DEBUG_CW_DECODER
      debug_serial_port->print(F("service_cw_decoder: decode_element_tone_average: "));
      debug_serial_port->println(decode_element_tone_average);
      debug_serial_port->print(F("service_cw_decoder: decode_element_no_tone_average: "));
      debug_serial_port->println(decode_element_no_tone_average);
      debug_serial_port->print(F("service_cw_decoder: decode_element_no_tone_average wpm: "));
      debug_serial_port->println(1200/decode_element_no_tone_average);
      debug_serial_port->print(F("service_cw_decoder: decoder_wpm: "));
      debug_serial_port->println(decoder_wpm);
      debug_serial_port->print(F("service_cw_decoder: decode_character: "));
      debug_serial_port->println(decode_character);
    #endif //DEBUG_CW_DECODER


    primary_serial_port->write(convert_cw_number_to_ascii(decode_character));
    screen_column++;

    process_cw(uint8_t(convert_cw_number_to_ascii(decode_character)));


      
    // reinitialize everything
    last_transition_time = 0;
    last_decode_time = millis();
    decode_element_pointer = 0; 
    decode_element_tone_average = 0;
    decode_element_no_tone_average = 0;
    space_sent = 0;
    no_tone_count = 0;
    tone_count = 0;
  } //if (decode_it_flag)
  

    if (screen_column > CW_DECODER_SCREEN_COLUMNS) {
      primary_serial_port->println();
      screen_column = 0;
    }

  
}



//---------------------------------------------------------------------------------------------------------------------------


int convert_cw_number_to_ascii (long number_in)
{

  // number_in:  1 = dit, 2 = dah, 9 = a space

  switch (number_in) {
    case 12: return 65; break;         // A
    case 2111: return 66; break;
    case 2121: return 67; break;
    case 211: return 68; break;
    case 1: return 69; break;
    case 1121: return 70; break;
    case 221: return 71; break;
    case 1111: return 72; break;
    case 11: return 73; break;
    case 1222: return 74; break;
    case 212: return 75; break;
    case 1211: return 76; break;
    case 22: return 77; break;
    case 21: return 78; break;
    case 222: return 79; break;
    case 1221: return 80; break;
    case 2212: return 81; break;
    case 121: return 82; break;
    case 111: return 83; break;
    case 2: return 84; break;
    case 112: return 85; break;
    case 1112: return 86; break;
    case 122: return 87; break;
    case 2112: return 88; break;
    case 2122: return 89; break;
    case 2211: return 90; break;    // Z

    case 22222: return 48; break;    // 0
    case 12222: return 49; break;
    case 11222: return 50; break;
    case 11122: return 51; break;
    case 11112: return 52; break;
    case 11111: return 53; break;
    case 21111: return 54; break;
    case 22111: return 55; break;
    case 22211: return 56; break;
    case 22221: return 57; break;
    case 112211: return '?'; break;  // ?
    case 21121: return 47; break;   // /
    case 2111212: return '*'; break; // BK 
    case 221122: return 44; break;  // ,
    case 121212: return '.'; break;
    case 122121: return '@'; break;
    case 222222: return 92; break;  // special hack; six dahs = \ (backslash)
    case 21112: return '='; break;  // BT
    case 111111:
    case 1111111:
    case 11111111:
    case 111111111: 
      return 254; break;  // clear out the buffer
    case 9: return 32; break;       // special 9 = space



    #ifdef OPTION_NON_ENGLISH_EXTENSIONS
      // for English/Cyrillic/Western European font LCD controller (HD44780UA02):
      case 12212: return 197; break;     // 'Å' - AA_capital (OZ, LA, SM)
      //case 12212: return 192; break;   // 'À' - A accent   
      case 1212: return 198; break;      // 'Æ' - AE_capital   (OZ, LA)
      //case 1212: return 196; break;    // 'Ä' - A_umlaut (D, SM, OH, ...)
      case 2222: return 138; break;      // CH  - (Russian letter symbol)
      case 22122: return 209; break;     // 'Ñ' - (EA)               
      //case 2221: return 214; break;    // 'Ö' – O_umlaut  (D, SM, OH, ...)
      //case 2221: return 211; break;    // 'Ò' - O accent
      case 2221: return 216; break;      // 'Ø' - OE_capital    (OZ, LA)
      case 1122: return 220; break;      // 'Ü' - U_umlaut     (D, ...)
      case 111111: return 223; break;    // beta - double S    (D?, ...)   
      case 21211: return 199; break;     // Ç
      case 11221: return 208; break;     // Ð
      case 12112: return 200; break;     // È
      case 11211: return 201; break;     // É
      case 221121: return 142; break;    // Ž
    #endif //OPTION_NON_ENGLISH_EXTENSIONS


    default: 
      return UNKNOWN_CW_CHARACTER; 
      break;

  }

}

//---------------------------------------------------------------------------------------------------------------------------

void process_cw(uint8_t incoming_cw_char){

  char unlock_phrase[] = UNLOCK_PHRASE;

  static uint8_t unlock_phrase_received = 0;
  static unsigned long unlock_inactivity_time = 0;
  static unsigned long unlock_hard_time = 0;
  static unsigned long buffer_inactivity_time = 0;
  static String incoming_cw_char_buffer = "";


  if ((unlock_phrase_received == 254) && (((millis()-unlock_inactivity_time) > UNLOCK_INACTIVITY_TIMEOUT_MS) || ((millis()-unlock_hard_time) > (UNLOCK_HARD_TIMEOUT_SECS*1000)))){  // check for unlock code inactivity timeout
    unlock_phrase_received = 0;
    digitalWrite (pin_unlocked, LOW);
    #if defined(DEBUG_PROCESS_CW)
	    debug_serial_port->println("\r\nprocess_cw: unlock timeout");
	  #endif   
  }

  if ((millis() - buffer_inactivity_time) > BUFFER_INACTIVITY_TIME_MS){
    incoming_cw_char_buffer = "";
  }

  if (incoming_cw_char == 32){ //filter out spaces
    return;
  }  

  if (unlock_phrase_received < 254){    // are we still waiting for unlock phrase?     

      if (incoming_cw_char == unlock_phrase[unlock_phrase_received]){
        unlock_phrase_received++;

	    if (unlock_phrase[unlock_phrase_received] == 0){  // did we get to the end of the string?
	      unlock_phrase_received = 254;       
	      digitalWrite (pin_unlocked, HIGH);
	      unlock_inactivity_time = millis();
        unlock_hard_time = millis();
        incoming_cw_char_buffer = "";
	      #if defined(DEBUG_PROCESS_CW)
	        debug_serial_port->println("\r\nprocess_cw: unlock phrase received");
	      #endif
	    }

      } else {
        unlock_phrase_received = 0;

      }

  } else { // we are currently in unlock mode
    unlock_inactivity_time = millis();
    if (incoming_cw_char == 254){  // clear out the buffer if we got an operator error cw char (i.e. di di di di di dit ...)
      incoming_cw_char_buffer = "";
      #if defined(DEBUG_PROCESS_CW)
        debug_serial_port->println("\r\nprocess_cw: clear out buffer");
      #endif             
    } else {
      if (incoming_cw_char_buffer.length() < 255){
        incoming_cw_char_buffer.concat((char)incoming_cw_char);
        buffer_inactivity_time = millis();
        #if defined(DEBUG_PROCESS_CW)
          debug_serial_port->print("\r\nprocess_cw: incoming_cw_char_buffer:[");
          debug_serial_port->print(incoming_cw_char_buffer);
          debug_serial_port->println("]");
        #endif   

        for (int x = 0;x < sizeof(control_phrases_pins);x++){
          if (incoming_cw_char_buffer.compareTo(control_phrases_pin_high[x]) == 0){
            #if defined(DEBUG_PROCESS_CW)
              debug_serial_port->print("\r\nprocess_cw: control_phrases_pin_high ");
              debug_serial_port->println(control_phrases_pins[x]);
            #endif   
            incoming_cw_char_buffer = ""; 
            digitalWrite(control_phrases_pins[x],HIGH);         
          }
        }


        for (int x = 0;x < sizeof(control_phrases_pins);x++){
          if (incoming_cw_char_buffer.compareTo(control_phrases_pin_low[x]) == 0){
            #if defined(DEBUG_PROCESS_CW)
              debug_serial_port->print("\r\nprocess_cw: control_phrases_pin_low ");
              debug_serial_port->println(control_phrases_pins[x]);
            #endif   
            incoming_cw_char_buffer = ""; 
            digitalWrite(control_phrases_pins[x],LOW);         
          }
        }        

      } else {
        #if defined(DEBUG_PROCESS_CW)
          debug_serial_port->println("\r\nprocess_cw: incoming_cw_char_buffer full");
        #endif           
      }
    }
  }


}
