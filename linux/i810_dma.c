/* i810_dma.c -- DMA support for the i810 -*- linux-c -*-
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
 *          Keith Whitwell <keithw@precisioninsight.com>
 *
 * $XFree86$
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include "i810_drv.h"

#include <linux/interrupt.h>	/* For task queue support */

#define I810_BUF_FREE		1
#define I810_BUF_USED		0

#define I810_REG(reg)		2
#define I810_BASE(reg)		((unsigned long) \
				dev->maplist[I810_REG(reg)]->handle)
#define I810_ADDR(reg)		(I810_BASE(reg) + reg)
#define I810_DEREF(reg)		*(__volatile__ int *)I810_ADDR(reg)
#define I810_READ(reg)		I810_DEREF(reg)
#define I810_WRITE(reg,val) 	do { I810_DEREF(reg) = val; } while (0)
#define I810_DEREF16(reg)	*(__volatile__ u16 *)I810_ADDR(reg)
#define I810_READ16(reg)	I810_DEREF16(reg)
#define I810_WRITE16(reg,val)	do { I810_DEREF16(reg) = val; } while (0)

#define RING_LOCALS	unsigned int outring, ringmask; volatile char *virt;

#define BEGIN_LP_RING(n) do {				\
	if (I810_VERBOSE)				\
		DRM_DEBUG("BEGIN_LP_RING(%d) in %s\n",	\
			  n, __FUNCTION__);		\
	if (dev_priv->ring.space < n*4) 		\
		i810_wait_ring(dev, n*4);		\
	dev_priv->ring.space -= n*4;			\
	outring = dev_priv->ring.tail;			\
	ringmask = dev_priv->ring.tail_mask;		\
	virt = dev_priv->ring.virtual_start;		\
} while (0)

#define ADVANCE_LP_RING() do {					\
	if (I810_VERBOSE) DRM_DEBUG("ADVANCE_LP_RING\n");	\
	dev_priv->ring.tail = outring;				\
	I810_WRITE(LP_RING + RING_TAIL, outring);		\
} while(0)

#define OUT_RING(n) do {						\
	if (I810_VERBOSE) DRM_DEBUG("   OUT_RING %x\n", (int)(n));	\
	*(volatile unsigned int *)(virt + outring) = n;			\
	outring += 4;							\
	outring &= ringmask;						\
} while (0);

static inline void i810_print_status_page(drm_device_t *dev)
{
   	drm_device_dma_t *dma = dev->dma;
      	drm_i810_private_t *dev_priv = dev->dev_private;
	u32 *temp = (u32 *)dev_priv->hw_status_page;
   	int i;

   	DRM_DEBUG(  "hw_status: Interrupt Status : %x\n", temp[0]);
   	DRM_DEBUG(  "hw_status: LpRing Head ptr : %x\n", temp[1]);
   	DRM_DEBUG(  "hw_status: IRing Head ptr : %x\n", temp[2]);
      	DRM_DEBUG(  "hw_status: Reserved : %x\n", temp[3]);
   	DRM_DEBUG(  "hw_status: Driver Counter : %d\n", temp[5]);
   	for(i = 6; i < dma->buf_count + 6; i++) {
	   	DRM_DEBUG(  "buffer status idx : %d used: %d\n", i - 6, temp[i]);
	}
}

static drm_buf_t *i810_freelist_get(drm_device_t *dev)
{
   	drm_device_dma_t *dma = dev->dma;
	int		 i;
   	int 		 used;
   
	/* Linear search might not be the best solution */

   	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
		/* In use is already a pointer */
	   	used = cmpxchg(buf_priv->in_use, I810_BUF_FREE, 
			       I810_BUF_USED);
	   	if(used == I810_BUF_FREE) {
			return buf;
		}
	}
   	return NULL;
}

/* This should only be called if the buffer is not sent to the hardware
 * yet, the hardware updates in use for us once its on the ring buffer.
 */

