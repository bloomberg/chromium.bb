/* bufs.c -- IOCTLs to manage buffers -*- c -*-
 * Created: Tue Feb  2 08:37:54 1999 by faith@precisioninsight.com
 * Revised: Fri Aug 20 22:48:10 1999 by faith@precisioninsight.com
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
 * $PI: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/bufs.c,v 1.8 1999/08/30 13:05:00 faith Exp $
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/bufs.c,v 1.1 1999/09/25 14:37:57 dawes Exp $
 *
 */

#define __NO_VERSION__
#include "drmP.h"
#include <sys/mman.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

				/* Compute order.  Can be made faster. */
int drm_order(unsigned long size)
{
	int	      order;
	unsigned long tmp;

	for (order = 0, tmp = size; tmp >>= 1; ++order);
	if (size & ~(1 << order)) ++order;
	return order;
}

int drm_addmap(dev_t kdev, u_long cmd, caddr_t data,
	       int flags, struct proc *p)
{
	drm_device_t	*dev	= kdev->si_drv1;
	drm_map_t	*map;
	
	if (!(dev->flags & (FREAD|FWRITE)))
		return EACCES; /* Require read/write */

	map	     = drm_alloc(sizeof(*map), DRM_MEM_MAPS);
	if (!map) return ENOMEM;
	*map = *(drm_map_t *) data;

	DRM_DEBUG("offset = 0x%08lx, size = 0x%08lx, type = %d\n",
		  map->offset, map->size, map->type);
	if ((map->offset & (PAGE_SIZE-1)) || (map->size & (PAGE_SIZE-1))) {
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		DRM_DEBUG("offset or size not page aligned\n");
		return EINVAL;
	}
	map->mtrr   = -1;
	map->handle = 0;

	switch (map->type) {
	case _DRM_REGISTERS:
	case _DRM_FRAME_BUFFER:	
		if (map->offset + map->size < map->offset
		    /* || map->offset < virt_to_phys(high_memory) */) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			DRM_DEBUG("bad frame buffer size\n");
			return EINVAL;
		}
#ifdef CONFIG_MTRR
		if (map->type == _DRM_FRAME_BUFFER
		    || (map->flags & _DRM_WRITE_COMBINING)) {
			map->mtrr = mtrr_add(map->offset, map->size,
					     MTRR_TYPE_WRCOMB, 1);
		}
#endif
		map->handle = drm_ioremap(map->offset, map->size);
		break;
			

	case _DRM_SHM:
		DRM_DEBUG("%ld %d\n", map->size, drm_order(map->size));
		map->handle = (void *)drm_alloc_pages(drm_order(map->size)
						      - PAGE_SHIFT,
						      DRM_MEM_SAREA);
		if (!map->handle) {
			drm_free(map, sizeof(*map), DRM_MEM_MAPS);
			return ENOMEM;
		}
		map->offset = (unsigned long)map->handle;
		if (map->flags & _DRM_CONTAINS_LOCK) {
			dev->lock.hw_lock = map->handle; /* Pointer to lock */
		}
		break;
#ifdef DRM_AGP
	case _DRM_AGP:
		map->offset = map->offset + dev->agp->base;
		break;
#endif
	default:
		drm_free(map, sizeof(*map), DRM_MEM_MAPS);
		DRM_DEBUG("bad type\n");
		return EINVAL;
	}

	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	if (dev->maplist) {
		++dev->map_count;
		dev->maplist = drm_realloc(dev->maplist,
					   (dev->map_count-1)
					   * sizeof(*dev->maplist),
					   dev->map_count
					   * sizeof(*dev->maplist),
					   DRM_MEM_MAPS);
	} else {
		dev->map_count = 1;
		dev->maplist = drm_alloc(dev->map_count*sizeof(*dev->maplist),
					 DRM_MEM_MAPS);
	}
	dev->maplist[dev->map_count-1] = map;
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);

	*(drm_map_t *) data = *map;
	if (map->type != _DRM_SHM)
		((drm_map_t *)data)->handle = (void *) map->offset;

	return 0;
}

