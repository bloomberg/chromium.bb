/*
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/* copied from ddx definitions, until rules-ng can handle this */

#define NV50_UPDATE_DISPLAY		0x80
#define NV50_UNK84				0x84
#define NV50_UNK88				0x88

#define NV50_DAC0_MODE_CTRL		0x400
	#define NV50_DAC_MODE_CTRL_OFF			(0 << 0)
	#define NV50_DAC_MODE_CTRL_CRTC0			(1 << 0)
	#define NV50_DAC_MODE_CTRL_CRTC1			(1 << 1)
#define NV50_DAC1_MODE_CTRL		0x480
#define NV50_DAC2_MODE_CTRL		0x500

#define NV50_DAC0_MODE_CTRL2		0x404
	#define NV50_DAC_MODE_CTRL2_NHSYNC			(1 << 0)
	#define NV50_DAC_MODE_CTRL2_NVSYNC			(2 << 0)
#define NV50_DAC1_MODE_CTRL2		0x484
#define NV50_DAC2_MODE_CTRL2		0x504

#define NV50_SOR0_MODE_CTRL		0x600
	#define NV50_SOR_MODE_CTRL_OFF			(0 << 0)
	#define NV50_SOR_MODE_CTRL_CRTC0			(1 << 0)
	#define NV50_SOR_MODE_CTRL_CRTC1			(1 << 1)
	#define NV50_SOR_MODE_CTRL_LVDS			(0 << 8)
	#define NV50_SOR_MODE_CTRL_TMDS			(1 << 8)
	#define NV50_SOR_MODE_CTRL_TMDS_DUAL_LINK	(4 << 8)
	#define NV50_SOR_MODE_CTRL_NHSYNC			(1 << 12)
	#define NV50_SOR_MODE_CTRL_NVSYNC			(2 << 12)
#define NV50_SOR1_MODE_CTRL		0x640

#define NV50_CRTC0_UNK800			0x800
#define NV50_CRTC0_CLOCK			0x804
#define NV50_CRTC0_INTERLACE		0x808

/* 0x810 is a reasonable guess, nothing more. */
#define NV50_CRTC0_DISPLAY_START				0x810
#define NV50_CRTC0_DISPLAY_TOTAL				0x814
#define NV50_CRTC0_SYNC_DURATION				0x818
#define NV50_CRTC0_SYNC_START_TO_BLANK_END		0x81C
#define NV50_CRTC0_MODE_UNK1					0x820
#define NV50_CRTC0_MODE_UNK2					0x824

#define NV50_CRTC0_UNK82C						0x82C

/* You can't have a palette in 8 bit mode (=OFF) */
#define NV50_CRTC0_CLUT_MODE		0x840
	#define NV50_CRTC0_CLUT_MODE_BLANK		0x00000000
	#define NV50_CRTC0_CLUT_MODE_OFF		0x80000000
	#define NV50_CRTC0_CLUT_MODE_ON		0xC0000000
#define NV50_CRTC0_CLUT_OFFSET		0x844

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC0_BLANK_UNK1		0x85C
	#define NV84_CRTC0_BLANK_UNK1_BLANK	0x0
	#define NV84_CRTC0_BLANK_UNK1_UNBLANK	0x1

#define NV50_CRTC0_FB_OFFSET		0x860

#define NV50_CRTC0_FB_SIZE			0x868
#define NV50_CRTC0_FB_PITCH		0x86C

#define NV50_CRTC0_DEPTH			0x870
	#define NV50_CRTC0_DEPTH_8BPP		0x1E00
	#define NV50_CRTC0_DEPTH_15BPP	0xE900
	#define NV50_CRTC0_DEPTH_16BPP	0xE800
	#define NV50_CRTC0_DEPTH_24BPP	0xCF00

/* I'm openminded to better interpretations. */
/* This is an educated guess. */
/* NV50 has RAMDAC and TMDS offchip, so it's unlikely to be that. */
#define NV50_CRTC0_BLANK_CTRL		0x874
	#define NV50_CRTC0_BLANK_CTRL_BLANK	0x0
	#define NV50_CRTC0_BLANK_CTRL_UNBLANK	0x1

#define NV50_CRTC0_CURSOR_CTRL	0x880
	#define NV50_CRTC0_CURSOR_CTRL_SHOW	0x85000000
	#define NV50_CRTC0_CURSOR_CTRL_HIDE	0x05000000

#define NV50_CRTC0_CURSOR_OFFSET	0x884

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC0_BLANK_UNK2		0x89C
	#define NV84_CRTC0_BLANK_UNK2_BLANK	0x0
	#define NV84_CRTC0_BLANK_UNK2_UNBLANK	0x1

#define NV50_CRTC0_DITHERING_CTRL	0x8A0
	#define NV50_CRTC0_DITHERING_CTRL_ON	0x11
	#define NV50_CRTC0_DITHERING_CTRL_OFF	0x0

