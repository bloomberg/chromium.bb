/* mga_dma.c -- DMA support for mga g200/g400 -*- linux-c -*-
 * Created: Mon Dec 13 01:50:01 1999 by jhartmann@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * All Rights Reserved.
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
 *	    Jeff Hartmann <jhartmann@precisioninsight.com>
 *
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/mga_dma.c,v 1.1 2000/02/11 17:26:07 dawes Exp $
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include "mga_drv.h"
#include "mgareg_flags.h"
#include "mga_dma.h"
#include "mga_state.h"

#include <linux/interrupt.h>	/* For task queue support */

#define MGA_REG(reg)		2
#define MGA_BASE(reg)		((unsigned long) \
				((drm_device_t *)dev)->maplist[MGA_REG(reg)]->handle)
#define MGA_ADDR(reg)		(MGA_BASE(reg) + reg)
#define MGA_DEREF(reg)		*(__volatile__ int *)MGA_ADDR(reg)
#define MGA_READ(reg)		MGA_DEREF(reg)
#define MGA_WRITE(reg,val) 	do { MGA_DEREF(reg) = val; } while (0)

#define PDEA_pagpxfer_enable 	     0x2
#define MGA_SYNC_TAG         	     0x423f4200

typedef enum {
	TT_GENERAL,
	TT_BLIT,
	TT_VECTOR,
	TT_VERTEX
} transferType_t;


static void mga_delay(void)
{
   	return;
}

