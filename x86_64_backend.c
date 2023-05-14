// X86_64 assembly generator
#include "aiwn.h"
//
// I called CrunkLord420's stuff Voluptous - April 30,2024
// I found a duck statue while voluentering - May 3,2024
// I got bored and played with a color chooser May 7,2024

//
// Assembly section
//



// https://cs61.seas.harvard.edu/site/pdf/x86-64-abi-20210928.pdf
static int64_t call_iregs[6] = { RDI, RSI, RDX, RCX, R8, R9 };
static int64_t call_fregs[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };


extern int64_t ICMov(CCmpCtrl* cctrl, CICArg* dst, CICArg* src, char* bin,
	int64_t code_off);
extern int64_t __OptPassFinal(CCmpCtrl* cctrl, CRPN* rpn, char* bin,
	int64_t code_off);

#define X86_64_BASE_REG 5
#define X86_64_STACK_REG 4

#define AIWNIOS_ADD_CODE(func, ...)                        \
	{                                                      \
		if (bin)                                           \
			code_off += func(bin + code_off, __VA_ARGS__); \
		else                                               \
			code_off += func(NULL, __VA_ARGS__);           \
	}

static uint8_t ErectREX(int64_t big, int64_t reg, int64_t index, int64_t base)
{
	if (index == -1)
		index = 0;
	if (base == -1)
		base = 0;
	return 0b01000000 | (big << 3) | (!!(reg & 0b1000) << 2) | (!!(index & 0b1000) << 1) | !!(base & 0b1000);
}
#define ADD_U8(v)            \
	{                        \
		if (to) {            \
			to[len++] = (v); \
		} else               \
			len++;           \
	}
#define ADD_U16(v)                       \
	{                                    \
		if (to) {                        \
			*(int16_t*)(to + len) = (v); \
			len += 2;                    \
		} else                           \
			len += 2;                    \
	}
#define ADD_U32(v)                       \
	{                                    \
		if (to) {                        \
			*(int32_t*)(to + len) = (v); \
			len += 4;                    \
		} else                           \
			len += 4;                    \
	}
#define ADD_U64(v)                       \
	{                                    \
		if (to) {                        \
			*(int64_t*)(to + len) = (v); \
			len += 8;                    \
		} else                           \
			len += 8;                    \
	}

#define SIB_BEGIN(_64, r, s, i, b, o)                                \
	do {                                                             \
		int64_t R = (r), S = (s), I = (i), B = (b), O = (o);         \
		switch(S) {default:S=-1;break;case 1:S=0;break;case 2:S=1;break;case 4:S=2;break;case 8:S=3;} \
		if (B == RIP) {                                              \
			if (_64 || (I & 0b1000) || (B & 0b1000) || (R & 0b1000)) \
				ADD_U8(ErectREX(_64, R, 0, 0));                      \
		} else if (I != -1 || B == R12) {                            \
			if (S == -1)                                   \
				I = RSP;                                             \
			if (_64 || (I & 0b1000) || (B & 0b1000) || (R & 0b1000)) \
				ADD_U8(ErectREX(_64, R, I, B));                      \
		} else {                                                     \
			if (_64 || (I & 0b1000) || (B & 0b1000) || (R & 0b1000)) \
				ADD_U8(ErectREX(_64, R, I, B));                      \
		}
#define SIB_END()                                                              \
	if (I == -1 && R12 != B && B != RIP && B != RSP) {                         \
		if (!O && B != RBP)                                                    \
			ADD_U8(((R & 0b111) << 3) | (B & 0b111))                           \
		else if (-0x7f <= O && O <= 0x7f) {                                    \
			ADD_U8(0b01000000 | ((R & 0b111) << 3) | (B & 0b111));             \
			ADD_U8(O);                                                         \
		} else {                                                               \
			ADD_U8(0b10000000 | ((R & 0b111) << 3) | (B & 0b111));             \
			ADD_U32(O);                                                        \
		}                                                                      \
		break;                                                                 \
	}                                                                          \
	if (B == RIP) {                                                            \
		ADD_U8(((R & 0b111) << 3) | 0b101)                                     \
		ADD_U32(O - (len + 4));                                                \
		break;                                                                 \
	} else {                                                                   \
		ADD_U8(MODRMSIB((R & 0b111)) | (((-0x7f <= O && O <= 0x7f) ? 0b01 : 0b10) << 6)) \
	}                                                                          \
	if (S == -1) {                                                   \
		ADD_U8((RSP << 3) | (B & 0b111));                                      \
	} else if (B == -1) {                                                      \
		ADD_U8((S << 6) | ((I & 0b111) << 3) | RBP);                                     \
	} else {                                                                   \
		ADD_U8((S << 6) | ((I & 0b111) << 3) | (B & 0b111));                             \
	}                                                                          \
	if ((-0x7f <= O && O <= 0x7f) && B != -1) {                                \
		ADD_U8(O);                                                             \
	} else {                                                                   \
		ADD_U32(O);                                                            \
	}                                                                          \
	}                                                                          \
	while (0)                                                                  \
		;

static int64_t Is32Bit(int64_t num)
{
	if (num >= -0x7fFFffFFu)
		if (num < 0x7fFFffFFu)
			return 1;
	return 0;
}
static uint8_t MODRMRegReg(int64_t a, int64_t b)
{
	return (0b11 << 6) | ((a & 0b111) << 3) | (b & 0b111);
}
static uint8_t MODRMRip32(int64_t a)
{
	return (0b00 << 6) | ((a & 0b111) << 4) | 0b101;
}

static uint8_t MODRMSIB(int64_t a)
{
	return ((a & 0b111) << 3) | 0b100;
}

static int64_t X86CmpRegReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x3b);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86CmpRegImm(char* to, int64_t a, int64_t imm)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0x3b);
	ADD_U8(MODRMRegReg(7, a));
	ADD_U32(imm);
	return len;
}

static int64_t X86MovRegReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, b, 0, a));
	ADD_U8(0x89);
	ADD_U8(MODRMRegReg(b, a));
	return len;
}

static int64_t X86Leave(char* to, int64_t ul)
{
	int64_t len = 0;
	ADD_U8(0xc9);
	return len;
}

static int64_t X86Ret(char* to, int64_t ul)
{
	int64_t len = 0;
	ADD_U8(0xc3);
	return len;
}

int64_t X86MovImm64(char* to, int64_t a, int64_t off)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, a));
	ADD_U8(0xB8 + (a & 0x7));
	ADD_U64(off);
	return len;
}
static int64_t X86MovF64RIP(char* to, int64_t a, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xF2);
	SIB_BEGIN(1, a, -1, -1, RIP, off);
	ADD_U8(0x0F);
	ADD_U8(0x10);
	SIB_END();
	return len;
}

static int64_t X86AddImm32(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0x81);
	ADD_U8(MODRMRegReg(0, a));
	ADD_U32(b);
	return len;
}

static int64_t X86SubImm32(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, a));
	ADD_U8(0x81);
	ADD_U8(MODRMRegReg(5, a));
	ADD_U32(b);
	return len;
}

static int64_t X86SubReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x2b);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86AddReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x03);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86AddSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0x03);
	SIB_END();
	return len;
}

static int64_t X86SubSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0x2b);
	SIB_END();
	return len;
}

static int64_t X86IncReg(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xff);
	ADD_U8(MODRMRegReg(0, a));
	return len;
}

static int64_t X86IncSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 0, s, i, b, off);
	ADD_U8(0xff);
	SIB_END();
	return len;
}

static int64_t X86DecReg(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xff);
	ADD_U8(MODRMRegReg(1, a));
	return len;
}

static int64_t X86DecSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 1, s, i, b, off);
	ADD_U8(0xff);
	SIB_END();
	return len;
}

static int64_t X86CVTTSD2SIRegSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x2c);
	SIB_END();
	return len;
}

static int64_t X86CVTTSI2SDRegSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x2a);
	SIB_END();
	return len;
}

static int64_t X86AndPDReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0x66);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x54);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86AndPDSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0x66);
	SIB_BEGIN(0, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x54);
	SIB_END();
	return len;
}

static int64_t X86OrPDReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0x66);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x56);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86OrPDSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0x66);
	SIB_BEGIN(0, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x56);
	SIB_END();
	return len;
}

static int64_t X86XorPDReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0x66);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x57);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86XorPDSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0x66);
	SIB_BEGIN(0, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x57);
	SIB_END();
	return len;
}

static int64_t X86COMISDRegReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0x66);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x2f);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86Test(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x85);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86AddSDReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x58);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86AddSDSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(0, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x58);
	SIB_END();
	return len;
}

static int64_t X86SubSDReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x5c);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86SubSDSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(0, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x5c);
	SIB_END();
	return len;
}

static int64_t X86RoundSD(char *to,int64_t a,int64_t b,int64_t mode) {
	int64_t len = 0;
	ADD_U8(0x66);
	ADD_U8(ErectREX(0,a,0,b));
	ADD_U8(0x0f);
	ADD_U8(0x3a);
	ADD_U8(0x0b);
	ADD_U8(MODRMRegReg(a,b));
	ADD_U8(mode);
	return len;
}

static int64_t X86MulSDReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x59);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86MulSDSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(0, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x59);
	SIB_END();
	return len;
}

static int64_t X86DivSDReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0x5E);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86DivSDSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(0, a, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0x5e);
	SIB_END();
	return len;
}

static int64_t X86AndReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x23);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86AndImm(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0x81);
	ADD_U8(MODRMRegReg(4, a));
	ADD_U32(b);
	return len;
}

static int64_t X86AndSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0x23);
	SIB_END();
	return len;
}

static int64_t X86OrReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x0b);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86OrSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0xb);
	SIB_END();
	return len;
}

static int64_t X86OrImm(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0x81);
	ADD_U8(MODRMRegReg(1, a));
	ADD_U32(b);
	return len;
}

static int64_t X86XorReg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x33);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86XorSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0x33);
	SIB_END();
	return len;
}

static int64_t X86XorImm(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0x81);
	ADD_U8(MODRMRegReg(6, a));
	ADD_U32(b);
	return len;
}

static int64_t X86Cqo(char* to, int64_t unused)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, 0));
	ADD_U8(0x99);
	return len;
}

static int64_t X86DivReg(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 6, 0, a));
	ADD_U8(0xf7);
	ADD_U8(MODRMRegReg(6, a));
	return len;
}

static int64_t X86DivSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 6, s, i, b, off);
	ADD_U8(0xf7);
	SIB_END();
	return len;
}

static int64_t X86IDivReg(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xf7);
	ADD_U8(MODRMRegReg(7, a));
	return len;
}

static int64_t X86IDivSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 7, s, i, b, off);
	ADD_U8(0xf7);
	SIB_END();
	return len;
}

static int64_t X86SarRCX(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xd3);
	ADD_U8(MODRMRegReg(7, a));
	return len;
}

static int64_t X86SarRCXSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 7, s, i, b, off);
	ADD_U8(0xd3);
	SIB_END();
	return len;
}

static int64_t X86SarImm(char* to, int64_t a, int64_t imm)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xc1);
	ADD_U8(MODRMRegReg(7, a));
	ADD_U8(imm);
	return len;
}

static int64_t X86SarImmSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 7, s, i, b, off);
	ADD_U8(0xc1);
	SIB_END();
	return len;
}

static int64_t X86ShrRCX(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xd3);
	ADD_U8(MODRMRegReg(5, a));
	return len;
}

static int64_t X86ShrImm(char* to, int64_t a, int64_t imm)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xc1);
	ADD_U8(MODRMRegReg(5, a));
	ADD_U8(imm);
	return len;
}

static int64_t X86ShlRCX(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xd3);
	ADD_U8(MODRMRegReg(4, a));
	return len;
}

static int64_t X86ShlImm(char* to, int64_t a, int64_t imm)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xc1);
	ADD_U8(MODRMRegReg(4, a));
	ADD_U8(imm);
	return len;
}

static int64_t X86MulReg(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xf7);
	ADD_U8(MODRMRegReg(4, a));
	return len;
}

static int64_t X86MulSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 4, s, i, b, off);
	ADD_U8(0xf7);
	SIB_END();
	return len;
}

static int64_t X86IMulReg(char* to, int64_t a)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 0, 0, a));
	ADD_U8(0xf7);
	ADD_U8(MODRMRegReg(5, a));
	return len;
}
static int64_t X86IMulSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 5, s, i, b, off);
	ADD_U8(0xf7);
	SIB_END();
	return len;
}

static int64_t X86IMul2Reg(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x0f);
	ADD_U8(0xaf);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86IMul2SIB64(char* to, int64_t r, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, r, s, i, b, off);
	ADD_U8(0x0f);
	ADD_U8(0xaf);
	SIB_END();
	return len;
}

int64_t X86Call32(char* to, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xe8);
	ADD_U32(off-5);
	return len;
}

int64_t X86CallReg(char* to, int64_t reg)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, 2, 0, reg));
	ADD_U8(0xff);
	ADD_U8(MODRMRegReg(2, reg));
	return len;
}

int64_t X86PushReg(char* to, int64_t reg)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, reg, 0, reg));
	ADD_U8(0x50 + (reg & 0x7));
	return len;
}
int64_t X86PopReg(char* to, int64_t reg)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, reg, 0, reg));
	ADD_U8(0x58 + (reg & 0x7));
	return len;
}

static int64_t X86CmpRegSIB64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t o)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, o);
	ADD_U8(0x3b);
	SIB_END();
	return len;
}

static int64_t X86CmpSIBReg64(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t o)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, o);
	ADD_U8(0x30);
	SIB_END();
	return len;
}

static int64_t X86MovQF64I64(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0x66);
	ADD_U8(ErectREX(1, a, 0, b));
	ADD_U8(0x0F)
	ADD_U8(0x6e);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

static int64_t X86LeaSIB(char* to, int64_t a, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, a, s, i, b, off);
	ADD_U8(0x8D);
	SIB_END();
	return len;
}

static int64_t X86MovQI64F64(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0x66);
	ADD_U8(ErectREX(1, b, 0, a));
	ADD_U8(0x0F)
	ADD_U8(0x7e);
	ADD_U8(MODRMRegReg(b, a));
	return len;
}

static int64_t X86Jmp(char* to, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xe9);
	ADD_U32(off - 5);
	return len;
}

static int64_t X86JmpSIB(char* to, int64_t scale, int64_t idx, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(0, 4, scale, idx, base, off);
	ADD_U8(0xff);
	SIB_END();
	return len;
}

static int64_t X86JmpReg(char* to, int64_t r)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1,0,0,r));
	ADD_U8(0xff);
	ADD_U8(MODRMRegReg(4,r));
	return len;
}

static int64_t X86NegReg(char* to, int64_t r)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1,3,0,r));
	ADD_U8(0xf7);
	ADD_U8(MODRMRegReg(3, r));
	return len;
}

static int64_t X86NegSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 3, s, i, b, off);
	ADD_U8(0xf7);
	SIB_END();
	return len;
}

static int64_t X86NotReg(char* to, int64_t r)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1,2,0,r));
	ADD_U8(0xf7);
	ADD_U8(MODRMRegReg(2, r));
	return len;
}

static int64_t X86NotSIB64(char* to, int64_t s, int64_t i, int64_t b, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, 2, s, i, b, off);
	ADD_U8(0xf7);
	SIB_END();
	return len;
}

// MOVSD
static int64_t X86MovRegRegF64(char* to, int64_t a, int64_t b)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	if ((a & 0b1000) || (b & 0b1000))
		ADD_U8(ErectREX(0, a, 0, b));
	ADD_U8(0x0F)
	ADD_U8(0x10);
	ADD_U8(MODRMRegReg(a, b));
	return len;
}

int64_t X86MovIndirRegF64(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x0F);
	ADD_U8(0x11);
	SIB_END();
	return len;
}
int64_t X86MovRegIndirF64(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0xf2);
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x0F);
	ADD_U8(0x10);
	SIB_END();
	return len;
}

int64_t X86MovRegIndirI64(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x8b);
	SIB_END();
	return len;
}

int64_t X86MovIndirRegI64(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x89);
	SIB_END();
	return len;
}

int64_t X86MovRegIndirI32(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(0, reg, scale, index, base, off);
	ADD_U8(0x8b);
	SIB_END();
	return len;
}

int64_t X86MovIndirRegI32(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(0, reg, scale, index, base, off);
	ADD_U8(0x89);
	SIB_END();
	return len;
}

int64_t X86MovRegIndirI16(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0x66);
	SIB_BEGIN(0, reg, scale, index, base, off);
	ADD_U8(0x8b);
	SIB_END();
	return len;
}

static int64_t X86MovZXRegIndirI8(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x0f);
	ADD_U8(0xb6);
	SIB_END();
	return len;
}

static int64_t X86MovZXRegRegI8(char* to, int64_t reg, int64_t reg2)
{
	int64_t len = 0;
	ADD_U8(ErectREX(1, reg, 0, reg2))
	ADD_U8(0x0f);
	ADD_U8(0xb6);
	ADD_U8(MODRMRegReg(reg, reg2));
	return len;
}

static int64_t X86MovZXRegIndirI16(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x0f);
	ADD_U8(0xb7);
	SIB_END();
	return len;
}

static int64_t X86MovSXRegIndirI8(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x0f);
	ADD_U8(0xbe);
	SIB_END();
	return len;
}

static int64_t X86MovSXRegIndirI16(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x0f);
	ADD_U8(0xbf);
	SIB_END();
	return len;
}

static int64_t X86MovSXRegIndirI32(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(1, reg, scale, index, base, off);
	ADD_U8(0x63);
	SIB_END();
	return len;
}

int64_t X86MovIndirRegI16(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0x66);
	SIB_BEGIN(0, reg, scale, index, base, off);
	ADD_U8(0x89);
	SIB_END();
	return len;
}

int64_t
X86MovRegIndirI8(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(0, reg, scale, index, base, off);
	ADD_U8(0x88);
	SIB_END();
	return len;
}

int64_t X86MovIndirRegI8(char* to, int64_t reg, int64_t scale, int64_t index, int64_t base, int64_t off)
{
	int64_t len = 0;
	SIB_BEGIN(0, reg, scale, index, base, off);
	ADD_U8(0x88);
	SIB_END();
	return len;
}
//
// Codegen section
//

static int64_t IsTerminalInst(CRPN* r)
{
	switch (r->type) {
		break;
	case IC_RET:
	case IC_GOTO:
		return 1;
	}
	return 0;
}

static int64_t IsConst(CRPN* rpn)
{
	switch (rpn->type) {
	case IC_CHR:
	case IC_F64:
	case IC_I64:
		return 1;
	}
	return 0;
}

static int64_t ConstVal(CRPN* rpn)
{
	switch (rpn->type) {
	case IC_CHR:
	case IC_I64:
		return rpn->integer;
	case IC_F64:
		return rpn->flt;
	}
	return 0;
}

//
// If we call a function the tmp regs go to poo poo land
//
static int64_t SpillsTmpRegs(CRPN* rpn)
{
	int64_t idx;
	switch (rpn->type) {
		break;
	case __IC_CALL:
	case IC_CALL:
		return 1;
		break;
	case __IC_VARGS:
		for (idx = 0; idx != rpn->length; idx++)
			if (SpillsTmpRegs(ICArgN(rpn, idx)))
				return 1;
		return 0;
	case IC_TO_F64:
	case IC_TO_I64:
	case IC_NEG:
	unop:
		return SpillsTmpRegs(rpn->base.next);
		break;
	case IC_POS:
		goto unop;
		break;
	case IC_POW:
		return 1; // calls pow
	binop:
		if (SpillsTmpRegs(rpn->base.next))
			return 1;
		if (SpillsTmpRegs(ICFwd(rpn->base.next)))
			return 1;
		break;
	case IC_ADD:
		goto binop;
		break;
	case IC_EQ:
		goto binop;
		break;
	case IC_SUB:
		goto binop;
		break;
	case IC_DIV:
		goto binop;
		break;
	case IC_MUL:
		goto binop;
		break;
	case IC_DEREF:
		goto unop;
		break;
	case IC_AND:
		goto binop;
		break;
	case IC_ADDR_OF:
		goto unop;
		break;
	case IC_XOR:
		goto binop;
		break;
	case IC_MOD:
		goto binop;
		break;
	case IC_OR:
		goto binop;
		break;
	case IC_LT:
		goto binop;
		break;
	case IC_GT:
		goto binop;
		break;
	case IC_LNOT:
		goto unop;
		break;
	case IC_BNOT:
		goto unop;
		break;
	case IC_POST_INC:
		goto binop;
		break;
	case IC_POST_DEC:
		goto binop;
		break;
	case IC_PRE_INC:
		goto binop;
		break;
	case IC_PRE_DEC:
		goto binop;
		break;
	case IC_AND_AND:
		goto binop;
		break;
	case IC_OR_OR:
		goto binop;
		break;
	case IC_XOR_XOR:
		goto binop;
		break;
	case IC_EQ_EQ:
		goto binop;
		break;
	case IC_NE:
		goto binop;
		break;
	case IC_LE:
		goto binop;
		break;
	case IC_GE:
		goto binop;
		break;
	case IC_LSH:
		goto binop;
		break;
	case IC_RSH:
		goto binop;
		break;
	case IC_ADD_EQ:
		goto binop;
		break;
	case IC_SUB_EQ:
		goto binop;
		break;
	case IC_MUL_EQ:
		goto binop;
		break;
	case IC_DIV_EQ:
		goto binop;
		break;
	case IC_LSH_EQ:
		goto binop;
		break;
	case IC_RSH_EQ:
		goto binop;
		break;
	case IC_AND_EQ:
		goto binop;
		break;
	case IC_OR_EQ:
		goto binop;
		break;
	case IC_XOR_EQ:
		goto binop;
		break;
	case IC_MOD_EQ:
		goto binop;
		break;
	case IC_STORE:
		goto binop;
	case IC_TYPECAST:
		goto unop;
	}
	return 0;
t:
	return 1;
}

