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
 *	    Keith Whitwell <keithw@precisioninsight.com>
 *
 * $XFree86$
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include "mga_drv.h"

#include <linux/interrupt.h>	/* For task queue support */

#define MGA_REG(reg)		2
#define MGA_BASE(reg)		((unsigned long) \
				((drm_device_t *)dev)->maplist[MGA_REG(reg)]->handle)
#define MGA_ADDR(reg)		(MGA_BASE(reg) + reg)
#define MGA_DEREF(reg)		*(__volatile__ int *)MGA_ADDR(reg)
#define MGA_READ(reg)		MGA_DEREF(reg)
#define MGA_WRITE(reg,val) 	do { MGA_DEREF(reg) = val; } while (0)

#define PDEA_pagpxfer_enable 	     0x2

static int mga_flush_queue(drm_device_t *dev);

static unsigned long mga_alloc_page(drm_device_t *dev)
{
	unsigned long address;
   
	address = __get_free_page(GFP_KERNEL);
	if(address == 0UL) {
		return 0;
	}
	atomic_inc(&mem_map[MAP_NR((void *) address)].count);
	set_bit(PG_locked, &mem_map[MAP_NR((void *) address)].flags);
   
	return address;
}

static void mga_free_page(drm_device_t *dev, unsigned long page)
{
	if(page == 0UL) {
		return;
	}
	atomic_dec(&mem_map[MAP_NR((void *) page)].count);
	clear_bit(PG_locked, &mem_map[MAP_NR((void *) page)].flags);
	wake_up(&mem_map[MAP_NR((void *) page)].wait);
	free_page(page);
	return;
}

static void mga_delay(void)
{
   	return;
}

static void mga_flush_write_combine(void)
{
   	int xchangeDummy;
   	__asm__ volatile(" push %%eax ; xchg %%eax, %0 ; pop %%eax" : : "m" (xchangeDummy));
   	__asm__ volatile(" push %%eax ; push %%ebx ; push %%ecx ; push %%edx ;"
			 " movl $0,%%eax ; cpuid ; pop %%edx ; pop %%ecx ; pop %%ebx ;"
			 " pop %%eax" : /* no outputs */ :  /* no inputs */ );
}

/* These are two age tags that will never be sent to
 * the hardware */
#define MGA_BUF_USED 	0xffffffff
#define MGA_BUF_FREE	0

static void mga_freelist_debug(drm_mga_freelist_t *item)
{
   	if(item->buf != NULL) {
     		DRM_DEBUG("buf index : %d\n", item->buf->idx);
	} else {
	   	DRM_DEBUG("Freelist head\n");
	}
   	DRM_DEBUG("item->age : %x\n", item->age);
   	DRM_DEBUG("item->next : %p\n", item->next);
   	DRM_DEBUG("item->prev : %p\n", item->prev);
}

static int mga_freelist_init(drm_device_t *dev)
{
      	drm_device_dma_t *dma = dev->dma;
   	drm_buf_t *buf;
   	drm_mga_buf_priv_t *buf_priv;
      	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
   	drm_mga_freelist_t *item;
   	int i;
   
   	dev_priv->head = drm_alloc(sizeof(drm_mga_freelist_t), DRM_MEM_DRIVER);
	if(dev_priv->head == NULL) return -ENOMEM;
   	memset(dev_priv->head, 0, sizeof(drm_mga_freelist_t));
   	dev_priv->head->age = MGA_BUF_USED;
   
   	for (i = 0; i < dma->buf_count; i++) {
	   	buf = dma->buflist[ i ];
	        buf_priv = buf->dev_private;
		item = drm_alloc(sizeof(drm_mga_freelist_t),
				 DRM_MEM_DRIVER);
	   	if(item == NULL) return -ENOMEM;
	   	memset(item, 0, sizeof(drm_mga_freelist_t));
	  	item->age = MGA_BUF_FREE;
	   	item->prev = dev_priv->head;
	   	item->next = dev_priv->head->next;
	   	if(dev_priv->head->next != NULL)
			dev_priv->head->next->prev = item;
	   	if(item->next == NULL) dev_priv->tail = item;
	   	item->buf = buf;
	   	buf_priv->my_freelist = item;
		buf_priv->discard = 0;
	   	dev_priv->head->next = item;
	}
   
   	item = dev_priv->head;
   	while(item) {
		mga_freelist_debug(item);
		item = item->next;
	}
   	DRM_DEBUG("Head\n");
   	mga_freelist_debug(dev_priv->head);
   	DRM_DEBUG("Tail\n");
   	mga_freelist_debug(dev_priv->tail);
   
   	return 0;
}