int mga_dma_cleanup(drm_device_t *dev)
{
	if(dev->dev_private) {
		drm_mga_private_t *dev_priv = 
			(drm_mga_private_t *) dev->dev_private;
      
		if(dev_priv->ioremap) {
			int temp = (dev_priv->warp_ucode_size + 
				    dev_priv->primary_size + 
				    PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;

			drm_ioremapfree((void *) dev_priv->ioremap, temp);
		}

		drm_free(dev->dev_private, sizeof(drm_mga_private_t), 
			 DRM_MEM_DRIVER);
		dev->dev_private = NULL;
	}

	return 0;
}

static int mga_alloc_kernel_queue(drm_device_t *dev)
{
	drm_queue_t *queue = NULL;
				/* Allocate a new queue */
	down(&dev->struct_sem);
	
   	if(dev->queue_count != 0) {
	   /* Reseting the kernel context here is not
	    * a race, since it can only happen when that
	    * queue is empty.
	    */
	   queue = dev->queuelist[DRM_KERNEL_CONTEXT];
	   printk("Kernel queue already allocated\n");
	} else {
	   queue = drm_alloc(sizeof(*queue), DRM_MEM_QUEUES);
	   if(!queue) {
	      up(&dev->struct_sem);
	      printk("out of memory\n");
	      return -ENOMEM;
	   }
	   ++dev->queue_count;
	   dev->queuelist = drm_alloc(sizeof(*dev->queuelist), 
				      DRM_MEM_QUEUES);
	   if(!dev->queuelist) {
	      up(&dev->struct_sem);
	      drm_free(queue, sizeof(*queue), DRM_MEM_QUEUES);
	      printk("out of memory\n");
	      return -ENOMEM;
	   }  
	}
	   
	memset(queue, 0, sizeof(*queue));
	atomic_set(&queue->use_count, 1);
   	atomic_set(&queue->finalization,  0);
	atomic_set(&queue->block_count,   0);
	atomic_set(&queue->block_read,    0);
	atomic_set(&queue->block_write,   0);
	atomic_set(&queue->total_queued,  0);
	atomic_set(&queue->total_flushed, 0);
	atomic_set(&queue->total_locks,   0);

	init_waitqueue_head(&queue->write_queue);
	init_waitqueue_head(&queue->read_queue);
	init_waitqueue_head(&queue->flush_queue);

	queue->flags = 0;

	drm_waitlist_create(&queue->waitlist, dev->dma->buf_count);
   
   	dev->queue_slots = 1;
   	dev->queuelist[DRM_KERNEL_CONTEXT] = queue;
   	dev->queue_count--;
	
	up(&dev->struct_sem);
	printk("%d (new)\n", dev->queue_count - 1);
	return DRM_KERNEL_CONTEXT;
}

static int mga_dma_initialize(drm_device_t *dev, drm_mga_init_t *init) {
	drm_mga_private_t *dev_priv;
	drm_map_t *prim_map = NULL;
	drm_map_t *sarea_map = NULL;
	int temp;


	dev_priv = drm_alloc(sizeof(drm_mga_private_t), DRM_MEM_DRIVER);
	if(dev_priv == NULL) return -ENOMEM;
	dev->dev_private = (void *) dev_priv;

	printk("dev_private\n");

	memset(dev_priv, 0, sizeof(drm_mga_private_t));
      	atomic_set(&dev_priv->pending_bufs, 0);

	if((init->reserved_map_idx >= dev->map_count) ||
	   (init->buffer_map_idx >= dev->map_count)) {
		mga_dma_cleanup(dev);
		printk("reserved_map or buffer_map are invalid\n");
		return -EINVAL;
	}
   
	if(mga_alloc_kernel_queue(dev) != DRM_KERNEL_CONTEXT) {
	   mga_dma_cleanup(dev);
	   DRM_ERROR("Kernel context queue not present\n");
	}

	dev_priv->reserved_map_idx = init->reserved_map_idx;
	dev_priv->buffer_map_idx = init->buffer_map_idx;
	sarea_map = dev->maplist[0];
	dev_priv->sarea_priv = (drm_mga_sarea_t *) 
		((u8 *)sarea_map->handle + 
		 init->sarea_priv_offset);
	printk("sarea_priv\n");

	/* Scale primary size to the next page */
	dev_priv->primary_size = ((init->primary_size + PAGE_SIZE - 1) / 
				  PAGE_SIZE) * PAGE_SIZE;
	dev_priv->warp_ucode_size = init->warp_ucode_size;
	dev_priv->chipset = init->chipset;
	dev_priv->fbOffset = init->fbOffset;
	dev_priv->backOffset = init->backOffset;
	dev_priv->depthOffset = init->depthOffset;
	dev_priv->textureOffset = init->textureOffset;
	dev_priv->textureSize = init->textureSize;
	dev_priv->cpp = init->cpp;
	dev_priv->sgram = init->sgram;
	dev_priv->stride = init->stride;

	dev_priv->frontOrg = init->frontOrg;
	dev_priv->backOrg = init->backOrg;
	dev_priv->depthOrg = init->depthOrg;
	dev_priv->mAccess = init->mAccess;
	
   
	printk("memcpy\n");
	memcpy(&dev_priv->WarpIndex, &init->WarpIndex, 
	       sizeof(mgaWarpIndex) * MGA_MAX_WARP_PIPES);
	printk("memcpy done\n");
	prim_map = dev->maplist[init->reserved_map_idx];
	dev_priv->prim_phys_head = dev->agp->base + init->reserved_map_agpstart;
	temp = init->warp_ucode_size + dev_priv->primary_size;
	temp = ((temp + PAGE_SIZE - 1) / 
		PAGE_SIZE) * PAGE_SIZE;
	printk("temp : %x\n", temp);
	printk("dev->agp->base: %lx\n", dev->agp->base);
	printk("init->reserved_map_agpstart: %x\n", init->reserved_map_agpstart);


	dev_priv->ioremap = drm_ioremap(dev->agp->base + init->reserved_map_agpstart, 
					temp);
	if(dev_priv->ioremap == NULL) {
		printk("Ioremap failed\n");
		mga_dma_cleanup(dev);
		return -ENOMEM;
	}



	dev_priv->prim_head = (u32 *)dev_priv->ioremap;
	printk("dev_priv->prim_head : %p\n", dev_priv->prim_head);
	dev_priv->current_dma_ptr = dev_priv->prim_head;
	dev_priv->prim_num_dwords = 0;
	dev_priv->prim_max_dwords = dev_priv->primary_size / 4;
   
	printk("dma initialization\n");

	/* Private is now filled in, initialize the hardware */
	{
		PRIMLOCALS;
		PRIMRESET( dev_priv );
		PRIMGETPTR( dev_priv );
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DWGSYNC, 0);
		PRIMOUTREG(MGAREG_SOFTRAP, 0);
		PRIMADVANCE( dev_priv );

		/* Poll for the first buffer to insure that
		 * the status register will be correct
		 */
	   	printk("phys_head : %lx\n", phys_head);
   
		MGA_WRITE(MGAREG_DWGSYNC, MGA_SYNC_TAG);

		while(MGA_READ(MGAREG_DWGSYNC) != MGA_SYNC_TAG) {
			int i;
			for(i = 0 ; i < 4096; i++) mga_delay();
		}

	   	MGA_WRITE(MGAREG_PRIMADDRESS, phys_head | TT_GENERAL);

		MGA_WRITE(MGAREG_PRIMEND, ((phys_head + num_dwords * 4) | 
					   PDEA_pagpxfer_enable));

		while(MGA_READ(MGAREG_DWGSYNC) == MGA_SYNC_TAG) {
			int i;
			for(i = 0; i < 4096; i++) mga_delay();
		}

	}
   
	printk("dma init was successful\n");
	return 0;
}

