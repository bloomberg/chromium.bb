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
 * $XFree86$
 */

#ifndef _MGA_DRV_H_
#define _MGA_DRV_H_

typedef struct {
   	unsigned int num_dwords;
   	unsigned int max_dwords;
   	u32 *current_dma_ptr;
   	u32 *head;
   	u32 phys_head;
   	int sec_used;
   	int idx;
   	int swap_pending;
   	u32 in_use;
   	atomic_t force_fire;
   	atomic_t needs_overflow;
} drm_mga_prim_buf_t;

typedef struct _drm_mga_freelist {
   	unsigned int age;
   	drm_buf_t *buf;
   	struct _drm_mga_freelist *next;
   	struct _drm_mga_freelist *prev;
} drm_mga_freelist_t;

typedef struct _drm_mga_private {
   	int reserved_map_idx;
   	int buffer_map_idx;
   	drm_mga_sarea_t *sarea_priv;
   	int primary_size;
   	int warp_ucode_size;
   	int chipset;
   	int frontOffset;
   	int backOffset;
   	int depthOffset;
   	int textureOffset;
   	int textureSize;
   	int cpp;
   	int stride;
   	int sgram;
	int use_agp;
   	drm_mga_warp_index_t WarpIndex[MGA_MAX_G400_PIPES];
	unsigned int WarpPipe;
   	__volatile__ unsigned long softrap_age;
   	u32 dispatch_lock;
   	atomic_t in_flush;
   	atomic_t in_wait;
   	atomic_t pending_bufs;
   	unsigned int last_sync_tag;
   	unsigned int sync_tag;
   	void *status_page;
   	unsigned long real_status_page;
   	u8 *ioremap;
   	drm_mga_prim_buf_t **prim_bufs;
   	drm_mga_prim_buf_t *next_prim;
   	drm_mga_prim_buf_t *last_prim;
   	drm_mga_prim_buf_t *current_prim;
   	int current_prim_idx;
   	struct pci_dev *device;
   	drm_mga_freelist_t *head;
   	drm_mga_freelist_t *tail;
   	wait_queue_head_t flush_queue;	/* Processes waiting until flush    */
      	wait_queue_head_t wait_queue;	/* Processes waiting until interrupt */

	/* Some validated register values:
	 */	
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

/* mga_dma_init does init and release */
extern int mga_dma_init(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long arg);
extern int mga_dma_cleanup(drm_device_t *dev);
extern int mga_flush_ioctl(struct inode *inode, struct file *filp,
			   unsigned int cmd, unsigned long arg);

extern unsigned int mga_create_sync_tag(drm_device_t *dev);
extern drm_buf_t *mga_freelist_get(drm_device_t *dev);
extern int mga_freelist_put(drm_device_t *dev, drm_buf_t *buf);
extern int mga_advance_primary(drm_device_t *dev);


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
extern int  mga_vertex(struct inode *inode, struct file *filp,
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


typedef enum {
	TT_GENERAL,
	TT_BLIT,
	TT_VECTOR,
	TT_VERTEX
} transferType_t;

typedef struct {
   	drm_mga_freelist_t *my_freelist;
	int discard;
} drm_mga_buf_priv_t;

#define DWGREG0 	0x1c00
#define DWGREG0_END 	0x1dff
#define DWGREG1		0x2c00
#define DWGREG1_END	0x2dff

#define ISREG0(r)	(r >= DWGREG0 && r <= DWGREG0_END)
#define ADRINDEX0(r)	(u8)((r - DWGREG0) >> 2)
#define ADRINDEX1(r)	(u8)(((r - DWGREG1) >> 2) | 0x80)
#define ADRINDEX(r)	(ISREG0(r) ? ADRINDEX0(r) : ADRINDEX1(r)) 

#define MGA_VERBOSE 0
#define MGA_NUM_PRIM_BUFS 	8

#define PRIMLOCALS	u8 tempIndex[4]; u32 *dma_ptr; u32 phys_head; \
			int outcount, num_dwords

#define PRIM_OVERFLOW(dev, dev_priv, length) do {			\
	drm_mga_prim_buf_t *tmp_buf =					\
		dev_priv->prim_bufs[dev_priv->current_prim_idx];	\
	if( tmp_buf->max_dwords - tmp_buf->num_dwords < length ||	\
	    tmp_buf->sec_used > MGA_DMA_BUF_NR/2) {			\
		atomic_set(&tmp_buf->force_fire, 1);			\
		mga_advance_primary(dev);				\
		mga_dma_schedule(dev, 1);				\
	} else if( atomic_read(&tmp_buf->needs_overflow)) {		\
		mga_advance_primary(dev);				\
		mga_dma_schedule(dev, 1);				\
	}								\
} while(0)

#define PRIMGETPTR(dev_priv) do {					\
	drm_mga_prim_buf_t *tmp_buf =					\
		dev_priv->prim_bufs[dev_priv->current_prim_idx];	\
	if(MGA_VERBOSE)							\
		DRM_DEBUG("PRIMGETPTR in %s\n", __FUNCTION__);		\
	dma_ptr = tmp_buf->current_dma_ptr;				\
	num_dwords = tmp_buf->num_dwords;				\
	phys_head = tmp_buf->phys_head;					\
	outcount = 0;							\
} while(0)

