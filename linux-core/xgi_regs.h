
/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.			
 *																			*
 * All Rights Reserved.														*
 *																			*
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including	
 * without limitation on the rights to use, copy, modify, merge,	
 * publish, distribute, sublicense, and/or sell copies of the Software,	
 * and to permit persons to whom the Software is furnished to do so,	
 * subject to the following conditions:					
 *																			*
 * The above copyright notice and this permission notice (including the	
 * next paragraph) shall be included in all copies or substantial	
 * portions of the Software.						
 *																			*
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,	
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF	
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND		
 * NON-INFRINGEMENT.  IN NO EVENT SHALL XGI AND/OR			
 * ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,		
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,		
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER			
 * DEALINGS IN THE SOFTWARE.												
 ***************************************************************************/

#ifndef _XGI_REGS_H_
#define _XGI_REGS_H_

#ifndef XGI_MMIO
#define XGI_MMIO 1
#endif

#if XGI_MMIO
#define OUTB(port, value)   writeb(value, info->mmio.vbase + port)
#define INB(port)           readb(info->mmio.vbase + port)
#define OUTW(port, value)   writew(value, info->mmio.vbase + port)
#define INW(port)           readw(info->mmio.vbase + port)
#define OUTDW(port, value)  writel(value, info->mmio.vbase + port)
#define INDW(port)          readl(info->mmio.vbase + port)
#else
#define OUTB(port, value)   outb(value, port)
#define INB(port)           inb(port)
#define OUTW(port, value)   outw(value, port)
#define INW(port)           inw(port)
#define OUTDW(port, value)  outl(value, port)
#define INDW(port)          inl(port)
#endif

/* Hardware access functions */
static inline void OUT3C5B(xgi_info_t * info, u8 index, u8 data)
{
	OUTB(0x3C4, index);
	OUTB(0x3C5, data);
}

static inline void OUT3X5B(xgi_info_t * info, u8 index, u8 data)
{
	OUTB(0x3D4, index);
	OUTB(0x3D5, data);
}

static inline void OUT3CFB(xgi_info_t * info, u8 index, u8 data)
{
	OUTB(0x3CE, index);
	OUTB(0x3CF, data);
}

static inline u8 IN3C5B(xgi_info_t * info, u8 index)
{
	volatile u8 data = 0;
	OUTB(0x3C4, index);
	data = INB(0x3C5);
	return data;
}

static inline u8 IN3X5B(xgi_info_t * info, u8 index)
{
	volatile u8 data = 0;
	OUTB(0x3D4, index);
	data = INB(0x3D5);
	return data;
}

static inline u8 IN3CFB(xgi_info_t * info, u8 index)
{
	volatile u8 data = 0;
	OUTB(0x3CE, index);
	data = INB(0x3CF);
	return data;
}

static inline void OUT3C5W(xgi_info_t * info, u8 index, u16 data)
{
	OUTB(0x3C4, index);
	OUTB(0x3C5, data);
}

static inline void OUT3X5W(xgi_info_t * info, u8 index, u16 data)
{
	OUTB(0x3D4, index);
	OUTB(0x3D5, data);
}

static inline void OUT3CFW(xgi_info_t * info, u8 index, u8 data)
{
	OUTB(0x3CE, index);
	OUTB(0x3CF, data);
}

static inline u8 IN3C5W(xgi_info_t * info, u8 index)
{
	volatile u8 data = 0;
	OUTB(0x3C4, index);
	data = INB(0x3C5);
	return data;
}

static inline u8 IN3X5W(xgi_info_t * info, u8 index)
{
	volatile u8 data = 0;
	OUTB(0x3D4, index);
	data = INB(0x3D5);
	return data;
}

static inline u8 IN3CFW(xgi_info_t * info, u8 index)
{
	volatile u8 data = 0;
	OUTB(0x3CE, index);
	data = INB(0x3CF);
	return data;
}

static inline u8 readAttr(xgi_info_t * info, u8 index)
{
	INB(0x3DA);		/* flip-flop to index */
	OUTB(0x3C0, index);
	return INB(0x3C1);
}

static inline void writeAttr(xgi_info_t * info, u8 index, u8 value)
{
	INB(0x3DA);		/* flip-flop to index */
	OUTB(0x3C0, index);
	OUTB(0x3C0, value);
}

/*
 * Graphic engine register (2d/3d) acessing interface
 */
