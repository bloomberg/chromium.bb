/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#ifndef __SAVAGE_DRV_H__
#define __SAVAGE_DRV_H__

/* these chip tags should match the ones in the 2D driver in savage_regs.h. */
enum savage_family {
    S3_UNKNOWN = 0,
    S3_SAVAGE3D,
    S3_SAVAGE_MX,
    S3_SAVAGE4,
    S3_PROSAVAGE,
    S3_TWISTER,
    S3_PROSAVAGEDDR,
    S3_SUPERSAVAGE,
    S3_SAVAGE2000,
    S3_LAST
};

#define S3_SAVAGE3D_SERIES(chip)  ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE_MX))

#define S3_SAVAGE4_SERIES(chip)  ((chip==S3_SAVAGE4)            \
                                  || (chip==S3_PROSAVAGE)       \
                                  || (chip==S3_TWISTER)         \
                                  || (chip==S3_PROSAVAGEDDR))

#define	S3_SAVAGE_MOBILE_SERIES(chip)	((chip==S3_SAVAGE_MX) || (chip==S3_SUPERSAVAGE))

#define S3_SAVAGE_SERIES(chip)    ((chip>=S3_SAVAGE3D) && (chip<=S3_SAVAGE2000))

#define S3_MOBILE_TWISTER_SERIES(chip)   ((chip==S3_TWISTER)    \
                                          ||(chip==S3_PROSAVAGEDDR))

/* flags */
#define SAVAGE_IS_AGP 1

typedef struct drm_savage_private {
	drm_savage_sarea_t *sarea_priv;

	enum savage_family chipset;
	unsigned flags;
	
} drm_savage_private_t;

#define SAVAGE_FB_SIZE_S3	0x01000000  /*  16MB */
#define SAVAGE_FB_SIZE_S4	0x02000000  /*  32MB */
#define SAVAGE_MMIO_SIZE        0x00080000  /* 512kB */
#define SAVAGE_APERTURE_OFFSET  0x02000000  /*  32MB */
#define SAVAGE_APERTURE_SIZE    0x05000000  /* 5 tiled surfaces, 16MB each */

#endif /* end #ifndef __SAVAGE_DRV_ */