//
// Will overwrite the arg's value,but it's ok as we are about to use it's
// value(and discard it)
//
static int64_t PutICArgIntoReg(CCmpCtrl* cctrl, CICArg* arg, int64_t raw_type,
	int64_t fallback, char* bin, int64_t code_off)
{
	CICArg tmp = { 0 };
	if (arg->mode == MD_REG) {
		if ((raw_type == RT_F64) ^ (arg->raw_type == RT_F64)) {
			// They differ so continue as usaul
		} else // They are the same
			return code_off;
	}
	tmp.raw_type = raw_type;
	tmp.reg = fallback;
	tmp.mode = MD_REG;
	code_off = ICMov(cctrl, &tmp, arg, bin, code_off);
	memcpy(arg, &tmp, sizeof(CICArg));
	return code_off;
}

static int64_t PushToStack(CCmpCtrl* cctrl, CICArg* arg, char* bin,
	int64_t code_off)
{
	CICArg tmp = *arg;
	code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, 0, bin, code_off);
	if (tmp.raw_type != RT_F64) {
		AIWNIOS_ADD_CODE(X86PushReg, tmp.reg);
	} else {
		AIWNIOS_ADD_CODE(X86PushReg, RAX);
		AIWNIOS_ADD_CODE(X86MovIndirRegF64, tmp.reg, -1, -1, RSP, 0);
	}
	return code_off;
}
static int64_t PopFromStack(CCmpCtrl* cctrl, CICArg* arg, char* bin,
	int64_t code_off)
{
	CICArg tmp = *arg;
	if (arg->mode == MD_REG) {
		// All is good
	} else {
		tmp.mode = MD_REG;
		tmp.reg = 0;
	}
	if (tmp.raw_type != RT_F64) {
		AIWNIOS_ADD_CODE(X86PopReg, tmp.reg);
	} else {
		AIWNIOS_ADD_CODE(X86MovRegIndirF64, tmp.reg, -1, -1, RSP, 0);
		AIWNIOS_ADD_CODE(X86PopReg, RAX);
	}
	if (arg->mode != MD_REG)
		code_off = ICMov(cctrl, arg, &tmp, bin, code_off);
	return code_off;
}

static int64_t __ICMoveI64(CCmpCtrl* cctrl, int64_t reg, uint64_t imm, char* bin,
	int64_t code_off)
{
	int64_t code;
	if (bin && !(cctrl->flags & CCF_AOT_COMPILE)) {
		if (Is32Bit(imm - (int64_t)(bin + code_off))) {
			AIWNIOS_ADD_CODE(X86LeaSIB, reg, -1, -1, RIP, imm - (int64_t)(bin + code_off));
			return code_off;
		}
	}
	AIWNIOS_ADD_CODE(X86MovImm64, reg, imm);
	return code_off;
}

static int64_t __ICMoveF64(CCmpCtrl* cctrl, int64_t reg, double imm, char* bin,
	int64_t code_off)
{
	CCodeMisc* misc = cctrl->code_ctrl->code_misc->next;
	char* dptr;
	for (misc; misc != cctrl->code_ctrl->code_misc; misc = misc->base.next) {
		if (misc->type == CMT_FLOAT_CONST)
			if (misc->integer == *(int64_t*)&imm) {
				goto found;
			}
	}
	misc = A_CALLOC(sizeof(CCodeMisc), cctrl->hc);
	misc->type = CMT_FLOAT_CONST;
	misc->integer = *(int64_t*)&imm;
	QueIns(misc, cctrl->code_ctrl->code_misc->last);
found:
	if (bin && misc->addr && cctrl->code_ctrl->final_pass >= 2) {
		dptr = (char*)misc->addr - (bin + code_off);
		AIWNIOS_ADD_CODE(X86MovF64RIP, reg, dptr);
	} else
		AIWNIOS_ADD_CODE(X86MovF64RIP, reg, 0);
	return code_off;
}

static void PushSpilledTmp(CCmpCtrl* cctrl, CRPN* rpn)
{
	int64_t raw_type = rpn->raw_type;
	CICArg* res = &rpn->res;
	if (rpn->flags & ICF_PRECOMPUTED)
		return;
	// These have no need to be put into a temporay
	switch (rpn->type) {
		break;
	case IC_I64:
		res->mode = MD_I64;
		res->integer = rpn->integer;
		res->raw_type = RT_I64i;
		return;
		break;
	case IC_F64:
		res->mode = MD_F64;
		res->flt = rpn->flt;
		res->raw_type = RT_F64;
		return;
		break;
	case IC_IREG:
	case IC_FREG:
		res->mode = MD_REG;
		res->raw_type = raw_type;
		res->reg = rpn->integer;
		return;
		break;
	case IC_BASE_PTR:
		res->mode = MD_FRAME;
		res->raw_type = raw_type;
		res->off = rpn->integer;
		return;
	case IC_STATIC:
		res->mode = MD_PTR;
		res->raw_type = raw_type;
		res->off = rpn->integer;
		return;
	case __IC_STATIC_REF:
		res->mode = MD_STATIC;
		res->raw_type = raw_type;
		res->off = rpn->integer;
		return;
	}
	rpn->flags |= ICF_SPILLED;
	if (raw_type != RT_F64) {
		res->mode = MD_FRAME;
		res->raw_type = raw_type;
		if (cctrl->backend_user_data0 < (res->off = cctrl->backend_user_data1 += 8)) { // Will set ref->off before the top
			cctrl->backend_user_data0 = res->off;
		}
	} else {
		res->mode = MD_FRAME;
		res->raw_type = raw_type;
		if (cctrl->backend_user_data0 < (res->off = cctrl->backend_user_data1 += 8)) { // Will set ref->off before the top
			cctrl->backend_user_data0 = res->off;
		}
	}
	// See two above notes
	res->off -= 8;
	// In functions,wiggle room comes after the function locals.
	if (cctrl->cur_fun && res->mode == MD_FRAME)
		res->off += cctrl->cur_fun->base.sz + 16; //+16 for LR/FP
	else if (res->mode == MD_FRAME) {
		if (cctrl->backend_user_data4) {
			res->off += cctrl->backend_user_data4; // wiggle room start(if set)
		} else
			// Anonymus potatoes like to assume LR/FP pair too.
			res->off += 16;
	}
}
int64_t TmpRegToReg(int64_t r)
{
	switch (r) {
		break;
	case 0:
		return R9;
	case 1:
		return R10;
	case 2:
		return R11;
	case 3:
		return RDI;
	case 4:
		return RSI;
	default:
		abort();
	}
}
//
// Takes an argument called inher. We can use the parent's destination
// as a temporary. I'll give an example
//
// DO: -~!a;
// - dst=0
//  ~ dst=0
//   ! dst=0
//
// DON'T: -~!a
// - dst=0
//  ~ dst=1
//   ! dst=2

static void PushTmp(CCmpCtrl* cctrl, CRPN* rpn, CICArg* inher_from)
{
	int64_t raw_type = rpn->raw_type;
	CICArg* res = &rpn->res;
	if (rpn->flags & ICF_PRECOMPUTED)
		return;
	// These have no need to be put into a temporay
	switch (rpn->type) {
		break;
	case __IC_STATIC_REF:
		res->mode = MD_STATIC;
		res->raw_type = raw_type;
		res->off = rpn->integer;
		return;
	case IC_STATIC:
		res->mode = MD_PTR;
		res->raw_type = raw_type;
		res->off = rpn->integer;
		return;
		// These dudes will compile down to a MD_I64/MD_F64
	case IC_I64:
		res->mode = MD_I64;
		res->integer = rpn->integer;
		res->raw_type = RT_I64i;
		return;
		break;
	case IC_F64:
		res->mode = MD_F64;
		res->flt = rpn->flt;
		res->raw_type = RT_F64;
		return;
	case IC_IREG:
	case IC_FREG:
		res->mode = MD_REG;
		res->raw_type = raw_type;
		res->reg = rpn->integer;
		return;
		break;
	case IC_BASE_PTR:
		res->mode = MD_FRAME;
		res->raw_type = raw_type;
		res->off = rpn->integer;
		return;
	}
	if (inher_from) {
		if (rpn->raw_type == RT_F64 && inher_from->raw_type == RT_F64 && inher_from->mode != MD_NULL) {
			rpn->res = *inher_from;
			rpn->flags |= ICF_TMP_NO_UNDO;
			return;
		} else if (rpn->raw_type != RT_F64 && inher_from->raw_type != RT_F64 && inher_from->mode != MD_NULL) {
			rpn->res = *inher_from;
			rpn->flags |= ICF_TMP_NO_UNDO;
			return;
		}
	}
use_defacto:
	if (raw_type != RT_F64) {
		if (cctrl->backend_user_data2 < AIWNIOS_TMP_IREG_CNT) {
			res->mode = MD_REG;
			res->raw_type = raw_type;
			res->off = 0;
			res->reg = TmpRegToReg(cctrl->backend_user_data2++);
		} else {
			PushSpilledTmp(cctrl, rpn);
		}
	} else {
		if (cctrl->backend_user_data3 < AIWNIOS_TMP_FREG_CNT) {
			res->mode = MD_REG;
			res->raw_type = raw_type;
			res->off = 0;
			res->reg = AIWNIOS_TMP_FREG_START + cctrl->backend_user_data3++;
		} else {
			PushSpilledTmp(cctrl, rpn);
		}
	}
}

static void PopTmp(CCmpCtrl* cctrl, CRPN* rpn)
{
	int64_t raw_type = rpn->raw_type;
	// We didnt modift the res with PushTmp,so dont modify the results here
	if (rpn->flags & ICF_PRECOMPUTED)
		return;
	if (rpn->flags & ICF_TMP_NO_UNDO)
		return;
	// These have no need to be put into a temporay
	switch (rpn->type) {
		break;
	case IC_I64:
	case IC_F64:
	case IC_IREG:
	case IC_FREG:
	case IC_BASE_PTR:
	case IC_STATIC:
	case __IC_STATIC_REF:
		return;
	}
	if (rpn->flags & ICF_SPILLED) {
		cctrl->backend_user_data1 -= 8;
	} else {
		if (raw_type != RT_F64) {
			assert(--cctrl->backend_user_data2 >= 0);
		} else {
			assert(--cctrl->backend_user_data3 >= 0);
		}
	}
	assert(cctrl->backend_user_data1 >= 0);
}


static int64_t IsDerefToSIB64(CRPN* r)
{
	switch (r->raw_type) {
	case RT_I64i:
	case RT_PTR:
	case RT_U64i:
	case RT_F64:
		switch (r->type) {
			break;
		case IC_DEREF:
			if(SpillsTmpRegs(r->base.next))
				return 0;
			return 1;
			break;
		case IC_BASE_PTR:
			return 1;
		}
	}
	return 0;
}


//Returns 1 if we hit the bottom
static int64_t PushTmpDepthFirst(CCmpCtrl *cctrl,CRPN *r,int64_t spilled,int64_t allow_sib) {
	switch(r->type) {
		case __IC_STATIC_REF:
		case IC_STATIC:
		case IC_GLOBAL:
		case IC_I64:
		case IC_F64:
		case IC_IREG:
		case IC_FREG:
		case IC_BASE_PTR:
		case IC_STR:
		case IC_CHR:
		if(!spilled) PushTmp(cctrl,r,NULL);
		else PushSpilledTmp(cctrl,r);
		return 1;
	}
	int64_t a,argc,old_icnt=cctrl->backend_user_data2,old_fcnt=cctrl->backend_user_data3,i,i2,old_scnt=cctrl->backend_user_data1;
	CRPN *arg,*arg2,*d;
	switch(r->type) {
	break;case IC_CALL:
	case __IC_CALL:
		for(i=r->length;i>=0;i--) {
			if(i==r->length)
				if(ICArgN(r,i)->type==IC_SHORT_ADDR)
					continue;
			// R9 is a Poop register in SystemV X86,and it is also a call register too
			// Replace any argument referencing poop with a stack argumen
check_for_poop:
			if(cctrl->backend_user_data2<AIWNIOS_TMP_IREG_CNT)			
				for(i2=0;i2!=sizeof(call_iregs)/sizeof(*call_iregs);i2++) {
					if(i2>=r->length) break;
					if(TmpRegToReg(cctrl->backend_user_data2)==call_iregs[i2]) {
						cctrl->backend_user_data2++;
						goto check_for_poop;
					}
				}
			if(i<sizeof(call_iregs)/sizeof(*call_iregs))
				if(call_iregs[i]==AIWNIOS_TMP_IREG_POOP||call_iregs[i]==AIWNIOS_TMP_IREG_POOP2) {
					PushTmpDepthFirst(cctrl,ICArgN(r,i),1,NULL);
					goto pass;
				} 
				for(i2=0;i2<i;i2++)
					if(SpillsTmpRegs(ICArgN(r,i2))) { //REVERSE polish notation
						PushTmpDepthFirst(cctrl,ICArgN(r,i),1,NULL);
						goto pass;
					}
				PushTmpDepthFirst(cctrl,ICArgN(r,i),0,NULL);
pass:;
		}
		cctrl->backend_user_data2=old_icnt;
		cctrl->backend_user_data3=old_fcnt;
		cctrl->backend_user_data1=old_scnt;
		goto fin;		
	break;case IC_TO_I64:
unop:
	PushTmpDepthFirst(cctrl,r->base.next,0,r);
	PopTmp(cctrl,r->base.next);
	goto fin;
  break;case IC_TO_F64: goto unop;
	break;case IC_PAREN: goto unop;
	break;case IC_NEG: goto unop;
	break;case IC_POS: goto unop;
	break;case IC_LT:
	case IC_GT:
	case IC_GE:
	case IC_LE:
cmp_binop:
	arg=ICArgN(r,1);
	arg2=ICArgN(r,0);
	PushTmpDepthFirst(cctrl,arg2,1,r);
	PushTmpDepthFirst(cctrl,arg,1,NULL);
	PopTmp(cctrl,arg);
	PopTmp(cctrl,arg2);
	goto fin;
	break;case IC_POW:
binop:
	arg=ICArgN(r,1);
	arg2=ICArgN(r,0);
	//
	// When we are looking for SIBs,the IC_DEREF is uselss so inhiere from the secret suace 
	//
	if(arg2->type==IC_DEREF&&IsDerefToSIB64(arg2)&&!SpillsTmpRegs(arg)) {
		PushTmpDepthFirst(cctrl,arg2->base.next,0,NULL);
		PushTmpDepthFirst(cctrl,arg,0,r);
		//Make a reference to AIWNIOS_TMP_IREG_POOP2 if our IC doesnt support SIBs
		arg2->res.mode=MD_REG;
		arg2->res.raw_type=arg2->raw_type;
		arg2->res.reg=arg2->raw_type==RT_F64?1:AIWNIOS_TMP_IREG_POOP2;
		PopTmp(cctrl,arg);
		PopTmp(cctrl,arg2->base.next);
		goto fin;
	} else 
		PushTmpDepthFirst(cctrl,arg2,SpillsTmpRegs(arg),NULL);
	PushTmpDepthFirst(cctrl,arg,0,r);
	PopTmp(cctrl,arg);
	PopTmp(cctrl,arg2);
	goto fin;
	break;case IC_ADD: goto binop;
	break;case IC_EQ: goto assign_xop;
	break;case IC_SUB: goto binop;
	break;case IC_DIV: goto binop;
	break;case IC_MUL: goto binop;
	break;case IC_DEREF: 
	//Sometimes IC_DEREF will boil down to a IC_DEREF->next,so inherit the SPILLED part
	if(spilled) {
deref_normal:
		PushTmpDepthFirst(cctrl,r->base.next,spilled,NULL);
		PopTmp(cctrl,r->base.next);
	} else if(allow_sib) {
		/*arg=ICArgN(r,0);
		if(arg->type==IC_ADD) {
			for(a=0;a!=2;a++) {
				if(ICArgN(arg,a)->IC_MUL)
					switch(ICArgN(arg,a)->integer) {
						case 1:
						case 2:
						case 4:
						case 8:
						if()
						PushTmpDepthFirst(cctrl,ICArgN(arg,2-a-1),0);
					}
			}
		}*/
		goto deref_normal;
	} else 
		goto deref_normal;
	goto fin;
	break;case IC_AND: goto binop;
	break;case IC_ADDR_OF: goto unop;
	break;case IC_XOR:goto binop;
	break;case IC_MOD:goto binop;
	break;case IC_OR:goto binop;
	break;case IC_LNOT:goto unop;
	break;case IC_BNOT:goto unop;
	break;case IC_POST_INC:
post_op:
	//This is the hack i was refering to mother-foofer
	//
	// Doing R9=(*R9)++ is a bad idea as we need to keep R9's pointer for storing the result
	//
	// MOV R9,[R9]
	// INC R9
	// MOV [R9], R9
	//
	// I will spill all postops that dont take a constant pointer
	arg=ICArgN(r,0);
	PushTmpDepthFirst(cctrl,arg,0,NULL);
	PopTmp(cctrl,arg);
	if(arg->type==IC_DEREF)
		spilled=1;
	if(!spilled) PushTmp(cctrl,r,NULL);
	else PushSpilledTmp(cctrl,r);
	return 1;
	break;case IC_POST_DEC: goto post_op;
	break;case IC_PRE_INC: goto post_op;
	break;case IC_PRE_DEC: goto post_op;
	break;case IC_AND_AND: goto binop;
	break;case IC_OR_OR: goto binop;
	break;case IC_XOR_XOR: goto binop;
	break;case IC_EQ_EQ: goto binop;
	break;case IC_NE: goto binop;
	break;case IC_LSH: goto binop;
	break;case IC_RSH: goto binop;
assign_xop:
	arg=ICArgN(r,1);
	arg2=ICArgN(r,0);
	PushTmpDepthFirst(cctrl,arg2,SpillsTmpRegs(arg),NULL);
	//Equals (when used with IC_DEREF) are weird,they will preserve the IC_DEREF->next
	// and the DEREF
	// *(arg->next)+=arg2
	// IS TRAETED THE SAME AS 
	// tmp=arg->next;
	// *tmp=*tmp=arg2;
	// 
	// arg is same as *(arg->next)
	//WE WILL NEED TO KEEP arg->next loaded in a register and such    
	if(arg->type==IC_DEREF) {
		PushTmp(cctrl,arg,NULL);
		if(PushTmpDepthFirst(cctrl,arg->base.next,0,r));
		PopTmp(cctrl,arg->base.next);
	} else
		PushTmpDepthFirst(cctrl,arg,0,r);
	PopTmp(cctrl,arg);
	PopTmp(cctrl,arg2);
	goto fin;
	break;case IC_ADD_EQ: goto assign_xop;
	break;case IC_SUB_EQ: goto assign_xop;
	break;case IC_MUL_EQ: goto assign_xop;
	break;case IC_DIV_EQ: goto assign_xop;
	break;case IC_LSH_EQ: goto assign_xop;
	break;case IC_RSH_EQ: goto assign_xop;
	break;case IC_AND_EQ: goto assign_xop;
	break;case IC_OR_EQ: goto assign_xop;
	break;case IC_XOR_EQ: goto assign_xop;
	break;case IC_MOD_EQ: goto assign_xop;
	break;case IC_COMMA: goto binop;
	break;case IC_TYPECAST: goto unop;
	break; case __IC_VARGS: 
	if(!spilled) PushTmp(cctrl,r,NULL);
	else PushSpilledTmp(cctrl,r);
	for(a=0;a!=r->length;a++) {
		PushTmpDepthFirst(cctrl,ICArgN(r,a),1,(a+1==r->length)?r:NULL); //TODO check if next vargs spill 
	}
	while(a--)
		PopTmp(cctrl,ICArgN(r,a));
	break; case IC_RELOC: goto fin;
	break;default: return 0;
	fin:
	if(!spilled) PushTmp(cctrl,r,NULL);
	else PushSpilledTmp(cctrl,r);
	}
	return 1;
}


