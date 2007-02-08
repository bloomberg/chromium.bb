/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, TX., USA
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellström <thomas-at-tungstengraphics-dot-com>
 */

#include "drmP.h"

int drm_bo_move_ttm(drm_device_t *dev,
		    drm_ttm_t *ttm, 
		    int evict,
		    int no_wait,
		    drm_bo_mem_reg_t *old_mem,
		    drm_bo_mem_reg_t *new_mem)
{
	uint32_t save_flags = old_mem->flags;
	uint32_t save_mask = old_mem->mask;
	int ret;

	if (old_mem->mem_type == DRM_BO_MEM_TT) {

		if (evict)
			drm_ttm_evict(ttm);
		else
			drm_ttm_unbind(ttm);

		mutex_lock(&dev->struct_mutex);
		drm_mm_put_block(old_mem->mm_node);
		old_mem->mm_node = NULL;
		mutex_unlock(&dev->struct_mutex);
		DRM_FLAG_MASKED(old_mem->flags, 
			     DRM_BO_FLAG_CACHED | DRM_BO_FLAG_MAPPABLE |
			     DRM_BO_FLAG_MEM_LOCAL, DRM_BO_MASK_MEMTYPE);
		old_mem->mem_type = DRM_BO_MEM_LOCAL;
		save_flags = old_mem->flags;
	} 
	if (new_mem->mem_type != DRM_BO_MEM_LOCAL) {
		ret = drm_bind_ttm(ttm, 
				   new_mem->flags & DRM_BO_FLAG_CACHED, 
				   new_mem->mm_node->start);
		if (ret)
			return ret;
	}

	*old_mem = *new_mem;
	new_mem->mm_node = NULL;
	old_mem->mask = save_mask;
	DRM_FLAG_MASKED(save_flags, new_mem->flags, DRM_BO_MASK_MEMTYPE);
	return 0;
}


/**
 * \c Return a kernel virtual address to the buffer object PCI memory.
 *
 * \param bo The buffer object.
 * \return Failure indication.
 * 
 * Returns -EINVAL if the buffer object is currently not mappable.
 * Returns -ENOMEM if the ioremap operation failed.
 * Otherwise returns zero.
 * 
 * After a successfull call, bo->iomap contains the virtual address, or NULL
 * if the buffer object content is not accessible through PCI space. 
 * Call bo->mutex locked.
 */


int drm_mem_reg_ioremap(drm_device_t *dev, drm_bo_mem_reg_t *mem, void **virtual)
{
	drm_buffer_manager_t *bm = &dev->bm;
	drm_mem_type_manager_t *man = &bm->man[mem->mem_type]; 
	unsigned long bus_offset;
	unsigned long bus_size;
	unsigned long bus_base;
	int ret;
	void *addr;

	*virtual = NULL;
	ret = drm_bo_pci_offset(dev, mem, &bus_base, &bus_offset, &bus_size);
	if (ret || bus_size == 0) 
		return ret;

	if (!(man->flags & _DRM_FLAG_NEEDS_IOREMAP))
		addr = (void *) (((u8 *)man->io_addr) + bus_offset);
	else {
		addr = ioremap_nocache(bus_base + bus_offset, bus_size);
		if (!addr)
			return -ENOMEM;
	}
	*virtual = addr;
	return 0;
}


/**
 * \c Unmap mapping obtained using drm_bo_ioremap
 *
 * \param bo The buffer object.
 *
 * Call bo->mutex locked.
 */

void drm_mem_reg_iounmap(drm_device_t *dev, drm_bo_mem_reg_t *mem,
			 void *virtual)
{
	drm_buffer_manager_t *bm; 
	drm_mem_type_manager_t *man; 


	bm = &dev->bm;
	man = &bm->man[mem->mem_type];
	
	if (virtual && (man->flags & _DRM_FLAG_NEEDS_IOREMAP)) 
		iounmap(virtual);
}


static int drm_copy_io_page(void *dst, void *src, unsigned long page)
{
	uint32_t *dstP = (uint32_t *)((unsigned long) dst + (page << PAGE_SHIFT));
	uint32_t *srcP = (uint32_t *)((unsigned long) src + (page << PAGE_SHIFT));

	int i;
	for (i=0; i < PAGE_SIZE / sizeof(uint32_t); ++i) 
		iowrite32(ioread32(srcP++), dstP++);
	return 0;
}

static int drm_copy_io_ttm_page(drm_ttm_t *ttm, void *src, unsigned long page) 
{
	struct page *d = drm_ttm_get_page(ttm, page);
	void *dst;

	if (!d)
		return -ENOMEM;

	src = (void *)((unsigned long) src + (page << PAGE_SHIFT));
	dst = kmap(d);
	if (!dst)
		return -ENOMEM;

	memcpy_fromio(dst, src, PAGE_SIZE);
	kunmap(dst);
	return 0;
}

static int drm_copy_ttm_io_page(drm_ttm_t *ttm, void *dst, unsigned long page) 
{
	struct page *s = drm_ttm_get_page(ttm, page);
	void *src;

	if (!s)
		return -ENOMEM;

	dst = (void *)((unsigned long) dst + (page << PAGE_SHIFT));
	src = kmap(s);
	if (!src)
		return -ENOMEM;

	memcpy_toio(dst, src, PAGE_SIZE);
	kunmap(src);
	return 0;
}

	
int drm_bo_move_memcpy(drm_device_t *dev,
		       drm_ttm_t *ttm, 
		       int evict,
		       int no_wait,
		       drm_bo_mem_reg_t *old_mem,
		       drm_bo_mem_reg_t *new_mem)
{
	void *old_iomap;
	void *new_iomap;
	int ret;
	uint32_t save_flags = old_mem->flags;
	uint32_t save_mask = old_mem->mask;
	unsigned long i;
	unsigned long page;
	unsigned long add = 0;
	int dir;
	
	
	ret = drm_mem_reg_ioremap(dev, old_mem, &old_iomap);
	if (ret)
		return ret;
	ret = drm_mem_reg_ioremap(dev, new_mem, &new_iomap);
	if (ret) 
		goto out;

	if (old_iomap == NULL && new_iomap == NULL)
		goto out2;
	
	add = 0;
	dir = 1;

	if ((old_mem->mem_type == new_mem->mem_type) && 
	    (new_mem->mm_node->start < 
	     old_mem->mm_node->start +  old_mem->mm_node->size)) {
		dir = -1;
		add = new_mem->num_pages - 1;
	}

	for (i=0; i < new_mem->num_pages; ++i) {
		page = i*dir + add; 
		if (old_iomap == NULL) 
			ret = drm_copy_ttm_io_page(ttm, new_iomap, page);
		else if (new_iomap == NULL)
			ret = drm_copy_io_ttm_page(ttm, old_iomap, page);
		else 
			ret = drm_copy_io_page(new_iomap, old_iomap, page);
		if (ret)
			goto out1;
	}
				
out2:	
	*old_mem = *new_mem;
	new_mem->mm_node = NULL;
	old_mem->mask = save_mask;
	DRM_FLAG_MASKED(save_flags, new_mem->flags, DRM_BO_MASK_MEMTYPE);
out1:
	drm_mem_reg_iounmap(dev, new_mem, &new_iomap);
out:
	drm_mem_reg_iounmap(dev, old_mem, old_iomap);
	return ret;
}