int mga_dma_init(struct inode *inode, struct file *filp,
		 unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_mga_init_t init;
   
	copy_from_user_ret(&init, (drm_mga_init_t *)arg, sizeof(init), -EFAULT);
   
	switch(init.func) {
	case MGA_INIT_DMA:
		return mga_dma_initialize(dev, &init);
	case MGA_CLEANUP_DMA:
		return mga_dma_cleanup(dev);
	}

	return -EINVAL;
}

#define MGA_ILOAD_CMD (DC_opcod_iload | DC_atype_rpl |          	\
		       DC_linear_linear | DC_bltmod_bfcol |       	\
		       (0xC << DC_bop_SHIFT) | DC_sgnzero_enable |	\
		       DC_shftzero_enable | DC_clipdis_enable)

static void __mga_iload_small(drm_device_t *dev,
			      drm_buf_t *buf,
			      int use_agp) 
{
   	drm_mga_private_t *dev_priv = dev->dev_private;
   	drm_mga_buf_priv_t *buf_priv = buf->dev_private;
      	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
   	unsigned long address = (unsigned long)buf->bus_address;
	int length = buf->used;
	int y1 = buf_priv->boxes[0].y1;
	int x1 = buf_priv->boxes[0].x1;
	int y2 = buf_priv->boxes[0].y2;
	int x2 = buf_priv->boxes[0].x2;
   	int dstorg = buf_priv->ContextState[MGA_CTXREG_DSTORG];
   	int maccess = buf_priv->ContextState[MGA_CTXREG_MACCESS];
   	PRIMLOCALS;

	PRIMRESET(dev_priv);		
   	PRIMGETPTR(dev_priv);
   
   	PRIMOUTREG(MGAREG_DSTORG, dstorg | use_agp);
   	PRIMOUTREG(MGAREG_MACCESS, maccess);
   	PRIMOUTREG(MGAREG_PITCH, (1 << 15));
   	PRIMOUTREG(MGAREG_YDST, y1 * (x2 - x1));   
   	PRIMOUTREG(MGAREG_LEN, 1);
   	PRIMOUTREG(MGAREG_FXBNDRY, ((x2 - x1) * (y2 - y1) - 1) << 16);
   	PRIMOUTREG(MGAREG_AR0, (x2 - x1) * (y2 - y1) - 1);
   	PRIMOUTREG(MGAREG_AR3, 0);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DWGCTL+MGAREG_MGA_EXEC, MGA_ILOAD_CMD);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_SECADDRESS, address | TT_BLIT);
   	PRIMOUTREG(MGAREG_SECEND, (address + length) | use_agp);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DWGSYNC, 0);
   	PRIMOUTREG(MGAREG_SOFTRAP, 0);
   	PRIMADVANCE(dev_priv);
#if 0
   	/* For now we need to set this in the ioctl */
	sarea_priv->dirty |= MGASAREA_NEW_CONTEXT;
#endif
   	MGA_WRITE(MGAREG_DWGSYNC, MGA_SYNC_TAG);
   	while(MGA_READ(MGAREG_DWGSYNC) != MGA_SYNC_TAG) ;

      	MGA_WRITE(MGAREG_PRIMADDRESS, dev_priv->prim_phys_head | TT_GENERAL);
	MGA_WRITE(MGAREG_PRIMEND, (phys_head + num_dwords * 4) | use_agp);   
}