// Returns the non-constant side of a +Const
static CRPN* __AddOffset(CRPN* r, int64_t* const_val)
{
	CRPN *n0 = ICArgN(r, 0), *n1 = ICArgN(r, 1);
	if (IsConst(n0) && Is32Bit(ConstVal(n0))) {
		if (const_val)
			*const_val = ConstVal(n0);
		return n1;
	}
	if (IsConst(n1) && Is32Bit(ConstVal(n1))) {
		if (const_val)
			*const_val = ConstVal(n1);
		return n0;
	}
	return NULL;
}
static int64_t DstRegAffectsMode(CICArg *d,CICArg *arg) {
	if(d->mode!=MD_REG) return 0;
	int64_t r=d->reg;
	switch(arg->mode) {
		break;case MD_REG: return r==arg->reg;
		break;case MD_INDIR_REG: return r==arg->reg;
		break;case __MD_X86_64_SIB: return r==arg->reg||r==arg->reg2;
	}
	return 0;
}
//
// Here's the deal,we can replace an *(reg+123) with MD_INDIR_REG with
// off==123
//
static int64_t DerefToSIB(CCmpCtrl* cctrl, CICArg* res, CRPN* rpn,
	int64_t base_reg_fallback, char* bin,
	int64_t code_off)
{
	if (rpn->type == IC_BASE_PTR) {
		res->mode = __MD_X86_64_SIB;
		res->off = -rpn->integer;
		res->raw_type = rpn->raw_type;
		res->reg = RBP;
		res->reg2 = -1;
		res->__SIB_scale = 0;
		return code_off;
	}
	if (rpn->type != IC_DEREF) {
		code_off = __OptPassFinal(cctrl, rpn, bin, code_off);
		*res = rpn->res;
		return code_off;
	}
	int64_t r = rpn->raw_type, rsz, off = 0, mul, base_reg;
	int64_t lea=0;
	rpn = rpn->base.next;
	CRPN *next = rpn->base.next, *next2, *next3, *next4, *tmp;
	CICArg to_reg;
	switch (r) {
		break;
	case RT_I8i:
		rsz = 1;
		break;
	case RT_U8i:
		rsz = 1;
		break;
	case RT_I16i:
		rsz = 2;
		break;
	case RT_U16i:
		rsz = 2;
		break;
	case RT_I32i:
		rsz = 4;
		break;
	case RT_U32i:
		rsz = 4;
		break;
	default:
		rsz = 8;
	}
	if (__AddOffset(rpn, &off))
		rpn = __AddOffset(rpn, &off);
	if (rpn->type == IC_ADD) {
		next2 = ICFwd(rpn->base.next);
		if (next->type == IC_I64) {
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			if(next2->res.mode!=MD_REG) {
				lea=1;
				code_off = PutICArgIntoReg(cctrl, &next2->res, RT_PTR, base_reg_fallback,
					bin, code_off);
			}
			res->mode = __MD_X86_64_SIB;
			res->off = next->integer + off;
			res->raw_type = r;
			res->reg = next2->res.reg;
			res->reg2 = -1;
			res->__SIB_scale = 0;
			return code_off;
		} else if (next2->type == IC_I64) {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			if(next->res.mode!=MD_REG) {
				lea=1;
				code_off = PutICArgIntoReg(cctrl, &next->res, RT_PTR, base_reg_fallback,
					bin, code_off);
			}
			res->mode = __MD_X86_64_SIB;
			res->off = next2->integer + off;
			res->raw_type = r;
			res->reg = next->res.reg;
			res->reg2 = -1;
			res->__SIB_scale = 0;
			return code_off;
		} /*else if (next->type == IC_MUL) {
		index_chk:
			next3 = ICArgN(next, 0);
			next4 = ICArgN(next, 1);
			if (1) {
				if (next3->type == IC_I64) {
					mul = next3->integer;
				idx:
					if ((mul == 1 || mul == 2 || mul == 4 || mul == 8) && next4->raw_type != RT_F64) {
						// IC_ADD==rpn
						//   IC_MUL==next
						//      IC_I64==next3
						//      mul==next4
						//   base==next2
						//

						// Store
						code_off = __OptPassFinal(cctrl, next2, bin, code_off);
						code_off = __OptPassFinal(cctrl, next4, bin, code_off);
						if (next4->res.mode != MD_REG) {
							lea=1;
							code_off = PutICArgIntoReg(cctrl, &next4->res, RT_I64i, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
						}
						if (IsConst(next2) && Is32Bit(ConstVal(next2))) {
							off += ConstVal(next2);
							base_reg = -1;
						} else if (next2->res.mode != MD_REG) {
							lea=1;
							code_off=PutICArgIntoReg(cctrl,&next2->res,RT_I64i,AIWNIOS_TMP_IREG_POOP,bin,code_off);
							base_reg=next2->res.reg;
						} else 
							base_reg = next2->res.reg;
						res->mode = __MD_X86_64_SIB;							
						if(!lea) {
						res->__SIB_scale = mul;
						res->off = off;
						res->reg = base_reg;
						res->reg2 = next4->res.reg;
						res->raw_type = r;
						} else {
							AIWNIOS_ADD_CODE(X86LeaSIB,base_reg_fallback,mul,next4->res.reg,base_reg,off);
							res->off = off;
							res->raw_type = r;
							res->reg = base_reg_fallback;
							res->reg2=-1;
							res->__SIB_scale=-1;
						}
						// Restore
						return code_off;
					}
				} else if (next4->type == IC_I64) {
					tmp = next3;
					next3 = next4;
					next4 = tmp;
					mul = next3->integer;
					goto idx;
				} else {
					// See above note
					//  IC_ADD==rpn==next
					//    mul==next4
					//    off==next2
					//
					next2 = next3;
					mul = 1;
					goto idx;
				}
			} else if (next2->type == IC_MUL) {
				tmp = next;
				next = next2;
				next2 = tmp;
				goto index_chk;
			}
		} else {
			mul = 1;
			next = rpn;
			goto index_chk;
		}*/
	}
	code_off = __OptPassFinal(cctrl, rpn, bin, code_off);
	code_off = PutICArgIntoReg(cctrl, &rpn->res, RT_PTR, base_reg_fallback, bin,
		code_off);
	res->mode = __MD_X86_64_SIB;
	res->off = 0;
	res->raw_type = r;
	res->reg = rpn->res.reg;
	res->reg2 = -1;
	res->__SIB_scale = 0;
	return code_off;
}

// See __IC_CALL
static int64_t __ICFCall(CCmpCtrl* cctrl, CRPN* rpn, char* bin,
	int64_t code_off)
{
	CRPN *rpn2 = rpn->base.next, *rpn3;
	CICArg *arg_dsts = NULL, tmp = { 0 },stack={0};
	CCodeMisc* reloc;
	char* fptr;

	//
	// argv_stki is the stack location of the first varg
	//
	int64_t i2, i = rpn->length, to, ii = 0, fi = 0, stki = 0, vargs_sz = 0,orig_wiggle_room=cctrl->backend_user_data1;
	while (--i >= 0)
		rpn2 = ICFwd(rpn2);
	AssignRawTypeToNode(cctrl, rpn2);
	to = rpn->length;
	arg_dsts = A_MALLOC(to * sizeof(CICArg),
		cctrl->hc);
	rpn2 = ICArgN(rpn, rpn->length);
	if(rpn2->type!=IC_SHORT_ADDR) {
		rpn2->raw_type=RT_PTR;
		rpn2->res.raw_type=RT_PTR;
		code_off=__OptPassFinal(cctrl,rpn2,bin,code_off);
	}
	rpn2 = rpn->base.next;
	for (i = 0; i != to;) {
		rpn2 = ICArgN(rpn, rpn->length - i - 1);
		if (rpn2->type == __IC_VARGS) { // Will pop off the vargs for you
			vargs_sz = 8 * rpn2->length;
			if (rpn2->length & 1) // Align to 16
				vargs_sz += 8;
		}
		arg_dsts[i].raw_type = rpn2->raw_type;
		if (rpn2->raw_type == RT_F64) {
			if (fi < sizeof(call_fregs) / sizeof(*call_fregs) && i < to) {
				arg_dsts[i].mode = MD_REG;
				arg_dsts[i].reg = call_fregs[fi++];
			} else {
				arg_dsts[i].mode = MD_INDIR_REG;
				arg_dsts[i].reg = X86_64_STACK_REG;
				arg_dsts[i].off = stki++ * 8;
			}
		} else {
			if (ii < sizeof(call_iregs) / sizeof(*call_iregs) && i < to) {
				arg_dsts[i].raw_type = RT_I64i;
				arg_dsts[i].mode = MD_REG;
				arg_dsts[i].reg = call_iregs[ii++];
			} else {
				arg_dsts[i].raw_type = RT_I64i;
				arg_dsts[i].mode = MD_INDIR_REG;
				arg_dsts[i].reg = X86_64_STACK_REG;
				arg_dsts[i].off = stki++ * 8;
			}
		}
	pass:
		code_off = __OptPassFinal(cctrl, rpn2, bin, code_off);
		rpn2 = ICFwd(rpn2);
		i++;
	}
	// Keep stack aligned to 16 bytes(8 bytes per item)
	if (stki % 2)
		stki++;
	if (stki)
		AIWNIOS_ADD_CODE(X86SubImm32, X86_64_STACK_REG, stki * 8);
	i = rpn->length - 1;
aloop:
	while (i >= 0) {
		i2 = i;
		// IC_CALL
		// ARGn <===rpn->length -1
		// FUN
		rpn2 = ICArgN(rpn, rpn->length - i - 1);
		code_off = ICMov(cctrl, &arg_dsts[i2], &rpn2->res, bin, code_off);
		i--;
	next:;
	}
	rpn2 = ICArgN(rpn, rpn->length);
	if (rpn2->type == IC_SHORT_ADDR) {
		AIWNIOS_ADD_CODE(X86Call32, 5); //+5 is the end of the instuction and will be RIP+0
		//We want RIP+0 so we can OR it later
		rpn2->code_misc->addr=bin+code_off-5;
		goto after_call;
	}
	if (rpn2->type == IC_GLOBAL) {
		if (rpn2->global_var->base.type & HTT_FUN) {
			fptr = ((CHashFun*)rpn2->global_var)->fun_ptr;
			if (!Is32Bit((int64_t)fptr - (int64_t)(bin + code_off)))
				goto defacto;
			if (cctrl->code_ctrl->final_pass >= 2) {
				AIWNIOS_ADD_CODE(X86Call32, (int64_t)fptr - (int64_t)(bin + code_off));
			} else
				goto defacto;
		} else
			goto defacto;
	} else {
	defacto:
		rpn2->res.raw_type=RT_PTR; //Not set for some reason
		code_off = PutICArgIntoReg(cctrl, &rpn2->res, RT_PTR, R10, bin, code_off);
		AIWNIOS_ADD_CODE(X86CallReg, rpn2->res.reg);
	}
after_call:
	if (stki || vargs_sz)
		AIWNIOS_ADD_CODE(X86AddImm32, RSP, stki * 8 + vargs_sz);
	if (rpn->raw_type != RT_U0) {
		tmp.reg = 0;
		tmp.mode = MD_REG;
		tmp.raw_type = rpn->raw_type;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
	}
	A_FREE(arg_dsts);
	rpn2 = ICArgN(rpn, rpn->length);
	cctrl->backend_user_data1=orig_wiggle_room;
	return code_off;
}

