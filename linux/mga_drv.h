/* mga_drv.h -- Private header for the Matrox g200/g400 driver -*- linux-c -*-
 * Created: Mon Dec 13 01:50:01 1999 by jhartmann@precisioninsight.com
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
 * Authors: Rickard E. (Rik) Faith <faith@precisioninsight.com>
 * 	    Jeff Hartmann <jhartmann@precisioninsight.com>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/mga_drv.h,v 1.1 2000/02/11 17:26:08 dawes Exp $
 */

#ifndef _MGA_DRV_H_
#define _MGA_DRV_H_
#include "mga_drm_public.h"

typedef struct _drm_mga_private {
   	int reserved_map_idx;
   	int buffer_map_idx;
   	drm_mga_sarea_t *sarea_priv;
   	int primary_size;
   	int warp_ucode_size;
   	int chipset;
   	int fbOffset;
   	int backOffset;
   	int depthOffset;
   	int textureOffset;
   	int textureSize;
   	int cpp;
   	int stride;
   	int sgram;
	int use_agp;
   	mgaWarpIndex WarpIndex[MGA_MAX_G400_PIPES];
   	__volatile__ unsigned long softrap_age;
   	atomic_t dispatch_lock;
   	atomic_t pending_bufs;
   	void *ioremap;
   	u32 *prim_head;
   	u32 *current_dma_ptr;
   	u32 prim_phys_head;
   	int prim_num_dwords;
   	int prim_max_dwords;


	/* Some validated register values:
	 */	
	u32 frontOrg;
	u32 backOrg;
	u32 depthOrg;
	u32 mAccess;

} drm_mga_private_t;

				/* mga_drv.c */