static int i810_freelist_put(drm_device_t *dev, drm_buf_t *buf)
{
   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
   	int used;
   
   	/* In use is already a pointer */
   	used = cmpxchg(buf_priv->in_use, I810_BUF_USED, I810_BUF_FREE);
   	if(used != I810_BUF_USED) {
	   	DRM_ERROR("Freeing buffer thats not in use : %d\n", buf->idx);
	   	return -EINVAL;
	}
   
   	return 0;
}

static int i810_dma_get_buffers(drm_device_t *dev, drm_dma_t *d)
{
	int		  i;
	drm_buf_t	  *buf;

	for (i = d->granted_count; i < d->request_count; i++) {
		buf = i810_freelist_get(dev);
		if (!buf) break;
		buf->pid     = current->pid;
		copy_to_user_ret(&d->request_indices[i],
				 &buf->idx,
				 sizeof(buf->idx),
				 -EFAULT);
		copy_to_user_ret(&d->request_sizes[i],
				 &buf->total,
				 sizeof(buf->total),
				 -EFAULT);
		++d->granted_count;
	}
	return 0;
}

static unsigned long i810_alloc_page(drm_device_t *dev)
{
	unsigned long address;
   
	address = __get_free_page(GFP_KERNEL);
	if(address == 0UL) 
		return 0;
	
	atomic_inc(&mem_map[MAP_NR((void *) address)].count);
	set_bit(PG_locked, &mem_map[MAP_NR((void *) address)].flags);
   
	return address;
}

static void i810_free_page(drm_device_t *dev, unsigned long page)
{
	if(page == 0UL) 
		return;
	
	atomic_dec(&mem_map[MAP_NR((void *) page)].count);
	clear_bit(PG_locked, &mem_map[MAP_NR((void *) page)].flags);
	wake_up(&mem_map[MAP_NR((void *) page)].wait);
	free_page(page);
	return;
}

static int i810_dma_cleanup(drm_device_t *dev)
{
	if(dev->dev_private) {
	   	drm_i810_private_t *dev_priv = 
	     		(drm_i810_private_t *) dev->dev_private;
	   
	   	if(dev_priv->ring.virtual_start) {
		   	drm_ioremapfree((void *) dev_priv->ring.virtual_start,
					dev_priv->ring.Size);
		}
	   	if(dev_priv->hw_status_page != 0UL) {
		   	i810_free_page(dev, dev_priv->hw_status_page);
		   	/* Need to rewrite hardware status page */
		   	I810_WRITE(0x02080, 0x1ffff000);
		}
	   	drm_free(dev->dev_private, sizeof(drm_i810_private_t), 
			 DRM_MEM_DRIVER);
	   	dev->dev_private = NULL;
	}
   	return 0;
}

static int i810_wait_ring(drm_device_t *dev, int n)
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
   	drm_i810_ring_buffer_t *ring = &(dev_priv->ring);
   	int iters = 0;
   	unsigned long end;

	end = jiffies + (HZ*3);
   	while (ring->space < n) {
	   	int i;
	
	   	ring->head = I810_READ(LP_RING + RING_HEAD) & HEAD_ADDR;
	   	ring->space = ring->head - (ring->tail+8);
	   
	   	if (ring->space < 0) ring->space += ring->Size;
	   
	   	iters++;
		if((signed)(end - jiffies) <= 0) {
		   	DRM_ERROR("space: %d wanted %d\n", ring->space, n);
		   	DRM_ERROR("lockup\n");
		   	goto out_wait_ring;
		}

	   	for (i = 0 ; i < 2000 ; i++) ;
	}

out_wait_ring:   
   	return iters;
}

