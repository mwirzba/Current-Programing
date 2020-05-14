#include <stdio.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#define key 10
#define actionKey 11

#pragma clang diagnostic push
#pragma ide diagnostic ignored "EndlessLoop"

void GetPlayerMove(int *prevCardId,char playerSign);
void SetInfoPanel(char playerSign);
bool CheckIfGameIsOver(char playerSign);
void SetInfoWindowMessage(char * msg,Window window);

Display *display;
int screen;
Window win;
XEvent event;
XColor card_color,dummy;
Colormap colormap;
Window cards[24];
Window infoWindow;


int shmid,shmidAction, sem;

typedef struct CardInfo {
    char state;
    char color[10];
    int index;
}CardInfo;

const char CARD_REVERSED = 'R';
const char CARD_HIDDEN = 'H';
const char FIRST_PLAYER_SIGN = 'X';
const char SECOND_PLAYER_SIGN = 'O';
const char WAIT_FOR_ANOTHER_PLAYER = 'W';
const char GAME_OVER = 'E';

XFontStruct *font;
XTextItem ti[1];


char cardColors[12][10] = {
        "blue",
        "red",
        "purple",
        "yellow",
        "pink",
        "green",
        "orange",
        "aqua",
        "brown",
        "coral",
        "indigo",
        "navy"
};

CardInfo cardInfos[24];
CardInfo * cardsPointer;
char * action;
//ipcs
//ipcrm -M (klucz)
//ipcrm -s (klucz)


void setRandomCardColor(int i) {
        bool set = false;
        while (set == false) {
            int index = rand() % 25;
            if (cardInfos[index].index == 99) {
                cardInfos[index].index = index;
                strcpy(cardInfos[index].color, cardColors[i]);
                cardInfos[index].state = CARD_HIDDEN;
                set = true;
            }
        }
}
CardInfo* setColorsForCards(bool random) {
    for (int i = 0; i < 24; ++i) {
        cardInfos[i].index = 99;
    }
    if(random) {
        for (int i = 0; i < 12; ++i) {
            setRandomCardColor(i);
            setRandomCardColor(i);
        }
    } else {
        int colorIndex = 0;
        for (int i = 0; i < 24; i+=2) {
            if(colorIndex == 12) {
                colorIndex = 0;
            }
            cardInfos[i].index = i;
            strcpy(cardInfos[i].color, cardColors[colorIndex]);
            cardInfos[i].state = CARD_HIDDEN;
            colorIndex++;
        }
    }

    return cardInfos;
}
//SETUP CARD INFO

void SetWindowColor(int i, char * color) {
    colormap = DefaultColormap(display, screen);
    XAllocNamedColor(display,colormap,color,&card_color,&dummy);
    XSetWindowBackground(display,cards[i],card_color.pixel);
    XUnmapWindow(display,cards[i]);
    XMapWindow(display,cards[i]);
    XFlush(display);
}



Window createCard(int x, int y,char * color,bool getClick) {
    colormap = DefaultColormap(display, screen);
    XAllocNamedColor(display,colormap,color,&card_color,&dummy);
    Window card = XCreateSimpleWindow(display,win,
                                      x,y,80,80,1,
                                      BlackPixel(display,screen),
                                      card_color.pixel);

    SetInfoWindowMessage(color,card);

    if(getClick) {
        XSelectInput(display,card, ButtonPressMask );
    }
    XMapWindow(display,card);
    return card;
}

Window createInfoWindow(int x, int y) {
    Window window =
            XCreateSimpleWindow(
                    display,win,
                    x,y,530,100,1,
                    BlackPixel(display,screen),
                    WhitePixel(display,screen));

    XMapWindow(display,window);
    return window;
}

void SetInfoWindowMessage(char * msg,Window window) {
    XClearWindow(display,window);
    font = XLoadQueryFont(display,"7x14");
    ti[0].chars = msg;
    ti[0].nchars = strlen(msg);
    ti[0].delta = 0;
    ti[0].font = font->fid;

    GC gc = DefaultGC(display,screen);
    XSetFont(display,gc,font->fid);
    XDrawText(display,window,gc,
            (530-XTextWidth(font,ti[0].chars,ti[0].nchars))/2,
                (100-(font->ascent+font->descent))/2+font->ascent
                ,ti,1);

    XUnloadFont(display,font->fid);
}

void ClearCards() {
    for (int i = 0; i < 24; ++i) {
        XDestroyWindow(display,cards[i]);
    }
    XFlush(display);
}


void CreateCards(bool getClick) {
    int x = 20 ,y = 20;
    int index =0;
    for (int i = 1; i <= 4; ++i) {
        for (int j = 1; j <= 6; ++j) {
            if(cardsPointer[index].state!=CARD_HIDDEN){
                cards[index] = createCard(x,y,cardsPointer[index].color,getClick);
            }else {
                cards[index] = createCard(x,y,"grey",getClick);
            }
            index++;
            x+=90;
        }
        x=20;
        y+=90;
    }
    XFlush(display);
}

