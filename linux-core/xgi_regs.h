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

#define MAKE_MASK(bits)  ((1U << (bits)) - 1)

#define ONE_BIT_MASK        MAKE_MASK(1)
#define TWENTY_BIT_MASK     MAKE_MASK(20)
#define TWENTYONE_BIT_MASK  MAKE_MASK(21)
#define TWENTYTWO_BIT_MASK  MAKE_MASK(22)


/* Port 0x3d4/0x3d5, index 0x2a */
#define XGI_INTERFACE_SEL 0x2a
#define DUAL_64BIT        (1U<<7)
#define INTERNAL_32BIT    (1U<<6)
#define EN_SEP_WR         (1U<<5)
#define POWER_DOWN_SEL    (1U<<4)
/*#define RESERVED_3      (1U<<3) */
#define SUBS_MCLK_PCICLK  (1U<<2)
#define MEM_SIZE_MASK     (3<<0)
#define MEM_SIZE_32MB     (0<<0)
#define MEM_SIZE_64MB     (1<<0)
#define MEM_SIZE_128MB    (2<<0)
#define MEM_SIZE_256MB    (3<<0)

/* Port 0x3d4/0x3d5, index 0x36 */
#define XGI_GE_CNTL 0x36
#define GE_ENABLE        (1U<<7)
/*#define RESERVED_6     (1U<<6) */
/*#define RESERVED_5     (1U<<5) */
#define GE_RESET         (1U<<4)
/*#define RESERVED_3     (1U<<3) */
#define GE_ENABLE_3D     (1U<<2)
/*#define RESERVED_1     (1U<<1) */
/*#define RESERVED_0     (1U<<0) */

/* Port 0x3ce/0x3cf, index 0x2a */
#define XGI_MISC_CTRL 0x2a
#define MOTION_VID_SUSPEND   (1U<<7)
#define DVI_CRTC_TIMING_SEL  (1U<<6)
#define LCD_SEL_CTL_NEW      (1U<<5)
#define LCD_SEL_EXT_DELYCTRL (1U<<4)
#define REG_LCDDPARST        (1U<<3)
#define LCD2DPAOFF           (1U<<2)
/*#define RESERVED_1         (1U<<1) */
#define EN_GEPWM             (1U<<0)  /* Enable GE power management */


#define BASE_3D_ENG 0x2800

#define M2REG_FLUSH_ENGINE_ADDRESS 0x000
#define M2REG_FLUSH_ENGINE_COMMAND 0x00
#define M2REG_FLUSH_FLIP_ENGINE_MASK              (ONE_BIT_MASK<<21)
#define M2REG_FLUSH_2D_ENGINE_MASK                (ONE_BIT_MASK<<20)
#define M2REG_FLUSH_3D_ENGINE_MASK                TWENTY_BIT_MASK

#define M2REG_RESET_ADDRESS 0x004
#define M2REG_RESET_COMMAND 0x01
#define M2REG_RESET_STATUS2_MASK                  (ONE_BIT_MASK<<10)
#define M2REG_RESET_STATUS1_MASK                  (ONE_BIT_MASK<<9)
#define M2REG_RESET_STATUS0_MASK                  (ONE_BIT_MASK<<8)
#define M2REG_RESET_3DENG_MASK                    (ONE_BIT_MASK<<4)
#define M2REG_RESET_2DENG_MASK                    (ONE_BIT_MASK<<2)

/* Write register */
#define M2REG_AUTO_LINK_SETTING_ADDRESS 0x010
#define M2REG_AUTO_LINK_SETTING_COMMAND 0x04
#define M2REG_CLEAR_TIMER_INTERRUPT_MASK          (ONE_BIT_MASK<<11)
#define M2REG_CLEAR_INTERRUPT_3_MASK              (ONE_BIT_MASK<<10)
#define M2REG_CLEAR_INTERRUPT_2_MASK              (ONE_BIT_MASK<<9)
#define M2REG_CLEAR_INTERRUPT_0_MASK              (ONE_BIT_MASK<<8)
#define M2REG_CLEAR_COUNTERS_MASK                 (ONE_BIT_MASK<<4)
#define M2REG_PCI_TRIGGER_MODE_MASK               (ONE_BIT_MASK<<1)
#define M2REG_INVALID_LIST_AUTO_INTERRUPT_MASK    (ONE_BIT_MASK<<0)

/* Read register */
#define M2REG_AUTO_LINK_STATUS_ADDRESS 0x010
#define M2REG_AUTO_LINK_STATUS_COMMAND 0x04
#define M2REG_ACTIVE_TIMER_INTERRUPT_MASK          (ONE_BIT_MASK<<11)
#define M2REG_ACTIVE_INTERRUPT_3_MASK              (ONE_BIT_MASK<<10)
#define M2REG_ACTIVE_INTERRUPT_2_MASK              (ONE_BIT_MASK<<9)
#define M2REG_ACTIVE_INTERRUPT_0_MASK              (ONE_BIT_MASK<<8)
#define M2REG_INVALID_LIST_AUTO_INTERRUPTED_MODE_MASK    (ONE_BIT_MASK<<0)

#define     M2REG_PCI_TRIGGER_REGISTER_ADDRESS 0x014
#define     M2REG_PCI_TRIGGER_REGISTER_COMMAND 0x05


/**
 * Begin instruction, double-word 0
 */
#define BEGIN_STOP_STORE_CURRENT_POINTER_MASK   (ONE_BIT_MASK<<22)
#define BEGIN_VALID_MASK                        (ONE_BIT_MASK<<20)
#define BEGIN_BEGIN_IDENTIFICATION_MASK         TWENTY_BIT_MASK

/**
 * Begin instruction, double-word 1
 */
#define BEGIN_LINK_ENABLE_MASK                  (ONE_BIT_MASK<<31)
#define BEGIN_COMMAND_LIST_LENGTH_MASK          TWENTYTWO_BIT_MASK


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

#endif