static void i810_kernel_lost_context(drm_device_t *dev)
{
      	drm_i810_private_t *dev_priv = dev->dev_private;
   	drm_i810_ring_buffer_t *ring = &(dev_priv->ring);
      
   	ring->head = I810_READ(LP_RING + RING_HEAD) & HEAD_ADDR;
     	ring->tail = I810_READ(LP_RING + RING_TAIL);
     	ring->space = ring->head - (ring->tail+8);
     	if (ring->space < 0) ring->space += ring->Size;
}

static int i810_freelist_init(drm_device_t *dev)
{
      	drm_device_dma_t *dma = dev->dma;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
   	u8 *hw_status = (u8 *)dev_priv->hw_status_page;
   	int i;
   	int my_idx = 24;
   
   	if(dma->buf_count > 1019) {
	   	/* Not enough space in the status page for the freelist */
	   	return -EINVAL;
	}

   	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
	   
	   	buf_priv->in_use = hw_status + my_idx;
	   	DRM_DEBUG("buf_priv->in_use : %p\n", buf_priv->in_use);
	   	*buf_priv->in_use = I810_BUF_FREE;
	   	buf_priv->my_use_idx = my_idx;
	   	my_idx += 4;
	}
	return 0;
}

static int i810_dma_initialize(drm_device_t *dev, 
			       drm_i810_private_t *dev_priv,
			       drm_i810_init_t *init)
{
	drm_map_t *sarea_map;

   	dev->dev_private = (void *) dev_priv;
   	memset(dev_priv, 0, sizeof(drm_i810_private_t));

   	if (init->ring_map_idx >= dev->map_count ||
	    init->buffer_map_idx >= dev->map_count) {
	   	i810_dma_cleanup(dev);
	   	DRM_ERROR("ring_map or buffer_map are invalid\n");
	   	return -EINVAL;
	}
   
   	dev_priv->ring_map_idx = init->ring_map_idx;
   	dev_priv->buffer_map_idx = init->buffer_map_idx;
	sarea_map = dev->maplist[0];
	dev_priv->sarea_priv = (drm_i810_sarea_t *) 
		((u8 *)sarea_map->handle + 
		 init->sarea_priv_offset);

   	atomic_set(&dev_priv->flush_done, 0);
	init_waitqueue_head(&dev_priv->flush_queue);
   	
   	dev_priv->ring.Start = init->ring_start;
   	dev_priv->ring.End = init->ring_end;
   	dev_priv->ring.Size = init->ring_size;
   	dev_priv->ring.virtual_start = drm_ioremap(dev->agp->base + 
						   init->ring_start, 
						   init->ring_size);
   	dev_priv->ring.tail_mask = dev_priv->ring.Size - 1;
   
   	if (dev_priv->ring.virtual_start == NULL) {
	   	i810_dma_cleanup(dev);
	   	DRM_ERROR("can not ioremap virtual address for"
			  " ring buffer\n");
	   	return -ENOMEM;
	}
   
   	/* Program Hardware Status Page */
   	dev_priv->hw_status_page = i810_alloc_page(dev);
   	memset((void *) dev_priv->hw_status_page, 0, PAGE_SIZE);
   	if(dev_priv->hw_status_page == 0UL) {
		i810_dma_cleanup(dev);
		DRM_ERROR("Can not allocate hardware status page\n");
		return -ENOMEM;
	}
   	DRM_DEBUG("hw status page @ %lx\n", dev_priv->hw_status_page);
   
   	I810_WRITE(0x02080, virt_to_bus((void *)dev_priv->hw_status_page));
   	DRM_DEBUG("Enabled hardware status page\n");
   
   	/* Now we need to init our freelist */
   	if(i810_freelist_init(dev) != 0) {
	   	i810_dma_cleanup(dev);
	   	DRM_ERROR("Not enough space in the status page for"
			  " the freelist\n");
	   	return -ENOMEM;
	}
   	return 0;
}