#define NV50_CRTC0_SCALE_CTRL		0x8A4
	#define	NV50_CRTC0_SCALE_CTRL_SCALER_INACTIVE	(0 << 0)
	/* It doesn't seem to be needed, hence i wonder what it does precisely. */
	#define	NV50_CRTC0_SCALE_CTRL_SCALER_ACTIVE	(9 << 0)
#define NV50_CRTC0_COLOR_CTRL		0x8A8
	#define NV50_CRTC_COLOR_CTRL_MODE_COLOR		(4 << 16)

#define NV50_CRTC0_FB_POS			0x8C0
#define NV50_CRTC0_REAL_RES		0x8C8

/* Added a macro, because the signed stuff can cause you problems very quickly. */
#define NV50_CRTC0_SCALE_CENTER_OFFSET		0x8D4
	#define NV50_CRTC_SCALE_CENTER_OFFSET_VAL(x, y) ((((unsigned)y << 16) & 0xFFFF0000) | (((unsigned)x) & 0x0000FFFF))
/* Both of these are needed, otherwise nothing happens. */
#define NV50_CRTC0_SCALE_RES1		0x8D8
#define NV50_CRTC0_SCALE_RES2		0x8DC

#define NV50_CRTC1_UNK800			0xC00
#define NV50_CRTC1_CLOCK			0xC04
#define NV50_CRTC1_INTERLACE		0xC08

/* 0xC10 is a reasonable guess, nothing more. */
#define NV50_CRTC1_DISPLAY_START				0xC10
#define NV50_CRTC1_DISPLAY_TOTAL				0xC14
#define NV50_CRTC1_SYNC_DURATION				0xC18
#define NV50_CRTC1_SYNC_START_TO_BLANK_END		0xC1C
#define NV50_CRTC1_MODE_UNK1					0xC20
#define NV50_CRTC1_MODE_UNK2					0xC24

#define NV50_CRTC1_CLUT_MODE		0xC40
	#define NV50_CRTC1_CLUT_MODE_BLANK		0x00000000
	#define NV50_CRTC1_CLUT_MODE_OFF		0x80000000
	#define NV50_CRTC1_CLUT_MODE_ON		0xC0000000
#define NV50_CRTC1_CLUT_OFFSET		0xC44

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC1_BLANK_UNK1		0xC5C
	#define NV84_CRTC1_BLANK_UNK1_BLANK	0x0
	#define NV84_CRTC1_BLANK_UNK1_UNBLANK	0x1

#define NV50_CRTC1_FB_OFFSET		0xC60

#define NV50_CRTC1_FB_SIZE			0xC68
#define NV50_CRTC1_FB_PITCH		0xC6C

#define NV50_CRTC1_DEPTH			0xC70
	#define NV50_CRTC1_DEPTH_8BPP		0x1E00
	#define NV50_CRTC1_DEPTH_15BPP	0xE900
	#define NV50_CRTC1_DEPTH_16BPP	0xE800
	#define NV50_CRTC1_DEPTH_24BPP	0xCF00

/* I'm openminded to better interpretations. */
#define NV50_CRTC1_BLANK_CTRL		0xC74
	#define NV50_CRTC1_BLANK_CTRL_BLANK	0x0
	#define NV50_CRTC1_BLANK_CTRL_UNBLANK	0x1

#define NV50_CRTC1_CURSOR_CTRL		0xC80
	#define NV50_CRTC1_CURSOR_CTRL_SHOW	0x85000000
	#define NV50_CRTC1_CURSOR_CTRL_HIDE	0x05000000

#define NV50_CRTC1_CURSOR_OFFSET	0xC84

/* Anyone know what part of the chip is triggered here precisely? */
#define NV84_CRTC1_BLANK_UNK2		0xC9C
	#define NV84_CRTC1_BLANK_UNK2_BLANK	0x0
	#define NV84_CRTC1_BLANK_UNK2_UNBLANK	0x1

#define NV50_CRTC1_DITHERING_CTRL	0xCA0
	#define NV50_CRTC1_DITHERING_CTRL_ON	0x11
	#define NV50_CRTC1_DITHERING_CTRL_OFF	0x0

#define NV50_CRTC1_SCALE_CTRL		0xCA4
#define NV50_CRTC1_COLOR_CTRL		0xCA8

#define NV50_CRTC1_FB_POS			0xCC0
#define NV50_CRTC1_REAL_RES		0xCC8

#define NV50_CRTC1_SCALE_CENTER_OFFSET		0xCD4
/* Both of these are needed, otherwise nothing happens. */
#define NV50_CRTC1_SCALE_RES1		0xCD8
#define NV50_CRTC1_SCALE_RES2		0xCDC
