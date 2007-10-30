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

#include "xgi_drv.h"

#define XGI_FB_HEAP_START 0x1000000

int xgi_alloc(struct xgi_info * info, struct xgi_mem_alloc * alloc,
	      struct drm_file * filp)
{
	struct drm_memblock_item *block;
	const char *const mem_name = (alloc->location == XGI_MEMLOC_LOCAL) 
		? "on-card" : "GART";


	if ((alloc->location != XGI_MEMLOC_LOCAL)
	    && (alloc->location != XGI_MEMLOC_NON_LOCAL)) {
		DRM_ERROR("Invalid memory pool (0x%08x) specified.\n",
			  alloc->location);
		return -EINVAL;
	}

	if ((alloc->location == XGI_MEMLOC_LOCAL) 
	    ? !info->fb_heap_initialized : !info->pcie_heap_initialized) {
		DRM_ERROR("Attempt to allocate from uninitialized memory "
			  "pool (0x%08x).\n", alloc->location);
		return -EINVAL;
	}

	mutex_lock(&info->dev->struct_mutex);
	block = drm_sman_alloc(&info->sman, alloc->location, alloc->size,
			       0, (unsigned long) filp);
	mutex_unlock(&info->dev->struct_mutex);

	if (block == NULL) {
		alloc->size = 0;
		DRM_ERROR("%s memory allocation failed\n", mem_name);
		return -ENOMEM;
	} else {
		alloc->offset = (*block->mm->offset)(block->mm,
						     block->mm_info);
		alloc->hw_addr = alloc->offset;
		alloc->index = block->user_hash.key;

		if (block->user_hash.key != (unsigned long) alloc->index) {
			DRM_ERROR("%s truncated handle %lx for pool %d "
				  "offset %x\n",
				  __func__, block->user_hash.key,
				  alloc->location, alloc->offset);
		}

		if (alloc->location == XGI_MEMLOC_NON_LOCAL) {
			alloc->hw_addr += info->pcie.base;
		}

		DRM_DEBUG("%s memory allocation succeeded: 0x%x\n",
			  mem_name, alloc->offset);
	}

	return 0;
}


int xgi_alloc_ioctl(struct drm_device * dev, void * data,
		    struct drm_file * filp)
{
	struct xgi_info *info = dev->dev_private;

	return xgi_alloc(info, (struct xgi_mem_alloc *) data, filp);
}


int xgi_free(struct xgi_info * info, unsigned long index,
	     struct drm_file * filp)
{
	int err;

	mutex_lock(&info->dev->struct_mutex);
	err = drm_sman_free_key(&info->sman, index);
	mutex_unlock(&info->dev->struct_mutex);

	return err;
}


int xgi_free_ioctl(struct drm_device * dev, void * data,
		   struct drm_file * filp)
{
	struct xgi_info *info = dev->dev_private;

	return xgi_free(info, *(unsigned long *) data, filp);
}


int xgi_fb_heap_init(struct xgi_info * info)
{
	int err;
	
	mutex_lock(&info->dev->struct_mutex);
	err = drm_sman_set_range(&info->sman, XGI_MEMLOC_LOCAL,
				 XGI_FB_HEAP_START,
				 info->fb.size - XGI_FB_HEAP_START);
	mutex_unlock(&info->dev->struct_mutex);

	info->fb_heap_initialized = (err == 0);
	return err;
}
