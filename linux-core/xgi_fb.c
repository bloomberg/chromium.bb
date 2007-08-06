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

struct kmem_cache *xgi_mem_block_cache = NULL;

static struct xgi_mem_block *xgi_mem_new_node(void);


int xgi_mem_heap_init(struct xgi_mem_heap *heap, unsigned int start,
		      unsigned int end)
{
	struct xgi_mem_block *block;

	INIT_LIST_HEAD(&heap->free_list);
	INIT_LIST_HEAD(&heap->used_list);
	INIT_LIST_HEAD(&heap->sort_list);
	heap->initialized = TRUE;

	block = kmem_cache_alloc(xgi_mem_block_cache, GFP_KERNEL);
	if (!block) {
		return -ENOMEM;
	}

	block->offset = start;
	block->size = end - start;

	list_add(&block->list, &heap->free_list);

	heap->max_freesize = end - start;

	return 0;
}


void xgi_mem_heap_cleanup(struct xgi_mem_heap * heap)
{
	struct list_head *free_list;
	struct xgi_mem_block *block;
	struct xgi_mem_block *next;
	int i;

	free_list = &heap->free_list;
	for (i = 0; i < 3; i++, free_list++) {
		list_for_each_entry_safe(block, next, free_list, list) {
			DRM_INFO
				("No. %d block->offset: 0x%lx block->size: 0x%lx \n",
				 i, block->offset, block->size);
			kmem_cache_free(xgi_mem_block_cache, block);
			block = NULL;
		}
	}
	
	heap->initialized = 0;
}


struct xgi_mem_block *xgi_mem_new_node(void)
{
	struct xgi_mem_block *block =
		kmem_cache_alloc(xgi_mem_block_cache, GFP_KERNEL);

	if (!block) {
		DRM_ERROR("kmem_cache_alloc failed\n");
		return NULL;
	}

	block->offset = 0;
	block->size = 0;
	block->filp = (struct drm_file *) -1;

	return block;
}


static struct xgi_mem_block *xgi_mem_alloc(struct xgi_mem_heap * heap,
					   unsigned long originalSize)
{
	struct xgi_mem_block *block, *free_block, *used_block;
	unsigned long size = (originalSize + PAGE_SIZE - 1) & PAGE_MASK;


	DRM_INFO("Original 0x%lx bytes requested, really 0x%lx allocated\n",
		 originalSize, size);

	if (size == 0) {
		DRM_ERROR("size == 0\n");
		return (NULL);
	}
	DRM_INFO("max_freesize: 0x%lx \n", heap->max_freesize);
	if (size > heap->max_freesize) {
		DRM_ERROR
		    ("size: 0x%lx is bigger than frame buffer total free size: 0x%lx !\n",
		     size, heap->max_freesize);
		return (NULL);
	}

	list_for_each_entry(block, &heap->free_list, list) {
		DRM_INFO("block: 0x%px \n", block);
		if (size <= block->size) {
			break;
		}
	}

	if (&block->list == &heap->free_list) {
		DRM_ERROR
		    ("Can't allocate %ldk size from frame buffer memory !\n",
		     size / 1024);
		return (NULL);
	}

	free_block = block;
	DRM_INFO("alloc size: 0x%lx from offset: 0x%lx size: 0x%lx \n",
		 size, free_block->offset, free_block->size);

	if (size == free_block->size) {
		used_block = free_block;
		DRM_INFO("size == free_block->size: free_block = 0x%p\n",
			 free_block);
		list_del(&free_block->list);
	} else {
		used_block = xgi_mem_new_node();

		if (used_block == NULL)
			return (NULL);

		if (used_block == free_block) {
			DRM_ERROR("used_block == free_block = 0x%p\n",
				  used_block);
		}

		used_block->offset = free_block->offset;
		used_block->size = size;

		free_block->offset += size;
		free_block->size -= size;
	}

	heap->max_freesize -= size;

	list_add(&used_block->list, &heap->used_list);

	return (used_block);
}

int xgi_mem_free(struct xgi_mem_heap * heap, unsigned long offset,
		 struct drm_file * filp)
{
	struct xgi_mem_block *used_block = NULL, *block;
	struct xgi_mem_block *prev, *next;

	unsigned long upper;
	unsigned long lower;

	list_for_each_entry(block, &heap->used_list, list) {
		if (block->offset == offset) {
			break;
		}
	}

	if (&block->list == &heap->used_list) {
		DRM_ERROR("can't find block: 0x%lx to free!\n", offset);
		return -ENOENT;
	}