int drm_addbufs(dev_t kdev, u_long cmd, caddr_t data,
		int flags, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_desc_t	 request;
	int		 count;
	int		 order;
	int		 size;
	int		 total;
	int		 page_order;
	drm_buf_entry_t	 *entry;
	unsigned long	 page;
	drm_buf_t	 *buf;
	int		 alignment;
	unsigned long	 offset;
	int		 i;
	int		 byte_count;
	int		 page_count;

	if (!dma) return EINVAL;

	request = *(drm_buf_desc_t *) data;

	count	   = request.count;
	order	   = drm_order(request.size);
	size	   = 1 << order;
	
	DRM_DEBUG("count = %d, size = %d (%d), order = %d, queue_count = %d\n",
		  request.count, request.size, size, order, dev->queue_count);

	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER) return EINVAL;
	if (dev->queue_count) return EBUSY; /* Not while in use */

	alignment  = (request.flags & _DRM_PAGE_ALIGN) ? round_page(size) :size;
	page_order = order - PAGE_SHIFT > 0 ? order - PAGE_SHIFT : 0;
	total	   = PAGE_SIZE << page_order;

	simple_lock(&dev->count_lock);
	if (dev->buf_use) {
		simple_unlock(&dev->count_lock);
		return EBUSY;
	}
	atomic_inc(&dev->buf_alloc);
	simple_unlock(&dev->count_lock);
	
	lockmgr(&dev->dev_lock, LK_EXCLUSIVE, 0, curproc);
	entry = &dma->bufs[order];
	if (entry->buf_count) {
		lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
		atomic_dec(&dev->buf_alloc);
		return ENOMEM;	/* May only call once for each order */
	}
	
	entry->buflist = drm_alloc(count * sizeof(*entry->buflist),
				   DRM_MEM_BUFS);
	if (!entry->buflist) {
		lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
		atomic_dec(&dev->buf_alloc);
		return ENOMEM;
	}
	memset(entry->buflist, 0, count * sizeof(*entry->buflist));

	entry->seglist = drm_alloc(count * sizeof(*entry->seglist),
				   DRM_MEM_SEGS);
	if (!entry->seglist) {
		drm_free(entry->buflist,
			 count * sizeof(*entry->buflist),
			 DRM_MEM_BUFS);
		lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);
		atomic_dec(&dev->buf_alloc);
		return ENOMEM;
	}
	memset(entry->seglist, 0, count * sizeof(*entry->seglist));

	dma->pagelist = drm_realloc(dma->pagelist,
				    dma->page_count * sizeof(*dma->pagelist),
				    (dma->page_count + (count << page_order))
				    * sizeof(*dma->pagelist),
				    DRM_MEM_PAGES);
	DRM_DEBUG("pagelist: %d entries\n",
		  dma->page_count + (count << page_order));


	entry->buf_size	  = size;
	entry->page_order = page_order;
	byte_count	  = 0;
	page_count	  = 0;
	while (entry->buf_count < count) {
		if (!(page = drm_alloc_pages(page_order, DRM_MEM_DMA))) break;
		entry->seglist[entry->seg_count++] = page;
		for (i = 0; i < (1 << page_order); i++) {
			DRM_DEBUG("page %d @ 0x%08lx\n",
				  dma->page_count + page_count,
				  page + PAGE_SIZE * i);
			dma->pagelist[dma->page_count + page_count++]
				= page + PAGE_SIZE * i;
		}
		for (offset = 0;
		     offset + size <= total && entry->buf_count < count;
		     offset += alignment, ++entry->buf_count) {
			buf	     = &entry->buflist[entry->buf_count];
			buf->idx     = dma->buf_count + entry->buf_count;
			buf->total   = alignment;
			buf->order   = order;
			buf->used    = 0;
			buf->offset  = (dma->byte_count + byte_count + offset);
			buf->address = (void *)(page + offset);
			buf->next    = NULL;
			buf->waiting = 0;
			buf->pending = 0;
			buf->dma_wait = 0;
			buf->pid     = 0;
#if DRM_DMA_HISTOGRAM
			timespecclear(&buf->time_queued);
			timespecclear(&buf->time_dispatched);
			timespecclear(&buf->time_completed);
			timespecclear(&buf->time_freed);
#endif
			DRM_DEBUG("buffer %d @ %p\n",
				  entry->buf_count, buf->address);
		}
		byte_count += PAGE_SIZE << page_order;
	}

	dma->buflist = drm_realloc(dma->buflist,
				   dma->buf_count * sizeof(*dma->buflist),
				   (dma->buf_count + entry->buf_count)
				   * sizeof(*dma->buflist),
				   DRM_MEM_BUFS);
	for (i = dma->buf_count; i < dma->buf_count + entry->buf_count; i++)
		dma->buflist[i] = &entry->buflist[i - dma->buf_count];

	dma->buf_count	+= entry->buf_count;
	dma->seg_count	+= entry->seg_count;
	dma->page_count += entry->seg_count << page_order;
	dma->byte_count += PAGE_SIZE * (entry->seg_count << page_order);
	
	drm_freelist_create(&entry->freelist, entry->buf_count);
	for (i = 0; i < entry->buf_count; i++) {
		drm_freelist_put(dev, &entry->freelist, &entry->buflist[i]);
	}
	
	lockmgr(&dev->dev_lock, LK_RELEASE, 0, curproc);

	request.count = entry->buf_count;
	request.size  = size;

	*(drm_buf_desc_t *) data = request;
	
	atomic_dec(&dev->buf_alloc);
	return 0;
}

