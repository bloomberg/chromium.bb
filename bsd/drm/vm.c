/* vm.c -- Memory mapping for DRM -*- c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@precisioninsight.com
 *
 * Copyright 1999 Precision Insight, Inc., Cedar Park, Texas.
 * Copyright 2000 VA Linux Systems, Inc., Sunnyvale, California.
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
 * Authors:
 *    Rickard E. (Rik) Faith <faith@valinux.com>
 *
 */

#define __NO_VERSION__
#include "drmP.h"

#include <vm/vm.h>
#include <vm/pmap.h>

static int drm_dma_mmap(dev_t kdev, vm_offset_t offset, int prot)
{
	drm_device_t	 *dev	 = kdev->si_drv1;
	drm_device_dma_t *dma	 = dev->dma;
	unsigned long	 physical;
	unsigned long	 page;

	if (!dma)		   return -1; /* Error */
	if (!dma->pagelist)	   return -1; /* Nothing allocated */

	page	 = offset >> PAGE_SHIFT;
	physical = dma->pagelist[page];

	DRM_DEBUG("0x%08x (page %lu) => 0x%08lx\n", offset, page, physical);
	return atop(physical);
}

int drm_mmap(dev_t kdev, vm_offset_t offset, int prot)
{
	drm_device_t	*dev	= kdev->si_drv1;
	drm_map_t	*map	= NULL;
	int		i;
	
	/* DRM_DEBUG("offset = 0x%x\n", offset); */

	if (dev->dma
	    && offset >= 0
	    && offset < ptoa(dev->dma->page_count))
		return drm_dma_mmap(kdev, offset, prot);

				/* A sequential search of a linked list is
				   fine here because: 1) there will only be
				   about 5-10 entries in the list and, 2) a
				   DRI client only has to do this mapping
				   once, so it doesn't have to be optimized
				   for performance, even if the list was a
				   bit longer. */
	for (i = 0; i < dev->map_count; i++) {
		map = dev->maplist[i];
		/* DRM_DEBUG("considering 0x%x..0x%x\n", map->offset, map->offset + map->size - 1); */
		if (offset >= map->offset
		    && offset < map->offset + map->size) break;
	}
	
	if (i >= dev->map_count) {
		DRM_DEBUG("can't find map\n");
		return -1;
	}
	if (!map || ((map->flags&_DRM_RESTRICTED) && suser(curproc))) {
		DRM_DEBUG("restricted map\n");
		return -1;
	}

	switch (map->type) {
	case _DRM_FRAME_BUFFER:
	case _DRM_REGISTERS:
	case _DRM_AGP:
		return atop(offset);
	case _DRM_SHM:
		return atop(vtophys(offset));
	default:
		return -1;	/* This should never happen. */
	}
	DRM_DEBUG("bailing out\n");
	
	return -1;
}
