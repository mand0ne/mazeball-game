/*
* NAPISALI: Mandal Anel i Mehmedagić Lejla;
* PROJEKTNI ZADATAK IZ UGRADBENIH SISTEMA;
* Elektrotehnicki fakultet Sarajevo
* Akademska 2018/19 godina
*/

#include "mbed.h"
#include <stack>
#include "SPI_TFT_ILI9341.h"
#include "Arial12x12.h"

                     // mosi, miso, sclk, cs, rst, dc
SPI_TFT_ILI9341 DISPLAY(PTD2, PTD3, PTD1, D5, D4,  D3, "TFT");
Serial WIFI(PTE0, PTE1);
Serial PC(USBTX, USBRX);    // Za testiranje kroz PuTTY

// Odstupanja od ekrana zbog crtanja okvira
const int paddingX = 31;
const int paddingY = 36;

enum Strana {GORE, DESNO, DOLJE, LIJEVO};
enum Stanje {MENU, IGRA, INSTRUKCIJE, BRZINA, ONAMA};

// Brzina kretanja loptice
int brzina = 1;

// Stoperica
Ticker tiker;

Stanje stanje; 

// Volatile za heads-up compileru (zbog Ticker-a)
volatile int trenutnoVrijeme = 0;
volatile bool promjenaVremena = true;  

void crtajOkvir(bool modIgra){

    int gornjiRed = 12;
    int donjiRed = 220;
    int lijevaKolona = 12;
    int desnaKolona = 300;

    int udaljenostKruzica = 8;
    int radijusKruzica = 3;

    for (int i = lijevaKolona; i<=desnaKolona; i+=udaljenostKruzica) {          // 37 kruzica u redu
        if (i >= 15 * 8 + 12 && i <= 22 * 8 + 12) {
            if (!modIgra)
                DISPLAY.fillcircle(i, gornjiRed, radijusKruzica, LightGrey);
        } else
            DISPLAY.fillcircle(i, gornjiRed, radijusKruzica, LightGrey);

        DISPLAY.fillcircle(i, donjiRed, radijusKruzica, LightGrey);
    }

    for (int i = gornjiRed; i<=donjiRed; i+=udaljenostKruzica) {                // 27 kruzica u koloni
        DISPLAY.fillcircle(lijevaKolona, i, radijusKruzica, LightGrey);
        DISPLAY.fillcircle(desnaKolona, i, radijusKruzica, LightGrey);
    }
}

// Cita znak poslan WiFi modulu
char ucitajZnak(char x) {
    char c;
    while(WIFI.readable()){
        c = WIFI.getc();
        PC.putc(c);
        if(c == '+'){
            while(1){
                if(WIFI.readable()){
                    c = WIFI.getc();
                    PC.putc(c);
                    if(c == ':')
                        break;
                 }
            }
            while(1){
                if(WIFI.readable()){
                    c = WIFI.getc();
                    PC.putc(c);
                    return c;
                }
            }
        }
    }
    return x;
} 

// Za azuriranje stoperice tokom igre
void azurirajVrijeme(){
    trenutnoVrijeme++;
    promjenaVremena = true;
}

// Funkcija koja daje sekvencu pseudo slučajnih brojeva
// Preuzeto iz knjige Numerical Recipes, William H. Press, Saul A. Teukolsky, William T. Vetterling i Brian P. Flannery.
unsigned int SEED = time(NULL);
int randomBroj(unsigned int* nSEED, int bound){
    *nSEED = (1664525 * *nSEED + 1013904223) % 4294967296;
    return *nSEED % bound;
}


class Polje {
private:

    int red;
    int kolona;
    bool posjeceno;
    bool zidovi[4];

public:

    Polje(){
        red = 0;
        kolona = 0;
        posjeceno = false;
        zidovi[0] = zidovi[1] = zidovi[2] = zidovi[3] = true;
    }

    Polje(int red, int kolona){
        this->red = red;
        this->kolona = kolona;
        posjeceno = false;
        zidovi[0] = zidovi[1] = zidovi[2] = zidovi[3] = true;
    }

    int dajRed(){
        return red;
    }

    int dajKolonu(){
        return kolona;
    }