int drm_infobufs(dev_t kdev, u_long cmd, caddr_t data,
		 int flags, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_info_t	 request;
	int		 i;
	int		 count;

	if (!dma) return EINVAL;

	simple_lock(&dev->count_lock);
	if (atomic_read(&dev->buf_alloc)) {
		simple_unlock(&dev->count_lock);
		return EBUSY;
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	simple_unlock(&dev->count_lock);

	request = *(drm_buf_info_t *) data;

	for (i = 0, count = 0; i < DRM_MAX_ORDER+1; i++) {
		if (dma->bufs[i].buf_count) ++count;
	}
	
	DRM_DEBUG("count = %d\n", count);
	
	if (request.count >= count) {
		for (i = 0, count = 0; i < DRM_MAX_ORDER+1; i++) {
			if (dma->bufs[i].buf_count) {
				int error;
				error = copyout(&dma->bufs[i].buf_count,
						&request.list[count].count,
						sizeof(dma->bufs[0]
						       .buf_count));
				if (error) return error;
				error = copyout(&dma->bufs[i].buf_size,
						&request.list[count].size,
						sizeof(dma->bufs[0].buf_size));
				if (error) return error;
				error = copyout(&dma->bufs[i]
						.freelist.low_mark,
						&request.list[count].low_mark,
						sizeof(dma->bufs[0]
						       .freelist.low_mark));
				if (error) return error;
				error = copyout(&dma->bufs[i]
						.freelist.high_mark,
						&request.list[count].high_mark,
						sizeof(dma->bufs[0]
						       .freelist.high_mark));
				if (error) return error;
				DRM_DEBUG("%d %d %d %d %d\n",
					  i,
					  dma->bufs[i].buf_count,
					  dma->bufs[i].buf_size,
					  dma->bufs[i].freelist.low_mark,
					  dma->bufs[i].freelist.high_mark);
				++count;
			}
		}
	}
	request.count = count;

	*(drm_buf_info_t *) data = request;
	
	return 0;
}

int drm_markbufs(dev_t kdev, u_long cmd, caddr_t data,
		 int flags, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_desc_t	 request;
	int		 order;
	drm_buf_entry_t	 *entry;

	if (!dma) return EINVAL;

	request = *(drm_buf_desc_t *) data;

	DRM_DEBUG("%d, %d, %d\n",
		  request.size, request.low_mark, request.high_mark);
	order = drm_order(request.size);
	if (order < DRM_MIN_ORDER || order > DRM_MAX_ORDER) return EINVAL;
	entry = &dma->bufs[order];

	if (request.low_mark < 0 || request.low_mark > entry->buf_count)
		return EINVAL;
	if (request.high_mark < 0 || request.high_mark > entry->buf_count)
		return EINVAL;

	entry->freelist.low_mark  = request.low_mark;
	entry->freelist.high_mark = request.high_mark;
	
	return 0;
}

int drm_freebufs(dev_t kdev, u_long cmd, caddr_t data,
		 int flags, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_device_dma_t *dma	 = dev->dma;
	drm_buf_free_t	 request;
	int		 i;
	int		 idx;
	int		 error;
	drm_buf_t	 *buf;

	if (!dma) return EINVAL;

	request = *(drm_buf_free_t *) data;

	DRM_DEBUG("%d\n", request.count);
	for (i = 0; i < request.count; i++) {
		error = copyin(&request.list[i], &idx, sizeof(idx));
		if (error)
			return error;
		if (idx < 0 || idx >= dma->buf_count) {
			DRM_ERROR("Index %d (of %d max)\n",
				  idx, dma->buf_count - 1);
			return EINVAL;
		}
		buf = dma->buflist[idx];
		if (buf->pid != p->p_pid) {
			DRM_ERROR("Process %d freeing buffer owned by %d\n",
				  p->p_pid, buf->pid);
			return EINVAL;
		}
		drm_free_buffer(dev, buf);
	}
	
	return 0;
}

int drm_mapbufs(dev_t kdev, u_long cmd, caddr_t data,
		int flags, struct proc *p)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_device_dma_t *dma	 = dev->dma;
	int		 retcode = 0;
	const int	 zero	 = 0;
	vm_offset_t	 virtual;
	vm_offset_t	 address;
	drm_buf_map_t	 request;
	int		 i;

	if (!dma) return EINVAL;
	
	DRM_DEBUG("\n");

	simple_lock(&dev->count_lock);
	if (atomic_read(&dev->buf_alloc)) {
		simple_unlock(&dev->count_lock);
		return EBUSY;
	}
	++dev->buf_use;		/* Can't allocate more after this call */
	simple_unlock(&dev->count_lock);

	request = *(drm_buf_map_t *) data;

	if (request.count >= dma->buf_count) {
		virtual = 0;
		retcode = vm_mmap(&p->p_vmspace->vm_map,
				  &virtual,
				  round_page(dma->byte_count),
				  PROT_READ|PROT_WRITE, VM_PROT_ALL,
				  MAP_SHARED,
				  SLIST_FIRST(&kdev->si_hlist),
				  0);
		if (retcode)
			goto done;

		request.virtual = (void *)virtual;

		for (i = 0; i < dma->buf_count; i++) {
			retcode = copyout(&dma->buflist[i]->idx,
					  &request.list[i].idx,
					  sizeof(request.list[0].idx));
			if (retcode) goto done;
			retcode = copyout(&dma->buflist[i]->total,
					  &request.list[i].total,
					  sizeof(request.list[0].total));
			if (retcode) goto done;
			retcode = copyout(&zero,
					  &request.list[i].used,
					  sizeof(request.list[0].used));
			if (retcode) goto done;
			address = virtual + dma->buflist[i]->offset;
			retcode = copyout(&address,
					  &request.list[i].address,
					  sizeof(address));
			if (retcode) goto done;
		}
	}
done:
	request.count = dma->buf_count;
	DRM_DEBUG("%d buffers, retcode = %d\n", request.count, retcode);

	*(drm_buf_map_t *) data = request;

	return retcode;
}