int i810_dma_init(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
   	drm_file_t *priv = filp->private_data;
   	drm_device_t *dev = priv->dev;
   	drm_i810_private_t *dev_priv;
   	drm_i810_init_t init;
   	int retcode = 0;
	
   	copy_from_user_ret(&init, (drm_i810_init_t *)arg, 
			   sizeof(init), -EFAULT);
	
   	switch(init.func) {
	 	case I810_INIT_DMA:
	   		dev_priv = drm_alloc(sizeof(drm_i810_private_t), 
					     DRM_MEM_DRIVER);
	   		if(dev_priv == NULL) return -ENOMEM;
	   		retcode = i810_dma_initialize(dev, dev_priv, &init);
	   	break;
	 	case I810_CLEANUP_DMA:
	   		retcode = i810_dma_cleanup(dev);
	   	break;
	 	default:
	   		retcode = -EINVAL;
	   	break;
	}
   
   	return retcode;
}

static void i810_dma_dispatch_general(drm_device_t *dev, drm_buf_t *buf,
				      int used )
{
      	drm_i810_private_t *dev_priv = dev->dev_private;
   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
	unsigned long address = (unsigned long)buf->bus_address;
	unsigned long start = address - dev->agp->base;
   	RING_LOCALS;

   	dev_priv->counter++;
   	DRM_DEBUG(  "dispatch counter : %ld\n", dev_priv->counter);
   	DRM_DEBUG(  "i810_dma_dispatch\n");
   	DRM_DEBUG(  "start : 0x%lx\n", start);
	DRM_DEBUG(  "used : 0x%x\n", used);
   	DRM_DEBUG(  "start + used - 4 : 0x%lx\n", start + used - 4);
   	i810_kernel_lost_context(dev);

   	BEGIN_LP_RING(10);
   	OUT_RING( CMD_OP_BATCH_BUFFER );
   	OUT_RING( start | BB1_PROTECTED );
   	OUT_RING( start + used - 4 );
      	OUT_RING( CMD_STORE_DWORD_IDX );
   	OUT_RING( 20 );
   	OUT_RING( dev_priv->counter );
   	OUT_RING( CMD_STORE_DWORD_IDX );
   	OUT_RING( buf_priv->my_use_idx );
   	OUT_RING( I810_BUF_FREE );   
      	OUT_RING( CMD_REPORT_HEAD );
      	ADVANCE_LP_RING();
}

static void i810_dma_dispatch_vertex(drm_device_t *dev, 
				     drm_buf_t *buf,
				     int discard,
				     int used)
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
   	drm_i810_sarea_t *sarea_priv = dev_priv->sarea_priv;
   	drm_clip_rect_t *box = sarea_priv->boxes;
   	int nbox = sarea_priv->nbox;
	unsigned long address = (unsigned long)buf->bus_address;
	unsigned long start = address - dev->agp->base;     
	int i = 0;
   	RING_LOCALS;

   
   	if (nbox > I810_NR_SAREA_CLIPRECTS) 
		nbox = I810_NR_SAREA_CLIPRECTS;
   
   	DRM_DEBUG("dispatch vertex addr 0x%lx, used 0x%x nbox %d\n", 
		  address, used, nbox);

   	dev_priv->counter++;
   	DRM_DEBUG(  "dispatch counter : %ld\n", dev_priv->counter);
   	DRM_DEBUG(  "i810_dma_dispatch\n");
   	DRM_DEBUG(  "start : %lx\n", start);
	DRM_DEBUG(  "used : %d\n", used);
   	DRM_DEBUG(  "start + used - 4 : %ld\n", start + used - 4);
   	i810_kernel_lost_context(dev);

	if (used) {
		do {
			if (i < nbox) {
				BEGIN_LP_RING(4);
				OUT_RING( GFX_OP_SCISSOR | SC_UPDATE_SCISSOR | 
					  SC_ENABLE );
				OUT_RING( GFX_OP_SCISSOR_INFO );
				OUT_RING( box[i].x1 | (box[i].y1 << 16) );
				OUT_RING( (box[i].x2-1) | ((box[i].y2-1)<<16) );
				ADVANCE_LP_RING();
			}
			
			BEGIN_LP_RING(4);
			OUT_RING( CMD_OP_BATCH_BUFFER );
			OUT_RING( start | BB1_PROTECTED );
			OUT_RING( start + used - 4 );
			OUT_RING( 0 );
			ADVANCE_LP_RING();
			
		} while (++i < nbox);
	}

	BEGIN_LP_RING(10);
	OUT_RING( CMD_STORE_DWORD_IDX );
	OUT_RING( 20 );
	OUT_RING( dev_priv->counter );
	OUT_RING( 0 );

	if (discard) {
		OUT_RING( CMD_STORE_DWORD_IDX );
		OUT_RING( buf_priv->my_use_idx );
		OUT_RING( I810_BUF_FREE );
		OUT_RING( 0 );
	}

      	OUT_RING( CMD_REPORT_HEAD );
	OUT_RING( 0 );
   	ADVANCE_LP_RING();
}