    void postaviRed(int red){
        this->red = red;
    }

    void postaviKolonu(int kolona){
        this->kolona = kolona;
    }

    bool jeLiPosjeceno(){
        return posjeceno;
    }

    void postaviPosjeceno(bool b){
        posjeceno = b;
    }
    
    void srusiZid(int Strana){
        zidovi[Strana] = false;
    }

    bool postojiZid(int Strana){
        return zidovi[Strana];
    }
};

class Labirint {
private:

    static const int brojRedova = 12;
    static const int brojKolona = 18;
    static const int sirinaPolja = 14;
    std::stack<int> stek;

    Polje tabla[216];
    
    int indexTrenutnogPolja;
    int lopticaX, lopticaY;
    int ciljX, ciljY;

    // 'Konvertuje 2D index u 1D'
    int index(int red, int kolona){
        if (red < 0 || kolona < 0 || red > brojRedova - 1 || kolona > brojKolona - 1)
            return -1;

        return kolona + red * brojKolona;
    }
    
    void srusiZidove(int trenutnoPolje, int susjednoPolje){

        int y = tabla[trenutnoPolje].dajKolonu() - tabla[susjednoPolje].dajKolonu();

        if (y == 1) {                               // Susjed je lijevo od trenutnog
            tabla[trenutnoPolje].srusiZid(LIJEVO);
            tabla[susjednoPolje].srusiZid(DESNO);
            return;
        } else if (y == -1) {                       // Susjed je desno od trenutnog
            tabla[trenutnoPolje].srusiZid(DESNO);
            tabla[susjednoPolje].srusiZid(LIJEVO);
            return;
        }

        int x = tabla[trenutnoPolje].dajRed() - tabla[susjednoPolje].dajRed();

        if (x == 1) {                               // Susjed je iznad trenutnog
            tabla[trenutnoPolje].srusiZid(GORE);
            tabla[susjednoPolje].srusiZid(DOLJE);
        } else if (x == -1) {                       // Susjed je ispod trenutnog
            tabla[trenutnoPolje].srusiZid(DOLJE);
            tabla[susjednoPolje].srusiZid(GORE);
        }
    }

    int dajIndexSlucajnogSusjeda(Polje polje){
        // 4 dostizna susjeda
        int susjedi[4];
        int brojSusjeda = 0;

        int gornjiIndex     = index(polje.dajRed() - 1, polje.dajKolonu());
        int desniIndex      = index(polje.dajRed(),     polje.dajKolonu() + 1);
        int donjiIndex      = index(polje.dajRed() + 1, polje.dajKolonu());
        int lijeviIndex     = index(polje.dajRed(),     polje.dajKolonu() - 1);

        if (gornjiIndex != -1) {
            if (!tabla[gornjiIndex].jeLiPosjeceno()) {
                susjedi[brojSusjeda] = gornjiIndex;
                brojSusjeda++;
            }
        }
        if (desniIndex != -1) {
            if (!tabla[desniIndex].jeLiPosjeceno()) {
                susjedi[brojSusjeda] = desniIndex;
                brojSusjeda++;
            }
        }
        if (lijeviIndex != -1) {
            if (!tabla[lijeviIndex].jeLiPosjeceno()) {
                susjedi[brojSusjeda] = lijeviIndex;
                brojSusjeda++;
            }
        }
        if (donjiIndex != -1) {
            if (!tabla[donjiIndex].jeLiPosjeceno()) {
                susjedi[brojSusjeda] = donjiIndex;
                brojSusjeda++;
            }
        }

        if (brojSusjeda > 0) {
            int slucajanIndex = randomBroj(&SEED, brojSusjeda);
            return susjedi[slucajanIndex];
        }

        return 0;
    }