static void mga_freelist_cleanup(drm_device_t *dev)
{
      	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
   	drm_mga_freelist_t *item;
   	drm_mga_freelist_t *prev;

   	item = dev_priv->head;
   	while(item) {
	   	prev = item;
	   	item = item->next;
	   	drm_free(prev, sizeof(drm_mga_freelist_t), DRM_MEM_DRIVER);
	}
   
   	dev_priv->head = dev_priv->tail = NULL;
}

/* Frees dispatch lock */
static inline void mga_dma_quiescent(drm_device_t *dev)
{
	drm_device_dma_t  *dma      = dev->dma;
	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
	__volatile__ unsigned int *status =
		(__volatile__ unsigned int *)dev_priv->status_page;
   	unsigned long end;
	int i;

	end = jiffies + (HZ*3);
    	while(1) {
		if(!test_and_set_bit(0, &dev_priv->dispatch_lock)) {
			break;
		}
	   	if((signed)(end - jiffies) <= 0) {
			DRM_ERROR("irqs: %d wanted %d\n", 
				  atomic_read(&dev->total_irq), 
				  atomic_read(&dma->total_lost));
			DRM_ERROR("lockup\n"); 
			goto out_nolock;
		}
		for (i = 0 ; i < 2000 ; i++) mga_delay();
	}
	end = jiffies + (HZ*3);
    	DRM_DEBUG("quiescent status : %x\n", MGA_READ(MGAREG_STATUS));
    	while((MGA_READ(MGAREG_STATUS) & 0x00030001) != 0x00020000) {
		if((signed)(end - jiffies) <= 0) {
			DRM_ERROR("irqs: %d wanted %d\n", 
				  atomic_read(&dev->total_irq), 
				  atomic_read(&dma->total_lost));
			DRM_ERROR("lockup\n"); 
			goto out_status;
		}
		for (i = 0 ; i < 2000 ; i++) mga_delay();	  
	}
    	DRM_DEBUG("status[1] : %x last_sync_tag : %x\n", status[1],
		  dev_priv->last_sync_tag);
    	sarea_priv->dirty |= MGA_DMA_FLUSH;

out_status:
    	clear_bit(0, &dev_priv->dispatch_lock);
out_nolock:
}

#define FREELIST_INITIAL (MGA_DMA_BUF_NR * 2)
#define FREELIST_COMPARE(age) ((age >> 2))

unsigned int mga_create_sync_tag(drm_device_t *dev)
{
      	drm_mga_private_t *dev_priv = 
     		(drm_mga_private_t *) dev->dev_private;
   	unsigned int temp;
      	drm_buf_t *buf;
   	drm_mga_buf_priv_t *buf_priv;
      	drm_device_dma_t *dma = dev->dma;
   	int i;
   
   	dev_priv->sync_tag++;
   
   	if(dev_priv->sync_tag < FREELIST_INITIAL) {
	   	dev_priv->sync_tag = FREELIST_INITIAL;
	}
   	if(dev_priv->sync_tag > 0x3fffffff) {
		mga_flush_queue(dev);
	   	mga_dma_quiescent(dev);
	    
	   	for (i = 0; i < dma->buf_count; i++) {
			buf = dma->buflist[ i ];
			buf_priv = buf->dev_private;
			buf_priv->my_freelist->age = MGA_BUF_FREE;
		}
	   	
	   	dev_priv->sync_tag = FREELIST_INITIAL;
	}
   	temp = dev_priv->sync_tag << 2;

	dev_priv->sarea_priv->last_enqueue = temp;

   	DRM_DEBUG("sync_tag : %x\n", temp);
   	return temp;
}

/* Least recently used :
 * These operations are not atomic b/c they are protected by the 
 * hardware lock */

drm_buf_t *mga_freelist_get(drm_device_t *dev)
{
   	drm_mga_private_t *dev_priv = 
     		(drm_mga_private_t *) dev->dev_private;
   	__volatile__ unsigned int *status = 
     		(__volatile__ unsigned int *)dev_priv->status_page;
	drm_mga_freelist_t *prev;
   	drm_mga_freelist_t *next;
   
   	if((dev_priv->tail->age >> 2) <= FREELIST_COMPARE(status[1])) {
		prev = dev_priv->tail->prev;
	   	next = dev_priv->tail;
	   	prev->next = NULL;
	   	next->prev = next->next = NULL;
	   	dev_priv->tail = prev;
	   	next->age = MGA_BUF_USED;
	   	return next->buf;
	} 

   	return NULL;
}

