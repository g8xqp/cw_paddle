// Test program to check arduino compiler on new computer
// configured correctly (generates cw from iambic key).
// dmc 2018  ---  build for nano

#include "Arduino.h"

#define PIN_PADDLE_DOT   4
#define PIN_PADDLE_DASH  5
#define PIN_SPEED_B0     7
#define PIN_SPEED_B1     8
#define PIN_SPEED_B2     9
#define PIN_SPEED_B3    10
//#define PIN_SPEED_B4    11
#define PIN_TONE        12
#define PIN_TX          13

#define SPEED_BASE 10

#define VERSION_LINE1 "Program source " __FILE__
#define VERSION_LINE2 "Compiled on " __DATE__ " at " __TIME__ " - m0dmc"

void ShowVersion(int wpm){
  Serial.println("This program generates iambic key tones.");
  Serial.print("Speed setting (wpm) = ");
  Serial.println(wpm);
  Serial.print("Paddle dot pin  = ");
  Serial.println(PIN_PADDLE_DOT);
  Serial.print("Paddle dash pin = ");
  Serial.println(PIN_PADDLE_DASH);
  Serial.print("Speed bit0      = ");
  Serial.println(PIN_SPEED_B0);
  Serial.print("Speed bit1      = ");
  Serial.println(PIN_SPEED_B1);
  Serial.print("Speed bit2      = ");
  Serial.println(PIN_SPEED_B2);
  Serial.print("Speed bit3      = ");
  Serial.println(PIN_SPEED_B3);
#ifdef PIN_SPEED_B4
  Serial.print("Speed bit4      = ");
  Serial.println(PIN_SPEED_B4);
#endif
  Serial.print("Speed range = ");
  Serial.print(SPEED_BASE);
  Serial.print(" to ");
#ifdef PIN_SPEED_B4
  Serial.println(SPEED_BASE+31);
#else
  Serial.println(SPEED_BASE+15);
#endif
  Serial.print("Tone output     = ");
  Serial.println(PIN_TONE);
  Serial.print("Tx enable out   = ");
  Serial.println(PIN_TX);

  Serial.println(VERSION_LINE1);
  Serial.println(VERSION_LINE2);
}
void Tx(boolean ToneEnable){
  digitalWrite(PIN_TX,!ToneEnable);
}
byte ReadPaddle(){
  byte p;
  p  =(digitalRead(PIN_PADDLE_DASH)?0:2);
  p +=(digitalRead(PIN_PADDLE_DOT)?0:1);
  return(p);
}
int ReadSpeed(){
  int s=SPEED_BASE;
  s+=(digitalRead(PIN_SPEED_B0)?0:1);
  s+=(digitalRead(PIN_SPEED_B1)?0:2);
  s+=(digitalRead(PIN_SPEED_B2)?0:4);
  s+=(digitalRead(PIN_SPEED_B3)?0:8);
#ifdef PIN_SPEED_B4
  s+=(digitalRead(PIN_SPEED_B4)?0:16);
#endif
  return(s);
}
void SetupPinIO(){
  pinMode(PIN_PADDLE_DOT,INPUT_PULLUP);
  pinMode(PIN_PADDLE_DASH,INPUT_PULLUP);
  pinMode(PIN_SPEED_B0,INPUT_PULLUP);
  pinMode(PIN_SPEED_B1,INPUT_PULLUP);
  pinMode(PIN_SPEED_B2,INPUT_PULLUP);
  pinMode(PIN_SPEED_B3,INPUT_PULLUP);
#ifdef PIN_SPEED_B4
  pinMode(PIN_SPEED_B4,INPUT_PULLUP);
#endif
  pinMode(PIN_TX,OUTPUT);
  pinMode(PIN_TONE,OUTPUT);
  Tx(false);
}

//#define XTAL_FREQ 20000000
#define XTAL_FREQ 16000000
#define MAXFREQ 30000

#define TONE_HZ 1000