    void crtajPolje(Polje polje){
        int x = polje.dajKolonu() * sirinaPolja + paddingX;
        int y = polje.dajRed() * sirinaPolja + paddingY;

        // Ako nema zidova
        DISPLAY.fillrect(x,                   y - 2,               x + sirinaPolja - 2, y,                   DarkCyan);     // GORE
        DISPLAY.fillrect(x + sirinaPolja - 2, y,                   x + sirinaPolja,     y + sirinaPolja - 2, DarkCyan);     // DESNO
        DISPLAY.fillrect(x,                   y + sirinaPolja - 2, x + sirinaPolja - 2, y + sirinaPolja,     DarkCyan);     // DOLJE
        DISPLAY.fillrect(x - 2,               y,                   x,                   y + sirinaPolja - 2, DarkCyan);     // LIJEVO

        // Ako ima zidova
        if (polje.postojiZid(GORE))
            DISPLAY.fillrect(x - 2,               y - 2,               x + sirinaPolja,     y,               White);
        if (polje.postojiZid(DESNO))
            DISPLAY.fillrect(x + sirinaPolja - 2, y - 2,               x + sirinaPolja,     y + sirinaPolja, White);
        if (polje.postojiZid(DOLJE))
            DISPLAY.fillrect(x - 2,               y + sirinaPolja - 2, x + sirinaPolja,     y + sirinaPolja, White);
        if (polje.postojiZid(LIJEVO))
            DISPLAY.fillrect(x - 2,               y - 2,               x,                   y + sirinaPolja, White);

    }
    
    // Koristi Depth-first search (DFS) algoritam za generisanje random labirinta
    void generisiLabirint(){

        // Loptica na pocetku
        lopticaX = 37;
        lopticaY = 42;
        crtajLopticu();

        // Cilj/Kraj je random polje u desnoj polovici Labirinta
        ciljX = paddingX + (9 + randomBroj(&SEED, 9))    * sirinaPolja + 6;
        ciljY = paddingY + randomBroj(&SEED, brojRedova) * sirinaPolja + 6;
        DISPLAY.fillcircle(ciljX, ciljY, 4, Black);

        tabla[indexTrenutnogPolja].postaviPosjeceno(true);
        stek.push(indexTrenutnogPolja);

        while(!stek.empty()) {

            int neighbourIndex = dajIndexSlucajnogSusjeda(tabla[indexTrenutnogPolja]);

            if (neighbourIndex != 0) {
                tabla[neighbourIndex].postaviPosjeceno(true);

                srusiZidove(indexTrenutnogPolja, neighbourIndex);
                crtajPolje(tabla[indexTrenutnogPolja]);

                indexTrenutnogPolja = neighbourIndex;
                stek.push(indexTrenutnogPolja);
            } else if (!stek.empty()) {
                indexTrenutnogPolja = stek.top();
                stek.pop();
            }
        }

        // Okvir labirinta
        DISPLAY.fillrect(28,   33,  284,   36,  White); // GORE
        DISPLAY.fillrect(28,   33,   31,  205,  White); // LIJEVO
        DISPLAY.fillrect(28,  202,  284,  205,  White); // DOLJE
        DISPLAY.fillrect(281,  33,  284,  205,  White); // DESNO

        // Stoperica
        DISPLAY.locate(145, 5);
        DISPLAY.printf("0:00");
    }

public:

    Labirint(){
        indexTrenutnogPolja = 0;
        lopticaX = 0;
        lopticaY = 0;

        // Podesi tablu
        for (int j(0); j < brojRedova; j++)
            for (int i(0); i < brojKolona; i++) {
                Polje polje(j, i);
                tabla[index(j, i)] = polje;
            }

        generisiLabirint();
    }

    // Funkcije za kretanje loptice kroz labirint
    void idiGore(){
        
        if ( (((lopticaY - 5) - 36) % 14) == 0 && tabla[index((lopticaY - 36) / 14, (lopticaX - 31) / 14)].postojiZid(GORE) ) 
            return;

        obrisiLopticu();
        lopticaY--;
        crtajLopticu();

        if ( index((lopticaY - 36) / 14, (lopticaX - 31) / 14) == index((ciljY - 36) / 14, (ciljX - 31) / 14) )
            dostignutCilj();
            
        wait_ms((3 - brzina) * 10);
    }

    void idiDolje(){
        
        if ( (((lopticaY + 5) - 36 + 2) % 14) == 0 && tabla[index((lopticaY - 36) / 14, (lopticaX - 31) / 14)].postojiZid(DOLJE) ) 
            return;
            
        obrisiLopticu();
        lopticaY++;
        crtajLopticu();

        if ( index((lopticaY - 36) / 14, (lopticaX - 31) / 14) == index((ciljY - 36) / 14, (ciljX - 31) / 14) )
            dostignutCilj();
            
        wait_ms((3 - brzina) * 10);
    }