static inline void WriteRegDWord(xgi_info_t * info, u32 addr, u32 data)
{
	/* Jong 05/25/2006 */
	XGI_INFO("Jong-WriteRegDWord()-Begin \n");
	XGI_INFO("Jong-WriteRegDWord()-info->mmio.vbase=0x%lx \n",
		 info->mmio.vbase);
	XGI_INFO("Jong-WriteRegDWord()-addr=0x%lx \n", addr);
	XGI_INFO("Jong-WriteRegDWord()-data=0x%lx \n", data);
	/* return; */

	*(volatile u32 *)(info->mmio.vbase + addr) = (data);
	XGI_INFO("Jong-WriteRegDWord()-End \n");
}

static inline void WriteRegWord(xgi_info_t * info, u32 addr, u16 data)
{
	*(volatile u16 *)(info->mmio.vbase + addr) = (data);
}

static inline void WriteRegByte(xgi_info_t * info, u32 addr, u8 data)
{
	*(volatile u8 *)(info->mmio.vbase + addr) = (data);
}

static inline u32 ReadRegDWord(xgi_info_t * info, u32 addr)
{
	volatile u32 data;
	data = *(volatile u32 *)(info->mmio.vbase + addr);
	return data;
}

static inline u16 ReadRegWord(xgi_info_t * info, u32 addr)
{
	volatile u16 data;
	data = *(volatile u16 *)(info->mmio.vbase + addr);
	return data;
}

static inline u8 ReadRegByte(xgi_info_t * info, u32 addr)
{
	volatile u8 data;
	data = *(volatile u8 *)(info->mmio.vbase + addr);
	return data;
}

#if 0
extern void OUT3C5B(xgi_info_t * info, u8 index, u8 data);
extern void OUT3X5B(xgi_info_t * info, u8 index, u8 data);
extern void OUT3CFB(xgi_info_t * info, u8 index, u8 data);
extern u8 IN3C5B(xgi_info_t * info, u8 index);
extern u8 IN3X5B(xgi_info_t * info, u8 index);
extern u8 IN3CFB(xgi_info_t * info, u8 index);
extern void OUT3C5W(xgi_info_t * info, u8 index, u8 data);
extern void OUT3X5W(xgi_info_t * info, u8 index, u8 data);
extern void OUT3CFW(xgi_info_t * info, u8 index, u8 data);
extern u8 IN3C5W(xgi_info_t * info, u8 index);
extern u8 IN3X5W(xgi_info_t * info, u8 index);
extern u8 IN3CFW(xgi_info_t * info, u8 index);

extern void WriteRegDWord(xgi_info_t * info, u32 addr, u32 data);
extern void WriteRegWord(xgi_info_t * info, u32 addr, u16 data);
extern void WriteRegByte(xgi_info_t * info, u32 addr, u8 data);
extern u32 ReadRegDWord(xgi_info_t * info, u32 addr);
extern u16 ReadRegWord(xgi_info_t * info, u32 addr);
extern u8 ReadRegByte(xgi_info_t * info, u32 addr);

extern void EnableProtect();
extern void DisableProtect();
#endif

#define Out(port, data)         OUTB(port, data)
#define bOut(port, data)        OUTB(port, data)
#define wOut(port, data)        OUTW(port, data)
#define dwOut(port, data)       OUTDW(port, data)

#define Out3x5(index, data)     OUT3X5B(info, index, data)
#define bOut3x5(index, data)    OUT3X5B(info, index, data)
#define wOut3x5(index, data)    OUT3X5W(info, index, data)

#define Out3c5(index, data)     OUT3C5B(info, index, data)
#define bOut3c5(index, data)    OUT3C5B(info, index, data)
#define wOut3c5(index, data)    OUT3C5W(info, index, data)

#define Out3cf(index, data)     OUT3CFB(info, index, data)
#define bOut3cf(index, data)    OUT3CFB(info, index, data)
#define wOut3cf(index, data)    OUT3CFW(info, index, data)

#define In(port)                INB(port)
#define bIn(port)               INB(port)
#define wIn(port)               INW(port)
#define dwIn(port)              INDW(port)

#define In3x5(index)            IN3X5B(info, index)
#define bIn3x5(index)           IN3X5B(info, index)
#define wIn3x5(index)           IN3X5W(info, index)

#define In3c5(index)            IN3C5B(info, index)
#define bIn3c5(index)           IN3C5B(info, index)
#define wIn3c5(index)           IN3C5W(info, index)

#define In3cf(index)            IN3CFB(info, index)
#define bIn3cf(index)           IN3CFB(info, index)
#define wIn3cf(index)           IN3CFW(info, index)