/* Interrupts are only for flushing */
static void i810_dma_service(int irq, void *device, struct pt_regs *regs)
{
	drm_device_t	 *dev = (drm_device_t *)device;
   	u16 temp;
   
	atomic_inc(&dev->total_irq);
      	temp = I810_READ16(I810REG_INT_IDENTITY_R);
   	temp = temp & ~(0x6000);
   	if(temp != 0) I810_WRITE16(I810REG_INT_IDENTITY_R, 
				   temp); /* Clear all interrupts */
   
   	queue_task(&dev->tq, &tq_immediate);
   	mark_bh(IMMEDIATE_BH);
}

static void i810_dma_task_queue(void *device)
{
	drm_device_t *dev = (drm_device_t *) device;
      	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;

   	atomic_set(&dev_priv->flush_done, 1);
   	wake_up_interruptible(&dev_priv->flush_queue);
}

int i810_irq_install(drm_device_t *dev, int irq)
{
	int retcode;
	u16 temp;
   
	if (!irq)     return -EINVAL;
	
	down(&dev->struct_sem);
	if (dev->irq) {
		up(&dev->struct_sem);
		return -EBUSY;
	}
	dev->irq = irq;
	up(&dev->struct_sem);
	
   	DRM_DEBUG(  "Interrupt Install : %d\n", irq);
	DRM_DEBUG("%d\n", irq);

	dev->context_flag     = 0;
	dev->interrupt_flag   = 0;
	dev->dma_flag	      = 0;
	
	dev->dma->next_buffer = NULL;
	dev->dma->next_queue  = NULL;
	dev->dma->this_buffer = NULL;

	dev->tq.next	      = NULL;
	dev->tq.sync	      = 0;
	dev->tq.routine	      = i810_dma_task_queue;
	dev->tq.data	      = dev;

				/* Before installing handler */
   	temp = I810_READ16(I810REG_HWSTAM);
   	temp = temp & 0x6000;
   	I810_WRITE16(I810REG_HWSTAM, temp);
   	
      	temp = I810_READ16(I810REG_INT_MASK_R);
   	temp = temp & 0x6000;
   	I810_WRITE16(I810REG_INT_MASK_R, temp); /* Unmask interrupts */
   	temp = I810_READ16(I810REG_INT_ENABLE_R);
   	temp = temp & 0x6000;
      	I810_WRITE16(I810REG_INT_ENABLE_R, temp); /* Disable all interrupts */

				/* Install handler */
	if ((retcode = request_irq(dev->irq,
				   i810_dma_service,
				   0,
				   dev->devname,
				   dev))) {
		down(&dev->struct_sem);
		dev->irq = 0;
		up(&dev->struct_sem);
		return retcode;
	}
   	temp = I810_READ16(I810REG_INT_ENABLE_R);
   	temp = temp & 0x6000;
   	temp = temp | 0x0003;
   	I810_WRITE16(I810REG_INT_ENABLE_R, 
		     temp); /* Enable bp & user interrupts */
	return 0;
}