int mga_freelist_put(drm_device_t *dev, drm_buf_t *buf)
{
      	drm_mga_private_t *dev_priv = 
     		(drm_mga_private_t *) dev->dev_private;
   	drm_mga_buf_priv_t *buf_priv = buf->dev_private;
	drm_mga_freelist_t *prev;
   	drm_mga_freelist_t *head;
   	drm_mga_freelist_t *next;

   	if(buf_priv->my_freelist->age == MGA_BUF_USED) {
		/* Discarded buffer, put it on the tail */
		next = buf_priv->my_freelist;
		next->age = MGA_BUF_FREE;
		prev = dev_priv->tail;
		prev->next = next;
		next->prev = prev;
		next->next = NULL;
		dev_priv->tail = next;
		DRM_DEBUG("Discarded\n");
	} else {
		/* Normally aged buffer, put it on the head + 1,
		 * as the real head is a sentinal element
		 */
		next = buf_priv->my_freelist;
		head = dev_priv->head;
		prev = head->next;
		head->next = next;
		prev->prev = next;
		next->prev = head;
		next->next = prev;
	}
   
   	return 0;
}

static void mga_print_all_primary(drm_device_t *dev)
{
      	drm_mga_private_t *dev_priv = dev->dev_private;
   	drm_mga_prim_buf_t *prim;
   	int i;

   	DRM_DEBUG("Full list of primarys\n");
     	for(i = 0; i < MGA_NUM_PRIM_BUFS; i++) {
		prim = dev_priv->prim_bufs[i];
		DRM_DEBUG("index : %d num_dwords : %d "
			  "max_dwords : %d phy_head : %x\n",
			  prim->idx, prim->num_dwords, 
			  prim->max_dwords, prim->phys_head);
		DRM_DEBUG("sec_used : %d swap_pending : %x "
			  "in_use : %x force_fire : %d\n",
			  prim->sec_used, prim->swap_pending, 
			  prim->in_use, atomic_read(&prim->force_fire));
		DRM_DEBUG("needs_overflow : %d\n", 
			  atomic_read(&prim->needs_overflow));
	}
   
   	DRM_DEBUG("current_idx : %d, next_idx : %d, last_idx : %d\n",
		  dev_priv->next_prim->idx, dev_priv->last_prim->idx,
		  dev_priv->current_prim->idx);
}