static void __mga_iload_xy(drm_device_t *dev,
			   drm_buf_t *buf,
			   int use_agp) 
{
      	drm_mga_private_t *dev_priv = dev->dev_private;
   	drm_mga_buf_priv_t *buf_priv = buf->dev_private;
      	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	unsigned long address = (unsigned long)buf->bus_address;
   	int length = buf->used;
	int y1 = buf_priv->boxes[0].y1;
	int x1 = buf_priv->boxes[0].x1;
	int y2 = buf_priv->boxes[0].y2;
	int x2 = buf_priv->boxes[0].x2;
   	int dstorg = buf_priv->ContextState[MGA_CTXREG_DSTORG];
   	int maccess = buf_priv->ContextState[MGA_CTXREG_MACCESS];
   	int pitch = buf_priv->ServerState[MGA_2DREG_PITCH];
   	int width, height;
   	int texperdword = 0;
   	PRIMLOCALS;
   
   	width = (x2 - x1);
   	height = (y2 - y1);
   	switch((maccess & 0x00000003)) {
	 	case 0:
	   	texperdword = 4;
      		break;
	 	case 1:
      		texperdword = 2;
	   	break;
    		case 2:
      		texperdword = 1;
      		break;
    		default:
      		DRM_ERROR("Invalid maccess value passed to __mga_iload_xy\n");
	   	return;  
	}
   
   	x2 = x1 + width;
   	x2 = (x2 + (texperdword - 1)) & ~(texperdword - 1);
   	x1 = (x1 + (texperdword - 1)) & ~(texperdword - 1);
   	width = x2 - x1;
   
	PRIMRESET(dev_priv);		
   	PRIMGETPTR(dev_priv);
   	PRIMOUTREG(MGAREG_DSTORG, dstorg | use_agp);
   	PRIMOUTREG(MGAREG_MACCESS, maccess);
   	PRIMOUTREG(MGAREG_PITCH, pitch);
   	PRIMOUTREG(MGAREG_YDSTLEN, (y1 << 16) | height);
   
   	PRIMOUTREG(MGAREG_FXBNDRY, ((x1+width-1) << 16) | x1);
   	PRIMOUTREG(MGAREG_AR0, width * height - 1);
      	PRIMOUTREG(MGAREG_AR3, 0 );
   	PRIMOUTREG(MGAREG_DWGCTL+MGAREG_MGA_EXEC, MGA_ILOAD_CMD);
	   	   
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_SECADDRESS, address | TT_BLIT);
   	PRIMOUTREG(MGAREG_SECEND, (address + length) | use_agp);
	   
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DMAPAD, 0);
   	PRIMOUTREG(MGAREG_DWGSYNC, 0);
   	PRIMOUTREG(MGAREG_SOFTRAP, 0);
   	PRIMADVANCE(dev_priv);
#if 0
   	/* For now we need to set this in the ioctl */
	sarea_priv->dirty |= MGASAREA_NEW_CONTEXT;
#endif
   	MGA_WRITE(MGAREG_DWGSYNC, MGA_SYNC_TAG);
   	while(MGA_READ(MGAREG_DWGSYNC) != MGA_SYNC_TAG) ;

      	MGA_WRITE(MGAREG_PRIMADDRESS, dev_priv->prim_phys_head | TT_GENERAL);
	MGA_WRITE(MGAREG_PRIMEND, (phys_head + num_dwords * 4) | use_agp);
}

static void mga_dma_dispatch_iload(drm_device_t *dev, drm_buf_t *buf)
{
   	drm_mga_buf_priv_t *buf_priv = buf->dev_private;

	int use_agp = PDEA_pagpxfer_enable;
	int x1 = buf_priv->boxes[0].x1;
	int x2 = buf_priv->boxes[0].x2;
   
   	if((x2 - x1) < 32) {
	   	printk("using iload small\n");
	   	__mga_iload_small(dev, buf, use_agp);
	} else {
	   	printk("using iload xy\n");
	   	__mga_iload_xy(dev, buf, use_agp); 
	}   
}