#define PRIMPTR(prim_buf) do {					\
	if(MGA_VERBOSE)						\
		DRM_DEBUG("PRIMPTR in %s\n", __FUNCTION__);	\
	dma_ptr = prim_buf->current_dma_ptr;			\
	num_dwords = prim_buf->num_dwords;			\
	phys_head = prim_buf->phys_head;			\
	outcount = 0;						\
} while(0)

#define PRIMFINISH(prim_buf) do {				\
	if (MGA_VERBOSE) {					\
		DRM_DEBUG( "PRIMFINISH in %s\n", __FUNCTION__);	\
                if (outcount & 3)				\
                      DRM_DEBUG(" --- truncation\n");	        \
        }							\
	prim_buf->num_dwords = num_dwords;			\
	prim_buf->current_dma_ptr = dma_ptr;			\
} while(0)

#define PRIMADVANCE(dev_priv)	do {				\
drm_mga_prim_buf_t *tmp_buf = 					\
	dev_priv->prim_bufs[dev_priv->current_prim_idx];	\
	if (MGA_VERBOSE) {					\
		DRM_DEBUG("PRIMADVANCE in %s\n", __FUNCTION__);	\
                if (outcount & 3)				\
                      DRM_DEBUG(" --- truncation\n");	\
        }							\
	tmp_buf->num_dwords = num_dwords;      			\
	tmp_buf->current_dma_ptr = dma_ptr;    			\
} while (0)

#define PRIMUPDATE(dev_priv)	do {					\
	drm_mga_prim_buf_t *tmp_buf =					\
		dev_priv->prim_bufs[dev_priv->current_prim_idx];	\
	tmp_buf->sec_used++;						\
} while (0)

#define PRIMOUTREG(reg, val) do {					\
	tempIndex[outcount]=ADRINDEX(reg);				\
	dma_ptr[1+outcount] = val;					\
	if (MGA_VERBOSE)						\
		DRM_DEBUG("   PRIMOUT %d: 0x%x -- 0x%x\n",		\
		       num_dwords + 1 + outcount, ADRINDEX(reg), val);	\
	if( ++outcount == 4) {						\
		outcount = 0;						\
		dma_ptr[0] = *(u32 *)tempIndex;				\
		dma_ptr+=5;						\
		num_dwords += 5;					\
	}								\
}while (0)

/* A reduced set of the mga registers.
 */