static int mga_init_primary_bufs(drm_device_t *dev, drm_mga_init_t *init)
{
   	drm_mga_private_t *dev_priv = dev->dev_private;
	drm_mga_prim_buf_t *prim_buffer;
   	int i, temp, size_of_buf;
   	int offset = init->reserved_map_agpstart;

   	DRM_DEBUG("mga_init_primary_bufs\n");
   	dev_priv->primary_size = ((init->primary_size + PAGE_SIZE - 1) / 
				  PAGE_SIZE) * PAGE_SIZE;
   	DRM_DEBUG("primary_size\n");
   	size_of_buf = dev_priv->primary_size / MGA_NUM_PRIM_BUFS;
	dev_priv->warp_ucode_size = init->warp_ucode_size;
   	dev_priv->prim_bufs = drm_alloc(sizeof(drm_mga_prim_buf_t *) * 
					(MGA_NUM_PRIM_BUFS + 1), 
					DRM_MEM_DRIVER);
   	if(dev_priv->prim_bufs == NULL) {
		DRM_ERROR("Unable to allocate memory for prim_buf\n");
		return -ENOMEM;
	}
   	DRM_DEBUG("memset\n");
   	memset(dev_priv->prim_bufs, 
	       0, sizeof(drm_mga_prim_buf_t *) * (MGA_NUM_PRIM_BUFS + 1));
   
   	temp = init->warp_ucode_size + dev_priv->primary_size;
	temp = ((temp + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;
	   
   	DRM_DEBUG("temp : %x\n", temp);
   	DRM_DEBUG("dev->agp->base: %lx\n", dev->agp->base);
   	DRM_DEBUG("init->reserved_map_agpstart: %x\n", 
		  init->reserved_map_agpstart);
   	DRM_DEBUG("ioremap\n");
	dev_priv->ioremap = drm_ioremap(dev->agp->base + offset, 
					temp);
	if(dev_priv->ioremap == NULL) {
		DRM_DEBUG("Ioremap failed\n");
		return -ENOMEM;
	}
   	init_waitqueue_head(&dev_priv->wait_queue);
   
   	for(i = 0; i < MGA_NUM_PRIM_BUFS; i++) {
	   	DRM_DEBUG("For loop\n");
	   	prim_buffer = drm_alloc(sizeof(drm_mga_prim_buf_t), 
					DRM_MEM_DRIVER);
	   	if(prim_buffer == NULL) return -ENOMEM;
	   	DRM_DEBUG("memset\n");
	   	memset(prim_buffer, 0, sizeof(drm_mga_prim_buf_t));
	   	prim_buffer->phys_head = offset + dev->agp->base;
	   	prim_buffer->current_dma_ptr = 
			prim_buffer->head = 
			(u32 *) (dev_priv->ioremap + 
				 offset - 
				 init->reserved_map_agpstart);
	   	prim_buffer->num_dwords = 0;
	   	prim_buffer->max_dwords = size_of_buf / sizeof(u32);
	   	prim_buffer->max_dwords -= 5; /* Leave room for the softrap */
	   	prim_buffer->sec_used = 0;
	   	prim_buffer->idx = i;
	   	offset = offset + size_of_buf;
	   	dev_priv->prim_bufs[i] = prim_buffer;
	   	DRM_DEBUG("Looping\n");
	}
	dev_priv->current_prim_idx = 0;
        dev_priv->next_prim = 
		dev_priv->last_prim = 
		dev_priv->current_prim =
        	dev_priv->prim_bufs[0];
   	set_bit(0, &dev_priv->current_prim->in_use);
	DRM_DEBUG("init done\n");
   	return 0;
}

void mga_fire_primary(drm_device_t *dev, drm_mga_prim_buf_t *prim)
{
       	drm_mga_private_t *dev_priv = dev->dev_private;
      	drm_device_dma_t  *dma	    = dev->dma;
       	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
 	int use_agp = PDEA_pagpxfer_enable;
	unsigned long end;
   	int i;
   	int next_idx;
       	PRIMLOCALS;
   
    	DRM_DEBUG("mga_fire_primary\n");
    	dev_priv->last_sync_tag = mga_create_sync_tag(dev); 
   	dev_priv->last_prim = prim;
   
 	/* We never check for overflow, b/c there is always room */
    	PRIMPTR(prim);
   	if(num_dwords <= 0) {
		DRM_DEBUG("num_dwords == 0 when dispatched\n");
		goto out_prim_wait;
	}
 	PRIMOUTREG( MGAREG_DMAPAD, 0);
 	PRIMOUTREG( MGAREG_DMAPAD, 0);
       	PRIMOUTREG( MGAREG_DWGSYNC, dev_priv->last_sync_tag);
   	PRIMOUTREG( MGAREG_SOFTRAP, 0);
    	PRIMFINISH(prim);

	end = jiffies + (HZ*3);
    	if(sarea_priv->dirty & MGA_DMA_FLUSH) {
		DRM_DEBUG("Dma top flush\n");	   
		while((MGA_READ(MGAREG_STATUS) & 0x00030001) != 0x00020000) {
			if((signed)(end - jiffies) <= 0) {
				DRM_ERROR("irqs: %d wanted %d\n", 
					  atomic_read(&dev->total_irq), 
					  atomic_read(&dma->total_lost));
				DRM_ERROR("lockup in fire primary "
					  "(Dma Top Flush)\n");
				goto out_prim_wait;
			}
	      
			for (i = 0 ; i < 4096 ; i++) mga_delay();
		}
		sarea_priv->dirty &= ~(MGA_DMA_FLUSH);
	} else {
		DRM_DEBUG("Status wait\n");
		while((MGA_READ(MGAREG_STATUS) & 0x00020001) != 0x00020000) {
			if((signed)(end - jiffies) <= 0) {
				DRM_ERROR("irqs: %d wanted %d\n", 
					  atomic_read(&dev->total_irq), 
					  atomic_read(&dma->total_lost));
				DRM_ERROR("lockup in fire primary "
					  "(Status Wait)\n");
				goto out_prim_wait;
			}
	   
			for (i = 0 ; i < 4096 ; i++) mga_delay();
		}
	}
	
   	mga_flush_write_combine();
    	atomic_inc(&dev_priv->pending_bufs);
	atomic_inc(&dma->total_lost);
       	MGA_WRITE(MGAREG_PRIMADDRESS, phys_head | TT_GENERAL);
 	MGA_WRITE(MGAREG_PRIMEND, (phys_head + num_dwords * 4) | use_agp);
   	prim->num_dwords = 0;
    
   	next_idx = prim->idx + 1;
    	if(next_idx >= MGA_NUM_PRIM_BUFS) 
		next_idx = 0;

    	dev_priv->next_prim = dev_priv->prim_bufs[next_idx];
	return;

 out_prim_wait:
	prim->num_dwords = 0;
	prim->sec_used = 0;
	clear_bit(0, &prim->in_use);
   	wake_up_interruptible(&dev_priv->wait_queue);
	clear_bit(0, &prim->swap_pending);
	clear_bit(0, &dev_priv->dispatch_lock);
	atomic_dec(&dev_priv->pending_bufs);
}

int mga_advance_primary(drm_device_t *dev)
{
   	DECLARE_WAITQUEUE(entry, current);
   	drm_mga_private_t *dev_priv = dev->dev_private;
   	drm_mga_prim_buf_t *prim_buffer;
   	drm_device_dma_t  *dma      = dev->dma;
   	int next_prim_idx;
   	int ret = 0;
   
   	/* This needs to reset the primary buffer if available,
	 * we should collect stats on how many times it bites
	 * it's tail */
   
   	next_prim_idx = dev_priv->current_prim_idx + 1;
   	if(next_prim_idx >= MGA_NUM_PRIM_BUFS)
     		next_prim_idx = 0;
   	prim_buffer = dev_priv->prim_bufs[next_prim_idx];
   	atomic_set(&dev_priv->in_wait, 1);
   
      	/* In use is cleared in interrupt handler */
   
   	if(test_and_set_bit(0, &prim_buffer->in_use)) {
	   	add_wait_queue(&dev_priv->wait_queue, &entry);
	   	for (;;) {
		   	current->state = TASK_INTERRUPTIBLE;
		   	mga_dma_schedule(dev, 0);
		   	if(!test_and_set_bit(0, &prim_buffer->in_use)) break;
		   	atomic_inc(&dev->total_sleeps);
		   	atomic_inc(&dma->total_missed_sched);
		   	mga_print_all_primary(dev);
		   	DRM_DEBUG("Schedule in advance\n");
		   	/* Three second delay */
		   	schedule_timeout(HZ*3);
		   	if (signal_pending(current)) {
			   	ret = -ERESTARTSYS;
			   	break;
			}
		}
	   	current->state = TASK_RUNNING;
	   	remove_wait_queue(&dev_priv->wait_queue, &entry);
	   	if(ret) return ret;
	}
   	atomic_set(&dev_priv->in_wait, 0);
   	/* This primary buffer is now free to use */
   	prim_buffer->current_dma_ptr = prim_buffer->head;
   	prim_buffer->num_dwords = 0;
   	prim_buffer->sec_used = 0;
   	atomic_set(&prim_buffer->needs_overflow, 0);
   	dev_priv->current_prim = prim_buffer;
   	dev_priv->current_prim_idx = next_prim_idx;
   	DRM_DEBUG("Primarys at advance\n");
   	mga_print_all_primary(dev);
   	return 0;
}

/* More dynamic performance decisions */
static inline int mga_decide_to_fire(drm_device_t *dev)
{
   	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
      	drm_device_dma_t  *dma	    = dev->dma;
	
   	if(atomic_read(&dev_priv->next_prim->force_fire))
	{
	   	atomic_inc(&dma->total_prio);
	   	return 1;
	}

	if (atomic_read(&dev_priv->in_flush) && dev_priv->next_prim->num_dwords)
	{
	   	atomic_inc(&dma->total_prio);
	   	return 1;
	}
   
   	if(atomic_read(&dev_priv->pending_bufs) <= MGA_NUM_PRIM_BUFS - 1) {
		if(test_bit(0, &dev_priv->next_prim->swap_pending)) {
			atomic_inc(&dma->total_dmas);
			return 1;
		}
	}

   	if(atomic_read(&dev_priv->pending_bufs) <= MGA_NUM_PRIM_BUFS / 2) {
		if(dev_priv->next_prim->sec_used >= MGA_DMA_BUF_NR / 8) {
			atomic_inc(&dma->total_hit);
			return 1;
		}
	}

   	if(atomic_read(&dev_priv->pending_bufs) >= MGA_NUM_PRIM_BUFS / 2) {
		if(dev_priv->next_prim->sec_used >= MGA_DMA_BUF_NR / 4) {
			atomic_inc(&dma->total_missed_free);
			return 1;
		}
	}

   	atomic_inc(&dma->total_tried);
   	return 0;
}

int mga_dma_schedule(drm_device_t *dev, int locked)
{
      	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
      	drm_device_dma_t  *dma	    = dev->dma;

   	if (test_and_set_bit(0, &dev->dma_flag)) {
		atomic_inc(&dma->total_missed_dma);
		return -EBUSY;
	}
   
       	DRM_DEBUG("mga_dma_schedule\n");

   	if(atomic_read(&dev_priv->in_flush) || 
	   atomic_read(&dev_priv->in_wait)) {
		locked = 1;
	}
   
   	if (!locked && 
	    !drm_lock_take(&dev->lock.hw_lock->lock, DRM_KERNEL_CONTEXT)) {
	   	atomic_inc(&dma->total_missed_lock);
	   	clear_bit(0, &dev->dma_flag);
	   	return -EBUSY;
	}
   	DRM_DEBUG("I'm locked\n");

   
   	if(!test_and_set_bit(0, &dev_priv->dispatch_lock)) {
	   	/* Fire dma buffer */
	   	if(mga_decide_to_fire(dev)) {
		   	DRM_DEBUG("mga_fire_primary\n");
		   	DRM_DEBUG("idx :%d\n", dev_priv->next_prim->idx);
		   	atomic_set(&dev_priv->next_prim->force_fire, 0);
		   	if(dev_priv->current_prim == dev_priv->next_prim &&
			   dev_priv->next_prim->num_dwords != 0) {
				/* Schedule overflow for a later time */
				atomic_set(
					&dev_priv->current_prim->needs_overflow,
					1);
			}
		   	mga_fire_primary(dev, dev_priv->next_prim);
		} else {
			clear_bit(0, &dev_priv->dispatch_lock);
		}
	} else {
		DRM_DEBUG("I can't get the dispatch lock\n");
	}
   	
	if (!locked) {
		if (drm_lock_free(dev, &dev->lock.hw_lock->lock,
				  DRM_KERNEL_CONTEXT)) {
			DRM_ERROR("\n");
		}
	}

   	clear_bit(0, &dev->dma_flag);
   
      	if(atomic_read(&dev_priv->in_flush) == 1 &&
	   dev_priv->next_prim->num_dwords == 0) {
	   	/* Everything is on the hardware */
	   	DRM_DEBUG("Primarys at Flush\n");
	   	mga_print_all_primary(dev);
	   	atomic_set(&dev_priv->in_flush, 0);
	   	wake_up_interruptible(&dev_priv->flush_queue);
	}

	return 0;
}

static void mga_dma_service(int irq, void *device, struct pt_regs *regs)
{
    	drm_device_t	 *dev = (drm_device_t *)device;
    	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
    	drm_mga_prim_buf_t *last_prim_buffer;
    	__volatile__ unsigned int *status = 
      		(__volatile__ unsigned int *)dev_priv->status_page;
    
    	atomic_inc(&dev->total_irq);
      	MGA_WRITE(MGAREG_ICLEAR, 0x00000001);
   	last_prim_buffer = dev_priv->last_prim;
    	last_prim_buffer->num_dwords = 0;
    	last_prim_buffer->sec_used = 0;
      	clear_bit(0, &last_prim_buffer->in_use);
   	wake_up_interruptible(&dev_priv->wait_queue);
      	clear_bit(0, &last_prim_buffer->swap_pending);
      	clear_bit(0, &dev_priv->dispatch_lock);
      	atomic_dec(&dev_priv->pending_bufs);
   	dev_priv->sarea_priv->last_dispatch = status[1];
   	queue_task(&dev->tq, &tq_immediate);
   	mark_bh(IMMEDIATE_BH);
}

static void mga_dma_task_queue(void *device)
{
   	drm_device_t *dev = (drm_device_t *) device;

	mga_dma_schedule(dev, 0);
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
	   	if(dev_priv->status_page != NULL) {
		   	iounmap(dev_priv->status_page);
		}
	   	if(dev_priv->real_status_page != 0UL) {
		   	mga_free_page(dev, dev_priv->real_status_page);
		}
	   	if(dev_priv->prim_bufs != NULL) {
		   	int i;
		   	for(i = 0; i < MGA_NUM_PRIM_BUFS; i++) {
			   	if(dev_priv->prim_bufs[i] != NULL) {
			     		drm_free(dev_priv->prim_bufs[i],
						 sizeof(drm_mga_prim_buf_t),
						 DRM_MEM_DRIVER);
				}
			}
		   	drm_free(dev_priv->prim_bufs, sizeof(void *) *
				 (MGA_NUM_PRIM_BUFS + 1), 
				 DRM_MEM_DRIVER);
		}
		if(dev_priv->head != NULL) {
		   	mga_freelist_cleanup(dev);
		}


		drm_free(dev->dev_private, sizeof(drm_mga_private_t), 
			 DRM_MEM_DRIVER);
		dev->dev_private = NULL;
	}

	return 0;
}

static int mga_dma_initialize(drm_device_t *dev, drm_mga_init_t *init) {
	drm_mga_private_t *dev_priv;
	drm_map_t *sarea_map = NULL;
	int i;

	dev_priv = drm_alloc(sizeof(drm_mga_private_t), DRM_MEM_DRIVER);
	if(dev_priv == NULL) return -ENOMEM;
	dev->dev_private = (void *) dev_priv;

	DRM_DEBUG("dev_private\n");

	memset(dev_priv, 0, sizeof(drm_mga_private_t));
      	atomic_set(&dev_priv->in_flush, 0);

	if((init->reserved_map_idx >= dev->map_count) ||
	   (init->buffer_map_idx >= dev->map_count)) {
		mga_dma_cleanup(dev);
		DRM_DEBUG("reserved_map or buffer_map are invalid\n");
		return -EINVAL;
	}
   
	dev_priv->reserved_map_idx = init->reserved_map_idx;
	dev_priv->buffer_map_idx = init->buffer_map_idx;
	sarea_map = dev->maplist[0];
	dev_priv->sarea_priv = (drm_mga_sarea_t *) 
		((u8 *)sarea_map->handle + 
		 init->sarea_priv_offset);
	DRM_DEBUG("sarea_priv\n");

	/* Scale primary size to the next page */
	dev_priv->chipset = init->chipset;
	dev_priv->frontOffset = init->frontOffset;
	dev_priv->backOffset = init->backOffset;
	dev_priv->depthOffset = init->depthOffset;
	dev_priv->textureOffset = init->textureOffset;
	dev_priv->textureSize = init->textureSize;
	dev_priv->cpp = init->cpp;
	dev_priv->sgram = init->sgram;
	dev_priv->stride = init->stride;

	dev_priv->mAccess = init->mAccess;
   	init_waitqueue_head(&dev_priv->flush_queue);
	dev_priv->WarpPipe = -1;

   	DRM_DEBUG("chipset: %d ucode_size: %d backOffset: %x depthOffset: %x\n",
		  dev_priv->chipset, dev_priv->warp_ucode_size, 
		  dev_priv->backOffset, dev_priv->depthOffset);
   	DRM_DEBUG("cpp: %d sgram: %d stride: %d maccess: %x\n",
		  dev_priv->cpp, dev_priv->sgram, dev_priv->stride, 
		  dev_priv->mAccess);
   
	memcpy(&dev_priv->WarpIndex, &init->WarpIndex, 
	       sizeof(drm_mga_warp_index_t) * MGA_MAX_WARP_PIPES);

   	for (i = 0 ; i < MGA_MAX_WARP_PIPES ; i++) 
		DRM_DEBUG("warp pipe %d: installed: %d phys: %lx size: %x\n",
			  i, 
			  dev_priv->WarpIndex[i].installed,
			  dev_priv->WarpIndex[i].phys_addr,
			  dev_priv->WarpIndex[i].size);

   	DRM_DEBUG("Doing init prim buffers\n");
   	if(mga_init_primary_bufs(dev, init) != 0) {
		DRM_ERROR("Can not initialize primary buffers\n");
		mga_dma_cleanup(dev);
		return -ENOMEM;
	}
   	DRM_DEBUG("Done with init prim buffers\n");
   	dev_priv->real_status_page = mga_alloc_page(dev);
      	if(dev_priv->real_status_page == 0UL) {
		mga_dma_cleanup(dev);
		DRM_ERROR("Can not allocate status page\n");
		return -ENOMEM;
	}
   	DRM_DEBUG("Status page at %lx\n", dev_priv->real_status_page);

   	dev_priv->status_page = 
		ioremap_nocache(virt_to_bus((void *)dev_priv->real_status_page),
				PAGE_SIZE);

   	if(dev_priv->status_page == NULL) {
		mga_dma_cleanup(dev);
		DRM_ERROR("Can not remap status page\n");
		return -ENOMEM;
	}

   	DRM_DEBUG("Status page remapped to %p\n", dev_priv->status_page);
   	/* Write status page when secend or softrap occurs */
   	MGA_WRITE(MGAREG_PRIMPTR, 
		  virt_to_bus((void *)dev_priv->real_status_page) | 0x00000003);
   
   	dev_priv->device = pci_find_device(0x102b, 0x0525, NULL);
   	if(dev_priv->device == NULL) {
		DRM_ERROR("Could not find pci device for card\n");
		mga_dma_cleanup(dev);
		return -EINVAL;
	}
   
   	DRM_DEBUG("dma initialization\n");

	/* Private is now filled in, initialize the hardware */
	{
	   	__volatile__ unsigned int *status = 
			(unsigned int *)dev_priv->status_page;
		PRIMLOCALS;
		PRIMGETPTR( dev_priv );
	   
	   	dev_priv->last_sync_tag = mga_create_sync_tag(dev);
	   
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DMAPAD, 0);
		PRIMOUTREG(MGAREG_DWGSYNC, dev_priv->last_sync_tag);
		PRIMOUTREG(MGAREG_SOFTRAP, 0);
		/* Poll for the first buffer to insure that
		 * the status register will be correct
		 */
	   	DRM_DEBUG("phys_head : %lx\n", (unsigned long)phys_head);
   		status[1] = 0;
	   
		mga_flush_write_combine();
	   	MGA_WRITE(MGAREG_PRIMADDRESS, phys_head | TT_GENERAL);

		MGA_WRITE(MGAREG_PRIMEND, ((phys_head + num_dwords * 4) | 
					   PDEA_pagpxfer_enable));
	   
	   	while(MGA_READ(MGAREG_DWGSYNC) != dev_priv->last_sync_tag) ;
	   	DRM_DEBUG("status[0] after initialization : %x\n", status[0]);
	   	DRM_DEBUG("status[1] after initialization : %x\n", status[1]);
	}

	if(mga_freelist_init(dev) != 0) {
	   	DRM_ERROR("Could not initialize freelist\n");
	   	mga_dma_cleanup(dev);
	   	return -ENOMEM;
	}
   	DRM_DEBUG("dma init was successful\n");
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
	
	DRM_DEBUG("install irq handler %d\n", irq);

	dev->context_flag     = 0;
	dev->interrupt_flag   = 0;
	dev->dma_flag	      = 0;
	dev->dma->next_buffer = NULL;
	dev->dma->next_queue  = NULL;
	dev->dma->this_buffer = NULL;
	dev->tq.next	      = NULL;
	dev->tq.sync	      = 0;
	dev->tq.routine	      = mga_dma_task_queue;
	dev->tq.data	      = dev;

				/* Before installing handler */
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
   	MGA_WRITE(MGAREG_ICLEAR, 0x00000001);
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
   	DRM_DEBUG("remove irq handler %d\n", irq);
      	MGA_WRITE(MGAREG_ICLEAR, 0x00000001);
	MGA_WRITE(MGAREG_IEN, 0);
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

static int mga_flush_queue(drm_device_t *dev)
{
   	DECLARE_WAITQUEUE(entry, current);
  	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
   	int ret = 0;

   	if(dev_priv == NULL) {
	   	return 0;
	}
   
   	if(dev_priv->next_prim->num_dwords != 0) {
	   	atomic_set(&dev_priv->in_flush, 1);
   		current->state = TASK_INTERRUPTIBLE;
   		add_wait_queue(&dev_priv->flush_queue, &entry);
   		for (;;) {
		   	mga_dma_schedule(dev, 0);
	   		if (atomic_read(&dev_priv->in_flush) == 0) 
				break;
		   	atomic_inc(&dev->total_sleeps);
		   	DRM_DEBUG("Schedule in flush_queue\n");
	      		schedule_timeout(HZ*3);
	      		if (signal_pending(current)) {
		   		ret = -EINTR; /* Can't restart */
		   		break;
			}
		}
   		current->state = TASK_RUNNING;
   		remove_wait_queue(&dev_priv->flush_queue, &entry);
	}
   	atomic_set(&dev_priv->in_flush, 0);
   	return ret;
}

/* Must be called with the lock held */
void mga_reclaim_buffers(drm_device_t *dev, pid_t pid)
{
	drm_device_dma_t *dma = dev->dma;
	int		 i;

	if (!dma) return;
      	if(dev->dev_private == NULL) return;

        mga_flush_queue(dev);

	for (i = 0; i < dma->buf_count; i++) {
	   	drm_buf_t *buf = dma->buflist[ i ];
	   	drm_mga_buf_priv_t *buf_priv = buf->dev_private;

		if (buf->pid == pid) {
		   	if(buf_priv == NULL) return;
		   	/* Only buffers that need to get reclaimed ever 
			 * get set to free */
			if(buf_priv->my_freelist->age == MGA_BUF_USED) 
		     		buf_priv->my_freelist->age = MGA_BUF_FREE;
		}
	}
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
   
   	DRM_DEBUG("%d (pid %d) requests lock (0x%08x), flags = 0x%08x\n",
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
		   mga_flush_queue(dev);
		   mga_dma_quiescent(dev);
		}
	}
   
	DRM_DEBUG("%d %s\n", lock.context, ret ? "interrupted" : "has lock");
	return ret;
}
		