static void mga_dma_dispatch_vertex(drm_device_t *dev, drm_buf_t *buf)
{
   	drm_mga_private_t *dev_priv = dev->dev_private;
	drm_mga_buf_priv_t *buf_priv = buf->dev_private;
	unsigned long address = (unsigned long)buf->bus_address;
	int length = buf->used;
	int use_agp = PDEA_pagpxfer_enable;
	int i, count;
   	PRIMLOCALS;

	PRIMRESET(dev_priv);
	
	count = buf_priv->nbox;
	if (count == 0) 
		count = 1;

	mgaEmitState( dev_priv, buf_priv );

	for (i = 0 ; i < count ; i++) {		
		if (i < buf_priv->nbox)
			mgaEmitClipRect( dev_priv, &buf_priv->boxes[i] );

		PRIMGETPTR(dev_priv);
		PRIMOUTREG( MGAREG_DMAPAD, 0);
		PRIMOUTREG( MGAREG_DMAPAD, 0);
		PRIMOUTREG( MGAREG_SECADDRESS, address | TT_VERTEX);
		PRIMOUTREG( MGAREG_SECEND, (address + length) | use_agp);

		PRIMOUTREG( MGAREG_DMAPAD, 0);
		PRIMOUTREG( MGAREG_DMAPAD, 0);
		PRIMOUTREG( MGAREG_DWGSYNC, 0);
		PRIMOUTREG( MGAREG_SOFTRAP, 0);
		PRIMADVANCE(dev_priv);
	}

	PRIMGETPTR( dev_priv );

   	MGA_WRITE(MGAREG_DWGSYNC, MGA_SYNC_TAG);
   	while(MGA_READ(MGAREG_DWGSYNC) != MGA_SYNC_TAG) ;

      	MGA_WRITE(MGAREG_PRIMADDRESS, dev_priv->prim_phys_head | TT_GENERAL);
	MGA_WRITE(MGAREG_PRIMEND, (phys_head + num_dwords * 4) | use_agp);
}


/* Used internally for the small buffers generated from client state
 * information. 
 */
static void mga_dma_dispatch_general(drm_device_t *dev, drm_buf_t *buf)
{
   	drm_mga_private_t *dev_priv = dev->dev_private;
	unsigned long address = (unsigned long)buf->bus_address;
	int length = buf->used;
	int use_agp = PDEA_pagpxfer_enable;
   	PRIMLOCALS;

	PRIMRESET(dev_priv);
   	PRIMGETPTR(dev_priv);

	PRIMOUTREG( MGAREG_DMAPAD, 0);
	PRIMOUTREG( MGAREG_DMAPAD, 0);
	PRIMOUTREG( MGAREG_SECADDRESS, address | TT_GENERAL);
	PRIMOUTREG( MGAREG_SECEND, (address + length) | use_agp);

	PRIMOUTREG( MGAREG_DMAPAD, 0);
	PRIMOUTREG( MGAREG_DMAPAD, 0);
      	PRIMOUTREG( MGAREG_DWGSYNC, 0);
   	PRIMOUTREG( MGAREG_SOFTRAP, 0);
   	PRIMADVANCE(dev_priv);

   	MGA_WRITE(MGAREG_DWGSYNC, MGA_SYNC_TAG);
   	while(MGA_READ(MGAREG_DWGSYNC) != MGA_SYNC_TAG) ;

      	MGA_WRITE(MGAREG_PRIMADDRESS, dev_priv->prim_phys_head | TT_GENERAL);
	MGA_WRITE(MGAREG_PRIMEND, (phys_head + num_dwords * 4) | use_agp);
}

/* Frees dispatch lock */
static inline void mga_dma_quiescent(drm_device_t *dev)
{
   	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;

   	while(1) {
	   atomic_inc(&dev_priv->dispatch_lock);
	   if(atomic_read(&dev_priv->dispatch_lock) == 1) {
	      break;
	   } else {
	      atomic_dec(&dev_priv->dispatch_lock);
	   }
	}
	while((MGA_READ(MGAREG_STATUS) & 0x00020001) != 0x00020000) ;
#if 0
   MGA_WRITE(MGAREG_DWGSYNC, MGA_SYNC_TAG);
#endif
   	while(MGA_READ(MGAREG_DWGSYNC) == MGA_SYNC_TAG) ;
   	MGA_WRITE(MGAREG_DWGSYNC, MGA_SYNC_TAG);
	while(MGA_READ(MGAREG_DWGSYNC) != MGA_SYNC_TAG) ;
   	atomic_dec(&dev_priv->dispatch_lock);
}

/* Keeps dispatch lock held */

static inline int mga_dma_is_ready(drm_device_t *dev)
{
	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
   
	atomic_inc(&dev_priv->dispatch_lock);
	if(atomic_read(&dev_priv->dispatch_lock) == 1) {
		/* We got the lock */
		return 1;
	} else {
		atomic_dec(&dev_priv->dispatch_lock);
		return 0;
	}
}