class IRQfreq{
private:
  int Prescale;
  int PreCount;
  int LastFreq;
public: 
  int Tcnt_1;
  void FreqSet(int f){
    int Freq;
    if (f!=LastFreq){
      LastFreq=f;
      switch (f){
      case 0:
      case 1:
	Prescale=2000;
	Freq=2000;
	break;
      case 2 ... 499:
	Prescale=2000/f;
	Freq=f*Prescale;
	break;
      case 500 ... (MAXFREQ - 1):
	Freq=f;
	Prescale=1;
	break;
      default:
	Freq=MAXFREQ;
	Prescale=1;
	break;
      }
      Tcnt_1=65536-XTAL_FREQ/Freq;
    }
  }
  bool CheckPrescale(){
    PreCount++;
    if (PreCount>=Prescale)PreCount=0;
    return(PreCount==0?true:false);
  }
#define SETBIT(x) ( 1 << x)
  void FreqInit(){
    FreqSet(1000); 
    noInterrupts();
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = Tcnt_1;
    TCCR1B |= SETBIT(CS10);   // hardware prescalar div1 
    TIMSK1 |= SETBIT(TOIE1);  // enable timer overflow interrupt
    interrupts();             // enable all interrupts
  }
  int FreqGet(){
    return(LastFreq);
  }
#undef SETBIT
  IRQfreq():PreCount(0),LastFreq(0){}
  ~IRQfreq(){}
}IRQfreq;

// Timing per dot (s) = 1.2/wpm
// 0.10s  for 12 wpm
// 0.06s  for 20 wpm
int DotPeriodCount=100;
void SetupDotTimeWPM(float wpm){
  float k;
  k = IRQfreq.FreqGet();
  DotPeriodCount=k*1.2/wpm;
}

enum DotState {Idle,Dot1,Dot2,Dash1,Dash2,Dash3,Dash4,Gap1,Gap2};
volatile DotState IambicState = Idle ;
volatile boolean DotPeriodEndFlag=false;
volatile int PeriodCount=0;
volatile boolean Buzz=false;

ISR(TIMER1_OVF_vect){
  TCNT1 = IRQfreq.Tcnt_1;
  if(IRQfreq.CheckPrescale()){
    // events to happen at chosen frequency
    PeriodCount++;
    switch(IambicState){
    default:
    case Idle:
      PeriodCount=0;
      DotPeriodEndFlag=true;
      break;
    case Dot1:
    case Dash1:
    case Dash2:
    case Dash3:
      digitalWrite(PIN_TONE,Buzz=!Buzz);
    case Dot2:
    case Dash4:
      if(PeriodCount==DotPeriodCount){
        PeriodCount=0;
        DotPeriodEndFlag=true;
      }
      break;
    }
  }
}