int i810_irq_uninstall(drm_device_t *dev)
{
	int irq;
   	u16 temp;

	down(&dev->struct_sem);
	irq	 = dev->irq;
	dev->irq = 0;
	up(&dev->struct_sem);
	
	if (!irq) return -EINVAL;

   	DRM_DEBUG(  "Interrupt UnInstall: %d\n", irq);	
	DRM_DEBUG("%d\n", irq);
   
   	temp = I810_READ16(I810REG_INT_IDENTITY_R);
   	temp = temp & ~(0x6000);
   	if(temp != 0) I810_WRITE16(I810REG_INT_IDENTITY_R, 
				   temp); /* Clear all interrupts */
   
   	temp = I810_READ16(I810REG_INT_ENABLE_R);
   	temp = temp & 0x6000;
   	I810_WRITE16(I810REG_INT_ENABLE_R, 
		     temp);                     /* Disable all interrupts */

   	free_irq(irq, dev);

	return 0;
}

int i810_control(struct inode *inode, struct file *filp, unsigned int cmd,
		  unsigned long arg)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_control_t	ctl;
	int		retcode;
   
   	DRM_DEBUG(  "i810_control\n");

	copy_from_user_ret(&ctl, (drm_control_t *)arg, sizeof(ctl), -EFAULT);
	
	switch (ctl.func) {
	case DRM_INST_HANDLER:
		if ((retcode = i810_irq_install(dev, ctl.irq)))
			return retcode;
		break;
	case DRM_UNINST_HANDLER:
		if ((retcode = i810_irq_uninstall(dev)))
			return retcode;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static inline void i810_dma_emit_flush(drm_device_t *dev)
{
   	drm_i810_private_t *dev_priv = dev->dev_private;
   	RING_LOCALS;

   	i810_kernel_lost_context(dev);
   	BEGIN_LP_RING(2);
      	OUT_RING( CMD_REPORT_HEAD );
   	OUT_RING( GFX_OP_USER_INTERRUPT );
      	ADVANCE_LP_RING();
}

static inline void i810_dma_quiescent_emit(drm_device_t *dev)
{
      	drm_i810_private_t *dev_priv = dev->dev_private;
   	RING_LOCALS;

  	i810_kernel_lost_context(dev);
   	BEGIN_LP_RING(4);

   	OUT_RING( INST_PARSER_CLIENT | INST_OP_FLUSH | INST_FLUSH_MAP_CACHE );
   	OUT_RING( CMD_REPORT_HEAD );
   	OUT_RING( GFX_OP_USER_INTERRUPT );
   	OUT_RING( 0 );
   	ADVANCE_LP_RING();
}

static void i810_dma_quiescent(drm_device_t *dev)
{
      	DECLARE_WAITQUEUE(entry, current);
  	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
	unsigned long end;      

   	if(dev_priv == NULL) {
	   	return;
	}
      	atomic_set(&dev_priv->flush_done, 0);
   	current->state = TASK_INTERRUPTIBLE;
   	add_wait_queue(&dev_priv->flush_queue, &entry);
   	end = jiffies + (HZ*3);
   
   	for (;;) {
	      	i810_dma_quiescent_emit(dev);
	   	if (atomic_read(&dev_priv->flush_done) == 1) break;
		if((signed)(end - jiffies) <= 0) {
		   	DRM_ERROR("lockup\n");
		   	break;
		}	   
	      	schedule_timeout(HZ*3);
	      	if (signal_pending(current)) {
		   	break;
		}
	}
   
   	current->state = TASK_RUNNING;
   	remove_wait_queue(&dev_priv->flush_queue, &entry);
   
   	return;
}

static int i810_flush_queue(drm_device_t *dev)
{
   	DECLARE_WAITQUEUE(entry, current);
  	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
	unsigned long end;
   	int ret = 0;      

   	if(dev_priv == NULL) {
	   	return 0;
	}
      	atomic_set(&dev_priv->flush_done, 0);
   	current->state = TASK_INTERRUPTIBLE;
   	add_wait_queue(&dev_priv->flush_queue, &entry);
   	end = jiffies + (HZ*3);
   	for (;;) {
	      	i810_dma_emit_flush(dev);
	   	if (atomic_read(&dev_priv->flush_done) == 1) break;
		if((signed)(end - jiffies) <= 0) {
		   	DRM_ERROR("lockup\n");
		   	break;
		}	   
	      	schedule_timeout(HZ*3);
	      	if (signal_pending(current)) {
		   	ret = -EINTR; /* Can't restart */
		   	break;
		}
	}
   
   	current->state = TASK_RUNNING;
   	remove_wait_queue(&dev_priv->flush_queue, &entry);
   
   	return ret;
}

/* Must be called with the lock held */
void i810_reclaim_buffers(drm_device_t *dev, pid_t pid)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
      	if(dev->dev_private == NULL) return;
	if(dma->buflist == NULL) return;
        i810_flush_queue(dev);

	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_i810_buf_priv_t *buf_priv = buf->dev_private;
	   
		/* Only buffers that need to get reclaimed ever 
		 * get set to free 
		 */
		if (buf->pid == pid && buf_priv) {
		   	cmpxchg(buf_priv->in_use, 
				I810_BUF_USED, I810_BUF_FREE);
		}
	}
}