#define MGAREG_MGA_EXEC 			0x0100
#define MGAREG_ALPHACTRL 			0x2c7c
#define MGAREG_AR0 				0x1c60
#define MGAREG_AR1 				0x1c64
#define MGAREG_AR2 				0x1c68
#define MGAREG_AR3 				0x1c6c
#define MGAREG_AR4 				0x1c70
#define MGAREG_AR5 				0x1c74
#define MGAREG_AR6 				0x1c78
#define MGAREG_CXBNDRY				0x1c80
#define MGAREG_CXLEFT 				0x1ca0
#define MGAREG_CXRIGHT				0x1ca4
#define MGAREG_DMAPAD 				0x1c54
#define MGAREG_DSTORG 				0x2cb8
#define MGAREG_DWGCTL 				0x1c00
#define MGAREG_DWGSYNC				0x2c4c
#define MGAREG_FCOL 				0x1c24
#define MGAREG_FIFOSTATUS 			0x1e10
#define MGAREG_FOGCOL 				0x1cf4
#define MGAREG_FXBNDRY				0x1c84
#define MGAREG_FXLEFT 				0x1ca8
#define MGAREG_FXRIGHT				0x1cac
#define MGAREG_ICLEAR 				0x1e18
#define MGAREG_IEN 				0x1e1c
#define MGAREG_LEN 				0x1c5c
#define MGAREG_MACCESS				0x1c04
#define MGAREG_PITCH 				0x1c8c
#define MGAREG_PLNWT 				0x1c1c
#define MGAREG_PRIMADDRESS 			0x1e58
#define MGAREG_PRIMEND				0x1e5c
#define MGAREG_PRIMPTR				0x1e50
#define MGAREG_SECADDRESS 			0x2c40
#define MGAREG_SECEND 				0x2c44
#define MGAREG_SETUPADDRESS 			0x2cd0
#define MGAREG_SETUPEND 			0x2cd4
#define MGAREG_SOFTRAP				0x2c48
#define MGAREG_SRCORG 				0x2cb4
#define MGAREG_STATUS 				0x1e14
#define MGAREG_STENCIL				0x2cc8
#define MGAREG_STENCILCTL 			0x2ccc
#define MGAREG_TDUALSTAGE0 			0x2cf8
#define MGAREG_TDUALSTAGE1 			0x2cfc
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
#define MGAREG_WACCEPTSEQ 			0x1dd4
#define MGAREG_WCODEADDR 			0x1e6c
#define MGAREG_WFLAG 				0x1dc4
#define MGAREG_WFLAG1 				0x1de0
#define MGAREG_WFLAGNB				0x1e64
#define MGAREG_WFLAGNB1 			0x1e08
#define MGAREG_WGETMSB				0x1dc8
#define MGAREG_WIADDR 				0x1dc0
#define MGAREG_WIADDR2				0x1dd8
#define MGAREG_WMISC 				0x1e70
#define MGAREG_WVRTXSZ				0x1dcc
#define MGAREG_YBOT 				0x1c9c
#define MGAREG_YDST 				0x1c90
#define MGAREG_YDSTLEN				0x1c88
#define MGAREG_YDSTORG				0x1c94
#define MGAREG_YTOP 				0x1c98
#define MGAREG_ZORG 				0x1c0c

#define DC_atype_rstr				0x10
#define DC_atype_blk				0x40
#define PDEA_pagpxfer_enable			0x2
#define WIA_wmode_suspend			0x0
#define WIA_wmode_start 			0x3
#define WIA_wagp_agp				0x4
#define DC_opcod_trap				0x4
#define DC_arzero_enable			0x1000
#define DC_sgnzero_enable			0x2000
#define DC_shftzero_enable			0x4000
#define DC_bop_SHIFT				16
#define DC_clipdis_enable			0x80000000
#define DC_solid_enable				0x800
#define DC_transc_enable			0x40000000
#define DC_opcod_bitblt 			0x8
#define DC_atype_rpl				0x0
#define DC_linear_xy				0x0
#define DC_solid_disable			0x0
#define DC_arzero_disable			0x0
#define DC_bltmod_bfcol				0x4000000
#define DC_pattern_disable			0x0
#define DC_transc_disable			0x0

#define MGA_CLEAR_CMD (DC_opcod_trap | DC_arzero_enable | 		\
		       DC_sgnzero_enable | DC_shftzero_enable | 	\
		       (0xC << DC_bop_SHIFT) | DC_clipdis_enable | 	\
		       DC_solid_enable | DC_transc_enable)
	  

#define MGA_COPY_CMD (DC_opcod_bitblt | DC_atype_rpl | DC_linear_xy |	\
		      DC_solid_disable | DC_arzero_disable | 		\
		      DC_sgnzero_enable | DC_shftzero_enable | 		\
		      (0xC << DC_bop_SHIFT) | DC_bltmod_bfcol | 	\
		      DC_pattern_disable | DC_transc_disable | 		\
		      DC_clipdis_enable)				\

#endif
