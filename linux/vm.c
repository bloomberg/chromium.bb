/* vm.c -- Memory mapping for DRM -*- linux-c -*-
 * Created: Mon Jan  4 08:58:31 1999 by faith@precisioninsight.com
 * Revised: Fri Aug 20 22:48:11 1999 by faith@precisioninsight.com
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
 * $PI: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/vm.c,v 1.7 1999/08/21 02:48:34 faith Exp $
 * $XFree86: xc/programs/Xserver/hw/xfree86/os-support/linux/drm/kernel/vm.c,v 1.1 1999/09/25 14:38:02 dawes Exp $
 *
 */

#define __NO_VERSION__
#include "drmP.h"

struct vm_operations_struct   drm_vm_ops = {
	nopage:	 drm_vm_nopage,
	open:	 drm_vm_open,
	close:	 drm_vm_close,
};

struct vm_operations_struct   drm_vm_shm_ops = {
	nopage:	 drm_vm_shm_nopage,
	open:	 drm_vm_open,
	close:	 drm_vm_close,
};

struct vm_operations_struct   drm_vm_dma_ops = {
	nopage:	 drm_vm_dma_nopage,
	open:	 drm_vm_open,
	close:	 drm_vm_close,
};

unsigned long drm_vm_nopage(struct vm_area_struct *vma,
			    unsigned long address,
			    int write_access)
{
	DRM_DEBUG("0x%08lx, %d\n", address, write_access);

	return 0;		/* Disallow mremap */
}

unsigned long drm_vm_shm_nopage(struct vm_area_struct *vma,
				unsigned long address,
				int write_access)
{
	drm_file_t	 *priv	 = vma->vm_file->private_data;
	drm_device_t	 *dev	 = priv->dev;
	unsigned long	 physical;
	unsigned long	 offset;
	unsigned long	 page;

	if (address > vma->vm_end) return 0; /* Disallow mremap */
	if (!dev->lock.hw_lock)    return 0; /* Nothing allocated */

	offset	 = address - vma->vm_start;
	page	 = offset >> PAGE_SHIFT;
	physical = (unsigned long)dev->lock.hw_lock + (offset & (~PAGE_MASK));
	atomic_inc(&mem_map[MAP_NR(physical)].count); /* Dec. by kernel */

	DRM_DEBUG("0x%08lx (page %lu) => 0x%08lx\n", address, page, physical);
	return physical;
}

unsigned long drm_vm_dma_nopage(struct vm_area_struct *vma,
				unsigned long address,
				int write_access)
{
	drm_file_t	 *priv	 = vma->vm_file->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_device_dma_t *dma	 = dev->dma;
	unsigned long	 physical;
	unsigned long	 offset;
	unsigned long	 page;

	if (!dma)		   return 0; /* Error */
	if (address > vma->vm_end) return 0; /* Disallow mremap */
	if (!dma->pagelist)	   return 0; /* Nothing allocated */

	offset	 = address - vma->vm_start; /* vm_offset should be 0 */
	page	 = offset >> PAGE_SHIFT;
	physical = dma->pagelist[page] + (offset & (~PAGE_MASK));
	atomic_inc(&mem_map[MAP_NR(physical)].count); /* Dec. by kernel */

	DRM_DEBUG("0x%08lx (page %lu) => 0x%08lx\n", address, page, physical);
	return physical;
}

void drm_vm_open(struct vm_area_struct *vma)
{
	drm_file_t	*priv	= vma->vm_file->private_data;
	drm_device_t	*dev	= priv->dev;
#if DRM_DEBUG_CODE
	drm_vma_entry_t *vma_entry;
#endif

	DRM_DEBUG("0x%08lx,0x%08lx\n",
		  vma->vm_start, vma->vm_end - vma->vm_start);
	atomic_inc(&dev->vma_count);
	MOD_INC_USE_COUNT;

#if DRM_DEBUG_CODE
	vma_entry = drm_alloc(sizeof(*vma_entry), DRM_MEM_VMAS);
	if (vma_entry) {
		down(&dev->struct_sem);
		vma_entry->vma	= vma;
		vma_entry->next = dev->vmalist;
		vma_entry->pid	= current->pid;
		dev->vmalist	= vma_entry;
		up(&dev->struct_sem);
	}
#endif
}