    void idiDesno(){
        
        if ( (((lopticaX + 5) - 31 + 2) % 14) == 0 && tabla[index((lopticaY - 36) / 14, (lopticaX - 31) / 14)].postojiZid(DESNO) ) 
            return;


        obrisiLopticu();
        lopticaX++;
        crtajLopticu();

        if ( index((lopticaY - 36) / 14, (lopticaX - 31) / 14) == index((ciljY - 36) / 14, (ciljX - 31) / 14) )
            dostignutCilj();
            
        wait_ms((3 - brzina) * 10);
    }

    void idiLijevo(){
        
        if ( (((lopticaX - 5) - 31) % 14) == 0 && tabla[index((lopticaY - 36) / 14, (lopticaX - 31) / 14)].postojiZid(LIJEVO)) 
            return;

        obrisiLopticu();
        lopticaX--;
        crtajLopticu();

        if ( index((lopticaY - 36) / 14, (lopticaX - 31) / 14) == index((ciljY - 36) / 14, (ciljX - 31) / 14) )
            dostignutCilj();
            
        wait_ms((3 - brzina) * 10);
    }

    void crtajLopticu(){
        DISPLAY.fillcircle(lopticaX, lopticaY, 4, Yellow);
        DISPLAY.circle(lopticaX, lopticaY, 4, Black);
    }

    void obrisiLopticu(){
        DISPLAY.fillcircle(lopticaX, lopticaY, 4, DarkCyan);
    }

    void dostignutCilj(){
        tiker.detach();
        
        DISPLAY.fillrect(46, 68, 280, 148, White);
        DISPLAY.fillrect(48, 70, 278, 146, DarkCyan);
        DISPLAY.fillrect(50, 72, 276, 144, White);
        DISPLAY.fillrect(52, 74, 274, 142, DarkCyan);

        DISPLAY.locate(100, 75);
        DISPLAY.printf("Igra zavrsena");

        DISPLAY.locate(54, 105);
        int minute = trenutnoVrijeme / 60;
        int sekunde = trenutnoVrijeme - minute*60;
        if (sekunde < 10)
            DISPLAY.printf("Vrijeme: %d:0%d", minute, sekunde);
        else
            DISPLAY.printf("Vrijeme: %d:%d", minute, sekunde);


        for (int k = 5; k>=1; k--) {
            DISPLAY.locate(54, 125);
            DISPLAY.printf("Povratak na glavni meni za %d", k);
            wait(1);
        }

        trenutnoVrijeme = 0;
        stanje = MENU;
    }
};

class StanjeIgre {
public:
    StanjeIgre(){
        DISPLAY.cls();
    }
    
    virtual void crtajIzgled(){}
    virtual void citajUlaz(){}
};

class Meni : StanjeIgre {
private:

    int opcija;
    
public:

    Meni(){
        opcija = 0;
        crtajOkvir(false);
        crtajIzgled();
        citajUlaz(); 
    }

    virtual void crtajIzgled(){
        DISPLAY.locate(110, 40);
        DISPLAY.printf("- MazeBall -");

        DISPLAY.fillcircle(60, 89, 4.5, Yellow); // Izaberi prvu opciju

        for(int i=0; i<4; i++)
            DISPLAY.circle(60, 89 + i*30, 4.5, White);

        DISPLAY.locate(90, 85);
        DISPLAY.printf("Zapocni igru");
        DISPLAY.locate(90, 115);
        DISPLAY.printf("Instrukcije");
        DISPLAY.locate(90, 145);
        DISPLAY.printf("Brzina");
        DISPLAY.locate(90, 175);
        DISPLAY.printf("O nama");
    }