int i810_lock(struct inode *inode, struct file *filp, unsigned int cmd,
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
   
   	DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
		  lock.context, current->pid, dev->lock.hw_lock->lock,
		  lock.flags);

	if (lock.context < 0) {
		return -EINVAL;
	}
	/* Only one queue:
	 */

	if (!ret) {
#if 0
	   	if (_DRM_LOCKING_CONTEXT(dev->lock.hw_lock->lock)
		    != lock.context) {
			long j = jiffies - dev->lock.lock_time;

			if (j > 0 && j <= DRM_LOCK_SLICE) {
				/* Can't take lock if we just had it and
				   there is contention. */
				current->state = TASK_INTERRUPTIBLE;
				schedule_timeout(j);
			}
		}
#endif
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
		   	DRM_DEBUG("Calling lock schedule\n");
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
		   DRM_DEBUG("_DRM_LOCK_QUIESCENT\n");
		   DRM_DEBUG("fred\n");
		   i810_dma_quiescent(dev);
		}
	}
	DRM_DEBUG("%d %s\n", lock.context, ret ? "interrupted" : "has lock");
	return ret;
}

int i810_flush_ioctl(struct inode *inode, struct file *filp, 
		     unsigned int cmd, unsigned long arg)
{
   	drm_file_t	  *priv	  = filp->private_data;
   	drm_device_t	  *dev	  = priv->dev;
   
   	DRM_DEBUG("i810_flush_ioctl\n");
   	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_flush_ioctl called without lock held\n");
		return -EINVAL;
	}

   	i810_flush_queue(dev);
   	return 0;
}

static int i810DmaGeneral(drm_device_t *dev, drm_i810_general_t *args)
{
	drm_device_dma_t *dma = dev->dma;
   	drm_buf_t *buf = dma->buflist[ args->idx ];
   	
	if (!args->used) {
	   	i810_freelist_put(dev, buf);
	} else {  
		i810_dma_dispatch_general( dev, buf, args->used );
		atomic_add(args->used, &dma->total_bytes);
		atomic_inc(&dma->total_dmas);
	}
	return 0; 
}

static int i810DmaVertex(drm_device_t *dev, drm_i810_vertex_t *args)
{
	drm_device_dma_t *dma = dev->dma;
	drm_buf_t *buf = dma->buflist[ args->idx ];
   	i810_dma_dispatch_vertex( dev, buf, args->discard, args->used );
   	atomic_add(args->used, &dma->total_bytes);
	atomic_inc(&dma->total_dmas);
   	return 0;
}