static inline int mga_dma_is_ready_no_hold(drm_device_t *dev)
{
	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
   
	atomic_inc(&dev_priv->dispatch_lock);
	if(atomic_read(&dev_priv->dispatch_lock) == 1) {
		/* We got the lock, but free it */
		atomic_dec(&dev_priv->dispatch_lock);
		return 1;
	} else {
		atomic_dec(&dev_priv->dispatch_lock);
		return 0;
	}
}

static void mga_dma_service(int irq, void *device, struct pt_regs *regs)
{
	drm_device_t	 *dev = (drm_device_t *)device;
	drm_device_dma_t *dma = dev->dma;
   	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;

	atomic_dec(&dev_priv->dispatch_lock);   	
	atomic_inc(&dev->total_irq);
      	MGA_WRITE(MGAREG_ICLEAR, 0xfa7);

				/* Free previous buffer */
   	if (test_and_set_bit(0, &dev->dma_flag)) {
	   	atomic_inc(&dma->total_missed_free);
	   	return;
	}
   	if (dma->this_buffer) {
	   	drm_free_buffer(dev, dma->this_buffer);
	   	dma->this_buffer = NULL;
	}
   	clear_bit(0, &dev->dma_flag);

				/* Dispatch new buffer */
   	queue_task(&dev->tq, &tq_immediate);
   	mark_bh(IMMEDIATE_BH);

}

/* Only called by mga_dma_schedule. */
static int mga_do_dma(drm_device_t *dev, int locked)
{
	drm_buf_t	 *buf;
	int		 retcode = 0;
	drm_device_dma_t *dma = dev->dma;
      	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
	drm_mga_buf_priv_t *buf_priv;

   	printk("mga_do_dma\n");
	if (test_and_set_bit(0, &dev->dma_flag)) {
		atomic_inc(&dma->total_missed_dma);
		return -EBUSY;
	}
	
	if (!dma->next_buffer) {
		DRM_ERROR("No next_buffer\n");
		clear_bit(0, &dev->dma_flag);
		return -EINVAL;
	}

	buf	= dma->next_buffer;

	printk("context %d, buffer %d\n", buf->context, buf->idx);

	if (buf->list == DRM_LIST_RECLAIM) {
		drm_clear_next_buffer(dev);
		drm_free_buffer(dev, buf);
		clear_bit(0, &dev->dma_flag);
		return -EINVAL;
	}

	if (!buf->used) {
		DRM_ERROR("0 length buffer\n");
		drm_clear_next_buffer(dev);
		drm_free_buffer(dev, buf);
		clear_bit(0, &dev->dma_flag);
		return 0;
	}
	
	if (mga_dma_is_ready(dev) == 0) {
		clear_bit(0, &dev->dma_flag);
		return -EBUSY;
	}
   
	/* Always hold the hardware lock while dispatching.
	 */
	if (!locked && !drm_lock_take(&dev->lock.hw_lock->lock,
				      DRM_KERNEL_CONTEXT)) {
		atomic_inc(&dma->total_missed_lock);
		clear_bit(0, &dev->dma_flag);
		atomic_dec(&dev_priv->dispatch_lock);
		return -EBUSY;
	}

   	dma->next_queue	 = dev->queuelist[DRM_KERNEL_CONTEXT];
	drm_clear_next_buffer(dev);
	buf->pending	 = 1;
	buf->waiting	 = 0;
	buf->list	 = DRM_LIST_PEND;

	buf_priv = buf->dev_private;

   	printk("dispatch!\n");
	switch (buf_priv->dma_type) {
	case MGA_DMA_GENERAL:
		mga_dma_dispatch_general(dev, buf);
		break;
	case MGA_DMA_VERTEX:
		mga_dma_dispatch_vertex(dev, buf);
		break;
/*  	case MGA_DMA_SETUP: */
/*  		mga_dma_dispatch_setup(dev, address, length); */
/*  		break; */
	case MGA_DMA_ILOAD:
		mga_dma_dispatch_iload(dev, buf);
		break;
	default:
		printk("bad buffer type %x in dispatch\n", buf_priv->dma_type);
		break;
	}
   	atomic_dec(&dev_priv->pending_bufs);

	drm_free_buffer(dev, dma->this_buffer);
	dma->this_buffer = buf;

	atomic_add(buf->used, &dma->total_bytes);
	atomic_inc(&dma->total_dmas);

	if (!locked) {
		if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}

	clear_bit(0, &dev->dma_flag);
   
   if(!atomic_read(&dev_priv->pending_bufs)) {
      wake_up_interruptible(&dev->queuelist[DRM_KERNEL_CONTEXT]->flush_queue);
   }
   
#if 0
   wake_up_interruptible(&dev->lock.lock_queue);
#endif
   
	/* We hold the dispatch lock until the interrupt handler
	 * frees it
	 */
	return retcode;
}

