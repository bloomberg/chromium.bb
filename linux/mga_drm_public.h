/* mga_drm_public.h -- Public header for the Matrox g200/g400 driver -*- linux-c -*-
 * Created: Tue Jan 25 01:50:01 1999 by jhartmann@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors: Jeff Hartmann <jhartmann@precisioninsight.com>
 *          Keith Whitwell <keithw@precisioninsight.com>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/mga_drm_public.h,v 1.1 2000/02/11 17:26:07 dawes Exp $
 */

#ifndef _MGA_DRM_PUBLIC_H_
#define _MGA_DRM_PUBLIC_H_

#define MGA_F  0x1		/* fog */
#define MGA_A  0x2		/* alpha */
#define MGA_S  0x4		/* specular */
#define MGA_T2 0x8		/* multitexture */

#define MGA_WARP_TGZ            0
#define MGA_WARP_TGZF           (MGA_F)
#define MGA_WARP_TGZA           (MGA_A)
#define MGA_WARP_TGZAF          (MGA_F|MGA_A)
#define MGA_WARP_TGZS           (MGA_S)
#define MGA_WARP_TGZSF          (MGA_S|MGA_F)
#define MGA_WARP_TGZSA          (MGA_S|MGA_A)
#define MGA_WARP_TGZSAF         (MGA_S|MGA_F|MGA_A)
#define MGA_WARP_T2GZ           (MGA_T2)
#define MGA_WARP_T2GZF          (MGA_T2|MGA_F)
#define MGA_WARP_T2GZA          (MGA_T2|MGA_A)
#define MGA_WARP_T2GZAF         (MGA_T2|MGA_A|MGA_F)
#define MGA_WARP_T2GZS          (MGA_T2|MGA_S)
#define MGA_WARP_T2GZSF         (MGA_T2|MGA_S|MGA_F)
#define MGA_WARP_T2GZSA         (MGA_T2|MGA_S|MGA_A)
#define MGA_WARP_T2GZSAF        (MGA_T2|MGA_S|MGA_F|MGA_A)


#define MGA_MAX_G400_PIPES 16
#define MGA_MAX_G200_PIPES  8	/* no multitex */


#define MGA_MAX_WARP_PIPES MGA_MAX_G400_PIPES

#define MGA_CARD_TYPE_G200 1
#define MGA_CARD_TYPE_G400 2


typedef struct _drm_mga_warp_index {
   	int installed;
   	unsigned long phys_addr;
   	int size;
} mgaWarpIndex;

typedef struct drm_mga_init {
   	enum { 
	   	MGA_INIT_DMA = 0x01,
	       	MGA_CLEANUP_DMA = 0x02
	} func;
   	int reserved_map_agpstart;
   	int reserved_map_idx;
   	int buffer_map_idx;
   	int sarea_priv_offset;
   	int primary_size;
   	int warp_ucode_size;
   	int fbOffset;
   	int backOffset;
   	int depthOffset;
   	int textureOffset;
   	int textureSize;
   	int cpp;
   	int stride;
   	int sgram;
	int chipset;
   	mgaWarpIndex WarpIndex[MGA_MAX_WARP_PIPES];

	/* Redundant?
	 */
	int frontOrg;
	int backOrg;
	int depthOrg;
	int mAccess;
} drm_mga_init_t;

typedef struct _xf86drmClipRectRec {
   	unsigned short x1;
   	unsigned short y1;
   	unsigned short x2;
   	unsigned short y2;
} xf86drmClipRectRec;

#define MGA_CLEAR_FRONT 0x1
#define MGA_CLEAR_BACK  0x2
#define MGA_CLEAR_DEPTH 0x4


/* Each context has a state:
 */
#define MGA_CTXREG_DSTORG   0	/* validated */
#define MGA_CTXREG_MACCESS  1	
#define MGA_CTXREG_PLNWT    2	
#define MGA_CTXREG_DWGCTL    3	
#define MGA_CTXREG_ALPHACTRL 4
#define MGA_CTXREG_FOGCOLOR  5
#define MGA_CTXREG_WFLAG     6
#define MGA_CTXREG_TDUAL0    7
#define MGA_CTXREG_TDUAL1    8
#define MGA_CTX_SETUP_SIZE   9

/* 2d state
 */
#define MGA_2DREG_PITCH 	0
#define MGA_2D_SETUP_SIZE 	1

/* Each texture unit has a state:
 */
#define MGA_TEXREG_CTL        0
#define MGA_TEXREG_CTL2       1
#define MGA_TEXREG_FILTER     2
#define MGA_TEXREG_BORDERCOL  3
#define MGA_TEXREG_ORG        4 /* validated */
#define MGA_TEXREG_ORG1       5
#define MGA_TEXREG_ORG2       6
#define MGA_TEXREG_ORG3       7
#define MGA_TEXREG_ORG4       8
#define MGA_TEXREG_WIDTH      9
#define MGA_TEXREG_HEIGHT     10
#define MGA_TEX_SETUP_SIZE    11


/* What needs to be changed for the current vertex dma buffer?
 */
#define MGASAREA_NEW_CONTEXT    0x1
#define MGASAREA_NEW_TEX0       0x2
#define MGASAREA_NEW_TEX1       0x4
#define MGASAREA_NEW_PIPE       0x8
#define MGASAREA_NEW_2D 	0x10


/* Keep this small for testing
 */
#define MGA_NR_SAREA_CLIPRECTS 2

/* Upto 128 regions.  Minimum region size of 256k.
 */
#define MGA_NR_TEX_REGIONS 128
#define MGA_MIN_LOG_TEX_GRANULARITY 18

typedef struct {
   unsigned char next, prev;	
   unsigned char in_use;	
   int age;			
} mgaTexRegion;

typedef struct 
{
   	unsigned int ContextState[MGA_CTX_SETUP_SIZE];
   	unsigned int ServerState[MGA_2D_SETUP_SIZE];
   	unsigned int TexState[2][MGA_TEX_SETUP_SIZE];
   	unsigned int WarpPipe;
   	unsigned int dirty;

   	unsigned int nbox;
   	xf86drmClipRectRec boxes[MGA_NR_SAREA_CLIPRECTS];

	/* kernel doesn't touch from here down */
   	int ctxOwner;		
	mgaTexRegion texList[MGA_NR_TEX_REGIONS+1];
	int texAge;	                            
} drm_mga_sarea_t;	


/* Device specific ioctls:
 */
typedef struct {
	int clear_color;
	int clear_depth;
	int flags;
} drm_mga_clear_t;


typedef struct {
	int flags;		/* not actually used? */
} drm_mga_swap_t;

typedef struct {
	unsigned int destOrg;
	unsigned int mAccess;
   	unsigned int pitch;
	xf86drmClipRectRec texture;
   	int idx;
} drm_mga_iload_t;


#define DRM_IOCTL_MGA_INIT    DRM_IOW( 0x40, drm_mga_init_t)
#define DRM_IOCTL_MGA_SWAP    DRM_IOW( 0x41, drm_mga_swap_t)
#define DRM_IOCTL_MGA_CLEAR   DRM_IOW( 0x42, drm_mga_clear_t)
#define DRM_IOCTL_MGA_ILOAD   DRM_IOW( 0x43, drm_mga_iload_t)
#endif