extern int  mga_init(void);
extern void mga_cleanup(void);
extern int  mga_version(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  mga_open(struct inode *inode, struct file *filp);
extern int  mga_release(struct inode *inode, struct file *filp);
extern int  mga_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  mga_unlock(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);

				/* mga_dma.c */
extern int  mga_dma_schedule(drm_device_t *dev, int locked);
extern int  mga_dma(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg);
extern int  mga_irq_install(drm_device_t *dev, int irq);
extern int  mga_irq_uninstall(drm_device_t *dev);
extern int  mga_control(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  mga_lock(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
#if 0
extern void mga_dma_init(drm_device_t *dev);
extern void mga_dma_cleanup(drm_device_t *dev);

#endif
extern int mga_dma_init(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int mga_dma_cleanup(drm_device_t *dev);

/* mga_dma_init does init and release */


				/* mga_bufs.c */
extern int  mga_addbufs(struct inode *inode, struct file *filp, 
			unsigned int cmd, unsigned long arg);
extern int  mga_infobufs(struct inode *inode, struct file *filp, 
			 unsigned int cmd, unsigned long arg);
extern int  mga_markbufs(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);
extern int  mga_freebufs(struct inode *inode, struct file *filp,
			 unsigned int cmd, unsigned long arg);
extern int  mga_mapbufs(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int  mga_addmap(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
				/* mga_state.c */
extern int  mga_clear_bufs(struct inode *inode, struct file *filp,
			   unsigned int cmd, unsigned long arg);
extern int  mga_swap_bufs(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  mga_iload(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg);
				/* mga_context.c */
extern int  mga_resctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  mga_addctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  mga_modctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  mga_getctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  mga_switchctx(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
extern int  mga_newctx(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg);
extern int  mga_rmctx(struct inode *inode, struct file *filp,
		      unsigned int cmd, unsigned long arg);

extern int  mga_context_switch(drm_device_t *dev, int old, int new);
extern int  mga_context_switch_complete(drm_device_t *dev, int new);



#define MGAREG_MGA_EXEC 			0x0100
#define MGAREG_AGP_PLL				0x1e4c
#define MGAREG_ALPHACTRL 			0x2c7c
#define MGAREG_ALPHASTART 			0x2c70
#define MGAREG_ALPHAXINC 			0x2c74
#define MGAREG_ALPHAYINC 			0x2c78
#define MGAREG_AR0 				0x1c60
#define MGAREG_AR1 				0x1c64
#define MGAREG_AR2 				0x1c68
#define MGAREG_AR3 				0x1c6c
#define MGAREG_AR4 				0x1c70
#define MGAREG_AR5 				0x1c74
#define MGAREG_AR6 				0x1c78
#define MGAREG_BCOL 				0x1c20
#define MGAREG_CXBNDRY				0x1c80
#define MGAREG_CXLEFT 				0x1ca0
#define MGAREG_CXRIGHT				0x1ca4
#define MGAREG_DMAPAD 				0x1c54
#define MGAREG_DR0_Z32LSB 			0x2c50
#define MGAREG_DR0_Z32MSB 			0x2c54
#define MGAREG_DR2_Z32LSB 			0x2c60
#define MGAREG_DR2_Z32MSB 			0x2c64
#define MGAREG_DR3_Z32LSB 			0x2c68
#define MGAREG_DR3_Z32MSB 			0x2c6c
#define MGAREG_DR0 				0x1cc0
#define MGAREG_DR2 				0x1cc8
#define MGAREG_DR3 				0x1ccc
#define MGAREG_DR4 				0x1cd0
#define MGAREG_DR6 				0x1cd8
#define MGAREG_DR7 				0x1cdc
#define MGAREG_DR8 				0x1ce0
#define MGAREG_DR10 				0x1ce8
#define MGAREG_DR11 				0x1cec
#define MGAREG_DR12 				0x1cf0
#define MGAREG_DR14 				0x1cf8
#define MGAREG_DR15 				0x1cfc
#define MGAREG_DSTORG 				0x2cb8
#define MGAREG_DWG_INDIR_WT 			0x1e80
#define MGAREG_DWGCTL 				0x1c00
#define MGAREG_DWGSYNC				0x2c4c
#define MGAREG_FCOL 				0x1c24
#define MGAREG_FIFOSTATUS 			0x1e10
#define MGAREG_FOGCOL 				0x1cf4
#define MGAREG_FOGSTART 			0x1cc4
#define MGAREG_FOGXINC				0x1cd4
#define MGAREG_FOGYINC				0x1ce4
#define MGAREG_FXBNDRY				0x1c84
#define MGAREG_FXLEFT 				0x1ca8
#define MGAREG_FXRIGHT				0x1cac
#define MGAREG_ICLEAR 				0x1e18
#define MGAREG_IEN 				0x1e1c
#define MGAREG_LEN 				0x1c5c
#define MGAREG_MACCESS				0x1c04
#define MGAREG_MCTLWTST 			0x1c08
#define MGAREG_MEMRDBK				0x1e44
#define MGAREG_OPMODE 				0x1e54
#define MGAREG_PAT0 				0x1c10
#define MGAREG_PAT1 				0x1c14
#define MGAREG_PITCH 				0x1c8c
#define MGAREG_PLNWT 				0x1c1c
#define MGAREG_PRIMADDRESS 			0x1e58
#define MGAREG_PRIMEND				0x1e5c
#define MGAREG_PRIMPTR				0x1e50
#define MGAREG_RST 				0x1e40
#define MGAREG_SECADDRESS 			0x2c40
#define MGAREG_SECEND 				0x2c44
#define MGAREG_SETUPADDRESS 			0x2cd0
#define MGAREG_SETUPEND 			0x2cd4
#define MGAREG_SGN 				0x1c58
#define MGAREG_SHIFT 				0x1c50
#define MGAREG_SOFTRAP				0x2c48
#define MGAREG_SPECBSTART 			0x2c98
#define MGAREG_SPECBXINC 			0x2c9c
#define MGAREG_SPECBYINC 			0x2ca0
#define MGAREG_SPECGSTART 			0x2c8c
#define MGAREG_SPECGXINC 			0x2c90
#define MGAREG_SPECGYINC 			0x2c94
#define MGAREG_SPECRSTART 			0x2c80
#define MGAREG_SPECRXINC 			0x2c84
#define MGAREG_SPECRYINC 			0x2c88
#define MGAREG_SRC0 				0x1c30
#define MGAREG_SRC1 				0x1c34
#define MGAREG_SRC2 				0x1c38
#define MGAREG_SRC3 				0x1c3c
#define MGAREG_SRCORG 				0x2cb4
#define MGAREG_STATUS 				0x1e14
#define MGAREG_STENCIL				0x2cc8
#define MGAREG_STENCILCTL 			0x2ccc
#define MGAREG_TDUALSTAGE0 			0x2cf8
#define MGAREG_TDUALSTAGE1 			0x2cfc
#define MGAREG_TEST0 				0x1e48
#define MGAREG_TEXBORDERCOL 			0x2c5c
#define MGAREG_TEXCTL 				0x2c30
#define MGAREG_TEXCTL2				0x2c3c
#define MGAREG_TEXFILTER 			0x2c58
#define MGAREG_TEXHEIGHT 			0x2c2c
#define MGAREG_TEXORG 				0x2c24
#define MGAREG_TEXORG1				0x2ca4
#define MGAREG_TEXORG2				0x2ca8
#define MGAREG_TEXORG3				0x2cac
#define MGAREG_TEXORG4				0x2cb0
#define MGAREG_TEXTRANS 			0x2c34
#define MGAREG_TEXTRANSHIGH 			0x2c38
#define MGAREG_TEXWIDTH 			0x2c28
#define MGAREG_TMR0 				0x2c00
#define MGAREG_TMR1 				0x2c04
#define MGAREG_TMR2 				0x2c08
#define MGAREG_TMR3 				0x2c0c
#define MGAREG_TMR4 				0x2c10
#define MGAREG_TMR5 				0x2c14
#define MGAREG_TMR6 				0x2c18
#define MGAREG_TMR7 				0x2c1c
#define MGAREG_TMR8 				0x2c20
#define MGAREG_VBIADDR0 			0x3e08
#define MGAREG_VBIADDR1 			0x3e0c
#define MGAREG_VCOUNT 				0x1e20
#define MGAREG_WACCEPTSEQ 			0x1dd4
#define MGAREG_WCODEADDR 			0x1e6c
#define MGAREG_WFLAG 				0x1dc4
#define MGAREG_WFLAG1 				0x1de0
#define MGAREG_WFLAGNB				0x1e64
#define MGAREG_WFLAGNB1 			0x1e08
#define MGAREG_WGETMSB				0x1dc8
#define MGAREG_WIADDR 				0x1dc0
#define MGAREG_WIADDR2				0x1dd8
#define MGAREG_WIADDRNB 			0x1e60
#define MGAREG_WIADDRNB1 			0x1e04
#define MGAREG_WIADDRNB2 			0x1e00
#define MGAREG_WIMEMADDR 			0x1e68
#define MGAREG_WIMEMDATA 			0x2000
#define MGAREG_WIMEMDATA1 			0x2100
#define MGAREG_WMISC 				0x1e70
#define MGAREG_WR 				0x2d00
#define MGAREG_WVRTXSZ				0x1dcc
#define MGAREG_XDST 				0x1cb0
#define MGAREG_XYEND 				0x1c44
#define MGAREG_XYSTRT 				0x1c40
#define MGAREG_YBOT 				0x1c9c
#define MGAREG_YDST 				0x1c90
#define MGAREG_YDSTLEN				0x1c88
#define MGAREG_YDSTORG				0x1c94
#define MGAREG_YTOP 				0x1c98
#define MGAREG_ZORG 				0x1c0c

#endif
