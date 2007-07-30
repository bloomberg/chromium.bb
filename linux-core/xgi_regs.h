/****************************************************************************
 * Copyright (C) 2003-2006 by XGI Technology, Taiwan.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the	
 * "Software"), to deal in the Software without restriction, including
 * without limitation on the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * XGI AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ***************************************************************************/

#ifndef _XGI_REGS_H_
#define _XGI_REGS_H_

#include "drmP.h"
#include "drm.h"


/* Hardware access functions */
static inline void OUT3C5B(struct drm_map * map, u8 index, u8 data)
{
	DRM_WRITE8(map, 0x3C4, index);
	DRM_WRITE8(map, 0x3C5, data);
}

static inline void OUT3X5B(struct drm_map * map, u8 index, u8 data)
{
	DRM_WRITE8(map, 0x3D4, index);
	DRM_WRITE8(map, 0x3D5, data);
}

static inline void OUT3CFB(struct drm_map * map, u8 index, u8 data)
{
	DRM_WRITE8(map, 0x3CE, index);
	DRM_WRITE8(map, 0x3CF, data);
}

static inline u8 IN3C5B(struct drm_map * map, u8 index)
{
	DRM_WRITE8(map, 0x3C4, index);
	return DRM_READ8(map, 0x3C5);
}

static inline u8 IN3X5B(struct drm_map * map, u8 index)
{
	DRM_WRITE8(map, 0x3D4, index);
	return DRM_READ8(map, 0x3D5);
}

static inline u8 IN3CFB(struct drm_map * map, u8 index)
{
	DRM_WRITE8(map, 0x3CE, index);
	return DRM_READ8(map, 0x3CF);
}


/*
 * Graphic engine register (2d/3d) acessing interface
 */
static inline void dwWriteReg(struct drm_map * map, u32 addr, u32 data)
{
#ifdef XGI_MMIO_DEBUG
	DRM_INFO("mmio_map->handle = 0x%p, addr = 0x%x, data = 0x%x\n",
		 map->handle, addr, data);
#endif
	DRM_WRITE32(map, addr, data);
}


static inline void xgi_enable_mmio(struct xgi_info * info)
{
	u8 protect = 0;
	u8 temp;

	/* Unprotect registers */
	DRM_WRITE8(info->mmio_map, 0x3C4, 0x11);
	protect = DRM_READ8(info->mmio_map, 0x3C5);
	DRM_WRITE8(info->mmio_map, 0x3C5, 0x92);

	DRM_WRITE8(info->mmio_map, 0x3D4, 0x3A);
	temp = DRM_READ8(info->mmio_map, 0x3D5);
	DRM_WRITE8(info->mmio_map, 0x3D5, temp | 0x20);

	/* Enable MMIO */
	DRM_WRITE8(info->mmio_map, 0x3D4, 0x39);
	temp = DRM_READ8(info->mmio_map, 0x3D5);
	DRM_WRITE8(info->mmio_map, 0x3D5, temp | 0x01);

	/* Protect registers */
	OUT3C5B(info->mmio_map, 0x11, protect);
}

static inline void xgi_disable_mmio(struct xgi_info * info)
{
	u8 protect = 0;
	u8 temp;

	/* Unprotect registers */
	DRM_WRITE8(info->mmio_map, 0x3C4, 0x11);
	protect = DRM_READ8(info->mmio_map, 0x3C5);
	DRM_WRITE8(info->mmio_map, 0x3C5, 0x92);

	/* Disable MMIO access */
	DRM_WRITE8(info->mmio_map, 0x3D4, 0x39);
	temp = DRM_READ8(info->mmio_map, 0x3D5);
	DRM_WRITE8(info->mmio_map, 0x3D5, temp & 0xFE);

	/* Protect registers */
	OUT3C5B(info->mmio_map, 0x11, protect);
}

static inline void xgi_enable_ge(struct xgi_info * info)
{
	unsigned char bOld3cf2a = 0;
	int wait = 0;

	// Enable GE
	OUT3C5B(info->mmio_map, 0x11, 0x92);

	// Save and close dynamic gating
	bOld3cf2a = IN3CFB(info->mmio_map, 0x2a);
	OUT3CFB(info->mmio_map, 0x2a, bOld3cf2a & 0xfe);

	// Reset both 3D and 2D engine
	OUT3X5B(info->mmio_map, 0x36, 0x84);
	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}
	OUT3X5B(info->mmio_map, 0x36, 0x94);
	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}
	OUT3X5B(info->mmio_map, 0x36, 0x84);
	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}
	// Enable 2D engine only
	OUT3X5B(info->mmio_map, 0x36, 0x80);

	// Enable 2D+3D engine
	OUT3X5B(info->mmio_map, 0x36, 0x84);

	// Restore dynamic gating
	OUT3CFB(info->mmio_map, 0x2a, bOld3cf2a);
}

static inline void xgi_disable_ge(struct xgi_info * info)
{
	int wait = 0;

	// Reset both 3D and 2D engine
	OUT3X5B(info->mmio_map, 0x36, 0x84);

	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}
	OUT3X5B(info->mmio_map, 0x36, 0x94);

	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}
	OUT3X5B(info->mmio_map, 0x36, 0x84);

	wait = 10;
	while (wait--) {
		DRM_READ8(info->mmio_map, 0x36);
	}

	// Disable 2D engine only
	OUT3X5B(info->mmio_map, 0x36, 0);
}

static inline void xgi_enable_dvi_interrupt(struct xgi_info * info)
{
	OUT3CFB(info->mmio_map, 0x39, IN3CFB(info->mmio_map, 0x39) & ~0x01);	//Set 3cf.39 bit 0 to 0
	OUT3CFB(info->mmio_map, 0x39, IN3CFB(info->mmio_map, 0x39) | 0x01);	//Set 3cf.39 bit 0 to 1
	OUT3CFB(info->mmio_map, 0x39, IN3CFB(info->mmio_map, 0x39) | 0x02);
}
static inline void xgi_disable_dvi_interrupt(struct xgi_info * info)
{
	OUT3CFB(info->mmio_map, 0x39, IN3CFB(info->mmio_map, 0x39) & ~0x02);
}

static inline void xgi_enable_crt1_interrupt(struct xgi_info * info)
{
	OUT3CFB(info->mmio_map, 0x3d, IN3CFB(info->mmio_map, 0x3d) | 0x04);
	OUT3CFB(info->mmio_map, 0x3d, IN3CFB(info->mmio_map, 0x3d) & ~0x04);
	OUT3CFB(info->mmio_map, 0x3d, IN3CFB(info->mmio_map, 0x3d) | 0x08);
}

static inline void xgi_disable_crt1_interrupt(struct xgi_info * info)
{
	OUT3CFB(info->mmio_map, 0x3d, IN3CFB(info->mmio_map, 0x3d) & ~0x08);
}

#endif