int i810_dma_general(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
	drm_i810_general_t general;
      	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
      	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 

	int retcode = 0;
	
	copy_from_user_ret(&general, (drm_i810_general_t *)arg, sizeof(general),
			   -EFAULT);

	DRM_DEBUG("i810 dma general idx %d used %d\n",
		  general.idx, general.used);
   
   	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_dma_general called without lock held\n");
		return -EINVAL;
	}

	retcode = i810DmaGeneral(dev, &general);
	sarea_priv->last_enqueue = dev_priv->counter-1;
   	sarea_priv->last_dispatch = (int) hw_status[5];
   
	return retcode;
}

int i810_dma_vertex(struct inode *inode, struct file *filp,
	       unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->dev;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
      	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 
	drm_i810_vertex_t vertex;
	int retcode = 0;

	copy_from_user_ret(&vertex, (drm_i810_vertex_t *)arg, sizeof(vertex),
			   -EFAULT);
   	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_dma_vertex called without lock held\n");
		return -EINVAL;
	}

	DRM_DEBUG("i810 dma vertex, idx %d used %d discard %d\n",
		  vertex.idx, vertex.used, vertex.discard);

	retcode = i810DmaVertex(dev, &vertex);
	sarea_priv->last_enqueue = dev_priv->counter-1;
   	sarea_priv->last_dispatch = (int) hw_status[5];
   
	return retcode;

}

int i810_getage(struct inode *inode, struct file *filp, unsigned int cmd,
		unsigned long arg)
{
   	drm_file_t	  *priv	    = filp->private_data;
	drm_device_t	  *dev	    = priv->dev;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
      	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 

      	sarea_priv->last_dispatch = (int) hw_status[5];
	return 0;
}

int i810_dma(struct inode *inode, struct file *filp, unsigned int cmd,
	    unsigned long arg)
{
	drm_file_t	  *priv	    = filp->private_data;
	drm_device_t	  *dev	    = priv->dev;
	drm_device_dma_t  *dma	    = dev->dma;
	int		  retcode   = 0;
	drm_dma_t	  d;
   	drm_i810_private_t *dev_priv = (drm_i810_private_t *)dev->dev_private;
   	u32 *hw_status = (u32 *)dev_priv->hw_status_page;
   	drm_i810_sarea_t *sarea_priv = (drm_i810_sarea_t *) 
     					dev_priv->sarea_priv; 


   	copy_from_user_ret(&d, (drm_dma_t *)arg, sizeof(d), -EFAULT);
	DRM_DEBUG("%d %d: %d send, %d req\n",
		  current->pid, d.context, d.send_count, d.request_count);
   
	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("i810_dma called without lock held\n");
		return -EINVAL;
	}

	/* Please don't send us buffers.
	 */
	if (d.send_count != 0) {
		DRM_ERROR("Process %d trying to send %d buffers via drmDMA\n",
			  current->pid, d.send_count);
		return -EINVAL;
	}
	
	/* We'll send you buffers.
	 */
	if (d.request_count < 0 || d.request_count > dma->buf_count) {
		DRM_ERROR("Process %d trying to get %d buffers (of %d max)\n",
			  current->pid, d.request_count, dma->buf_count);
		return -EINVAL;
	}
	
	d.granted_count = 0;

	if (!retcode && d.request_count) {
		retcode = i810_dma_get_buffers(dev, &d);
	}

	DRM_DEBUG("i810_dma: %d returning, granted = %d\n",
		  current->pid, d.granted_count);

	copy_to_user_ret((drm_dma_t *)arg, &d, sizeof(d), -EFAULT);   
   	sarea_priv->last_dispatch = (int) hw_status[5];

	return retcode;
}