static void mga_dma_schedule_timer_wrapper(unsigned long dev)
{
	mga_dma_schedule((drm_device_t *)dev, 0);
}

static void mga_dma_schedule_tq_wrapper(void *dev)
{
	mga_dma_schedule(dev, 0);
}

int mga_dma_schedule(drm_device_t *dev, int locked)
{
	drm_queue_t	 *q;
	drm_buf_t	 *buf;
	int		 retcode   = 0;
	int		 processed = 0;
	int		 missed;
	int		 expire	   = 20;
	drm_device_dma_t *dma	   = dev->dma;

      	printk("mga_dma_schedule\n");

	if (test_and_set_bit(0, &dev->interrupt_flag)) {
				/* Not reentrant */
		atomic_inc(&dma->total_missed_sched);
		return -EBUSY;
	}
	missed = atomic_read(&dma->total_missed_sched);

again:
	/* There is only one queue:
	 */
	if (!dma->next_buffer && DRM_WAITCOUNT(dev, DRM_KERNEL_CONTEXT)) {
		q   = dev->queuelist[DRM_KERNEL_CONTEXT];
		buf = drm_waitlist_get(&q->waitlist);
		dma->next_buffer = buf;
		dma->next_queue	 = q;
		if (buf && buf->list == DRM_LIST_RECLAIM) {
			drm_clear_next_buffer(dev);
			drm_free_buffer(dev, buf);
		}
	}

	if (dma->next_buffer) {
		if (!(retcode = mga_do_dma(dev, locked))) 
			++processed;
	}

	/* Try again if we succesfully dispatched a buffer, or if someone 
	 * tried to schedule while we were working.
	 */
	if (--expire) {
		if (missed != atomic_read(&dma->total_missed_sched)) {
			atomic_inc(&dma->total_lost);
		   	if (mga_dma_is_ready_no_hold(dev)) 
				goto again;
		}

		if (processed && mga_dma_is_ready_no_hold(dev)) {
			atomic_inc(&dma->total_lost);
			processed = 0;
			goto again;
		}
	}
	
	clear_bit(0, &dev->interrupt_flag);
	
	return retcode;
}

int mga_irq_install(drm_device_t *dev, int irq)
{
	int retcode;

	if (!irq)     return -EINVAL;
	
	down(&dev->struct_sem);
	if (dev->irq) {
		up(&dev->struct_sem);
		return -EBUSY;
	}
	dev->irq = irq;
	up(&dev->struct_sem);
	
	printk("install irq handler %d\n", irq);

	dev->context_flag     = 0;
	dev->interrupt_flag   = 0;
	dev->dma_flag	      = 0;
	dev->dma->next_buffer = NULL;
	dev->dma->next_queue  = NULL;
	dev->dma->this_buffer = NULL;
	dev->tq.next	      = NULL;
	dev->tq.sync	      = 0;
	dev->tq.routine	      = mga_dma_schedule_tq_wrapper;
	dev->tq.data	      = dev;

				/* Before installing handler */
	MGA_WRITE(MGAREG_ICLEAR, 0xfa7);
	MGA_WRITE(MGAREG_IEN, 0);

   				/* Install handler */
	if ((retcode = request_irq(dev->irq,
				   mga_dma_service,
				   0,
				   dev->devname,
				   dev))) {
		down(&dev->struct_sem);
		dev->irq = 0;
		up(&dev->struct_sem);
		return retcode;
	}

				/* After installing handler */
   	MGA_WRITE(MGAREG_ICLEAR, 0xfa7);
	MGA_WRITE(MGAREG_IEN, 0x00000001);

	return 0;
}