void setup() {
  int wpm;
  Serial.begin(9600);
  IRQfreq.FreqInit();
  IRQfreq.FreqSet(2*TONE_HZ);
  SetupPinIO();
  wpm=ReadSpeed();
  SetupDotTimeWPM(wpm);
  delay(100);
  ShowVersion(wpm);
}
byte DotDashChar=0;
byte DotDashN=0;
void SetStateDot(){
  Tx(true);
  IambicState=Dot1;
  if(DotDashN<=7){
    DotDashChar|=(1<<DotDashN);
    DotDashN++;
  }
}
void SetStateDash(){
  Tx(true);
  IambicState=Dash1;
  if(DotDashN<=7)DotDashN++;
}
#define ERR_CHAR "[*]"
int LineChars=0;
void SetStateGap(){
  byte x;
  char MorseString[5]; 
  IambicState=Gap1;
  x=DotDashChar&((1<<DotDashN)-1);
  strcpy(MorseString,ERR_CHAR); 
  // identify last character
  switch(DotDashN){
  case 1:
    switch (x){
    case B0:
      strcpy(MorseString,"t");
      break;
    case B1:
      strcpy(MorseString,"e");
      break;
    }
    break;
  case 2:
    switch (x){
    case B00:
      strcpy(MorseString,"m");
      break;
    case B01:
      strcpy(MorseString,"a");
      break;
    case B10:
      strcpy(MorseString,"n");
      break;
    case B11:
      strcpy(MorseString,"i");
      break;
    }
    break;
  case 3:
    switch (x){
    case B000:
      strcpy(MorseString,"o");
      break;
    case B001:
      strcpy(MorseString,"w");
      break;
    case B010:
      strcpy(MorseString,"k");
      break;
    case B011:
      strcpy(MorseString,"u");
      break;
    case B100:
      strcpy(MorseString,"g");
      break;
    case B101:
      strcpy(MorseString,"r");
      break;
    case B110:
      strcpy(MorseString,"d");
      break;
    case B111:
      strcpy(MorseString,"s");
      break;
    }
    break;
  case 4:
    switch (x){
    case B0001:
      strcpy(MorseString,"j");
      break;
    case B0010:
      strcpy(MorseString,"y");
      break;
    case B0100:
      strcpy(MorseString,"q");
      break;
    case B0110:
      strcpy(MorseString,"x");
      break;
    case B0111:
      strcpy(MorseString,"v");
      break;
    case B1001:
      strcpy(MorseString,"p");
      break;
    case B1010:
      strcpy(MorseString,"c");
      break;
    case B1011:
      strcpy(MorseString,"f");
      break;
    case B1100:
      strcpy(MorseString,"z");
      break;
    case B1101:
      strcpy(MorseString,"l");
      break;
    case B1110:
      strcpy(MorseString,"b");
      break;
    case B1111:
      strcpy(MorseString,"h");
      break;
    }
    break;
  case 5:
    switch (x){
    case B00001:
      strcpy(MorseString,"1");
      break;
    case B00011:
      strcpy(MorseString,"2");
      break;
    case B00111:
      strcpy(MorseString,"3");
      break;
    case B01111:
      strcpy(MorseString,"4");
      break;
    case B11111:
      strcpy(MorseString,"5");
      break;
    case B11110:
      strcpy(MorseString,"6");
      break;
    case B11100:
      strcpy(MorseString,"7");
      break;
    case B11000:
      strcpy(MorseString,"8");
      break;
    case B10000:
      strcpy(MorseString,"9");
      break;
    case B00000:
      strcpy(MorseString,"0");
      break;
    case B10110:
      strcpy(MorseString,"/");
      break;
    }
    break;
  case 6:
    switch (x){
    case B110011:
      strcpy(MorseString,"?");
      break;
    case B001100:
      strcpy(MorseString,"!");
      break;
    case B100001:
      strcpy(MorseString,"'");
      break;
    case B011110:
      strcpy(MorseString,"-");
      break;
    case B101101:
      strcpy(MorseString,"\"");
      break;
    case B010010:
      strcpy(MorseString,"|");
      break;
    }
  }
  LineChars+=strlen(MorseString);
  if(LineChars>60){
    LineChars=0;
    Serial.println(MorseString);
  } else {
    Serial.print(MorseString);
  }
  DotDashChar=0;
  DotDashN=0;
}
void ChangeState(byte Paddle){
  switch(IambicState){
  default:
  case Idle:
    switch (Paddle){
    default:
    case 0:
      break;
    case 1:
    case 3:
      SetStateDot();
      break;
    case 2:
      SetStateDash();
      break;
    }
    break;
  case Dot1:
  case Dash1:
    Tx(false);
  case Dash2:
  case Dash3:
  case Gap1:
    IambicState=(DotState)(IambicState+1);
    break;
  case Gap2:
    IambicState=Idle;
    break;
  case Dot2:
    switch (Paddle){
    default:
    case 0:
      SetStateGap();
      break;
    case 1:
      SetStateDot();
      break;
    case 2:
    case 3:
      SetStateDash();
      break;
    }
    break;
  case Dash4:
    switch (Paddle){
    default:
      SetStateGap();
      break;
    case 1:
    case 3:
      SetStateDot();
      break;
    case 2:
      SetStateDash();
      break;
    }
    break;
  }
}
void loop() {
  while(true){
    if(DotPeriodEndFlag){
      DotPeriodEndFlag=false;
      ChangeState(ReadPaddle());
    }
  }
}