void setColors() {
    for (int i = 0; i < 24; ++i) {
        if(cardsPointer[i].state != CARD_HIDDEN)
        {
            SetWindowColor(i,cardsPointer[i].color);
        }
    }
}

 void setCard(int * prevCardId, int id,char playerSign) {
    if(*prevCardId == id || cardsPointer[id].state != CARD_HIDDEN) {
        GetPlayerMove(prevCardId, playerSign);
        return;
    }
    else if(*prevCardId != id) {
        cardsPointer[id].state = CARD_REVERSED;
        SetWindowColor(id, cardsPointer[id].color);
        if (*prevCardId == 99)
            *prevCardId = id;
        else {
            int cpm = strcmp(cardsPointer[id].color, cardsPointer[*prevCardId].color);
            if (cpm != 0) {
                sleep(1);
                SetWindowColor(*prevCardId, "grey");
                SetWindowColor(id, "grey");
                cardsPointer[id].state = CARD_HIDDEN;
                cardsPointer[*prevCardId].state = CARD_HIDDEN;
                *prevCardId = 99;
            } else {
                cardsPointer[*prevCardId].state = playerSign;
                cardsPointer[id].state = playerSign;
                *prevCardId = 99;
                if(CheckIfGameIsOver(playerSign)){
                    return;
                }
                GetPlayerMove(prevCardId, playerSign);
                GetPlayerMove(prevCardId, playerSign);
            }
        }
    }
}

void removeMomory(int dummy) {
    semctl(sem, 0, IPC_RMID, 0);
    shmdt(cardsPointer);
    shmctl(shmid, IPC_RMID, 0);
    shmdt(action);
    shmctl(shmidAction, IPC_RMID, 0);
    exit(0);
}



bool CheckIfGameIsOver(char playerSign) {
    int playerPoints = 0;
    for (int i = 0; i < 24; ++i) {
        if(cardsPointer[i].state != FIRST_PLAYER_SIGN && cardsPointer[i].state != SECOND_PLAYER_SIGN) {
            return false;
        }
        if(cardsPointer[i].state == playerSign) {
            playerPoints++;
        }
    }
    *action = GAME_OVER;
    return true;
}


void WaitForAnotherPlayer(char playerSign) {
    XNextEvent(display, &event);
    if (event.type == Expose) {
        char *result = "GRASZ JAKO PIERWSZY GRACZ - OCZEKIWANIE NA DRUGIEGO GRACZA";
        if(playerSign == FIRST_PLAYER_SIGN)
        {
            SetInfoWindowMessage(result,infoWindow);
            ClearCards();
            CreateCards(false);
        } else {
            result = "TURA PRZECIWNIKA";
            SetInfoWindowMessage(result,infoWindow);
            ClearCards();
            CreateCards(false);
        }

    }
    while (*action == WAIT_FOR_ANOTHER_PLAYER) {}
}

void countPoints(char playerSign, char result[]) {
    int playerPoints = 0;
    for (int i = 0; i < 24; ++i) {
        if(cardsPointer[i].state == playerSign) {
            playerPoints++;
        }
    }
    char strPlayerPoints[5];
    if(24 - playerPoints < 12) {
        strcpy(result,"WYGRALES ZNALEZIONE PARY: ");
        sprintf(strPlayerPoints,"%d",(playerPoints/2));
        strcat(result,strPlayerPoints);
    } else if(24 - playerPoints > 12) {
        strcpy(result,"PRZEGRALES ZNALEZIONE PARY: ");
        sprintf(strPlayerPoints,"%d",(playerPoints/2));
        strcat(result,strPlayerPoints);
    } else {
        strcpy(result,"REMIS");
    }
}

