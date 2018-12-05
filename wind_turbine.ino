#include <EtherCard.h>
#include <OneWire.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>

char char2[7]="&field";
char char1[6]=""; //bedzie przechowywac chwilowo wartosci w formie znakow 
char wpis[100]; //bufor
int k=0; //licznik wpisywania znakow do bufora
float napiecie=NULL, natezenie=NULL, moc=NULL, cisnienie=NULL, t1=NULL, t2=NULL;
bool czy_liczyc=1;
Adafruit_BMP085 bmp; // inicjaliza barometru

OneWire ds(7);  // 7 pin - pin termometru

static uint8_t mymac[6] = {0x54, 0x55, 0x58, 0x10, 0x00, 0x24}; // ustawienie adresu mac
static uint8_t ip[4] = {192, 168, 0, 115};         // ustawienie adresu IP
static uint16_t port = 80;  //ustawienie portu
byte Ethernet::buffer[700]; //zdefiniowanie wielkości bufora
static uint32_t timer;
const char website[] PROGMEM = "api.thingspeak.com";

//liczenie obrotów
 double obroty;
 unsigned long t=0;
 unsigned long start;
 int x; //aktualny stan czujnika odbiciowego
 int y; //poprzedni stan czujnika odbiciowego
 int n=0;
 unsigned long t0=0, tx; //zmienne przechowujące czasy procesora - poprzedni i aktualny
 #define tab 10 //obroty pomiarow do usrednienia
 int pred[tab]={0,0,0,0,0,0,0,0,0,0};
 int ruch= 6; //pin sterowania siłownikiem
 int skok= 5000;

void setup () 
{
  Serial.begin(57600);
  bmp.begin(); //inicjalizacja barometru
  pinMode(ruch, OUTPUT); 
}

//funkcja wpisywania wartosci do bufora
void wpisz (char fieldnumber, float wartosc,  int cyfry, int poprzecinku) 
	{
		if (wartosc!=NULL)
			{
				for(int i=0;i<6;i++) //wpisanie slowa &field1=, &field2=    itd.
					{
					   wpis[k]=char2[i];
					   k++;
					}
				wpis[k]=fieldnumber; k++;
				wpis[k]='='; k++;
				memset( (void *)char1, '\0', sizeof(char1));
				dtostrf(wartosc,cyfry,poprzecinku,char1); //przepisanie wartosci do char1
				for(int i=0;i<cyfry;i++) //przepisanie char1 do bufora "wpis"
				    if (char1[i]!=' ')
					        {
					          wpis[k]=char1[i];
					          k++;
					        }
			}
	}
	
//funkcja usuwająca wartości, jest to istotne, gdyż w przeciwnym razie w przypadku uszkodzenia sensora ciagle był by wysyłany ten sam wynik, co opoźnia diagnozę awarii
void zer ()
	{
		napiecie=NULL;
		natezenie=NULL,
		moc=NULL,
		cisnienie=NULL,
		t1=NULL,
		t2=NULL,
	}

void loop ()  //cześć programu wykonywana w pętli 
{  
  licz_obr();
  ruch_ogona();
  if (millis() > timer) 
    {
      czy_liczyc=0;
      timer = millis() + 5000;
      
     //przykladowe wartosci 
      natezenie=(analogRead(A1)*5./1024.-0.5)/0.133; //przeliczenie natężenia prądu za pomocą funkcji dostarczonej w specyfikacji przez producenta czujnika 
      napiecie=analogRead(A0)*(37.1+2.11)*5./(2.11*1024.); //obliczanie napięcia na podstawie użytych oporników w dzielniki napięcia
      moc=napiecie*natezenie; //obliczenie mocy
      cisnienie=bmp.readPressure(); //odczytanie cisnienia
	  t1 = getTemp(); //odczytanie temperatury 1
	  t2=bmp.readTemperature(); //odczytanie temperatury 2
      k=0; //wyzerowanie licznika wpisywania znakow do bufora  
      memset( (void *)wpis, '\0', sizeof(wpis)); //wyzerowanie bufora
    
	//wpisywanie wartosci do bufora, struktura: wpisz(numer kanału, zmienna, ilosc cyfr rozwiniecia dziesiętnego, ilosc cyfr po przecinku); 
      wpisz('1',obroty,4,0); 
      wpisz('2',napiecie,4,1);
      wpisz('3',natezenie,5,2);
      wpisz('4',moc,5,0);
      wpisz('5',cisnienie,6,0);
      wpisz('6',t1,5,2);
      wpisz('7',t2,5,2);
      zer(); //zerowanie wartosci
     }
}

//funkcja zwracająca temperaturę
float getTemp()
{
	byte data[12];
	byte addr[8];
	if ( !ds.search(addr)) 
		{
			ds.reset_search();
			return NULL;
		}
	ds.reset();
	ds.select(addr);
	ds.write(0x44,1); 
	byte present = ds.reset();
	ds.select(addr);
	ds.write(0xBE); 
	for (int i = 0; i < 9; i++) 
		{
			data[i] = ds.read();
		}
	ds.reset_search();
	byte MSB = data[1];
	byte LSB = data[0];
	float tempRead = ((MSB << 8) | LSB);
	float TemperatureSum = tempRead / 16;
	return TemperatureSum;
}

//funkcja obliczania prędkości obrotowej na podstawie wskazań czynnika odbiciowego
void licz_obr()
{
   int a=analogRead(A3); //wartość z czujnika odbiciowego
   int b; //wartość określająca skwantowaną odczytaną jasność powierzchni
   if(a>500) b=HIGH;
   else b=LOW;  
   x=b;
   if(x!=y)
   {
     y=x;
     if (x==0)
     {
       n=n+1;
       tx=millis(); //oczytanie czasu procesora
       t=tx-t0; //obliczenie czasu od ostatniej zmiany koloru (czarny->biały)
       t0=tx;

       if(czy_liczyc)
       {
         for (int i=tab-1;i>0;i--)   //przepisywanie w dol wartości w tablicy
             pred[i]=pred[i-1];
         
         pred[0]=60000./t;     //obliczenie prędkości obrotowej w obr/min
         obroty=0;	
         for (int i=0;i<tab;i++)
             obroty=obroty+pred[i];
         obroty=obroty/(double)tab; //obliczenie wartości prędkości obrotowej jako średniej z ostatnich 10 pomiarów
       }
       else
        czy_liczyc=1;
     }
   }
   if (millis()-t0>10000) obroty=0.; //jeśli nie ma odczytu przez 10 sekund, przyjmowana jesr prędkość obrotowa = 0 
}
 
//funkcja sterująca ruchami ogona
void ruch_ogona()
{
    if(obroty>500)
      zsun();
    else
     {
       if (obroty<480)
          rozsun();
     } 
}
    
//funkcja do rozsuwania silownika
void zsun()    
    {
      digitalWrite(ruch, LOW); 
    }
    
//funkcja do zsuwania silownika
void rozsun()
    {
      digitalWrite(ruch, HIGH);
    }
    



