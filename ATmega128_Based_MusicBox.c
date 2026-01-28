#include <mega128.h>
#include <delay.h>
#include <stdint.h>

// ========== LCD 관련 상수 및 선언 ==========
#define ENABLE PORTA.2
#define FUNCSET 0x28
#define ENTMODE 0x06
#define ALLCLR 0x01
#define DISPON 0x0c

void LCD_init(void);
void LCD_String(char *str);
void Busy(void);
void Command(uint8_t byte);
void Data(uint8_t byte);
void Create_Custom_Char(uint8_t addr, uint8_t *pattern);
void Draw_Rabbit(uint8_t row, uint8_t col, uint8_t head_id, uint8_t body_id);
void Erase_Rabbit(uint8_t row, uint8_t col);

// ========== 커스텀 문자 ==========
uint8_t rabbit_jump_head[8] = {
    0b00000, 0b00010, 0b00101, 0b01110,
    0b11111, 0b01111, 0b00011, 0b00010
};

uint8_t rabbit_jump_body[8] = {
    0b00000, 0b10000, 0b00000, 0b00001,
    0b11110, 0b11110, 0b11110, 0b00010
};

uint8_t rabbit_land_head[8] = {
    0b00010, 0b00101, 0b01110, 0b11111,
    0b01111, 0b00011, 0b00010, 0b00000
};

uint8_t rabbit_land_body[8] = {
    0b00000, 0b00000, 0b00001, 0b11110,
    0b11110, 0b11110, 0b00011, 0b00000
};

// ========== 부저 및 음악 관련 ==========
#define DO 0
#define RE 1
#define MI 2
#define FA 3
#define SOL 4
#define RA 5
#define SI 6
#define DDO 7
#define REST 8
#define EOS -1

#define ON 0
#define OFF 1

#define N2 1250
#define N4 (N2 / 2)
#define N8 (N2 / 4)
#define N16 (N2 / 8)
#define N8N16 (N8 + N16)
#define R 1

volatile int state = OFF, tone = 0;

char f_table[] = {17, 43, 66, 77, 97, 114, 129, 137, 255};
char LED[]     = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00};

int song[] = {
    SOL, MI, REST, MI, SOL, MI, DO, RE, MI, RE, DO, MI,
    SOL, DDO, SOL, DDO, SOL, DDO, SOL, MI, SOL, RE,
    FA, MI, RE, DO, EOS
};

int time[] = {
    N4, N8, R, N8, N8, N8, N4, N4, N8, N8, N8, N8,
    N4, N8N16, N16, N8, N8, N8, N8, N4, N4, N8,
    N8, N8, N8, N4, N4
};

// ========== 타이머 인터럽트 ==========
interrupt [TIM0_OVF] void timer0_ovf_isr(void)
{
    TCNT0 = f_table[tone];
    if (state == OFF) {
        PORTE |= (1 << 3);  // Buzzer ON
        state = ON;
    } else {
        PORTE &= ~(1 << 3); // Buzzer OFF
        state = OFF;
    }
}

// ========== 메인 루프 ==========
void main(void)
{
    int i;
    uint8_t block_col;
    bit direction;

    // 포트 설정
    DDRA = 0xFF;        // LCD
    DDRC = 0xFF;        // LED
    DDRE |= (1 << 3);   // Buzzer

    // LCD 초기화 및 문자 등록
    LCD_init();
    Create_Custom_Char(0, rabbit_jump_head);
    Create_Custom_Char(1, rabbit_jump_body);
    Create_Custom_Char(2, rabbit_land_head);
    Create_Custom_Char(3, rabbit_land_body);

    // 타이머0 설정
    TCCR0 = 0x03;
    TIMSK |= (1 << TOIE0);
    TCNT0 = f_table[song[0]];
    #asm("sei")

    while (1)
    {
		    // 이전 위치의 토끼 지우기
		    Erase_Rabbit(direction ? 1 : 0, block_col);
		    
        i = 0;
        block_col = 15;
        direction = 0;

        do {
            tone = song[i];
            PORTC = LED[tone];

            if (tone != REST) {
                Erase_Rabbit(direction ? 1 : 0, block_col);

                if (block_col == 0) {
                    block_col = 15;
                    direction = 0;
                } else {
                    block_col--;
                    direction = !direction;
                }

                if (direction == 0) {
                    Draw_Rabbit(0, block_col, 0, 1);  // 점프
                } else {
                    Draw_Rabbit(1, block_col, 2, 3);  // 착지
                }
            }

            delay_ms(time[i]);
            i++;
        } while (song[i] != EOS);
    }
}

// ========== LCD 함수 구현 ==========
void LCD_init(void)
{
    delay_ms(15);
    Command(0x20); delay_ms(5);
    Command(0x20); delay_us(100);
    Command(0x20);
    Command(FUNCSET);
    Command(DISPON);
    Command(ALLCLR);
    Command(ENTMODE);
}

void Busy(void) { delay_ms(2); }

void Command(uint8_t byte)
{
    Busy();
    PORTA = 0x00;
    PORTA |= (byte & 0xF0);
    ENABLE = 1; delay_us(1); ENABLE = 0;

    PORTA = 0x00;
    PORTA |= (byte << 4);
    ENABLE = 1; delay_us(1); ENABLE = 0;
}

void Data(uint8_t byte)
{
    Busy();
    PORTA = 0x01;
    PORTA |= (byte & 0xF0);
    ENABLE = 1; delay_us(1); ENABLE = 0;

    PORTA = 0x01;
    PORTA |= (byte << 4);
    ENABLE = 1; delay_us(1); ENABLE = 0;
}

void LCD_String(char *str)
{
    while (*str) Data(*str++);
}

void Create_Custom_Char(uint8_t addr, uint8_t *pattern)
{
    int i;
    addr &= 0x07;
    Command(0x40 | (addr << 3));
    for (i = 0; i < 8; i++) {
        Data(pattern[i]);
    }
}

void Draw_Rabbit(uint8_t row, uint8_t col, uint8_t head_id, uint8_t body_id)
{
    Command(0x80 + (row * 0x40) + col);
    Data(head_id);
    Data(body_id);
}

void Erase_Rabbit(uint8_t row, uint8_t col)
{
    Command(0x80 + (row * 0x40) + col);
    Data(' ');
    Data(' ');
}