    virtual void citajUlaz(){
        char c = 'C', prethodni = 'C';

        while(1) {
            c = ucitajZnak(prethodni);

            if (c == 'U' && c != prethodni) { // Idi gore
                prethodni = 'U';
                odselektujStavku(opcija);
                opcija = (opcija + 3) % 4;
                selektujStavku(opcija);
            } else if (c == 'D' && c != prethodni) { // Idi dolje
                prethodni = 'D';
                odselektujStavku(opcija);
                opcija = (opcija + 1) % 4;
                selektujStavku(opcija);
            } else if (c == 'C')
                prethodni = 'C';
            else if (c == 'K')
                break;
        }

        zapocniStanje(opcija);
    }

    void selektujStavku(int i){
        DISPLAY.fillcircle(60, 89 + i*30, 4.5, Yellow);
        DISPLAY.circle(    60, 89 + i*30, 4.5, White);
    }

    void odselektujStavku(int i){
        DISPLAY.fillcircle(60, 89 + i*30, 4.5, DarkCyan);
        DISPLAY.circle(    60, 89 + i*30, 4.5, White);
    }

    void zapocniStanje(int i);
};

class Brzina : StanjeIgre {
private:

    int opcija;
    
public:

    Brzina(){
        opcija = 0;
        crtajOkvir(false);
        crtajIzgled();
        citajUlaz(); 
    }

    virtual void crtajIzgled(){
        DISPLAY.locate(140, 40);
        DISPLAY.printf("Brzina");

        DISPLAY.fillcircle(60, 79, 4, Yellow);

        for(int i=0; i<3; i++)
            DISPLAY.circle(60, 79 + i*40, 4, White);

        DISPLAY.locate(90, 75);
        DISPLAY.printf("Sporo");
        DISPLAY.locate(90, 115);
        DISPLAY.printf("Normalno");
        DISPLAY.locate(90, 155);
        DISPLAY.printf("Brzo");
    }

    virtual void citajUlaz(){
        char c = 'C', prethodni = 'C';

        while(true) {
            c = ucitajZnak(prethodni);

            if (c == 'U' && c != prethodni) {       // Idi gore
                prethodni = 'U';
                odselektujStavku(opcija);
                opcija = (opcija + 2) % 3;
                selektujStavku(opcija);
            } else if (c == 'D' && c != prethodni) { // Idi dolje
                prethodni = 'D';
                odselektujStavku(opcija);
                opcija = (opcija + 1) % 3;
                selektujStavku(opcija);
            } else if (c == 'C')
                prethodni = 'C';
            else if (c == 'K')
                break;
            else if (c == 'B') {
                opcija = brzina;
                break;
            }
        }

        odaberiopciju(opcija);
    }
    
    void selektujStavku(int i){
        DISPLAY.fillcircle(60, 79 + i * 40, 4, Yellow);
        DISPLAY.circle(    60, 79 + i * 40, 4, White);
    }

    void odselektujStavku(int i){
        DISPLAY.fillcircle(60, 79 + i * 40, 4, DarkCyan);
        DISPLAY.circle(    60, 79 + i * 40, 4, White);
    }

    void odaberiopciju(int opcija){
        brzina = opcija;
        stanje = MENU;
    }

};

class ONama : StanjeIgre {
public:

    ONama() {
        crtajOkvir(false);
        crtajIzgled();
        citajUlaz(); 
    }

    virtual void crtajIzgled(){
        DISPLAY.locate(120, 50);
        DISPLAY.printf("- MazeBall -");
        DISPLAY.locate(45, 100);
        DISPLAY.printf("Mandal Anel");
        DISPLAY.locate(45, 125);
        DISPLAY.printf("Mehmedagic Lejla");
        DISPLAY.locate(150, 200);
        DISPLAY.printf("ETF Sarajevo 2019");
    }

    virtual void citajUlaz(){
        char c = 'C';
        while(ucitajZnak(c) != 'B');

        stanje = MENU;
    }

};

class Instrukcije : StanjeIgre {
public:

    Instrukcije(){
        crtajOkvir(false);
        crtajIzgled();
        citajUlaz(); 
    }

    virtual void crtajIzgled(){
        DISPLAY.locate(115, 40);
        DISPLAY.printf("Instrukcije");

        DISPLAY.locate(30, 85);
        DISPLAY.printf("\"Zapocni igru\" generise novi,");
        DISPLAY.locate(30, 100);
        DISPLAY.printf("slucajni labirint svaki put");

        DISPLAY.locate(30, 125);
        DISPLAY.printf("Nagnite telefon da pomjerite lopticu");

        DISPLAY.rect(25, 170, 205, 190, White);
        DISPLAY.locate(30, 175);
        DISPLAY.printf("Dostignite cilj sto prije");
    }