int main(int argc,char * argv[]) {
    int seed;
    time_t tt;
    seed = time(&tt);
    srand(seed);
    signal(SIGINT, removeMomory);
    char playerSign,otherPlayerSign;
    shmid = shmget(key,sizeof(CardInfo) * 24, 0777 | IPC_CREAT);

    if(shmid< 0){
        perror("SGMGET");
        exit(1);
    }
    cardsPointer = shmat(shmid, NULL, 0);
    if(cardsPointer == (CardInfo *) -1)
    {
        perror("SHMAT");
        exit(1);
    }
    //SETUP ACTION IN MEMORY AND COLORS
    shmidAction = shmget(actionKey, sizeof(char), 0777 | IPC_CREAT | IPC_EXCL);
    if(shmidAction != -1)
    {
        playerSign = FIRST_PLAYER_SIGN;
        otherPlayerSign = SECOND_PLAYER_SIGN;
        semctl(sem, 0, SETVAL, 1);
        semctl(sem, 1, SETVAL, 0);
        setColorsForCards(true);

        for (int i = 0; i < 24; ++i) {
            strcpy(cardsPointer[i].color,cardInfos[i].color);
            cardsPointer[i].state = cardInfos[i].state;
            cardsPointer[i].index = cardInfos[i].index;
        }
        action = shmat(shmidAction,NULL,0);
        *action = WAIT_FOR_ANOTHER_PLAYER;
    } else {
        shmidAction = shmget(actionKey, sizeof(char), 0777 | IPC_CREAT );
        action = shmat(shmidAction,NULL,0);
        playerSign = SECOND_PLAYER_SIGN;
        otherPlayerSign = FIRST_PLAYER_SIGN;
        *action = otherPlayerSign;
    }

    //SETUP SEMAPTHORS AND COLORS

fprintf(stderr,"%s",argv[1]);
	char * displayName = argv[1];
    //SETUP MAIN WINDOW
    int prevCardId = 99;
    display = XOpenDisplay(displayName);
if(display == NULL) {
	fprintf(stderr,"\nNIE ZNALEZIONO");
	return;
}

    screen = DefaultScreen(display);
    win = XCreateSimpleWindow(display, RootWindow(display, screen),
            200, 200, 570, 520, 1, BlackPixel(display, screen), WhitePixel(display, screen));
    XSelectInput(display, win, ExposureMask | KeyPressMask);
    XMapWindow(display, win);


    XFlush(display);
    infoWindow = createInfoWindow(20,400);
    CreateCards(true);
    WaitForAnotherPlayer(playerSign);

    /*
    for (int i = 0; i < 24; ++i) {
        fprintf(stderr," %s ",cardsPointer[i].color);
    }*/

    while (*action == WAIT_FOR_ANOTHER_PLAYER) {}

    /*
    for (int j = 0; j < 4; ++j) {
        cardsPointer[j].state = 'X';
    }
    for (int j = 4; j < 24; ++j) {
        cardsPointer[j].state = 'O';
    }*/


    while (true)
    {
        SetInfoPanel(playerSign);
        ClearCards();
        CreateCards(false);
        while ( *action != playerSign) {
            setColors();
            if(*action == GAME_OVER) {
                break;
            }
        }
        if(CheckIfGameIsOver(playerSign))
            break;
        ClearCards();
        CreateCards(true);
        SetInfoPanel(playerSign);
        GetPlayerMove(&prevCardId,playerSign);
        GetPlayerMove(&prevCardId,playerSign);
        ClearCards();
        CreateCards(false);
        *action = otherPlayerSign;
        if(CheckIfGameIsOver(playerSign))
            break;
    }
    ClearCards();
    CreateCards(false);
    char gameResult[30];
    countPoints(playerSign,gameResult);


    while (true)
    {
        XNextEvent(display,&event);
        if (event.type == Expose) {
            SetInfoWindowMessage(gameResult,infoWindow);
            XFlush(display);
            sleep(5);
            removeMomory(1);
            break;
        }
    }
}

void GetPlayerMove(int *prevCardId,char playerSign) {
    do
    {
        XNextEvent(display, &event);
        if (event.xany.window==cards[0]) {
            if(event.type == ButtonPress) {
               setCard(prevCardId, 0,playerSign);
            }
        }
        else if (event.xany.window==cards[1]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 1,playerSign);
            }
        }
        else if (event.xany.window==cards[2]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 2,playerSign);
            }
        }
        else if (event.xany.window==cards[3]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 3,playerSign);
            }
        }
        else if (event.xany.window==cards[4]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 4,playerSign);
            }
        }
        else if (event.xany.window==cards[5]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 5,playerSign);
            }
        }
        else if (event.xany.window==cards[6]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 6,playerSign);
            }
        }else if (event.xany.window==cards[7]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 7,playerSign);
            }
        }else if (event.xany.window==cards[8]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 8,playerSign);
            }
        }else if (event.xany.window==cards[9]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 9,playerSign);
            }
        }else if (event.xany.window==cards[10]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 10,playerSign);
            }
        }else if (event.xany.window==cards[11]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 11,playerSign);
            }
        }else if (event.xany.window==cards[12]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 12,playerSign);
            }
        }
        else if (event.xany.window==cards[13]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 13,playerSign);
            }
        }else if (event.xany.window==cards[14]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 14,playerSign);
            }
        }
        else if (event.xany.window==cards[15]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 15,playerSign);
            }
        }else if (event.xany.window==cards[16]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId, 16,playerSign);
            }
        }else if (event.xany.window==cards[17]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId,17,playerSign);
            }
        }else if (event.xany.window==cards[18]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId,18,playerSign);
            }
        }else if (event.xany.window==cards[19]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId,19,playerSign);
            }
        }else if (event.xany.window==cards[20]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId,20,playerSign);
            }
        }else if (event.xany.window==cards[21]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId,21,playerSign);
            }
        }else if (event.xany.window==cards[22]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId,22,playerSign);
            }
        }else if (event.xany.window==cards[23]) {
            if(event.type == ButtonPress) {
                setCard(prevCardId,23,playerSign);
            }
        }
    }while(event.type != ButtonPress);
}

void SetInfoPanel(char playerSign) {
    XNextEvent(display, &event);
    if (event.type == Expose) {
        if(*action != playerSign ) {
            SetInfoWindowMessage("TURA PRZECIWNIKA",infoWindow);
        } else  {
            SetInfoWindowMessage("TWOJA TURA",infoWindow);
        }
    }
}



#pragma clang diagnostic pop