int mga_flush_ioctl(struct inode *inode, struct file *filp, 
		    unsigned int cmd, unsigned long arg)
{
       	drm_file_t	  *priv	  = filp->private_data;
    	drm_device_t	  *dev	  = priv->dev;
	drm_lock_t	  lock;
      	drm_mga_private_t *dev_priv = (drm_mga_private_t *)dev->dev_private;
   	drm_mga_sarea_t *sarea_priv = dev_priv->sarea_priv;
      	__volatile__ unsigned int *status = 
     		(__volatile__ unsigned int *)dev_priv->status_page;
	int		 i;
 
	copy_from_user_ret(&lock, (drm_lock_t *)arg, sizeof(lock), -EFAULT);

	if(!_DRM_LOCK_IS_HELD(dev->lock.hw_lock->lock)) {
		DRM_ERROR("mga_flush_ioctl called without lock held\n");
		return -EINVAL;
	}

   	if(lock.flags & _DRM_LOCK_FLUSH || lock.flags & _DRM_LOCK_FLUSH_ALL) {
		mga_flush_queue(dev);

		if((MGA_READ(MGAREG_STATUS) & 0x00030001) == 0x00020000 &&
		   status[1] != dev_priv->last_sync_tag) 
		{
			DRM_DEBUG("Reseting hardware status\n");
			MGA_WRITE(MGAREG_DWGSYNC, dev_priv->last_sync_tag);

			while(MGA_READ(MGAREG_DWGSYNC) != 
			      dev_priv->last_sync_tag) 
			{
				for(i = 0; i < 4096; i++) mga_delay();
			}

			status[1] = 
				sarea_priv->last_dispatch = 
				dev_priv->last_sync_tag;
		} else {
			sarea_priv->last_dispatch = status[1];
		}
	}
   	if(lock.flags & _DRM_LOCK_QUIESCENT) {
		mga_dma_quiescent(dev);
	}

    	return 0;
}