#define dwWriteReg(addr, data)  WriteRegDWord(info, addr, data)
#define wWriteReg(addr, data)   WriteRegWord(info, addr, data)
#define bWriteReg(addr, data)   WriteRegByte(info, addr, data)
#define dwReadReg(addr)         ReadRegDWord(info, addr)
#define wReadReg(addr)          ReadRegWord(info, addr)
#define bReadReg(addr)          ReadRegByte(info, addr)

static inline void xgi_protect_all(xgi_info_t * info)
{
	OUTB(0x3C4, 0x11);
	OUTB(0x3C5, 0x92);
}

static inline void xgi_unprotect_all(xgi_info_t * info)
{
	OUTB(0x3C4, 0x11);
	OUTB(0x3C5, 0x92);
}

static inline void xgi_enable_mmio(xgi_info_t * info)
{
	u8 protect = 0;

	/* Unprotect registers */
	outb(0x11, 0x3C4);
	protect = inb(0x3C5);
	outb(0x92, 0x3C5);

	outb(0x3A, 0x3D4);
	outb(inb(0x3D5) | 0x20, 0x3D5);

	/* Enable MMIO */
	outb(0x39, 0x3D4);
	outb(inb(0x3D5) | 0x01, 0x3D5);

	OUTB(0x3C4, 0x11);
	OUTB(0x3C5, protect);
}

static inline void xgi_disable_mmio(xgi_info_t * info)
{
	u8 protect = 0;

	/* unprotect registers */
	OUTB(0x3C4, 0x11);
	protect = INB(0x3C5);
	OUTB(0x3C5, 0x92);

	/* Disable MMIO access */
	OUTB(0x3D4, 0x39);
	OUTB(0x3D5, INB(0x3D5) & 0xFE);

	/* Protect registers */
	outb(0x11, 0x3C4);
	outb(protect, 0x3C5);
}

static inline void xgi_enable_ge(xgi_info_t * info)
{
	unsigned char bOld3cf2a = 0;
	int wait = 0;

	// Enable GE
	OUTW(0x3C4, 0x9211);

	// Save and close dynamic gating
	bOld3cf2a = bIn3cf(0x2a);
	bOut3cf(0x2a, bOld3cf2a & 0xfe);

	// Reset both 3D and 2D engine
	bOut3x5(0x36, 0x84);
	wait = 10;
	while (wait--) {
		bIn(0x36);
	}
	bOut3x5(0x36, 0x94);
	wait = 10;
	while (wait--) {
		bIn(0x36);
	}
	bOut3x5(0x36, 0x84);
	wait = 10;
	while (wait--) {
		bIn(0x36);
	}
	// Enable 2D engine only
	bOut3x5(0x36, 0x80);

	// Enable 2D+3D engine
	bOut3x5(0x36, 0x84);

	// Restore dynamic gating
	bOut3cf(0x2a, bOld3cf2a);
}

static inline void xgi_disable_ge(xgi_info_t * info)
{
	int wait = 0;

	// Reset both 3D and 2D engine
	bOut3x5(0x36, 0x84);

	wait = 10;
	while (wait--) {
		bIn(0x36);
	}
	bOut3x5(0x36, 0x94);

	wait = 10;
	while (wait--) {
		bIn(0x36);
	}
	bOut3x5(0x36, 0x84);

	wait = 10;
	while (wait--) {
		bIn(0x36);
	}

	// Disable 2D engine only
	bOut3x5(0x36, 0);
}

static inline void xgi_enable_dvi_interrupt(xgi_info_t * info)
{
	Out3cf(0x39, In3cf(0x39) & ~0x01);	//Set 3cf.39 bit 0 to 0
	Out3cf(0x39, In3cf(0x39) | 0x01);	//Set 3cf.39 bit 0 to 1
	Out3cf(0x39, In3cf(0x39) | 0x02);
}
static inline void xgi_disable_dvi_interrupt(xgi_info_t * info)
{
	Out3cf(0x39, In3cf(0x39) & ~0x02);
}

static inline void xgi_enable_crt1_interrupt(xgi_info_t * info)
{
	Out3cf(0x3d, In3cf(0x3d) | 0x04);
	Out3cf(0x3d, In3cf(0x3d) & ~0x04);
	Out3cf(0x3d, In3cf(0x3d) | 0x08);
}

static inline void xgi_disable_crt1_interrupt(xgi_info_t * info)
{
	Out3cf(0x3d, In3cf(0x3d) & ~0x08);
}

#endif