void drm_vm_close(struct vm_area_struct *vma)
{
	drm_file_t	*priv	= vma->vm_file->private_data;
	drm_device_t	*dev	= priv->dev;
#if DRM_DEBUG_CODE
	drm_vma_entry_t *pt, *prev;
#endif

	DRM_DEBUG("0x%08lx,0x%08lx\n",
		  vma->vm_start, vma->vm_end - vma->vm_start);
	MOD_DEC_USE_COUNT;
	atomic_dec(&dev->vma_count);

#if DRM_DEBUG_CODE
	down(&dev->struct_sem);
	for (pt = dev->vmalist, prev = NULL; pt; prev = pt, pt = pt->next) {
		if (pt->vma == vma) {
			if (prev) {
				prev->next = pt->next;
			} else {
				dev->vmalist = pt->next;
			}
			drm_free(pt, sizeof(*pt), DRM_MEM_VMAS);
			break;
		}
	}
	up(&dev->struct_sem);
#endif
}

int drm_mmap_dma(struct file *filp, struct vm_area_struct *vma)
{
	drm_file_t	 *priv	 = filp->private_data;
	drm_device_t	 *dev	 = priv->dev;
	drm_device_dma_t *dma	 = dev->dma;
	unsigned long	 length	 = vma->vm_end - vma->vm_start;
	
	DRM_DEBUG("start = 0x%lx, end = 0x%lx, offset = 0x%lx\n",
		  vma->vm_start, vma->vm_end, vma->vm_offset);

				/* Length must match exact page count */
	if ((length >> PAGE_SHIFT) != dma->page_count) return -EINVAL;

	vma->vm_ops   = &drm_vm_dma_ops;
	vma->vm_flags |= VM_LOCKED | VM_SHM; /* Don't swap */
	
#if LINUX_VERSION_CODE < 0x020203 /* KERNEL_VERSION(2,2,3) */
				/* In Linux 2.2.3 and above, this is
				   handled in do_mmap() in mm/mmap.c. */
	++filp->f_count;
#endif
	vma->vm_file  =	 filp;	/* Needed for drm_vm_open() */
	drm_vm_open(vma);
	return 0;
}

int drm_mmap(struct file *filp, struct vm_area_struct *vma)
{
	drm_file_t	*priv	= filp->private_data;
	drm_device_t	*dev	= priv->dev;
	drm_map_t	*map	= NULL;
	int		i;
	
	DRM_DEBUG("start = 0x%lx, end = 0x%lx, offset = 0x%lx\n",
		  vma->vm_start, vma->vm_end, vma->vm_offset);

	if (!vma->vm_offset) return drm_mmap_dma(filp, vma);

				/* A sequential search of a linked list is
				   fine here because: 1) there will only be
				   about 5-10 entries in the list and, 2) a
				   DRI client only has to do this mapping
				   once, so it doesn't have to be optimized
				   for performance, even if the list was a
				   bit longer. */
	for (i = 0; i < dev->map_count; i++) {
		map = dev->maplist[i];
		if (map->offset == vma->vm_offset) break;
	}
	
	if (i >= dev->map_count) return -EINVAL;
	if (!map || ((map->flags&_DRM_RESTRICTED) && !capable(CAP_SYS_ADMIN)))
		return -EPERM;

				/* Check for valid size. */
	if (map->size != vma->vm_end - vma->vm_start) return -EINVAL;
	

	switch (map->type) {
	case _DRM_FRAME_BUFFER:
	case _DRM_REGISTERS:
		if (vma->vm_offset >= __pa(high_memory)) {
#if defined(__i386__)
			if (boot_cpu_data.x86 > 3) {
				pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
				pgprot_val(vma->vm_page_prot) &= ~_PAGE_PWT;
			}
#endif
			vma->vm_flags |= VM_IO;	/* not in core dump */
		}
		if (remap_page_range(vma->vm_start,
				     vma->vm_offset,
				     vma->vm_end - vma->vm_start,
				     vma->vm_page_prot))
				return -EAGAIN;
		vma->vm_ops = &drm_vm_ops;
		break;
	case _DRM_SHM:
		vma->vm_ops = &drm_vm_shm_ops;
				/* Don't let this area swap.  Change when
				   DRM_KERNEL advisory is supported. */
		vma->vm_flags |= VM_LOCKED;
		break;
	default:
		return -EINVAL;	/* This should never happen. */
	}
	vma->vm_flags |= VM_LOCKED | VM_SHM; /* Don't swap */
	if (map->flags & _DRM_READ_ONLY) {
		pgprot_val(vma->vm_page_prot) &= ~_PAGE_RW;
	}

	
#if LINUX_VERSION_CODE < 0x020203 /* KERNEL_VERSION(2,2,3) */
				/* In Linux 2.2.3 and above, this is
				   handled in do_mmap() in mm/mmap.c. */
	++filp->f_count;
#endif
	vma->vm_file  =	 filp;	/* Needed for drm_vm_open() */
	drm_vm_open(vma);
	return 0;
}