    virtual void citajUlaz(){
        char c = 'C';
        while(ucitajZnak(c) != 'B');

        stanje = MENU;
    }
};

class Igra : StanjeIgre {
    Labirint labirint;
public:
    Igra() {
        crtajOkvir(true);
        tiker.attach(&azurirajVrijeme, 1);
        citajUlaz();
    }

    virtual void citajUlaz(){
        char c = 'C';

        while(1) { 
            c = ucitajZnak(c);
         
            if(stanje == MENU)
                return;
            if (c == 'U')
                labirint.idiGore();
            else if (c == 'D')
                labirint.idiDolje();
            else if (c == 'L')
                labirint.idiLijevo();
            else if (c == 'R')
                labirint.idiDesno();
            else if (c == 'B')
                break;

            if (promjenaVremena) {
                DISPLAY.fillrect(140, 0, 180, 20, DarkCyan);
                DISPLAY.locate(145, 5);
                int minute = trenutnoVrijeme / 60;
                int sekunde = trenutnoVrijeme - minute * 60;
                if (sekunde < 10)
                    DISPLAY.printf("%d:0%d", minute, sekunde);
                else
                    DISPLAY.printf("%d:%d", minute, sekunde);

                promjenaVremena = false;
            }
        }

        tiker.detach();
        trenutnoVrijeme = 0;
        stanje = MENU;
    }
};

void Meni::zapocniStanje(int i){
    switch(i) {
    case 0:
        stanje = IGRA;
        break;
    case 1:
        stanje = INSTRUKCIJE;
        break;
    case 2:
        stanje = BRZINA;
        break;
    case 3:
        stanje = ONAMA;
        break;
    }
}


void inicijalizirajDISPLAY(){
    DISPLAY.claim(stdout);
    DISPLAY.set_orientation(1);
    DISPLAY.background(DarkCyan);
    DISPLAY.foreground(White);
    DISPLAY.cls();
    DISPLAY.set_font((unsigned char *) Arial12x12);
    DISPLAY.locate(90, 110);
    DISPLAY.printf("Inicijaliziram Wi-Fi...");
}


void dajResponse(){
    wait(0.2);
    while(WIFI.readable())
        PC.putc(WIFI.getc());
}

//  Poslije svake AT komande potrebno je:
//  \r    \u000d: carriage return CR     (Ctrl + M)
//  \n    \u000a: linefeed LF            (Ctrl + J)
void inicijalizirajWIFI(){
    PC.baud(115200);
    WIFI.baud(115200);
    wait(0.5);
    
    WIFI.printf("AT+RST\r\n");
    wait(0.5);
    dajResponse();
    
    // Konfiguracija ESP8266 kao softAP
    WIFI.printf("AT+CWMODE=2\r\n");
    dajResponse();
    WIFI.printf("AT+CIPAP=\"192.168.1.11\"\r\n");
    dajResponse();
    WIFI.printf("AT+CWSAP=\"MazeBall\",\"123456789\",1,3\r\n");
    dajResponse();
    WIFI.printf("AT+CIPMUX=1\r\n");
    dajResponse();
    
    // Komunikacija preko 192.168.1.11:1234
    WIFI.printf("AT+CIPSERVER=1,1234\r\n");
    dajResponse();
}


int main(){
    inicijalizirajDISPLAY();
    inicijalizirajWIFI();
    
    stanje = MENU;

    // Praćenje stanja igre preko globalne varijable
    // kako ne bismo natrpavali Call Stack pozivanjem 
    // novih stanja iz metoda već kreiranog stanja
    
    while(1) {
        switch(stanje) {
        case MENU:
            Meni();
            break;
        case IGRA:
            Igra();
            break;
        case INSTRUKCIJE:
            Instrukcije();
            break;
        case BRZINA:
            Brzina();
            break;
        case ONAMA:
            ONama();
            break;
        }
    }
}