int64_t ICMov(CCmpCtrl* cctrl, CICArg* dst, CICArg* src, char* bin,
	int64_t code_off)
{
	int64_t use_reg, use_reg2, restore_from_tmp = 0, indir_off = 0,
							   indir_off2 = 0, opc;
	assert(src->mode>0);
	assert(src->mode<100);
	CICArg tmp = { 0 };
	if (dst->mode == MD_NULL)
		return code_off;
	if ((dst->raw_type == RT_F64) == (src->raw_type == RT_F64))
		if (dst->mode == src->mode) {
			switch (dst->mode) {
				break;
			case __MD_X86_64_SIB:
				if (dst->off == src->off && dst->__SIB_scale == src->__SIB_scale
					&& dst->reg == src->reg && dst->reg2 == src->reg2) {
					return code_off;
				}
				break;
			case MD_STATIC:
				if (dst->off == src->off)
					return code_off;
				break;
			case MD_PTR:
				if (dst->off == src->off)
					return code_off;
				break;
			case MD_REG:
				if (dst->reg == src->reg)
					return code_off;
				break;
			case MD_FRAME:
				if (dst->off == src->off)
					return code_off;
				break;
			case MD_INDIR_REG:
				if (dst->off == src->off && dst->reg == src->reg)
					return code_off;
			}
		}
	switch (dst->mode) {
		break;
	case __MD_X86_64_SIB:
		if (src->mode == MD_REG && ((src->raw_type == RT_F64) == (dst->raw_type == RT_F64))) {
			switch (dst->raw_type) {
				break;
			case RT_U8i:
			case RT_I8i:
				AIWNIOS_ADD_CODE(X86MovIndirRegI8, src->reg, dst->__SIB_scale, dst->reg2, dst->reg, dst->off);
				break;
			case RT_U16i:
			case RT_I16i:
				AIWNIOS_ADD_CODE(X86MovIndirRegI16, src->reg, dst->__SIB_scale, dst->reg2, dst->reg, dst->off);
				break;
			case RT_U32i:
			case RT_I32i:
				AIWNIOS_ADD_CODE(X86MovIndirRegI32, src->reg, dst->__SIB_scale, dst->reg2, dst->reg, dst->off);
				break;
			case RT_PTR:
			case RT_U64i:
			case RT_I64i:
			case RT_FUNC:
				AIWNIOS_ADD_CODE(X86MovIndirRegI64, src->reg, dst->__SIB_scale, dst->reg2, dst->reg, dst->off);
				break;
			case RT_F64:
				AIWNIOS_ADD_CODE(X86MovIndirRegF64, src->reg, dst->__SIB_scale, dst->reg2, dst->reg, dst->off);
				break;
			default: abort();
 			}
			return code_off;
		}
		goto dft;
		break;
	case MD_STATIC:
		use_reg2 = RAX;
		indir_off = 0;
		if (cctrl->code_ctrl->final_pass >= 2) {
			AIWNIOS_ADD_CODE(X86LeaSIB, use_reg2, -1, -1, RIP, dst->off - code_off + cctrl->code_ctrl->statics_offset);
		} else
			AIWNIOS_ADD_CODE(X86LeaSIB, use_reg2, -1, -1, RIP, 0xffeeff);
		goto indir_r2;
		break;
	case MD_INDIR_REG:
		use_reg2 = dst->reg;
		indir_off = dst->off;
	indir_r2:
		if (src->raw_type == dst->raw_type && src->raw_type == RT_F64 && src->mode == MD_REG) {
			use_reg = src->reg;
		} else if ((src->raw_type == RT_F64) ^ (RT_F64 == dst->raw_type)) { // Do they differ
			goto dft;
		} else if (src->mode == MD_FRAME || src->mode == MD_PTR || src->mode == MD_INDIR_REG) {
			goto dft;
		} else if (src->mode == MD_REG)
			use_reg = src->reg;
		else {
		dft:
			tmp.raw_type = src->raw_type;
			if (dst->raw_type != RT_F64)
				use_reg = tmp.reg = AIWNIOS_TMP_IREG_POOP;
			else
				use_reg = tmp.reg = 0;
			tmp.mode = MD_REG;
			if (dst->raw_type == RT_F64 && src->raw_type != dst->raw_type) {
				code_off = PushToStack(cctrl, src, bin, code_off);
				AIWNIOS_ADD_CODE(X86CVTTSI2SDRegSIB64, 2, -1, -1, RSP, 0);
				AIWNIOS_ADD_CODE(X86AddImm32,RSP,8);
				tmp.raw_type = RT_F64;
				tmp.reg = 2;
			} else if (src->raw_type == RT_F64 && src->raw_type != dst->raw_type) {
				code_off = PushToStack(cctrl, src, bin, code_off);
				AIWNIOS_ADD_CODE(X86CVTTSD2SIRegSIB64, 0, -1, -1, RSP, 0);
				AIWNIOS_ADD_CODE(X86AddImm32,RSP,8);
				tmp.raw_type = RT_I64i;
				tmp.reg = 0;
			} else
				code_off = ICMov(cctrl, &tmp, src, bin, code_off);
			code_off = ICMov(cctrl, dst, &tmp, bin, code_off);
			return code_off;
		}
		goto store_r2;
		break;
	case MD_PTR:
		use_reg2 = RAX;
		code_off = __ICMoveI64(cctrl, use_reg2, dst->off, bin, code_off);
		indir_off = 0;
		goto indir_r2;
	store_r2:
		switch (dst->raw_type) {
		case RT_U0:
			break;
		case RT_F64:
			AIWNIOS_ADD_CODE(X86MovIndirRegF64, use_reg, -1, -1, use_reg2, indir_off);
			break;
		case RT_U8i:
		case RT_I8i:
			AIWNIOS_ADD_CODE(X86MovIndirRegI8, use_reg, -1, -1, use_reg2, indir_off);
			break;
		case RT_U16i:
		case RT_I16i:
			AIWNIOS_ADD_CODE(X86MovIndirRegI16, use_reg, -1, -1, use_reg2, indir_off);
			break;
		case RT_U32i:
		case RT_I32i:
			AIWNIOS_ADD_CODE(X86MovIndirRegI32, use_reg, -1, -1, use_reg2, indir_off);
			break;
		case RT_U64i:
		case RT_I64i:
		case RT_FUNC: // func ptr
		case RT_PTR:
			AIWNIOS_ADD_CODE(X86MovIndirRegI64, use_reg, -1, -1, use_reg2, indir_off);
			break;
		default:
			abort();
		}
		break;
	case MD_FRAME:
		use_reg2 = X86_64_BASE_REG;
		indir_off = -dst->off;
		goto indir_r2;
		break;
	case MD_REG:
		use_reg = dst->reg;
		if (src->mode == MD_FRAME) {
			if (src->raw_type == RT_F64 && src->raw_type == dst->raw_type) {
				use_reg2 = X86_64_BASE_REG;
				indir_off = -src->off;
				goto load_r2;
			} else if ((src->raw_type == RT_F64) ^ (RT_F64 == dst->raw_type)) {
				goto dft;
			}
			use_reg2 = X86_64_BASE_REG;
			indir_off = -src->off;
			goto load_r2;
		} else if (src->mode == __MD_X86_64_SIB) {
			if ((src->raw_type == RT_F64) != (dst->raw_type == RT_F64))
				goto dft;
			switch (src->raw_type) {
			case RT_U0:
				break;
			case RT_U8i:
				AIWNIOS_ADD_CODE(X86MovZXRegIndirI8, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
				break;
			case RT_I8i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI8, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
				break;
			case RT_U16i:
				AIWNIOS_ADD_CODE(X86MovZXRegIndirI16, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
				break;
			case RT_I16i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI16, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
				break;
			case RT_U32i:
				AIWNIOS_ADD_CODE(X86MovRegIndirI32, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
				break;
			case RT_I32i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI32, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
				break;
			case RT_U64i:
			case RT_PTR:
			case RT_FUNC:
			case RT_I64i:
				AIWNIOS_ADD_CODE(X86MovRegIndirI64, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
				break;
			case RT_F64:
				AIWNIOS_ADD_CODE(X86MovRegIndirF64, dst->reg, src->__SIB_scale, src->reg2, src->reg, src->off);
			}
			return code_off;
		} else if (src->mode == MD_REG) {

			if (src->raw_type == RT_F64 && src->raw_type == dst->raw_type) {
				AIWNIOS_ADD_CODE(X86MovRegRegF64, dst->reg, src->reg);
			} else if (src->raw_type != RT_F64 && dst->raw_type != RT_F64) {
				AIWNIOS_ADD_CODE(X86MovRegReg, dst->reg, src->reg);
			} else if (src->raw_type == RT_F64 && dst->raw_type != RT_F64) {
				goto dft;
			} else if (src->raw_type != RT_F64 && dst->raw_type == RT_F64) {
				goto dft;
			}
		} else if (src->mode == MD_PTR) {
			if (src->raw_type == RT_F64 && src->raw_type == dst->raw_type) {
				use_reg2 = 0;
				code_off = __ICMoveI64(cctrl, use_reg2, src->off, bin, code_off);
				goto load_r2;
			} else if ((src->raw_type == RT_F64) ^ (RT_F64 == dst->raw_type)) {
				goto dft;
			}
			switch (src->raw_type) {
			case RT_U0:
				break;
			case RT_U8i:
				AIWNIOS_ADD_CODE(X86MovZXRegIndirI8, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
				break;
			case RT_I8i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI8, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
				break;
			case RT_U16i:
				AIWNIOS_ADD_CODE(X86MovZXRegIndirI16, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
				break;
			case RT_I16i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI16, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
				break;
			case RT_U32i:
				AIWNIOS_ADD_CODE(X86MovRegIndirI32, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
				break;
			case RT_I32i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI32, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
				break;
			case RT_U64i:
			case RT_PTR:
			case RT_FUNC:
			case RT_I64i:
				AIWNIOS_ADD_CODE(X86MovRegIndirI64, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
				break;
			case RT_F64:
				AIWNIOS_ADD_CODE(X86MovRegIndirF64, dst->reg, -1, -1, RIP, (char*)src->integer-(bin+code_off));
			}
			return code_off;
		load_r2:
			switch (src->raw_type) {
			case RT_U0:
				break;
			case RT_U8i:
				AIWNIOS_ADD_CODE(X86MovZXRegIndirI8, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			case RT_I8i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI8, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			case RT_U16i:
				AIWNIOS_ADD_CODE(X86MovZXRegIndirI16, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			case RT_I16i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI16, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			case RT_U32i:
				AIWNIOS_ADD_CODE(X86MovRegIndirI32, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			case RT_I32i:
				AIWNIOS_ADD_CODE(X86MovSXRegIndirI32, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			case RT_U64i:
			case RT_PTR:
			case RT_FUNC:
			case RT_I64i:
				AIWNIOS_ADD_CODE(X86MovRegIndirI64, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			case RT_F64:
				AIWNIOS_ADD_CODE(X86MovRegIndirF64, dst->reg, -1, -1, use_reg2, indir_off);
				break;
			default:
				abort();
			}
		} else if (src->mode == MD_INDIR_REG) {
			use_reg2 = src->reg;
			indir_off = src->off;
			if (src->raw_type == RT_F64 && src->raw_type == dst->raw_type) {
				use_reg2 = src->reg;
			} else if ((src->raw_type == RT_F64) ^ (RT_F64 == dst->raw_type)) {
				goto dft;
			}
			goto load_r2;
		} else if (src->mode == MD_I64 && dst->raw_type != RT_F64) {
			code_off = __ICMoveI64(cctrl, dst->reg, src->integer, bin, code_off);
		} else if (src->mode == MD_F64 && dst->raw_type == RT_F64) {
			code_off = __ICMoveF64(cctrl, dst->reg, src->flt, bin, code_off);
		} else if (src->mode == MD_I64 && dst->raw_type == RT_F64) {
			code_off = __ICMoveF64(cctrl, dst->reg, src->integer, bin, code_off);
		} else if (src->mode == MD_F64 && dst->raw_type != RT_F64) {
			code_off = __ICMoveI64(cctrl, dst->reg, src->flt, bin, code_off);
		} else if (src->mode == MD_STATIC) {
			use_reg2 = RAX;
			indir_off = 0;
			AIWNIOS_ADD_CODE(X86LeaSIB, use_reg2, -1, -1, RIP, src->off - code_off + cctrl->code_ctrl->statics_offset);
			if (src->raw_type == RT_F64 && src->raw_type == dst->raw_type) {
			} else if ((src->raw_type == RT_F64) ^ (RT_F64 == dst->raw_type)) {
				goto dft;
			}
			goto load_r2;
		} else
			goto dft;
		break;
	default:
		abort();
	}
	return code_off;
}

static int64_t DerefToICArg(CCmpCtrl* cctrl, CICArg* res, CRPN* rpn,
	int64_t base_reg_fallback, char* bin,
	int64_t code_off)
{
	if (rpn->type != IC_DEREF) {
		code_off = __OptPassFinal(cctrl, rpn, bin, code_off);
		*res = rpn->res;
		return code_off;
	}
	int64_t r = rpn->raw_type, rsz, off, mul,lea=0;
	rpn = rpn->base.next;
	CRPN *next = rpn->base.next, *next2, *next3, *next4, *tmp;
	switch (r) {
		break;
	case RT_I8i:
		rsz = 1;
		break;
	case RT_U8i:
		rsz = 1;
		break;
	case RT_I16i:
		rsz = 2;
		break;
	case RT_U16i:
		rsz = 2;
		break;
	case RT_I32i:
		rsz = 4;
		break;
	case RT_U32i:
		rsz = 4;
		break;
	default:
		rsz = 8;
	}
	if (rpn->type == IC_ADD) {
		next2 = ICFwd(rpn->base.next);
		if (next->type == IC_I64) {
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_PTR, base_reg_fallback,
				bin, code_off);
			res->mode = MD_INDIR_REG;
			res->off = next->integer;
			res->raw_type = r;
			res->reg = next2->res.reg;
			return code_off;
		} else if (next2->type == IC_I64) {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, RT_PTR, base_reg_fallback,
				bin, code_off);
			res->mode = MD_INDIR_REG;
			res->off = next2->integer;
			res->raw_type = r;
			res->reg = next->res.reg;
			return code_off;
		}/* else if (next->type == IC_MUL) {
				index_chk:
			off = 0;
			next3 = ICArgN(next, 0);
			next4 = ICArgN(next, 1);
			if(SpillsTmpRegs(next4)) goto exit;
			if (cctrl->backend_user_data2 + 2 - off <= AIWNIOS_TMP_IREG_CNT) {
				if (next3->type == IC_I64) {
					mul = next3->integer;
				idx:
					if (rsz == mul && next4->raw_type != RT_F64) {
						// IC_ADD==rpn
						//   IC_MUL==next
						//      IC_I64==next3
						//      mul==next4
						//   off==next2
						//

						// Store
						code_off = __OptPassFinal(cctrl, next2, bin, code_off);
						code_off = __OptPassFinal(cctrl, next4, bin, code_off);
						if(next4->res.mode != MD_REG) {
							lea=1;
							code_off=PutICArgIntoReg(cctrl,&next4->res,RT_PTR,AIWNIOS_TMP_IREG_POOP2,bin,code_off);
						}
						if(next2->res.mode != MD_REG) {
							lea=1;
							code_off=PutICArgIntoReg(cctrl,&next2->res,RT_PTR,AIWNIOS_TMP_IREG_POOP2,bin,code_off);
						}
						if(!lea&&0) {
							res->mode = __MD_X86_64_SIB;
							res->__SIB_scale = mul;
							res->reg = next2->res.reg;
							res->reg2 = next4->res.reg;
							res->raw_type = r;
							res->off=0;
						} else {
							AIWNIOS_ADD_CODE(X86LeaSIB,base_reg_fallback,mul,next4->res.reg,next2->res.reg,off);
							res->mode = MD_INDIR_REG;
							res->off = 0;
							res->raw_type = r;
							res->reg = base_reg_fallback;
						}
						return code_off;
					}
				} else if (next4->type == IC_I64) {
					tmp = next3;
					next3 = next4;
					next4 = tmp;
					mul = next3->integer;
					goto idx;
				} else {
					// See above note
					//  IC_ADD==rpn==next
					//    mul==next4
					//    off==next2
					//
					next2 = next3;
					mul = 1;
					goto idx;
				}
			} else if (next2->type == IC_MUL) {
				tmp = next;
				next = next2;
				next2 = tmp;
				goto index_chk;
			}
		} else {
			mul = 1;
			next = rpn;
			goto index_chk;
		}*/
	}
exit:
	code_off = __OptPassFinal(cctrl, rpn, bin, code_off);
	code_off = PutICArgIntoReg(cctrl, &rpn->res, RT_PTR, base_reg_fallback, bin,
		code_off);
	res->mode = MD_INDIR_REG;
	res->off = 0;
	res->raw_type = r;
	res->reg = rpn->res.reg;
	return code_off;
}

#define TYPECAST_ASSIGN_BEGIN(DST, SRC)                                                    \
	{                                                                                      \
		int64_t pop = 0, pop2 = 0;                                                         \
		CICArg _orig = { 0 };                                                              \
		CRPN* _tc = DST;                                                                   \
		CRPN* SRC2 = SRC;                                                                  \
		DST = DST->base.next;                                                              \
		if (DST->type == IC_DEREF) {                                                       \
			code_off = DerefToICArg(cctrl, &_orig, DST, 1, bin, code_off);                 \
			DST->res = _orig;                                                              \
			DST->raw_type = _tc->raw_type;                                                 \
		} else if ((DST->raw_type == RT_F64) ^ (SRC2->raw_type == RT_F64)) {               \
			pop2 = pop = 1;                                                                \
			_orig = DST->res;                                                              \
			code_off = PutICArgIntoReg(cctrl, &DST->res, DST->raw_type, 0, bin, code_off); \
			code_off = PushToStack(cctrl, &DST->res, bin, code_off);                       \
			DST->res.mode = MD_INDIR_REG;                                                  \
			DST->res.off = 0;                                                              \
			DST->res.reg = X86_64_STACK_REG;                                               \
			DST->res.raw_type = _tc->raw_type;                                             \
			DST->raw_type = _tc->raw_type;                                                 \
		} else {                                                                           \
			pop2 = 1;                                                                      \
			DST->res.raw_type = _tc->raw_type;                                             \
			DST->raw_type = _tc->raw_type;                                                 \
		}
#define TYPECAST_ASSIGN_END(DST)                               \
	if (pop)                                                   \
		code_off = PopFromStack(cctrl, &_orig, bin, code_off); \
	}

static int64_t __SexyPreOp(CCmpCtrl* cctrl, CRPN* rpn, int64_t (*i_imm)(char*, int64_t, int64_t),
	int64_t (*ireg)(char*, int64_t, int64_t),
	int64_t (*freg)(char*, int64_t, int64_t),
	int64_t (*incr)(char*, int64_t),
	int64_t (*incr_sib)(char*, int64_t,int64_t,int64_t,int64_t),
	char* bin, int64_t code_off)
{
	int64_t code;
	CRPN *next = rpn->base.next, *tc;
	CICArg derefed = { 0 }, tmp = { 0 }, tmp2 = { 0 };
	if (next->type == IC_TYPECAST) {
		TYPECAST_ASSIGN_BEGIN(next, next);
		if (next->raw_type == RT_F64) {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp = derefed; //==RT_F64
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, 1, bin, code_off);
			code_off = __ICMoveF64(cctrl, 0, rpn->integer, bin, code_off);
			AIWNIOS_ADD_CODE(freg, tmp.reg, 0);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		} else if (rpn->integer != 1) {
			code_off = DerefToICArg(cctrl, &derefed, next, 3, bin, code_off);
			tmp = derefed;
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, 2, bin, code_off);
			AIWNIOS_ADD_CODE(i_imm, tmp.reg, rpn->integer);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		} else if(IsDerefToSIB64(next)) {
			code_off = DerefToSIB(cctrl,&derefed,next,AIWNIOS_TMP_IREG_POOP2,bin,code_off);
			AIWNIOS_ADD_CODE(incr_sib, derefed.__SIB_scale,derefed.reg2,derefed.reg,derefed.off);
			code_off = ICMov(cctrl, &rpn->res, &derefed, bin, code_off);
		} else {
			CICArg orig;
			code_off = DerefToICArg(cctrl, &next->res, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			orig = next->res;
			tmp=next->res;
			code_off=PutICArgIntoReg(cctrl,&tmp,RT_I64i,RAX,bin,code_off);
			AIWNIOS_ADD_CODE(incr, tmp.reg);
			code_off = ICMov(cctrl, &orig, &tmp, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		TYPECAST_ASSIGN_END(next);
	} else {
		if (next->raw_type == RT_F64) {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp = derefed; //==RT_F64
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, 1, bin, code_off);
			code_off = __ICMoveF64(cctrl, 0, rpn->integer, bin, code_off);
			AIWNIOS_ADD_CODE(freg, tmp.reg, 0);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		} else if (rpn->integer != 1) {
			code_off = DerefToICArg(cctrl, &derefed, next, 3, bin, code_off);
			tmp = derefed;
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, 2, bin, code_off);
			AIWNIOS_ADD_CODE(i_imm, tmp.reg, rpn->integer);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		} else if(IsDerefToSIB64(next)) {
			code_off = DerefToSIB(cctrl,&derefed,next,AIWNIOS_TMP_IREG_POOP2,bin,code_off);
			AIWNIOS_ADD_CODE(incr_sib, derefed.__SIB_scale,derefed.reg2,derefed.reg,derefed.off);
			code_off = ICMov(cctrl, &rpn->res, &derefed, bin, code_off);
		} else {
			CICArg orig;
			code_off = DerefToICArg(cctrl, &next->res, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			orig = next->res;
			tmp=next->res;
			code_off=PutICArgIntoReg(cctrl,&tmp,RT_I64i,RAX,bin,code_off);
			AIWNIOS_ADD_CODE(incr, tmp.reg);
			code_off = ICMov(cctrl, &orig, &tmp, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
	}
	return code_off;
}

static int64_t __SexyAssignOp(CCmpCtrl* cctrl, CRPN* rpn,
	char* bin, int64_t code_off)
{
	CRPN *next = ICArgN(rpn, 1), *next2 = ICArgN(rpn, 0);
	CICArg old = rpn->res, dummy,old2={0};
enter:;
	int64_t old_type = rpn->type, od = cctrl->backend_user_data2, next_old = next->type, decr = 0,
			old_next2 = next2->type, pop = 0,use_tc=0,use_raw_type=rpn->raw_type,use_f64=0,old_raw_type=rpn->raw_type;
	if(next->type==IC_TYPECAST) {
			use_raw_type=next->raw_type;
			next=ICArgN(next,0);
			use_tc=1;
			goto enter;
		}
	switch (rpn->type) {
		break;
	case IC_ADD_EQ:
// PushTmp will set next->res
#define XXX_EQ_OP(op)                                                                \
	rpn->type = op;                                                                  \
	use_f64=next2->raw_type==RT_F64||next->raw_type==RT_F64; \
	switch (next->type) {                                                            \
	case IC_IREG:                                                                    \
	case IC_FREG:                                                                    \
	case IC_BASE_PTR:                                                                \
		rpn->res = next->res;                                                        \
		if(use_f64) rpn->raw_type=RT_F64; \
		code_off = __OptPassFinal(cctrl, rpn, bin, code_off);                        \
		rpn->raw_type=old_raw_type; \
		code_off = ICMov(cctrl, &old, &rpn->res, bin, code_off);                     \
		break;                                                                       \
	case IC_DEREF:                                                                   \
		code_off = __OptPassFinal(cctrl, next2, bin, code_off);            \
		code_off = __OptPassFinal(cctrl, next->base.next, bin, code_off);                      \
		dummy = ((CRPN*)next->base.next)->res;                                       \
		old2=dummy; \
		code_off = PutICArgIntoReg(cctrl, &next2->res, next2->res.raw_type,          \
			use_f64 ? AIWNIOS_TMP_IREG_POOP2 : 1, bin, code_off);  \
		code_off = PutICArgIntoReg(cctrl, &dummy, RT_U64i,AIWNIOS_TMP_IREG_POOP , bin, code_off);  \
		next->res.reg=dummy.reg; \
		next->res.mode=MD_INDIR_REG; \
		next->res.off=0; \
		code_off = PutICArgIntoReg(cctrl, &next->res, use_f64?RT_F64:rpn->res.raw_type,use_f64?0:AIWNIOS_TMP_IREG_POOP , bin, code_off);  \
		next->flags |= ICF_PRECOMPUTED;                                              \
		next2->flags |= ICF_PRECOMPUTED;                                             \
		rpn->res.mode=MD_REG; \
		rpn->res.reg=use_f64?1:RAX;								\
		if(use_f64) rpn->raw_type=RT_F64; \
		rpn->res.raw_type=rpn->raw_type; \
		code_off = __OptPassFinal(cctrl, rpn, bin, code_off);                        \
		rpn->raw_type=old_raw_type; \
		next->flags &= ~ICF_PRECOMPUTED;                                             \
		next2->flags &= ~ICF_PRECOMPUTED;                                            \
		code_off = PutICArgIntoReg(cctrl, &old2, RT_PTR,RDX, bin, code_off);  \
		if(!use_tc) 																	 \
			old2.raw_type = next->raw_type;                                         \
		else 																			\
			old2.raw_type = use_raw_type;                                         \
		old2.mode=MD_INDIR_REG; \
		old2.off=0; \
		code_off=ICMov(cctrl,&old2,&rpn->res,bin,code_off) ; \
		code_off=ICMov(cctrl,&old,&rpn->res,bin,code_off) ; \
		break;                                                                       \
	default:                                                                         \
		rpn->res = next->res;                                                        \
		if(use_f64) rpn->raw_type=RT_F64; \
		code_off = __OptPassFinal(cctrl, rpn, bin, code_off);                        \
		rpn->raw_type=old_raw_type; \
		code_off = ICMov(cctrl, &next->res, &rpn->res, bin, code_off);               \
	}                                                                                \
	rpn->type = old_type, rpn->res = old;                                            \
	next->type = next_old;                                                           \
	next2->type = old_next2;
		XXX_EQ_OP(IC_ADD);
		break;
	case IC_SUB_EQ:
		XXX_EQ_OP(IC_SUB);
		break;
	case IC_MUL_EQ:
		XXX_EQ_OP(IC_MUL);
		break;
	case IC_DIV_EQ:
		XXX_EQ_OP(IC_DIV);
		break;
	case IC_LSH_EQ:
		XXX_EQ_OP(IC_LSH);
		break;
	case IC_RSH_EQ:
		XXX_EQ_OP(IC_RSH);
		break;
	case IC_AND_EQ:
		XXX_EQ_OP(IC_AND);
		break;
	case IC_OR_EQ:
		XXX_EQ_OP(IC_OR);
		break;
	case IC_XOR_EQ:
		XXX_EQ_OP(IC_XOR);
		break;
	case IC_MOD_EQ:
		XXX_EQ_OP(IC_MOD);
	}
	return code_off;
}

static int64_t IsSavedIReg(int64_t r)
{
	switch (r) {
	case RBX:
	case RSP:
	case RBP:
	case R12:
	case R13:
	case R14:
	case R15:
	}
	return 1;
}

static int64_t IsSavedFReg(int64_t r)
{
	// Poopacle
	return 0;
}
// This is used for FuncProlog/Epilog. It is used for finding the non-violatle
// registers that must be saved and pushed to the stack
//
// to_push is a char[16] ,1 is push,0 is not push
//
// Returns number of pushed regs
//
static int64_t __FindPushedIRegs(CCmpCtrl* cctrl, char* to_push)
{
	CMemberLst* lst;
	CRPN* r;
	int64_t cnt = 0;
	memset(to_push, 0, 16);
	if (!cctrl->cur_fun) {
		// Perhaps we are being called from HolyC and we aren't using a "cur_fun"
		for (r = cctrl->code_ctrl->ir_code->next; r != cctrl->code_ctrl->ir_code; r = r->base.next) {
			if (r->type == IC_IREG && r->raw_type != RT_F64) {
				if (!to_push[r->integer])
					if (IsSavedIReg(r->integer)) {
						to_push[r->integer] = 1;
						cnt++;
					}
			}
		}
		return cnt;
	}
	for (lst = cctrl->cur_fun->base.members_lst; lst; lst = lst->next) {
		if (lst->reg != REG_NONE && lst->member_class->raw_type != RT_F64) {
			assert(lst->reg >= 0);
			to_push[lst->reg] = 1;
			cnt++;
		}
	}
	return cnt;
}

static int64_t __FindPushedFRegs(CCmpCtrl* cctrl, char* to_push)
{
	CMemberLst* lst;
	int64_t cnt = 0;
	memset(to_push, 0, 16);
	CRPN* r;
	if (!cctrl->cur_fun) {
		// Perhaps we are being called from HolyC and we aren't using a "cur_fun"
		for (r = cctrl->code_ctrl->ir_code->next; r != cctrl->code_ctrl->ir_code; r = r->base.next) {
			if (r->type == IC_FREG && r->raw_type == RT_F64) {
				if (!to_push[r->integer])
					if (IsSavedFReg(r->integer)) {
						to_push[r->integer] = 1;
						cnt++;
					}
			}
		}
		return cnt;
	}
	for (lst = cctrl->cur_fun->base.members_lst; lst; lst = lst->next) {
		if (lst->reg != REG_NONE && lst->member_class->raw_type == RT_F64) {
			assert(lst->reg >= 0);
			to_push[lst->reg] = 1;
			cnt++;
		}
	}
	return cnt;
}

// Add 1 in first bit for oppositive
#define X86_COND_A 0x10
#define X86_COND_AE 0x20
#define X86_COND_B 0x30
#define X86_COND_BE 0x40
#define X86_COND_C 0x50
#define X86_COND_E 0x60
#define X86_COND_Z X86_COND_E
#define X86_COND_G 0x70
#define X86_COND_GE 0x80
#define X86_COND_L 0x90
#define X86_COND_LE 0xa0
static int64_t X86Jcc(char* to, int64_t cond, int64_t off)
{
	int64_t len = 0;
	ADD_U8(0x0f);
	switch (cond) {
		break;
	case X86_COND_A:
		ADD_U8(0x87);
		break;
	case X86_COND_AE:
		ADD_U8(0x83);
		break;
	case X86_COND_B:
		ADD_U8(0x82);
		break;
	case X86_COND_BE:
		ADD_U8(0x86);
		break;
	case X86_COND_C:
		ADD_U8(0x82);
		break;
	case X86_COND_E:
		ADD_U8(0x84);
		break;
	case X86_COND_G:
		ADD_U8(0x8f);
		break;
	case X86_COND_GE:
		ADD_U8(0x8d);
		break;
	case X86_COND_L:
		ADD_U8(0x8c);
		break;
	case X86_COND_LE:
		ADD_U8(0x8e);

		break;
	case X86_COND_A | 1:
		ADD_U8(0x86);
		break;
	case X86_COND_AE | 1:
		ADD_U8(0x82);
		break;
	case X86_COND_B | 1:
		ADD_U8(0x83);
		break;
	case X86_COND_BE | 1:
		ADD_U8(0x87);
		break;
	case X86_COND_C | 1:
		ADD_U8(0x83);
		break;
	case X86_COND_E | 1:
		ADD_U8(0x85);
		break;
	case X86_COND_G | 1:
		ADD_U8(0x8e);
		break;
	case X86_COND_GE | 1:
		ADD_U8(0x8c);
		break;
	case X86_COND_L | 1:
		ADD_U8(0x8d);
		break;
	case X86_COND_LE | 1:
		ADD_U8(0x8f);
	}
	ADD_U32(off - 6); //-6 as inst is 6 bytes long(RIP comes after INST);
	return len;
}
// 1st MOV RDI,1
//...
// 6th MOV R8,6
// PUSH 7
// CALL FOO
// ADD RSP,8 //To remove 7 from the stack

static int64_t X86Setcc(char* to, int64_t cond, int64_t r)
{
	int64_t len = 0;
	if ((r & 0b1000)||r==RSI||r==RDI)
		ADD_U8(ErectREX(0, 0, 0, r))
	ADD_U8(0x0f);
	switch (cond) {
		break;
	case X86_COND_A:
		ADD_U8(0x97);
		break;
	case X86_COND_AE:
		ADD_U8(0x93);
		break;
	case X86_COND_B:
		ADD_U8(0x92);
		break;
	case X86_COND_BE:
		ADD_U8(0x96);
		break;
	case X86_COND_C:
		ADD_U8(0x92);
		break;
	case X86_COND_E:
		ADD_U8(0x94);
		break;
	case X86_COND_G:
		ADD_U8(0x9f);
		break;
	case X86_COND_GE:
		ADD_U8(0x9d);
		break;
	case X86_COND_L:
		ADD_U8(0x9c);
		break;
	case X86_COND_LE:
		ADD_U8(0x9e);

		break;
	case X86_COND_A | 1:
		ADD_U8(0x96);
		break;
	case X86_COND_AE | 1:
		ADD_U8(0x92);
		break;
	case X86_COND_B | 1:
		ADD_U8(0x93);
		break;
	case X86_COND_BE | 1:
		ADD_U8(0x97);
		break;
	case X86_COND_C | 1:
		ADD_U8(0x93);
		break;
	case X86_COND_E | 1:
		ADD_U8(0x95);
		break;
	case X86_COND_G | 1:
		ADD_U8(0x9e);
		break;
	case X86_COND_GE | 1:
		ADD_U8(0x9c);
		break;
	case X86_COND_L | 1:
		ADD_U8(0x9d);
		break;
	case X86_COND_LE | 1:
		ADD_U8(0x9f);
	}
	ADD_U8(MODRMRegReg(r, r)); // REG is not used by processor Mr poopneilius
	if (!to)
		len += X86MovZXRegRegI8(NULL, r, r);
	else
		len += X86MovZXRegRegI8(to + len, r, r);
	return len;
}

static int64_t FuncProlog(CCmpCtrl* cctrl, char* bin, int64_t code_off)
{
	char push_ireg[16];
	char push_freg[16];
	char ilist[16];
	char flist[16];
	int64_t i, i2, i3, r, r2, ireg_arg_cnt, freg_arg_cnt, stk_arg_cnt, fsz, code, off;
	CMemberLst* lst;
	CICArg fun_arg = { 0 }, write_to = { 0 },stack={0};
	CRPN *rpn, *arg;
	if (cctrl->cur_fun) {
		fsz = cctrl->cur_fun->base.sz + 16; //+16 for RBP/return address
	} else {
		fsz = 16;
		for (rpn = cctrl->code_ctrl->ir_code->next; rpn != cctrl->code_ctrl->ir_code; rpn = rpn->base.next) {
			if (rpn->type == __IC_SET_FRAME_SIZE) {
				fsz = rpn->integer;
				break;
			}
		}
	}
	// ALIGN TO 8
	if (fsz % 8)
		fsz += 8 - fsz % 8;
	int64_t to_push = (i2 = __FindPushedIRegs(cctrl, push_ireg)) * 8 + (i3 = __FindPushedFRegs(cctrl, push_freg)) * 8 + cctrl->backend_user_data0 + fsz, old_regs_start;
	if (i2 % 2)
		to_push += 8; // We push a dummy register in a pair if not aligned to 2
	if (i3 % 2)
		to_push += 8; // Ditto

	if (to_push % 16)
		to_push += 8;
	cctrl->backend_user_data4 = fsz;
	old_regs_start = fsz + cctrl->backend_user_data0;

	i2 = 0;
	for (i = 0; i != 16; i++)
		if (push_ireg[i])
			ilist[i2++] = i;
	if (i2 % 2)
		ilist[i2++] = 1; // Dont use 0 as it is a reutrn register
	i3 = 0;
	for (i = 0; i != 16; i++)
		if (push_freg[i])
			flist[i3++] = i;
	if (i3 % 2)
		flist[i3++] = 1; // Dont use 0 as it is a reutrn register

	AIWNIOS_ADD_CODE(X86PushReg, RBP);
	AIWNIOS_ADD_CODE(X86MovRegReg, RBP, RSP);
	AIWNIOS_ADD_CODE(X86SubImm32, RSP, to_push);
	off = -old_regs_start;
	for (i = 0; i != i2; i++) {
		AIWNIOS_ADD_CODE(X86MovIndirRegI64, ilist[i], -1, -1, RBP, off);
		off -= 8;
	}
	for (i = 0; i != i3; i++) {
		AIWNIOS_ADD_CODE(X86MovIndirRegF64, flist[i], -1, -1, RBP, off);
		off -= 8;
	}
	stk_arg_cnt = ireg_arg_cnt = freg_arg_cnt = 0;
	if (cctrl->cur_fun) {
		lst = cctrl->cur_fun->base.members_lst;
		for (i = 0; i != cctrl->cur_fun->argc; i++) {
			if (lst->member_class->raw_type == RT_F64) {
				if (freg_arg_cnt < sizeof(call_fregs)/sizeof(*call_fregs)) {
					fun_arg.mode = MD_REG;
					fun_arg.raw_type = RT_F64;
					fun_arg.reg = call_fregs[freg_arg_cnt++];
				} else {
				stk:
					fun_arg.mode = MD_INDIR_REG;
					fun_arg.raw_type = lst->member_class->raw_type;
					fun_arg.reg=RBP;
					fun_arg.off = 16 + stk_arg_cnt++ * 8;
				}
			} else {
				if (ireg_arg_cnt < sizeof(call_iregs)/sizeof(*call_iregs)) {
					fun_arg.mode = MD_REG;
					fun_arg.raw_type = RT_I64i;
					fun_arg.reg = call_iregs[ireg_arg_cnt++];
				} else
					goto stk;
			}
			// This *shoudlnt* mutate any of the argument registers
			if (lst->reg >= 0 && lst->reg != REG_NONE) {
				write_to.mode = MD_REG;
				write_to.reg = lst->reg;
				write_to.off = 0;
				write_to.raw_type = lst->member_class->raw_type;
			} else {
				write_to.mode = MD_FRAME;
				write_to.off = lst->off;
				write_to.raw_type = lst->member_class->raw_type;
			}
			code_off = ICMov(cctrl, &write_to, &fun_arg, bin, code_off);
			lst = lst->next;
		}
	} else {
		// Things from the HolyC side use __IC_ARG
		// We go backwards as this is REVERSE polish notation
		for (rpn = cctrl->code_ctrl->ir_code->last; rpn != cctrl->code_ctrl->ir_code; rpn = rpn->base.last) {
			if (rpn->type == __IC_ARG) {
				if ((arg = ICArgN(rpn, 0))->raw_type == RT_F64) {
					if (freg_arg_cnt < 8) {
						fun_arg.mode = MD_REG;
						fun_arg.raw_type = RT_F64;
						fun_arg.reg = call_fregs[freg_arg_cnt++];
					} else {
					stk2:
						fun_arg.mode = MD_INDIR_REG;
						fun_arg.raw_type = arg->raw_type;
						if (arg->raw_type < RT_I64i)
							arg->raw_type = RT_I64i;
						fun_arg.reg=RBP;
						fun_arg.off = 16 + stk_arg_cnt++ * 8;
					}
				} else {
					if (ireg_arg_cnt < sizeof(call_iregs)/sizeof(*call_iregs)) {
						fun_arg.mode = MD_REG;
						fun_arg.raw_type = RT_I64i;
						fun_arg.reg = call_iregs[ireg_arg_cnt++];
					} else
						goto stk2;
				}
				
				PushTmp(cctrl,arg,NULL);
				PopTmp(cctrl,arg);
				code_off = ICMov(cctrl, &arg->res, &fun_arg, bin, code_off);
			}
		}
	}
	return code_off;
}

//
// DO NOT CHANGE WITHOUT LOOKING CLOSEY AT FuncProlog
//
static int64_t FuncEpilog(CCmpCtrl* cctrl, char* bin, int64_t code_off)
{
	char push_ireg[16], ilist[16];
	char push_freg[16], flist[16];
	int64_t i, r, r2, fsz, i2, i3, off;
	CICArg spill_loc = { 0 }, write_to = { 0 };
	CRPN* rpn;
	/* <== OLD SP
	 * saved regs
	 * wiggle room
	 * locals
	 * OLD FP,LR<===FP=SP
	 */
	if (cctrl->cur_fun)
		fsz = cctrl->cur_fun->base.sz + 16; //+16 for LR/FP
	else {
		fsz = 16;
		for (rpn = cctrl->code_ctrl->ir_code->next; rpn != cctrl->code_ctrl->ir_code; rpn = rpn->base.next) {
			if (rpn->type == __IC_SET_FRAME_SIZE) {
				fsz = rpn->integer;
				break;
			}
		}
	}
	// ALIGN TO 8
	if (fsz % 8)
		fsz += 8 - fsz % 8;
	int64_t to_push = (i2 = __FindPushedIRegs(cctrl, push_ireg)) * 8 + (i3 = __FindPushedFRegs(cctrl, push_freg)) * 8 + fsz + cctrl->backend_user_data0, old_regs_start; // old_FP,old_LR
	if (i2 % 2)
		to_push += 8; // We push a dummy register in a pair if not aligned to 2
	if (i3 % 2)
		to_push += 8; // Ditto
	if (to_push % 16)
		to_push += 8;
	old_regs_start = fsz + cctrl->backend_user_data0;
	i2 = 0;
	for (i = 0; i != 16; i++)
		if (push_ireg[i])
			ilist[i2++] = i;
	if (i2 % 2)
		ilist[i2++] = 1; // Dont use 0 as it is a reutrn register
	i3 = 0;
	for (i = 0; i != 16; i++)
		if (push_freg[i])
			flist[i3++] = i;
	if (i3 % 2)
		flist[i3++] = 1; // Dont use 0 as it is a reutrn register
	//<==== OLD SP
	// first saved reg pair<==-16
	// next saved reg pair<===-32
	//
	off = -old_regs_start;
	for (i = 0; i != i2; i++) {
		AIWNIOS_ADD_CODE(X86MovRegIndirI64, ilist[i], -1, -1, RBP, off);
		off -= 8;
	}
	for (i = 0; i != i3; i++) {
		AIWNIOS_ADD_CODE(X86MovRegIndirF64, ilist[i], -1, -1, RBP, off);
		off -= 8;
	}
	AIWNIOS_ADD_CODE(X86Leave, 0);
	AIWNIOS_ADD_CODE(X86Ret, 0);
	return code_off;
}

static int64_t __SexyPostOp(CCmpCtrl* cctrl, CRPN* rpn, int64_t (*i_imm)(char*, int64_t, int64_t),
	int64_t (*ireg)(char*, int64_t, int64_t),
	int64_t (*freg)(char*, int64_t, int64_t),
	int64_t (*incr)(char*, int64_t),
	int64_t (*incr_sib)(char*, int64_t,int64_t ,int64_t,int64_t),
	char* bin, int64_t code_off)
{
	//
	// See hack in PushTmpDepthFirst motherfucker
	// 
	CRPN *next = rpn->base.next, *tc;
	int64_t code;
	CICArg derefed = { 0 }, tmp = { 0 }, tmp2 = { 0 };
	if (next->type == IC_TYPECAST) {
		TYPECAST_ASSIGN_BEGIN(next, next);
		if (next->raw_type == RT_F64) {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp = derefed; //==RT_F64
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, 1, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			code_off = __ICMoveF64(cctrl, 0, rpn->integer, bin, code_off);
			AIWNIOS_ADD_CODE(freg, tmp.reg, 0);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
		} else if (rpn->integer != 1) {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp = derefed;
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, AIWNIOS_TMP_IREG_POOP, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			AIWNIOS_ADD_CODE(i_imm, tmp.reg, rpn->integer);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
		} else if(IsDerefToSIB64(next)) {
			code_off = DerefToSIB(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res,&derefed, bin, code_off);
			AIWNIOS_ADD_CODE(incr_sib, derefed.__SIB_scale,derefed.reg2,derefed.reg,derefed.off);
		} else {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp=derefed;
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, AIWNIOS_TMP_IREG_POOP, bin, code_off);
			AIWNIOS_ADD_CODE(incr, tmp.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		TYPECAST_ASSIGN_END(next);
	} else {
		if (next->raw_type == RT_F64) {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp = derefed; //==RT_F64
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, 1, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			code_off = __ICMoveF64(cctrl, 0, rpn->integer, bin, code_off);
			AIWNIOS_ADD_CODE(freg, tmp.reg, 0);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
		} else if (rpn->integer != 1) {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp = derefed;
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, AIWNIOS_TMP_IREG_POOP, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			AIWNIOS_ADD_CODE(i_imm, tmp.reg, rpn->integer);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
		} else if(IsDerefToSIB64(next)) {
			code_off = DerefToSIB(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res,&derefed, bin, code_off);
			AIWNIOS_ADD_CODE(incr_sib, derefed.__SIB_scale,derefed.reg2,derefed.reg,derefed.off);
		} else {
			code_off = DerefToICArg(cctrl, &derefed, next, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp=derefed;
			code_off = PutICArgIntoReg(cctrl, &tmp, tmp.raw_type, AIWNIOS_TMP_IREG_POOP, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			AIWNIOS_ADD_CODE(incr, tmp.reg);
			code_off = ICMov(cctrl, &derefed, &tmp, bin, code_off);
		}
	}
	return code_off;
}

static void DoNothing() {
	puts("undefined extern");
}

static int64_t IsCompoundCompare(CRPN* r)
{
	CRPN* next = r->base.next;
	switch (r->type) {
	case IC_LT:
	case IC_GT:
	case IC_LE:
	case IC_GE:
		next = ICFwd(next);
		switch (next->type) {
		case IC_LT:
		case IC_GT:
		case IC_LE:
		case IC_GE:
			return 1;
		}
		return 0;
	case IC_NE:
	case IC_EQ_EQ:
		next = ICFwd(next);
		switch (next->type) {
		case IC_EQ_EQ:
		case IC_NE:
			return 1;
		}
		return 0;
	}
	return 0;
}
//
// ALWAYS ASSUME WORST CASE if we dont have a ALLOC'ed peice of RAM
//
int64_t __OptPassFinal(CCmpCtrl* cctrl, CRPN* rpn, char* bin,
	int64_t code_off)
{
	if (rpn->flags & ICF_PRECOMPUTED)
		return code_off;
	CCodeMisc* misc;
	CRPN *next, *next2, **range, **range_args, *next3, *a, *b;
	CICArg tmp = { 0 }, orig_dst = { 0 }, tmp2 = { 0 }, derefed = { 0 };
	int64_t i = 0, cnt, i2, use_reg, a_reg, b_reg, into_reg, use_flt_cmp, reverse, is_typecast;
	int64_t *range_cmp_types, use_flags = rpn->res.set_flags;
	rpn->res.set_flags = 0;
	char *enter_addr2, *enter_addr, *exit_addr, **fail1_addr, **fail2_addr, ***range_fail_addrs;
	if (cctrl->code_ctrl->dbg_info && cctrl->code_ctrl->final_pass == 3 && rpn->ic_line) { // Final run
		if (MSize(cctrl->code_ctrl->dbg_info) / 8 > rpn->ic_line - cctrl->code_ctrl->min_ln) {
			i = cctrl->code_ctrl->dbg_info[rpn->ic_line - cctrl->code_ctrl->min_ln];
			if (!i)
				cctrl->code_ctrl->dbg_info[rpn->ic_line - cctrl->code_ctrl->min_ln] = bin + code_off;
			else if ((int64_t)bin + code_off < i)
				cctrl->code_ctrl->dbg_info[rpn->ic_line - cctrl->code_ctrl->min_ln] = bin + code_off;
		}
	}
	switch (rpn->type) {
		break;
	case IC_SHORT_ADDR:
		// This is used for function calls only!!!
		abort();
		break;
	case IC_RELOC:
		if (rpn->res.mode == MD_REG)
			into_reg = rpn->res.reg;
		else
			into_reg = 0;
		misc = rpn->code_misc;
		i = -code_off + misc->aot_before_hint;
		AIWNIOS_ADD_CODE(X86MovRegIndirI64, into_reg, -1, -1, RIP, (char*)misc->addr - (bin + code_off));
		if (rpn->res.mode != MD_REG) {
			tmp.mode = MD_REG;
			tmp.raw_type = RT_I64i;
			tmp.reg = 0;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_GOTO_IF:
		reverse = 0;
		next = rpn->base.next;
	gti_enter:
		if (!IsCompoundCompare(next))
			switch (next->type) {
				break;
			case IC_LNOT:
				reverse = !reverse;
				next = next->base.next;
				goto gti_enter;
				break;
			case IC_EQ_EQ:
#define CMP_AND_JMP(COND)                                                                       \
	{                                                                                           \
		next3 = next->base.next;                                                                \
		next2 = ICFwd(next3);                                                                   \
		PushTmpDepthFirst(cctrl,next3,SpillsTmpRegs(next2),NULL); \
		PushTmpDepthFirst(cctrl,next2,0,NULL); \
		code_off = __OptPassFinal(cctrl, next3, bin, code_off);                                 \
		code_off = __OptPassFinal(cctrl, next2, bin, code_off);                                 \
		PopTmp(cctrl,next2); \
		PopTmp(cctrl,next3); \
		if (next3->raw_type == RT_F64 || next2->raw_type == RT_F64) {                           \
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_F64, 1, bin, code_off);           \
			code_off = PutICArgIntoReg(cctrl, &next3->res, RT_F64, 0, bin, code_off);           \
			AIWNIOS_ADD_CODE(X86COMISDRegReg, next2->res.reg, next3->res.reg);                  \
		} else {                                                                                \
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_I64i, AIWNIOS_TMP_IREG_POOP2, bin, code_off);          \
			code_off = PutICArgIntoReg(cctrl, &next3->res, RT_I64i, AIWNIOS_TMP_IREG_POOP, bin, code_off);          \
			AIWNIOS_ADD_CODE(X86CmpRegReg, next2->res.reg, next3->res.reg);                     \
		}                                                                                       \
		AIWNIOS_ADD_CODE(X86Jcc, (COND) ^ reverse, (char*)rpn->code_misc->addr - (bin + code_off)); \
	}
				CMP_AND_JMP(X86_COND_E);
				goto ret;
				break;
			case IC_NE:
				CMP_AND_JMP(X86_COND_E | 1);
				goto ret;
				break;
			case IC_LT:
				next3 = next->base.next;
				next2 = ICFwd(next3);
				if (next3->raw_type == RT_U64i || next2->raw_type == RT_U64i||next3->raw_type==RT_F64||next2->raw_type==RT_F64) {
					CMP_AND_JMP(X86_COND_B);
				} else {
					CMP_AND_JMP(X86_COND_L);
				}
				goto ret;
				break;
			case IC_GT:
				next3 = next->base.next;
				next2 = ICFwd(next3);
				if (next3->raw_type == RT_U64i || next2->raw_type == RT_U64i||next3->raw_type==RT_F64||next2->raw_type==RT_F64) {
					CMP_AND_JMP(X86_COND_A);
				} else {
					CMP_AND_JMP(X86_COND_G);
				}
				goto ret;
				break;
			case IC_LE:
				next3 = next->base.next;
				next2 = ICFwd(next3);
				if (next3->raw_type == RT_U64i || next2->raw_type == RT_U64i||next3->raw_type==RT_F64||next2->raw_type==RT_F64) {
					CMP_AND_JMP(X86_COND_BE);
				} else {
					CMP_AND_JMP(X86_COND_LE);
				}
				goto ret;
				break;
			case IC_GE:
				next3 = next->base.next;
				next2 = ICFwd(next3);
				if (next3->raw_type == RT_U64i || next2->raw_type == RT_U64i||next3->raw_type==RT_F64||next2->raw_type==RT_F64) {
					CMP_AND_JMP(X86_COND_AE);
				} else {
					CMP_AND_JMP(X86_COND_GE);
				}
				goto ret;
			}

		next->res.set_flags = 1;
		PushTmpDepthFirst(cctrl,next,0,NULL);
		PopTmp(cctrl,next);
		code_off = __OptPassFinal(cctrl, next, bin, code_off);
		if (next->res.set_flags) {
			if (reverse) {
				AIWNIOS_ADD_CODE(X86Jcc, X86_COND_E| 1, (char*)rpn->code_misc->addr - (bin + code_off));
			} else {
				AIWNIOS_ADD_CODE(X86Jcc, X86_COND_E , (char*)rpn->code_misc->addr - (bin + code_off));
			}
		} else if (next->raw_type == RT_F64) {
			code_off = PutICArgIntoReg(cctrl, &next->res, RT_F64, 1, bin, code_off);
			code_off = __ICMoveF64(cctrl, 0, 0, bin, code_off);
			AIWNIOS_ADD_CODE(X86COMISDRegReg, next->res.reg, 0);
			if (!reverse)
				AIWNIOS_ADD_CODE(X86Jcc, X86_COND_E|1, (char*)rpn->code_misc->addr - (bin + code_off))
			else
				AIWNIOS_ADD_CODE(X86Jcc, X86_COND_E, (char*)rpn->code_misc->addr - (bin + code_off))
		} else {
			code_off = PutICArgIntoReg(cctrl, &next->res, RT_I64i, 0, bin, code_off);
			AIWNIOS_ADD_CODE(X86Test, next->res.reg, next->res.reg);
			if (!reverse)
				AIWNIOS_ADD_CODE(X86Jcc, X86_COND_E|1, (char*)rpn->code_misc->addr - (bin + code_off))
			else
				AIWNIOS_ADD_CODE(X86Jcc, X86_COND_E, (char*)rpn->code_misc->addr - (bin + code_off))
		}
		break;
		// These both do the same thing,only thier type detirmines what happens
	case IC_TO_F64:
	case IC_TO_I64:
		next=rpn->base.next;
		code_off = __OptPassFinal(cctrl, next, bin, code_off);
		code_off = ICMov(cctrl, &rpn->res, &next->res, bin, code_off);
		break;
	case IC_TYPECAST:
		next = rpn->base.next;
		if (next->raw_type == RT_F64 && rpn->raw_type != RT_F64) {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, next->raw_type, 0, bin, code_off);
			tmp.mode=MD_REG;
			tmp.reg=0;
			tmp.raw_type=rpn->raw_type;
			if(rpn->res.mode==MD_REG)
				tmp.reg=rpn->res.reg;
			AIWNIOS_ADD_CODE(X86MovQI64F64, tmp.reg, next->res.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		} else if (next->raw_type != RT_F64 && rpn->raw_type == RT_F64) {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, next->raw_type, 0, bin, code_off);
			tmp.mode=MD_REG;
			tmp.reg=0;
			tmp.raw_type=rpn->raw_type;
			if(rpn->res.mode==MD_REG)
				tmp.reg=rpn->res.reg;
			AIWNIOS_ADD_CODE(X86MovQF64I64, tmp.reg, next->res.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		} else if (next->type == IC_DEREF) {
			code_off = DerefToICArg(cctrl, &tmp, next, 1, bin, code_off);
			tmp.raw_type = rpn->raw_type;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		} else {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &next->res, bin, code_off);
		}
		break;
	case IC_GOTO:
		if (cctrl->code_ctrl->final_pass >= 2)
			AIWNIOS_ADD_CODE(X86Jmp, (char*)rpn->code_misc->addr - (code_off + bin))
		else
			AIWNIOS_ADD_CODE(X86Jmp, 0);
		break;
	case IC_LABEL:
		rpn->code_misc->addr = bin + code_off;
		break;
	case IC_GLOBAL:
		//
		// Here's the deal. Global arrays should be acceessed via
		// IC_ADDR_OF. So loading int register here
		//
		if (rpn->global_var->base.type & HTT_FUN) {
			next = rpn;
			goto get_glbl_ptr;
		} else {
			assert(!rpn->global_var->dim.next);
			tmp.mode = MD_PTR;
			tmp.off = rpn->global_var->data_addr;
			tmp.raw_type = rpn->global_var->var_class->raw_type;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_NOP:
		// Bungis
		break;
	case IC_PAREN:
		abort();
		break;
	case IC_NEG:
#define BACKEND_UNOP(flt_op, int_op)                                                \
	next = rpn->base.next;                                                          \
	code_off = __OptPassFinal(cctrl, next, bin, code_off);                          \
	into_reg = 0;                                                               \
	if(rpn->res.mode==MD_NULL) break; \
	tmp.mode=MD_REG;tmp.raw_type=RT_I64i;tmp.reg=into_reg;\
	code_off=ICMov(cctrl,&tmp,&next->res,bin,code_off); \
	if (rpn->raw_type == RT_F64) {                                                  \
		AIWNIOS_ADD_CODE(flt_op, into_reg);                                         \
	} else                                                                          \
		AIWNIOS_ADD_CODE(int_op, into_reg);                                         \
	code_off=ICMov(cctrl,&rpn->res,&tmp,bin,code_off);
		if (rpn->raw_type == RT_F64) {
			next=ICArgN(rpn,0);
			static double ul;
			((int64_t*)&ul)[0] = 0x80000000ll << 32;
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			tmp=rpn->res;
			code_off = ICMov(cctrl, &rpn->res, &next->res, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &tmp, RT_F64, 1, bin, code_off);
			code_off = __ICMoveF64(cctrl, 0, ul, bin, code_off);
			AIWNIOS_ADD_CODE(X86XorPDReg, tmp.reg, 0);
			code_off=ICMov(cctrl,&rpn->res,&tmp,bin,code_off);
		} else {
			BACKEND_UNOP(X86NegReg, X86NegReg);
		}
		break;
	case IC_POS:
		next=ICArgN(rpn,0);
		code_off = __OptPassFinal(cctrl, next, bin, code_off);
		code_off = ICMov(cctrl, &rpn->res, &next->res, bin, code_off);
		break;
	case IC_NAME:
		abort();
		break;
	case IC_STR:
		if (rpn->res.mode == MD_REG) {
			into_reg = rpn->res.reg;
		} else
			into_reg = 0; // Store into reg 0

		if (cctrl->flags & CCF_STRINGS_ON_HEAP) {
			//
			// README: We will "NULL"ify the rpn->code_misc->str later so we dont free it
			//
			code_off = __ICMoveI64(cctrl, into_reg, rpn->code_misc->str, bin, code_off);
		} else {
			AIWNIOS_ADD_CODE(X86LeaSIB, into_reg, -1, -1, RIP, (char*)rpn->code_misc->addr - (bin + code_off));
		}
		if (rpn->res.mode != MD_REG) {
			tmp.raw_type = rpn->raw_type;
			tmp.reg = 0;
			tmp.mode = MD_REG;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_CHR:
		if (rpn->res.mode == MD_REG) {
			into_reg = rpn->res.reg;
		} else
			into_reg = 0; // Store into reg 0

		code_off = __ICMoveI64(cctrl, into_reg, rpn->integer, bin, code_off);

		if (rpn->res.mode != MD_REG) {
			tmp.raw_type = rpn->raw_type;
			tmp.reg = 0;
			tmp.mode = MD_REG;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_POW:
		// TODO
		break;
		break;
	case IC_UNBOUNDED_SWITCH:
		//
		// Load poop into tmp,then we continue the sexyness as normal
		//
		// Also make sure R8 has lo bound(See IC_BOUNDED_SWITCH)
		//
		next2 = ICArgN(rpn, 0);
		PushTmpDepthFirst(cctrl,next2,0,NULL);
		PopTmp(cctrl,next2);
		code_off = __OptPassFinal(cctrl, next2, bin, code_off);
		tmp.raw_type = RT_I64i;
		tmp.mode = MD_REG;
		tmp.reg = 0;
		code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
		code_off = __ICMoveI64(cctrl, R8, rpn->code_misc->lo, bin, code_off); // X1
		goto jmp_tab_sexy;
		break;
	case IC_BOUNDED_SWITCH:
		next2 = ICArgN(rpn, 0);
		PushTmpDepthFirst(cctrl,next2,0,NULL);
		PopTmp(cctrl,next2);
		code_off = __OptPassFinal(cctrl, next2, bin, code_off);
		tmp.raw_type = RT_I64i;
		tmp.mode = MD_REG;
		tmp.reg = RCX;
		code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
		code_off = __ICMoveI64(cctrl, RAX, rpn->code_misc->hi, bin, code_off);
		code_off = __ICMoveI64(cctrl, R8, rpn->code_misc->lo, bin, code_off);
		AIWNIOS_ADD_CODE(X86CmpRegReg, tmp.reg, R8);
		fail1_addr = bin + code_off;
		AIWNIOS_ADD_CODE(X86Jcc, X86_COND_L, 0);
		AIWNIOS_ADD_CODE(X86CmpRegReg, tmp.reg, RAX);
		fail2_addr = bin + code_off;
		AIWNIOS_ADD_CODE(X86Jcc, X86_COND_G, 0);
	jmp_tab_sexy:
		//
		// See LAMA snail
		//
		// The jump table offsets are relative to the start of the function
		//   to make it so the code is Position-Independant
		AIWNIOS_ADD_CODE(X86SubReg, tmp.reg, R8); // R8 has low bound
		AIWNIOS_ADD_CODE(X86LeaSIB,R9,-1, -1, RIP,-code_off);
		AIWNIOS_ADD_CODE(X86LeaSIB,RDX,-1, -1, RIP,(char*)rpn->code_misc->addr - (code_off + bin));
		AIWNIOS_ADD_CODE(X86AddSIB64,R9,8,tmp.reg,RDX,0);
		AIWNIOS_ADD_CODE(X86JmpReg, R9);
		if (rpn->type == IC_BOUNDED_SWITCH&&bin) {
			X86Jcc(fail1_addr, X86_COND_L, (char*)rpn->code_misc->dft_lab->addr - (char*)fail1_addr);
			X86Jcc(fail2_addr, X86_COND_G, (char*)rpn->code_misc->dft_lab->addr - (char*)fail2_addr);
		}
		break;
	case IC_SUB_RET:
		AIWNIOS_ADD_CODE(X86Ret, 0);
		break;
	case IC_SUB_PROLOG:
		break;
	case IC_SUB_CALL:
		AIWNIOS_ADD_CODE(X86Call32, (char*)rpn->code_misc->addr - (bin + code_off));
		break;
	case IC_ADD:
//
//  /\   /\      ____
//  ||___||     /    \
//  | o_o | <==(README)
//  | \_/ |     \____/
//  ||   ||
//  \/   \/
// We compute the righthand argument first
// Because the lefthand side can be assigned into(and we will need
// the temporary register containg the pointer presetn and not messed up)
//
// RIGHT ==> may have func call that invalidates tmp regs
// LEFT ==> may have a MD_INDIR_REG(where reg is a tmp reg)
// *LEFT=RIHGT ==> Write into the left's address
//
#define BACKEND_BINOP(f_op, i_op, f_sib_op, i_sib_op)                                                             \
	do {                                                                                                          \
		next = ICArgN(rpn, 1);                                                                                    \
		next2 = ICArgN(rpn, 0);                                                                                   \
		orig_dst = next->res;                                                                                     \
		if (!SpillsTmpRegs(next)&&IsDerefToSIB64(next2)&&((next2->raw_type==RT_F64)==(rpn->raw_type==RT_F64))) {                                                                              \
			code_off = DerefToSIB(cctrl, &derefed, next2, AIWNIOS_TMP_IREG_POOP2, bin, code_off);                                      \
			code_off = __OptPassFinal(cctrl, next, bin, code_off);                                                \
			if(rpn->res.mode==MD_NULL) break; \
			if (rpn->res.mode == MD_REG&&!DstRegAffectsMode(&rpn->res,&derefed)) {                                                                        \
				if(DstRegAffectsMode(&rpn->res,&next->res)) \
				into_reg = 0;                                                                                     \
				else \
				into_reg = rpn->res.reg;                                                                          \
			} else                                                                                                \
				into_reg = 0;                                                                                     \
			tmp.mode=MD_REG; \
			tmp.raw_type=rpn->raw_type; \
			tmp.reg=into_reg;                                                                                         \
			code_off = ICMov(cctrl, &tmp, &next->res, bin, code_off);                                             \
			if (rpn->raw_type == RT_F64) {                                                                        \
				AIWNIOS_ADD_CODE(f_sib_op, tmp.reg, derefed.__SIB_scale, derefed.reg2, derefed.reg, derefed.off); \
			} else {                                                                                              \
				AIWNIOS_ADD_CODE(i_sib_op, tmp.reg, derefed.__SIB_scale, derefed.reg2, derefed.reg, derefed.off); \
			}                                                                                                     \
			if (rpn->res.mode != MD_NULL) {                                                                       \
				code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);                                          \
			}                                                                                                     \
			break;                                                                                                \
		}                                                                                                         \
		code_off = __OptPassFinal(cctrl, next2, bin, code_off);                                                    \
		code_off = __OptPassFinal(cctrl, next, bin, code_off);                                                   \
		if(rpn->res.mode==MD_NULL) break; \
		if (rpn->res.mode == MD_REG&&!DstRegAffectsMode(&rpn->res,&next2->res)) {                                                                            \
			if(DstRegAffectsMode(&rpn->res,&next->res)&&next->res.mode!=MD_REG) \
			into_reg = 0;                                                                                         \
			else \
			into_reg = rpn->res.reg;                                                                              \
		} else                                                                                                    \
			into_reg = 0;                                                                                         \
		tmp.mode=MD_REG; \
		tmp.raw_type=rpn->raw_type; \
		tmp.reg=into_reg;                                                                                         \
		code_off = ICMov(cctrl, &tmp, &next->res, bin, code_off);                                                 \
		if(rpn->raw_type==RT_F64) { \
		code_off = PutICArgIntoReg(cctrl, &tmp, rpn->raw_type, 0, bin, code_off);             \
		code_off = PutICArgIntoReg(cctrl, &next2->res, rpn->raw_type, 1, bin, code_off);     \
		} else { \
		code_off = PutICArgIntoReg(cctrl, &tmp, rpn->raw_type, AIWNIOS_TMP_IREG_POOP, bin, code_off);             \
		code_off = PutICArgIntoReg(cctrl, &next2->res, rpn->raw_type, AIWNIOS_TMP_IREG_POOP2, bin, code_off);     \
		} \
		if (rpn->raw_type == RT_F64) {                                                                            \
			AIWNIOS_ADD_CODE(f_op, tmp.reg, next2->res.reg);                                                      \
		} else {                                                                                                  \
			AIWNIOS_ADD_CODE(i_op, tmp.reg, next2->res.reg);                                                      \
		}                                                                                                         \
		if (rpn->res.mode != MD_NULL) {                                                                           \
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);                                              \
		}                                                                                                         \
	} while (0);
#define BACKEND_BINOP_IMM(i_imm_op, i_op)                                                                                        \
	next = ICArgN(rpn, 1);                                                                                                       \
	next2 = ICArgN(rpn, 0);                                                                                                      \
	if (rpn->raw_type != RT_F64 && IsConst(next2) && Is32Bit(ConstVal(next2)) && next2->type != IC_F64 && next2->integer >= 0) { \
		code_off = __OptPassFinal(cctrl, next, bin, code_off);                                                                   \
		tmp2 = next->res;                                                                                                        \
		if(rpn->res.mode==MD_NULL) break; \
		code_off = PutICArgIntoReg(cctrl, &tmp2, RT_I64i, 1, bin, code_off);                                                     \
		if (rpn->res.mode == MD_REG)                                                                                             \
			into_reg = rpn->res.reg;                                                                                             \
		else                                                                                                                     \
			into_reg = 0;                                                                                                        \
		if (Is32Bit(next2->integer)) {                                                                                           \
			code_off = ICMov(cctrl, &rpn->res, &tmp2, bin, code_off);                                                            \
			AIWNIOS_ADD_CODE(i_imm_op, into_reg, next2->integer);                                                                \
		} else {                                                                                                                 \
			tmp.mode = MD_REG;                                                                                                   \
			tmp.reg = AIWNIOS_TMP_IREG_POOP;                                                                                     \
			tmp.raw_type = RT_I64i;                                                                                              \
			code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);                                                           \
			AIWNIOS_ADD_CODE(i_op, into_reg, tmp.reg);                                                                           \
		}                                                                                                                        \
		if (rpn->res.mode != MD_REG && rpn->res.mode != MD_NULL) {                                                               \
			tmp.raw_type = rpn->raw_type;                                                                                        \
			tmp.reg = 0;                                                                                                         \
			tmp.mode = MD_REG;                                                                                                   \
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);                                                             \
		}                                                                                                                        \
		break;                                                                                                                   \
	}

		BACKEND_BINOP_IMM(X86AddImm32, X86AddReg);
		BACKEND_BINOP(X86AddSDReg, X86AddReg, X86AddSDSIB64, X86AddSIB64);
		break;
	case IC_COMMA:
		next = ICArgN(rpn, 1);
		next2 = ICArgN(rpn, 0);
		orig_dst = next->res;
		code_off = __OptPassFinal(cctrl, next2, bin, code_off);
		code_off = __OptPassFinal(cctrl, next, bin, code_off);
		// TODO WHICH ONE
		code_off = ICMov(cctrl, &rpn->res, &next->res, bin, code_off);
		break;
	case IC_EQ:
		next = ICArgN(rpn, 1);
		next2 = ICArgN(rpn, 0);
		orig_dst = next->res;
		code_off = __OptPassFinal(cctrl, next2, bin, code_off);
		if (next->type == IC_TYPECAST) {
			TYPECAST_ASSIGN_BEGIN(next, next2);
			code_off = ICMov(cctrl, &next->res, &next2->res, bin, code_off);
			TYPECAST_ASSIGN_END(next);
		} else if (next->type == IC_DEREF) {
			tmp=next->res;
			code_off=DerefToICArg(cctrl,&tmp,next,AIWNIOS_TMP_IREG_POOP2,bin,code_off);
			code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
			if(!DstRegAffectsMode(&rpn->res,&tmp))
				code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			else {
				code_off=PutICArgIntoReg(cctrl,&tmp,rpn->raw_type,AIWNIOS_TMP_IREG_POOP2,bin,code_off);
				code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			}
			break;
		} else {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = ICMov(cctrl, &next->res, &next2->res, bin, code_off);
		}
		code_off = ICMov(cctrl, &rpn->res, &next2->res, bin, code_off);
		break;
	case IC_SUB:
		BACKEND_BINOP_IMM(X86SubImm32, X86SubReg);
		BACKEND_BINOP(X86SubSDReg, X86SubReg, X86SubSDSIB64, X86SubSIB64);
		break;
	case IC_DIV:
		next = rpn->base.next;
		next2 = ICArgN(rpn, 1);
		if (next->type == IC_I64 && next2->raw_type != RT_F64) {
			if (__builtin_popcountll(next->integer) == 1) {
				code_off = __OptPassFinal(cctrl, next2, bin, code_off);
				if(rpn->res.mode==MD_REG)
					into_reg=rpn->res.reg;
				else
					into_reg=RAX;
				tmp.mode=MD_REG;
				tmp.raw_type=RT_I64i;
				tmp.reg=into_reg;
				code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
				switch (next2->raw_type) {
				case RT_I8i:
				case RT_I16i:
				case RT_I32i:
				case RT_I64i:
					AIWNIOS_ADD_CODE(X86SarImm, into_reg, __builtin_ffsll(next->integer) - 1);
					break;
				default:
					AIWNIOS_ADD_CODE(X86ShrImm, into_reg, __builtin_ffsll(next->integer) - 1);
					break;
				}
				code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
				break;
			}
		}
		if (rpn->raw_type == RT_U64i) {
#define CQO_OP(use_reg, op, sib_op)                                                            \
	next = ICArgN(rpn, 1);                                                                     \
	next2 = ICArgN(rpn, 0);                                                                    \
	orig_dst = next->res;                                                                      \
	if (!SpillsTmpRegs(next)&&IsDerefToSIB64(next2)&&((next2->raw_type==RT_F64)==(rpn->raw_type==RT_F64))) {         \
		code_off = DerefToSIB(cctrl, &derefed, next2, AIWNIOS_TMP_IREG_POOP2, bin, code_off);  \
		code_off = __OptPassFinal(cctrl, next, bin, code_off);                                 \
		tmp.raw_type = RT_I64i;                                                                \
		tmp.reg = RAX;                                                                         \
		tmp.mode = MD_REG;                                                                     \
		code_off = ICMov(cctrl, &tmp, &next->res, bin, code_off);                              \
		if(rpn->raw_type==RT_U64i) {AIWNIOS_ADD_CODE(X86XorReg,RDX,RDX);} else { AIWNIOS_ADD_CODE(X86Cqo, 0);} \
		AIWNIOS_ADD_CODE(sib_op, derefed.__SIB_scale, derefed.reg2, derefed.reg, derefed.off); \
		tmp.reg = use_reg;                                                                     \
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);                               \
		break;                                                                                 \
	}                                                                                          \
	tmp.raw_type = RT_I64i;                                                                    \
	tmp.reg = RAX;                                                                             \
	tmp.mode = MD_REG;                                                                         \
	code_off = __OptPassFinal(cctrl, next2, bin, code_off);                                    \
	code_off = __OptPassFinal(cctrl, next, bin, code_off);                                     \
	code_off = ICMov(cctrl, &tmp, &next->res, bin, code_off);                                  \
	code_off = PutICArgIntoReg(cctrl, &next2->res, RT_U64i, R8, bin, code_off);                \
	if(rpn->raw_type==RT_U64i) {AIWNIOS_ADD_CODE(X86XorReg,RDX,RDX);} else {AIWNIOS_ADD_CODE(X86Cqo, 0);} \
	AIWNIOS_ADD_CODE(op, next2->res.reg);                                                      \
	tmp.reg = use_reg;                                                                         \
	code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			CQO_OP(RAX, X86DivReg, X86DivSIB64);
		} else if (rpn->raw_type != RT_F64) {
			CQO_OP(RAX, X86IDivReg, X86IDivSIB64);
		} else {
			BACKEND_BINOP(X86DivSDReg, X86DivSDReg, X86DivSDSIB64, X86DivSDSIB64);
		}
		break;
	case IC_MUL:
		next = rpn->base.next;
		next2 = ICArgN(rpn, 1);
		if (next->type == IC_I64 && next2->raw_type != RT_F64) {
			if (__builtin_popcountll(next->integer) == 1) {
				code_off = __OptPassFinal(cctrl, next2, bin, code_off);
				code_off = PutICArgIntoReg(cctrl, &next2->res, next2->raw_type, AIWNIOS_TMP_IREG_POOP, bin, code_off);
				if (rpn->res.mode == MD_REG) {
					code_off = ICMov(cctrl, &rpn->res, &next2->res, bin, code_off);
					AIWNIOS_ADD_CODE(X86ShlImm, rpn->res.reg, __builtin_ffsll(next->integer) - 1);
				} else {
					tmp.raw_type = RT_I64i;
					tmp.reg = RAX;
					tmp.mode = MD_REG;
					code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
					AIWNIOS_ADD_CODE(X86ShlImm, tmp.reg, __builtin_ffsll(next->integer) - 1);
					code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
				}
				break;
			}
		}
		if (rpn->raw_type == RT_U64i) {
			CQO_OP(RAX, X86MulReg, X86MulSIB64);
		} else {
			BACKEND_BINOP(X86MulSDReg, X86IMul2Reg, X86MulSDSIB64, X86IMul2SIB64);
		}
		break;
	case IC_DEREF:
		next = rpn->base.next;
		i = 0;
		//
		// Here's the Donald Trump deal,Derefernced function pointers
		// don't derefence(you dont copy a function into memory).
		//
		// I64 (*fptr)()=&Foo;
		// (*fptr)(); //Doesn't "dereference"
		//
		if (rpn->raw_type == RT_FUNC) {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &next->res, bin, code_off);
			goto ret;
		}
		code_off = DerefToICArg(cctrl, &tmp, rpn, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		break;
	case IC_AND:
		BACKEND_BINOP_IMM(X86AndImm, X86AndImm);
		BACKEND_BINOP(X86AndPDReg, X86AndReg, X86AndPDSIB64, X86AndSIB64);
		break;
	case IC_ADDR_OF:
		next = rpn->base.next;
		switch (next->type) {
			break;
		case __IC_STATIC_REF:
			if (rpn->res.mode == MD_REG)
				into_reg = rpn->res.reg;
			else
				into_reg = 0;
			AIWNIOS_ADD_CODE(X86LeaSIB, into_reg, -1, -1, RIP, (rpn->integer + cctrl->code_ctrl->statics_offset) - code_off);
			goto restore_reg;
			break;
		case IC_DEREF:
			next = rpn->base.next;
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, RT_PTR, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			code_off = ICMov(cctrl, &rpn->res, &next->res, bin, code_off);
			break;
		case IC_BASE_PTR:
			if (rpn->res.mode == MD_REG)
				into_reg = rpn->res.reg;
			else
				into_reg = 0;
			AIWNIOS_ADD_CODE(X86LeaSIB, into_reg, -1, -1, RBP, -next->integer);
			goto restore_reg;
			break;
		case IC_STATIC:
			if (rpn->res.mode == MD_REG)
				into_reg = rpn->res.reg;
			else
				into_reg = 0;
			AIWNIOS_ADD_CODE(X86LeaSIB, into_reg, -1, -1, RIP, (char*)rpn->integer - (bin + code_off));
			goto restore_reg;
		case IC_GLOBAL:
		get_glbl_ptr:
			if (rpn->res.mode == MD_REG)
				into_reg = rpn->res.reg;
			else
				into_reg = 0;
			if (next->global_var->base.type & HTT_GLBL_VAR) {
				enter_addr = next->global_var->data_addr;
			} else if (next->global_var->base.type & HTT_FUN) {
				enter_addr = ((CHashFun*)next->global_var)->fun_ptr;
			} else
				abort();
			// Undefined?
			if (!enter_addr) {
				misc = AddRelocMisc(cctrl, next->global_var->base.str);
				AIWNIOS_ADD_CODE(X86MovRegIndirI64, into_reg, -1, -1, RIP, (char*)misc->addr - (bin + code_off));
				goto restore_reg;
			} else if (Is32Bit(enter_addr - (bin + code_off))) {
				AIWNIOS_ADD_CODE(X86LeaSIB, into_reg, -1, -1, RIP, enter_addr - (bin + code_off));
				goto restore_reg;
			}
			code_off = __ICMoveI64(cctrl, into_reg, enter_addr, bin,
				code_off);
		restore_reg:
			tmp.mode = MD_REG;
			tmp.raw_type = rpn->raw_type;
			tmp.reg = into_reg;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			break;
		default:
			abort();
		}
		break;
	case IC_XOR:
		if (rpn->raw_type != RT_F64) {
			BACKEND_BINOP_IMM(X86XorImm, X86XorReg);
		}
		BACKEND_BINOP(X86XorPDReg, X86XorReg, X86XorPDSIB64, X86XorSIB64);
		break;
	case IC_MOD:
		next = ICArgN(rpn, 1);
		next2 = ICArgN(rpn, 0);
		if (rpn->raw_type != RT_F64) {
			if (rpn->raw_type == RT_U64i) {
				CQO_OP(RDX, X86DivReg, X86DivSIB64);
			} else {
				CQO_OP(RDX, X86IDivReg, X86IDivSIB64);
			}
		} else {
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_F64, 1, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, RT_F64, 0, bin, code_off);
			tmp2.reg=2;
			tmp2.mode=MD_REG;
			tmp2.raw_type=RT_F64;
			code_off = ICMov(cctrl, &tmp2, &next->res, bin, code_off);
			AIWNIOS_ADD_CODE(X86DivSDReg,tmp2.reg,next2->res.reg);
			code_off=PushToStack(cctrl,&tmp2,bin,code_off);
			AIWNIOS_ADD_CODE(X86CVTTSD2SIRegSIB64,RAX,-1,-1,RSP,0);
			AIWNIOS_ADD_CODE(X86MovIndirRegI64,RAX,-1,-1,RSP,0);
			AIWNIOS_ADD_CODE(X86CVTTSI2SDRegSIB64,tmp2.reg,-1,-1,RSP,0);
			AIWNIOS_ADD_CODE(X86PopReg,RAX);
			AIWNIOS_ADD_CODE(X86MulSDReg,tmp2.reg,next2->res.reg);
			tmp.reg=0;
			tmp.mode=MD_REG;
			tmp.raw_type=RT_F64;
			code_off = ICMov(cctrl, &tmp, &next->res, bin, code_off); //tmp.reg==0
			AIWNIOS_ADD_CODE(X86SubSDReg,tmp.reg,tmp2.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_OR:
		BACKEND_BINOP_IMM(X86OrImm, X86OrReg);
		BACKEND_BINOP(X86OrPDReg, X86OrReg, X86OrPDSIB64, X86OrSIB64);
		break;
	case IC_LT:
	case IC_GT:
	case IC_LE:
	case IC_GE:
		//
		// Ask nroot how this works if you want to know
		//
		// X/V3 is for lefthand side
		// X/V4 is for righthand side
		range = NULL;
		range_args = NULL;
		range_fail_addrs = NULL;
		range_cmp_types = NULL;
	get_range_items:
		cnt = 0;
		for (next = rpn; 1; next = ICFwd(next->base.next) // TODO explain
		) {
			switch (next->type) {
			case IC_LT:
			case IC_GT:
			case IC_LE:
			case IC_GE:
				goto cmp_pass;
			}
			if (range_args)
				range_args[cnt] = next; // See below
			break;
		cmp_pass:
			//
			// <
			//    right rpn.next
			//    left  ICFwd(rpn.next)
			//
			if (range_args)
				range_args[cnt] = next->base.next;
			if (range)
				range[cnt] = next;
			cnt++;
		}
		if (!range) {
			// This are reversed
			range_args = A_MALLOC(sizeof(CRPN*) * (cnt + 1), cctrl->hc);
			range = A_MALLOC(sizeof(CRPN*) * cnt, cctrl->hc);
			range_fail_addrs = A_MALLOC(sizeof(CRPN*) * cnt, cctrl->hc);
			range_cmp_types = A_MALLOC(sizeof(int64_t) * cnt, cctrl->hc);
			goto get_range_items;
		}
		code_off = __OptPassFinal(cctrl, next = range_args[cnt], bin, code_off);
		for (i = cnt - 1; i >= 0; i--) {
			next2 = range_args[i];
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			memcpy(&tmp, &next->res, sizeof(CICArg));
			memcpy(&tmp2, &next2->res, sizeof(CICArg));
			use_flt_cmp = next->raw_type == RT_F64 || next2->raw_type == RT_F64;
			i2 = RT_I64i;
			i2 = next->res.raw_type > i2 ? next->res.raw_type : i2;
			i2 = next2->res.raw_type > i2 ? next2->res.raw_type : i2;
			code_off = PutICArgIntoReg(cctrl, &tmp, i2, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &tmp2, i2, 0, bin, code_off);
			if (use_flt_cmp) {
				AIWNIOS_ADD_CODE(X86COMISDRegReg, tmp.reg, tmp2.reg);
			} else {
				AIWNIOS_ADD_CODE(X86CmpRegReg, tmp.reg, tmp2.reg);
			}
			//
			// We use the opposite compare because if fail we jump to the fail
			// zone
			//
			if (use_flt_cmp) {
			unsigned_cmp:
				switch (range[i]->type) {
					break;
				case IC_LT:
					range_cmp_types[i] = X86_COND_B | 1;
					break;
				case IC_GT:
					range_cmp_types[i] = X86_COND_A | 1;
					break;
				case IC_LE:
					range_cmp_types[i] = X86_COND_BE | 1;
					break;
				case IC_GE:
					range_cmp_types[i] = X86_COND_AE | 1;
				}
			} else if (next->raw_type == RT_U64i || next2->raw_type == RT_U64i) {
				goto unsigned_cmp;
			} else {
				switch (range[i]->type) {
					break;
				case IC_LT:
					range_cmp_types[i] = X86_COND_L | 1;
					break;
				case IC_GT:
					range_cmp_types[i] = X86_COND_G | 1;
					break;
				case IC_LE:
					range_cmp_types[i] = X86_COND_LE | 1;
					break;
				case IC_GE:
					range_cmp_types[i] = X86_COND_GE | 1;
				}
			}
			range_fail_addrs[i] = bin + code_off;
			AIWNIOS_ADD_CODE(X86Jcc, range_cmp_types[i], 0); // WILL BE FILLED IN LATER
			next = next2;
		}
		tmp.mode = MD_I64;
		tmp.integer = 1;
		tmp.raw_type = RT_I64i;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		exit_addr = bin + code_off;
		AIWNIOS_ADD_CODE(X86Jmp, 0);
		if (bin)
			for (i = 0; i != cnt; i++) {
				X86Jcc(range_fail_addrs[i], range_cmp_types[i],
					(bin + code_off) - (char*)range_fail_addrs[i]); // TODO unsigned
			}
		tmp.mode = MD_I64;
		tmp.integer = 0;
		tmp.raw_type = RT_I64i;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		if (bin)
			X86Jmp(exit_addr, (bin + code_off) - exit_addr);
		A_FREE(range);
		A_FREE(range_args);
		A_FREE(range_fail_addrs);
		A_FREE(range_cmp_types);
		break;
	case IC_LNOT:
		next = rpn->base.next;
		code_off = __OptPassFinal(cctrl, next, bin, code_off);
		code_off = PutICArgIntoReg(cctrl, &next->res, rpn->raw_type, 0, bin, code_off);
		if (next->res.raw_type != RT_F64) {
			AIWNIOS_ADD_CODE(X86Test, next->res.reg, next->res.reg);
		} else {
			code_off = __ICMoveF64(cctrl, 1, 0, bin, code_off);
			AIWNIOS_ADD_CODE(X86COMISDRegReg, 1, 0);
		}
		if (rpn->res.mode == MD_REG && rpn->res.mode != RT_F64) {
			into_reg = rpn->res.reg;
		} else
			into_reg = 0; // Store into reg 0
		AIWNIOS_ADD_CODE(X86Setcc, X86_COND_Z, into_reg);
		if (!(rpn->res.mode == MD_REG && rpn->res.mode != RT_F64)) {
			tmp.raw_type = RT_I64i;
			tmp.reg = 0;
			tmp.mode = MD_REG;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_BNOT:
		if (rpn->raw_type == RT_F64) {
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, rpn->raw_type, 1, bin, code_off);
			static double ul;
			*(int64_t*)&ul = ~0ll;
			code_off = __ICMoveF64(cctrl, 0, ul, bin, code_off);
			AIWNIOS_ADD_CODE(X86XorPDReg, next->res.reg, 0);
			goto ret;
		}
		BACKEND_UNOP(X86NotReg, X86NotReg);
		break;
	case IC_POST_INC:
		code_off = __SexyPostOp(cctrl, rpn, X86AddImm32, X86AddReg, X86AddSDReg, X86IncReg,X86IncSIB64, bin, code_off);
		break;
	case IC_POST_DEC:
		code_off = __SexyPostOp(cctrl, rpn, X86SubImm32, X86SubReg, X86SubSDReg, X86DecReg,X86DecSIB64, bin, code_off);
		break;
	case IC_PRE_INC:
		code_off = __SexyPreOp(cctrl, rpn, X86AddImm32, X86AddReg, X86AddSDReg, X86IncReg,X86IncSIB64, bin, code_off);
		break;
	case IC_PRE_DEC:
		code_off = __SexyPreOp(cctrl, rpn, X86SubImm32, X86SubReg, X86SubSDReg, X86DecReg,X86DecSIB64, bin, code_off);
		break;
	case IC_AND_AND:
#define BACKEND_BOOLIFY(to, reg, rt)                        \
	if ((rt) != RT_F64) {                                   \
		AIWNIOS_ADD_CODE(X86Test, reg, reg);                \
		AIWNIOS_ADD_CODE(X86Setcc, X86_COND_Z | 1, to);     \
	} else {                                                \
		code_off = __ICMoveF64(cctrl, 2, 0, bin, code_off); \
		AIWNIOS_ADD_CODE(X86COMISDRegReg, reg, 2);          \
		AIWNIOS_ADD_CODE(X86Setcc, X86_COND_E | 1, to);     \
	}
		if (!rpn->code_misc)
			rpn->code_misc = CodeMiscNew(cctrl, CMT_LABEL);
		a = ICArgN(rpn, 1);
		b = ICArgN(rpn, 0);
		code_off = __OptPassFinal(cctrl, a, bin, code_off);
		if (a->res.raw_type != RT_F64) {
			code_off = PutICArgIntoReg(cctrl, &a->res, RT_I64i, 0, bin, code_off);
		} else {
			//
			// avoid messing up a's reg
			//
			BACKEND_BOOLIFY(0, a->res.reg, RT_F64);
			a->res.reg = 0;
			a->res.raw_type = RT_I64i;
		}
		code_off = ICMov(cctrl, &rpn->res, &a->res, bin, code_off);
		AIWNIOS_ADD_CODE(X86Test, a->res.reg, a->res.reg);
		AIWNIOS_ADD_CODE(X86Jcc, X86_COND_Z, (char*)rpn->code_misc->addr - (bin + code_off));
		code_off = __OptPassFinal(cctrl, b, bin, code_off);
		code_off = PutICArgIntoReg(cctrl, &b->res, b->raw_type, 0, bin, code_off);
		//
		// avoid messing up b's reg
		//
		BACKEND_BOOLIFY(0, b->res.reg, b->raw_type);
		b->res.reg = 0;
		b->res.raw_type = RT_I64i;
		code_off = ICMov(cctrl, &rpn->res, &b->res, bin, code_off);
		if (cctrl->code_ctrl->final_pass >= 2) {
			rpn->code_misc->addr = bin + code_off;
		}
		break;
	case IC_OR_OR:
		if (!rpn->code_misc)
			rpn->code_misc = CodeMiscNew(cctrl, CMT_LABEL);
		a = ICArgN(rpn, 1);
		b = ICArgN(rpn, 0);
		code_off = __OptPassFinal(cctrl, a, bin, code_off);
		code_off = PutICArgIntoReg(cctrl, &a->res, a->raw_type, 0, bin, code_off);
		//
		// Avoid messing up the value of a's register(use 0 instead)
		//
		BACKEND_BOOLIFY(0, a->res.reg, a->raw_type);
		a->res.reg = 0;
		a->res.raw_type = RT_I64i;
		code_off = ICMov(cctrl, &rpn->res, &a->res, bin, code_off);
		AIWNIOS_ADD_CODE(X86Test, a->res.reg, a->res.reg);
		AIWNIOS_ADD_CODE(X86Jcc, X86_COND_Z | 1, (char*)rpn->code_misc->addr - (bin + code_off));

		code_off = __OptPassFinal(cctrl, b, bin, code_off);
		code_off = PutICArgIntoReg(cctrl, &b->res, b->raw_type, 0, bin, code_off);
		//
		// Avoid messing up the value of a's register(use 0 instead)
		//
		BACKEND_BOOLIFY(0, b->res.reg, b->raw_type);
		b->res.reg = 0;
		b->res.raw_type = RT_I64i;
		code_off = ICMov(cctrl, &rpn->res, &b->res, bin, code_off);
		if (cctrl->code_ctrl->final_pass >= 2) {
			rpn->code_misc->addr = bin + code_off;
		}
		break;
	case IC_XOR_XOR:
#define BACKEND_LOGICAL_BINOP(op)                                                    \
	next = ICArgN(rpn, 1);                                                           \
	next2 = ICArgN(rpn, 0);                                                          \
	code_off = __OptPassFinal(cctrl, next2, bin, code_off);                           \
	code_off = __OptPassFinal(cctrl, next, bin, code_off);                          \
	if (rpn->res.mode == MD_REG) {                                                   \
		into_reg = rpn->res.reg;                                                     \
	} else                                                                           \
		into_reg = 0;                                                                \
	code_off = PutICArgIntoReg(cctrl, &next->res, rpn->raw_type, AIWNIOS_TMP_IREG_POOP2, bin, code_off);  \
	code_off = PutICArgIntoReg(cctrl, &next2->res, rpn->raw_type, AIWNIOS_TMP_IREG_POOP, bin, code_off); \
	BACKEND_BOOLIFY(AIWNIOS_TMP_IREG_POOP2, next->res.reg, next->res.raw_type);      \
	BACKEND_BOOLIFY(into_reg, next2->res.reg, next2->res.raw_type);                  \
	AIWNIOS_ADD_CODE(op, into_reg, AIWNIOS_TMP_IREG_POOP2);                          \
	if (rpn->res.mode != MD_REG) {                                                   \
		tmp.raw_type = rpn->raw_type;                                                \
		tmp.reg = 0;                                                                 \
		tmp.mode = MD_REG;                                                           \
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);                     \
	}
		BACKEND_LOGICAL_BINOP(X86XorReg);
		break;
	case IC_EQ_EQ:
#define BACKEND_CMP                                                                                      \
	next = ICArgN(rpn, 1);                                                                               \
	next2 = ICArgN(rpn, 0);                                                                              \
	code_off = __OptPassFinal(cctrl, next2, bin, code_off);                                               \
	code_off = __OptPassFinal(cctrl, next, bin, code_off);                                              \
	code_off = PutICArgIntoReg(cctrl, &next2->res, rpn->raw_type, (rpn->raw_type==RT_F64)?1:AIWNIOS_TMP_IREG_POOP2, bin, code_off); \
	code_off = PutICArgIntoReg(cctrl, &next->res, rpn->raw_type, 0, bin, code_off);                     \
	if (rpn->raw_type == RT_F64) {                                                                       \
		AIWNIOS_ADD_CODE(X86COMISDRegReg, next->res.reg, next2->res.reg);                                \
	} else {                                                                                             \
		AIWNIOS_ADD_CODE(X86CmpRegReg, next->res.reg, next2->res.reg);                                   \
	}
		BACKEND_CMP;
		if (use_flags)
			goto ret;
		if (rpn->res.mode == MD_REG && rpn->res.mode != RT_F64) {
			into_reg = rpn->res.reg;
		} else
			into_reg = 0;
		AIWNIOS_ADD_CODE(X86Setcc, X86_COND_E, into_reg);
		if (!(rpn->res.mode == MD_REG && rpn->res.mode != RT_F64)) {
			tmp.raw_type = RT_I64i;
			tmp.reg = 0;
			tmp.mode = MD_REG;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_NE:
		BACKEND_CMP;
		if (use_flags)
			goto ret;
		if (rpn->res.mode == MD_REG && rpn->res.mode != RT_F64) {
			into_reg = rpn->res.reg;
		} else
			into_reg = 0;
		AIWNIOS_ADD_CODE(X86Setcc, X86_COND_E | 1, into_reg);
		if (!(rpn->res.mode == MD_REG && rpn->res.mode != RT_F64)) {
			tmp.raw_type = RT_I64i;
			tmp.reg = 0;
			tmp.mode = MD_REG;
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_LSH:
		if (rpn->raw_type != RT_F64) {
			next2 = ICArgN(rpn, 0);
			next = ICArgN(rpn, 1);
			if (IsConst(next2) && Is32Bit(ConstVal(next2))) {
				BACKEND_BINOP_IMM(X86ShrImm, X86AddReg); // X86AddReg wont be used here
				break;
			}
			tmp.raw_type = RT_I64i;
			tmp.reg = RCX;
			tmp.mode = MD_REG;
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_U64i, RCX, bin, code_off);
			tmp2.mode=MD_REG; //TODO IMPROVE
			tmp2.raw_type=RT_PTR;
			tmp2.reg=0;
			code_off = ICMov(cctrl, &tmp2, &next->res, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &tmp2, RT_U64i, AIWNIOS_TMP_IREG_POOP, bin, code_off);
			code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
			AIWNIOS_ADD_CODE(X86ShlRCX, tmp2.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp2, bin, code_off);
		} else {
			next = ICArgN(rpn, 1);
			next2 = ICArgN(rpn, 0);
			if (IsConst(next2) && Is32Bit(ConstVal(next2))) {
				BACKEND_BINOP_IMM(X86ShrImm, X86AddReg); // X86AddReg wont be used here
			}
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, rpn->raw_type, 1, bin, code_off);
			code_off = PushToStack(cctrl, &next->res, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_I64i, 0, bin, code_off);
			tmp.raw_type = RT_I64i;
			tmp.mode = MD_REG;
			tmp.reg = RAX;
			code_off = PopFromStack(cctrl, &next->res, bin, code_off);
			AIWNIOS_ADD_CODE(X86ShlImm, tmp.reg, next2->res.integer);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		}
		break;
	case IC_RSH:
		if (rpn->raw_type == RT_F64) {
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next->res, rpn->raw_type, 0, bin, code_off);
			code_off = PushToStack(cctrl, &next->res, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_I64i, AIWNIOS_TMP_IREG_POOP2, bin, code_off);
			tmp.raw_type = RT_I64i;
			tmp.mode = MD_REG;
			tmp.reg = RAX;
			code_off = PopFromStack(cctrl, &next->res, bin, code_off);
			AIWNIOS_ADD_CODE(X86ShrImm, tmp.reg, next2->res.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
			break;
		}
		next=ICArgN(rpn,1);
		next2=ICArgN(rpn,0);
		switch (rpn->raw_type) {
		case RT_U8i:
		case RT_U16i:
		case RT_U32i:
		case RT_U64i:
			if (IsConst(next2) && Is32Bit(ConstVal(next2))) {
				BACKEND_BINOP_IMM(X86ShrImm, X86AddReg); // X86AddReg wont be used here
			}
			next2 = ICArgN(rpn, 0);
			next = ICArgN(rpn, 1);
			tmp.raw_type = RT_I64i;
			tmp.reg = RCX;
			tmp.mode = MD_REG;
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_U64i, RCX, bin, code_off);
			tmp2.mode=MD_REG; //TODO IMPROVE
			tmp2.raw_type=RT_PTR;
			tmp2.reg=0;
			code_off = ICMov(cctrl, &tmp2, &next->res, bin, code_off);
			// POOP2 is RCX
			code_off = PutICArgIntoReg(cctrl, &tmp2, RT_U64i, AIWNIOS_TMP_IREG_POOP, bin, code_off);
			code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
			AIWNIOS_ADD_CODE(X86ShrRCX, tmp2.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp2, bin, code_off);
			break;
		default:
			if (IsConst(next2) && Is32Bit(ConstVal(next2))) {
				BACKEND_BINOP_IMM(X86SarImm, X86AddReg); // X86AddReg wont be used here
			}
			tmp.raw_type = RT_I64i;
			tmp.reg = RCX;
			tmp.mode = MD_REG;
			code_off = __OptPassFinal(cctrl, next2, bin, code_off);
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &next2->res, RT_U64i, RCX, bin, code_off);
			tmp2.mode=MD_REG; //TODO IMPROVE
			tmp2.raw_type=RT_PTR;
			tmp2.reg=0;
			code_off = ICMov(cctrl, &tmp2, &next->res, bin, code_off);
			code_off = PutICArgIntoReg(cctrl, &tmp2, RT_U64i, AIWNIOS_TMP_IREG_POOP, bin, code_off);
			code_off = ICMov(cctrl, &tmp, &next2->res, bin, code_off);
			AIWNIOS_ADD_CODE(X86SarRCX, tmp2.reg);
			code_off = ICMov(cctrl, &rpn->res, &tmp2, bin, code_off);
		}
		break;
	case __IC_CALL:
	case IC_CALL:
		//
		// ()_()_()_()
		// /          \     // When we call a function,all temp registers
		// | O______O |     // are invalidated,so we can reset the temp register
		// | \______/ |===> // counts (for the new expressions).
		// \__________/     //
		//   \./   \./      // We will restore the temp register counts after the
		//   call
		//
		i = cctrl->backend_user_data2;
		i2 = cctrl->backend_user_data3;
		cctrl->backend_user_data2 = 0;
		cctrl->backend_user_data3 = 0;
		code_off = __ICFCall(cctrl, rpn, bin, code_off);
		cctrl->backend_user_data2 = i;
		cctrl->backend_user_data3 = i2;
		break;
	case __IC_VARGS:
		//  ^^^^^^^^
		// (        \      ___________________
		// ( (*) (*)|     /                   \
    //  \   ^   |    |__IFCall            |
		//   \  <>  | <==| Will unwind for you| //TODO validate for X86_64
		//    \    /      \__________________/
		//      \_/
		// Align to 16
		if (rpn->length & 1)
			AIWNIOS_ADD_CODE(X86SubImm32, X86_64_STACK_REG, 8 + 8 * rpn->length)
		else
			AIWNIOS_ADD_CODE(X86SubImm32, X86_64_STACK_REG, 8 * rpn->length)
		for (i = 0; i != rpn->length; i++) {
			next = ICArgN(rpn, rpn->length - i - 1);
			code_off = __OptPassFinal(cctrl, next, bin, code_off);
			tmp.reg = X86_64_STACK_REG;
			tmp.off = 8 * i;
			tmp.mode = MD_INDIR_REG;
			tmp.raw_type = next->raw_type;
			if (tmp.raw_type < RT_I64i)
				tmp.raw_type = RT_I64i;
			code_off = ICMov(cctrl, &tmp, &next->res, bin, code_off);
		}
		tmp.reg = X86_64_STACK_REG;
		tmp.mode = MD_REG;
		tmp.raw_type = RT_I64i;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		break;
	case IC_ADD_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_SUB_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_MUL_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_DIV_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_LSH_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_RSH_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_AND_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_OR_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_XOR_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_ARROW:
		abort();
		break;
	case IC_DOT:
		abort();
		break;
	case IC_MOD_EQ:
		code_off = __SexyAssignOp(cctrl, rpn, bin, code_off);
		break;
	case IC_I64:
		rpn->res.mode = MD_I64;
		rpn->res.raw_type = RT_I64i;
		rpn->res.integer = rpn->integer;
		break;
	case IC_F64:
		rpn->res.mode = MD_F64;
		rpn->res.raw_type = RT_F64;
		rpn->res.flt = rpn->flt;
		break;
	case IC_ARRAY_ACC:
		abort();
		break;
	case IC_RET:
		next = ICArgN(rpn, 0);
		PushTmpDepthFirst(cctrl,next,0,NULL);
		PopTmp(cctrl,next);
		code_off = __OptPassFinal(cctrl, next, bin, code_off);
		if (cctrl->cur_fun)
			tmp.raw_type = cctrl->cur_fun->return_class->raw_type;
		else if (rpn->ic_class) {
			// No return type so just return what we have
			tmp.raw_type = rpn->ic_class->raw_type;
		} else
			// No return type so just return what we have
			tmp.raw_type = next->raw_type;
		tmp.reg = 0; // 0 is return register
		tmp.mode = MD_REG;
		code_off = ICMov(cctrl, &tmp, &next->res, bin, code_off);
		// TempleOS will always store F64 result in RAX(integer register)
		// Let's merge the two togheter
		if (tmp.raw_type == RT_F64) {
			AIWNIOS_ADD_CODE(X86MovQI64F64, 0, 0);
		} else {
			// Vise versa
			AIWNIOS_ADD_CODE(X86MovQF64I64, 0, 0);
		}
		// TODO  jump to return area,not generate epilog for each poo poo
		AIWNIOS_ADD_CODE(X86Jmp, cctrl->epilog_offset - code_off);
		break;
	case IC_BASE_PTR:
		break;
	case IC_STATIC:
		tmp.raw_type = rpn->raw_type;
		tmp.off = rpn->integer;
		tmp.mode = MD_PTR;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		break;
	case IC_IREG:
		tmp.raw_type = RT_I64i;
		tmp.reg = rpn->integer;
		tmp.mode = MD_REG;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		break;
	case IC_FREG:
		tmp.raw_type = RT_F64;
		tmp.reg = rpn->integer;
		tmp.mode = MD_REG;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		break;
	case IC_LOAD:
		abort();
		break;
	case IC_STORE:
		abort();
		break;
	case __IC_STATIC_REF:
		tmp.raw_type = rpn->raw_type;
		tmp.off = rpn->integer;
		tmp.mode = MD_STATIC;
		code_off = ICMov(cctrl, &rpn->res, &tmp, bin, code_off);
		break;
	case __IC_SET_STATIC_DATA:
		// Final pass
		if (cctrl->code_ctrl->final_pass == 3) {
			memcpy(
				bin + cctrl->code_ctrl->statics_offset + rpn->code_misc->integer,
				rpn->code_misc->str,
				rpn->code_misc->str_len);
		}
	}
ret:
	return code_off;
}

// This dude calls __OptPassFinal 3 times.
// 1. Get size of WORST CASE compiled body,and generate any extra needed
// CCodeMisc's
//    This pass  will also asign CRPN->res with tmp registers/FRAME offsets
// 2. Fill in function body,accounting for not worst case jumps
// 3. Fill in the poo poo's
//
char* OptPassFinal(CCmpCtrl* cctrl, int64_t* res_sz, char** dbg_info)
{
	int64_t code_off, run, idx, cnt = 0, cnt2, final_size, is_terminal;
	int64_t min_ln = 0, max_ln = 0, statics_sz = 0;
	char* bin = NULL;
	char* ptr;
	CCodeMisc* misc;
	CHashImport* import;
	CRPN* r;
	for (r = cctrl->code_ctrl->ir_code->next; r != cctrl->code_ctrl->ir_code; r = r->base.next) {
		if (r->ic_line) {
			if (!min_ln)
				min_ln = r->ic_line;
			min_ln = (min_ln < r->ic_line) ? min_ln : r->ic_line;
			if (!max_ln)
				max_ln = r->ic_line;
			max_ln = (max_ln > r->ic_line) ? max_ln : r->ic_line;
		}
		if (r->type == IC_BASE_PTR) {
		} else if (r->type == __IC_STATICS_SIZE)
			statics_sz = r->integer;
	}
	for (r = cctrl->code_ctrl->ir_code->next; r != cctrl->code_ctrl->ir_code; r = ICFwd(r)) {
		cnt++;
	}
	if (dbg_info) {
		// Dont allocate on cctrl->hc heap ctrl as we want to share our datqa
		cctrl->code_ctrl->dbg_info = dbg_info;
		cctrl->code_ctrl->min_ln = min_ln;
	}
	CRPN* forwards[cnt2 = cnt];
	cnt = 0;
	for (r = cctrl->code_ctrl->ir_code->next; r != cctrl->code_ctrl->ir_code; r = ICFwd(r)) {
		forwards[cnt2 - ++cnt] = r;
	}
	// We DONT reset the wiggle room
	cctrl->backend_user_data0 = 0;
	// 0 get maximum code size,
	// 1 misc addrs
	// 2 fill in label addresses
	// 3 final in case anything changed
	for (run = 0; run != 4; run++) {
		cctrl->code_ctrl->final_pass = run;
		cctrl->backend_user_data1 = 0;
		cctrl->backend_user_data2 = 0;
		cctrl->backend_user_data3 = 0;
		if (run == 0) {
			code_off = 0;
			bin = NULL;
		} else if (run == 1) {
			// Dont allocate on cctrl->hc heap ctrl as we want to share our data
			bin = A_MALLOC(code_off+256, NULL);
			code_off = 0;
		} else {
			code_off = 0;
		}
		code_off = FuncProlog(cctrl, bin, code_off);
		for (cnt = 0; cnt < cnt2; cnt++) {
		enter:
			r = forwards[cnt];
			if(PushTmpDepthFirst(cctrl,r,0,NULL))
				PopTmp(cctrl,r);
			assert(cctrl->backend_user_data1==0);
			assert(cctrl->backend_user_data2==0);
			assert(cctrl->backend_user_data3==0);
			code_off = __OptPassFinal(cctrl, r, bin, code_off);
			if (IsTerminalInst(r)) {
				cnt++;
				for (; cnt < cnt2; cnt++) {
					if (forwards[cnt]->type == IC_LABEL)
						goto enter;
				}
			}
		}
		cctrl->epilog_offset = code_off;
		code_off = FuncEpilog(cctrl, bin, code_off);
		for (misc = cctrl->code_ctrl->code_misc->next;
			 misc != cctrl->code_ctrl->code_misc; misc = misc->base.next) {
			switch (misc->type) {
				break;
			case CMT_INT_CONST:
				if (code_off % 8)
					code_off += 8 - code_off % 8;
				misc->addr = bin + code_off;
				if (bin)
					*(int64_t*)(bin + code_off) = misc->integer;
				code_off += 8;
				break;
			case CMT_FLOAT_CONST:
				if (code_off % 8)
					code_off += 8 - code_off % 8;
				misc->addr = bin + code_off;
				if (bin)
					*(double*)(bin + code_off) = misc->flt;
				code_off += 8;
				break;
			case CMT_JMP_TAB:
				if (code_off % 8)
					code_off += 8 - code_off % 8;
				misc->addr = bin + code_off;
				if (bin) {
					for (idx = 0; idx <= misc->hi - misc->lo; idx++)
						*(void**)(bin + code_off + idx * 8) = (char*)misc->jmp_tab[idx]->addr-bin;
				}
				code_off += (misc->hi - misc->lo + 1) * 8;
				break;
			case CMT_LABEL:
				break;
			case CMT_SHORT_ADDR:
				if (run == 3 && misc->patch_addr) {
					*misc->patch_addr = misc->addr;
				}
				break;
			case CMT_RELOC_U64:
				if (code_off % 8)
					code_off += 8 - code_off % 8;
				misc->addr = bin + code_off;
				if (bin)
					*(int64_t*)(bin + code_off) = &DoNothing;
				code_off += 8;
				//
				// 3 is the final run,we add the relocation to the hash table
				//
				if (run == 3) {
					import = A_CALLOC(sizeof(CHashImport), NULL);
					import->base.type = HTT_IMPORT_SYS_SYM;
					import->base.str = A_STRDUP(misc->str, NULL);
					import->address = misc->addr;
					HashAdd(import, Fs->hash_table);
				}
				if (run == 3 && misc->patch_addr) {
					*misc->patch_addr = misc->addr;
				}
				break;
			case CMT_STRING:
				if (!(cctrl->flags & CCF_STRINGS_ON_HEAP)) {
					if (code_off % 8)
						code_off += 8 - code_off % 8;
					misc->addr = bin + code_off;
					if (bin)
						memcpy(bin + code_off, misc->str, misc->str_len);
					code_off += misc->str_len;
				} else {
					if (run == 3) {
						// See IC_STR: We "steal" the ->str because it's already on the heap.
						//  IC_STR steals the ->str point so we dont want to Free it
						misc->str = NULL;
					}
				}
			}
		}
		if (run < 3) {
			if (code_off % 8) // Align to 8
				code_off += 8 - code_off % 8;
			cctrl->code_ctrl->statics_offset = code_off;
			if (statics_sz)
				code_off += statics_sz + 8;
			if (run == 2)
				final_size = code_off;
		}
	}
	__builtin___clear_cache(bin, bin + MSize(bin));
	if (dbg_info) {
		cnt = MSize(dbg_info) / 8;
		ptr = dbg_info[0] = bin;
		for (idx = 1; idx < cnt; idx++) {
			if (!dbg_info[idx]) {
				dbg_info[idx] = NULL;
				continue;
			}
			if (ptr > dbg_info[idx]) {
				// No backwards jumps
				dbg_info[idx] = NULL;
				continue;
			}
			ptr = dbg_info[idx];
		}
	}
	if (res_sz)
		*res_sz = final_size;
	return bin;
}