int mga_irq_uninstall(drm_device_t *dev)
{
	int irq;

	down(&dev->struct_sem);
	irq	 = dev->irq;
	dev->irq = 0;
	up(&dev->struct_sem);
	
	if (!irq) return -EINVAL;
	
	printk("remove irq handler %d\n", irq);

      	MGA_WRITE(MGAREG_ICLEAR, 0xfa7);
	MGA_WRITE(MGAREG_IEN, 0);
   	MGA_WRITE(MGAREG_ICLEAR, 0xfa7);

	free_irq(irq, dev);

	return 0;
}


int mga_control(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_control_t	ctl;
   
	copy_from_user_ret(&ctl, (drm_control_t *)arg, sizeof(ctl), -EFAULT);
	
	switch (ctl.func) {
	case DRM_INST_HANDLER:
		return mga_irq_install(dev, ctl.irq);
	case DRM_UNINST_HANDLER:
		return mga_irq_uninstall(dev);
	default:
		return -EINVAL;
	}
}

int mga_flush_queue(drm_device_t *dev)
{
   	DECLARE_WAITQUEUE(entry, current);
	drm_queue_t	  *q = dev->queuelist[DRM_KERNEL_CONTEXT];
   	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
   	int ret = 0;
   	
   	printk("mga_flush_queue\n");
   	if(atomic_read(&dev_priv->pending_bufs) != 0) {
	   current->state = TASK_INTERRUPTIBLE;
	   add_wait_queue(&q->flush_queue, &entry);
	   for (;;) {
	      	if (!atomic_read(&dev_priv->pending_bufs)) break;
	      	printk("Calling schedule from flush_queue : %d\n",
		       atomic_read(&dev_priv->pending_bufs));
	      	mga_dma_schedule(dev, 1);
	      	schedule();
	      	if (signal_pending(current)) {
		   	ret = -EINTR; /* Can't restart */
		   	break;
		}
	   }
	   printk("Exited out of schedule from flush_queue\n");
	   current->state = TASK_RUNNING;
	   remove_wait_queue(&q->flush_queue, &entry);
	}
   
   	return ret;
}

int mga_lock(struct inode *inode, struct file *filp, unsigned int cmd,
	       unsigned long arg)
{
	drm_file_t	  *priv	  = filp->private_data;
	drm_device_t	  *dev	  = priv->dev;
	DECLARE_WAITQUEUE(entry, current);
	int		  ret	= 0;
	drm_lock_t	  lock;

	copy_from_user_ret(&lock, (drm_lock_t *)arg, sizeof(lock), -EFAULT);

	if (lock.context == DRM_KERNEL_CONTEXT) {
		DRM_ERROR("Process %d using kernel context %d\n",
			  current->pid, lock.context);
		return -EINVAL;
	}

   	printk("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
		  lock.context, current->pid, dev->lock.hw_lock->lock,
		  lock.flags);


	if (lock.context < 0) {
		return -EINVAL;
	}

	/* Only one queue:
	 */

	if (!ret) {
		add_wait_queue(&dev->lock.lock_queue, &entry);
		for (;;) {
			if (!dev->lock.hw_lock) {
				/* Device has been unregistered */
				ret = -EINTR;
				break;
			}
			if (drm_lock_take(&dev->lock.hw_lock->lock,
					  lock.context)) {
				dev->lock.pid	    = current->pid;
				dev->lock.lock_time = jiffies;
				atomic_inc(&dev->total_locks);
				break;	/* Got lock */
			}
			
				/* Contention */
			atomic_inc(&dev->total_sleeps);
			current->state = TASK_INTERRUPTIBLE;
		   	current->policy |= SCHED_YIELD;
		   	printk("Calling lock schedule\n");
			schedule();
			if (signal_pending(current)) {
				ret = -ERESTARTSYS;
				break;
			}
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&dev->lock.lock_queue, &entry);
	}
	
	if (!ret) {
		if (lock.flags & _DRM_LOCK_QUIESCENT) {
		   	printk("_DRM_LOCK_QUIESCENT\n");
		   	ret = mga_flush_queue(dev);
		   	if(ret != 0) {
			   drm_lock_free(dev, &dev->lock.hw_lock->lock,
					 lock.context);
			} else {
			   mga_dma_quiescent(dev);
			}
		}
	}
	printk("%d %s\n", lock.context, ret ? "interrupted" : "has lock");
	return ret;
}
		