	if (block->filp != filp) {
		return -EPERM;
	}

	used_block = block;
	DRM_INFO("used_block: 0x%p, offset = 0x%lx, size = 0x%lx\n",
		 used_block, used_block->offset, used_block->size);

	heap->max_freesize += used_block->size;

	prev = next = NULL;
	upper = used_block->offset + used_block->size;
	lower = used_block->offset;

	list_for_each_entry(block, &heap->free_list, list) {
		if (block->offset == upper) {
			next = block;
		} else if ((block->offset + block->size) == lower) {
			prev = block;
		}
	}

	DRM_INFO("next = 0x%p, prev = 0x%p\n", next, prev);
	list_del(&used_block->list);

	if (prev && next) {
		prev->size += (used_block->size + next->size);
		list_del(&next->list);
		DRM_INFO("free node 0x%p\n", next);
		kmem_cache_free(xgi_mem_block_cache, next);
		kmem_cache_free(xgi_mem_block_cache, used_block);
	}
	else if (prev) {
		prev->size += used_block->size;
		DRM_INFO("free node 0x%p\n", used_block);
		kmem_cache_free(xgi_mem_block_cache, used_block);
	}
	else if (next) {
		next->size += used_block->size;
		next->offset = used_block->offset;
		DRM_INFO("free node 0x%p\n", used_block);
		kmem_cache_free(xgi_mem_block_cache, used_block);
	}
	else {
		list_add(&used_block->list, &heap->free_list);
		DRM_INFO("Recycled free node %p, offset = 0x%lx, size = 0x%lx\n",
			 used_block, used_block->offset, used_block->size);
	}

	return 0;
}


int xgi_alloc(struct xgi_info * info, struct xgi_mem_alloc * alloc,
		 struct drm_file * filp)
{
	struct xgi_mem_block *block;

	mutex_lock(&info->dev->struct_mutex);
	block = xgi_mem_alloc((alloc->location == XGI_MEMLOC_LOCAL)
			      ? &info->fb_heap : &info->pcie_heap,
			      alloc->size);
	mutex_unlock(&info->dev->struct_mutex);

	if (block == NULL) {
		alloc->size = 0;
		DRM_ERROR("Video RAM allocation failed\n");
		return -ENOMEM;
	} else {
		DRM_INFO("Video RAM allocation succeeded: 0x%p\n",
			 (char *)block->offset);
		alloc->size = block->size;
		alloc->offset = block->offset;
		alloc->hw_addr = block->offset;

		if (alloc->location == XGI_MEMLOC_NON_LOCAL) {
			alloc->hw_addr += info->pcie.base;
		}

		block->filp = filp;
	}

	return 0;
}


int xgi_fb_alloc_ioctl(struct drm_device * dev, void * data,
		       struct drm_file * filp)
{
	struct xgi_mem_alloc *alloc = 
		(struct xgi_mem_alloc *) data;
	struct xgi_info *info = dev->dev_private;

	alloc->location = XGI_MEMLOC_LOCAL;
	return xgi_alloc(info, alloc, filp);
}


int xgi_fb_free(struct xgi_info * info, unsigned long offset,
		struct drm_file * filp)
{
	int err = 0;

	mutex_lock(&info->dev->struct_mutex);
	err = xgi_mem_free(&info->fb_heap, offset, filp);
	mutex_unlock(&info->dev->struct_mutex);

	return err;
}


int xgi_fb_free_ioctl(struct drm_device * dev, void * data,
		      struct drm_file * filp)
{
	struct xgi_info *info = dev->dev_private;

	return xgi_fb_free(info, *(u32 *) data, filp);
}


int xgi_fb_heap_init(struct xgi_info * info)
{
	return xgi_mem_heap_init(&info->fb_heap, XGI_FB_HEAP_START,
				 info->fb.size);
}

/**
 * Free all blocks associated with a particular file handle.
 */
void xgi_fb_free_all(struct xgi_info * info, struct drm_file * filp)
{
	if (!info->fb_heap.initialized) {
		return;
	}

	mutex_lock(&info->dev->struct_mutex);

	do {
		struct xgi_mem_block *block;

		list_for_each_entry(block, &info->fb_heap.used_list, list) {
			if (block->filp == filp) {
				break;
			}
		}

		if (&block->list == &info->fb_heap.used_list) {
			break;
		}

		(void) xgi_mem_free(&info->fb_heap, block->offset, filp);
	} while(1);

	mutex_unlock(&info->dev->struct_mutex);
}
