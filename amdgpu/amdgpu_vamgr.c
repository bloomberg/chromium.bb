/*
 * Copyright 2014 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
*/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "amdgpu.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"
#include "util_math.h"

static struct amdgpu_bo_va_mgr vamgr = {{0}};

static void amdgpu_vamgr_init(struct amdgpu_bo_va_mgr *mgr, struct amdgpu_device *dev)
{
	mgr->va_offset = dev->dev_info.virtual_address_offset;
	mgr->va_max = dev->dev_info.virtual_address_max;
	mgr->va_alignment = dev->dev_info.virtual_address_alignment;

	list_inithead(&mgr->va_holes);
	pthread_mutex_init(&mgr->bo_va_mutex, NULL);
}

static void amdgpu_vamgr_deinit(struct amdgpu_bo_va_mgr *mgr)
{
	struct amdgpu_bo_va_hole *hole;
	LIST_FOR_EACH_ENTRY(hole, &mgr->va_holes, list) {
		list_del(&hole->list);
		free(hole);
	}
	pthread_mutex_destroy(&mgr->bo_va_mutex);
}

struct amdgpu_bo_va_mgr * amdgpu_vamgr_get_global(struct amdgpu_device *dev)
{
	int ref;
	ref = atomic_inc_return(&vamgr.refcount);

	if (ref == 1)
		amdgpu_vamgr_init(&vamgr, dev);
	return &vamgr;
}

void amdgpu_vamgr_reference(struct amdgpu_bo_va_mgr **dst,
				struct amdgpu_bo_va_mgr *src)
{
	if (update_references(&(*dst)->refcount, NULL))
		amdgpu_vamgr_deinit(*dst);
	*dst = src;
}

uint64_t amdgpu_vamgr_find_va(struct amdgpu_bo_va_mgr *mgr, uint64_t size,
				uint64_t alignment, uint64_t base_required)
{
	struct amdgpu_bo_va_hole *hole, *n;
	uint64_t offset = 0, waste = 0;

	alignment = MAX2(alignment, mgr->va_alignment);
	size = ALIGN(size, mgr->va_alignment);

	if (base_required % alignment)
		return AMDGPU_INVALID_VA_ADDRESS;

	pthread_mutex_lock(&mgr->bo_va_mutex);
	/* TODO: using more appropriate way to track the holes */
	/* first look for a hole */
	LIST_FOR_EACH_ENTRY_SAFE(hole, n, &vamgr.va_holes, list) {
		if (base_required) {
			if(hole->offset > base_required ||
				(hole->offset + hole->size) < (base_required + size))
				continue;
			waste = base_required - hole->offset;
			offset = base_required;
		} else {
			offset = hole->offset;
			waste = offset % alignment;
			waste = waste ? alignment - waste : 0;
			offset += waste;
			if (offset >= (hole->offset + hole->size)) {
				continue;
			}
		}
		if (!waste && hole->size == size) {
			offset = hole->offset;
			list_del(&hole->list);
			free(hole);
			pthread_mutex_unlock(&mgr->bo_va_mutex);
			return offset;
		}
		if ((hole->size - waste) > size) {
			if (waste) {
				n = calloc(1, sizeof(struct amdgpu_bo_va_hole));
				n->size = waste;
				n->offset = hole->offset;
				list_add(&n->list, &hole->list);
			}
			hole->size -= (size + waste);
			hole->offset += size + waste;
			pthread_mutex_unlock(&mgr->bo_va_mutex);
			return offset;
		}
		if ((hole->size - waste) == size) {
			hole->size = waste;
			pthread_mutex_unlock(&mgr->bo_va_mutex);
			return offset;
		}
	}

	if (base_required) {
		if (base_required < mgr->va_offset)
			return AMDGPU_INVALID_VA_ADDRESS;
		offset = mgr->va_offset;
		waste = base_required - mgr->va_offset;
	} else {
		offset = mgr->va_offset;
		waste = offset % alignment;
		waste = waste ? alignment - waste : 0;
	}

	if (offset + waste + size > mgr->va_max) {
		pthread_mutex_unlock(&mgr->bo_va_mutex);
		return AMDGPU_INVALID_VA_ADDRESS;
	}

	if (waste) {
		n = calloc(1, sizeof(struct amdgpu_bo_va_hole));
		n->size = waste;
		n->offset = offset;
		list_add(&n->list, &mgr->va_holes);
	}

	offset += waste;
	mgr->va_offset += size + waste;
	pthread_mutex_unlock(&mgr->bo_va_mutex);
	return offset;
}

void amdgpu_vamgr_free_va(struct amdgpu_bo_va_mgr *mgr,
				uint64_t va, uint64_t size)
{
	struct amdgpu_bo_va_hole *hole;

	if (va == AMDGPU_INVALID_VA_ADDRESS)
		return;

	size = ALIGN(size, mgr->va_alignment);

	pthread_mutex_lock(&mgr->bo_va_mutex);
	if ((va + size) == mgr->va_offset) {
		mgr->va_offset = va;
		/* Delete uppermost hole if it reaches the new top */
		if (!LIST_IS_EMPTY(&mgr->va_holes)) {
			hole = container_of(mgr->va_holes.next, hole, list);
			if ((hole->offset + hole->size) == va) {
				mgr->va_offset = hole->offset;
				list_del(&hole->list);
				free(hole);
			}
		}
	} else {
		struct amdgpu_bo_va_hole *next;

		hole = container_of(&mgr->va_holes, hole, list);
		LIST_FOR_EACH_ENTRY(next, &mgr->va_holes, list) {
			if (next->offset < va)
				break;
			hole = next;
		}

		if (&hole->list != &mgr->va_holes) {
			/* Grow upper hole if it's adjacent */
			if (hole->offset == (va + size)) {
				hole->offset = va;
				hole->size += size;
				/* Merge lower hole if it's adjacent */
				if (next != hole
						&& &next->list != &mgr->va_holes
						&& (next->offset + next->size) == va) {
					next->size += hole->size;
					list_del(&hole->list);
					free(hole);
				}
				goto out;
			}
		}

		/* Grow lower hole if it's adjacent */
		if (next != hole && &next->list != &mgr->va_holes &&
				(next->offset + next->size) == va) {
			next->size += size;
			goto out;
		}

		/* FIXME on allocation failure we just lose virtual address space
		 * maybe print a warning
		 */
		next = calloc(1, sizeof(struct amdgpu_bo_va_hole));
		if (next) {
			next->size = size;
			next->offset = va;
			list_add(&next->list, &hole->list);
		}
	}
out:
	pthread_mutex_unlock(&mgr->bo_va_mutex);
}

int amdgpu_va_range_alloc(amdgpu_device_handle dev,
			  enum amdgpu_gpu_va_range va_range_type,
			  uint64_t size,
			  uint64_t va_base_alignment,
			  uint64_t va_base_required,
			  uint64_t *va_base_allocated,
			  amdgpu_va_handle *va_range_handle)
{
	va_base_alignment = MAX2(va_base_alignment, dev->vamgr->va_alignment);
	size = ALIGN(size, vamgr.va_alignment);

	*va_base_allocated = amdgpu_vamgr_find_va(dev->vamgr, size,
					va_base_alignment, va_base_required);

	if (*va_base_allocated != AMDGPU_INVALID_VA_ADDRESS) {
		struct amdgpu_va* va;
		va = calloc(1, sizeof(struct amdgpu_va));
		if(!va){
			amdgpu_vamgr_free_va(dev->vamgr, *va_base_allocated, size);
			return -ENOMEM;
		}
		va->dev = dev;
		va->address = *va_base_allocated;
		va->size = size;
		va->range = va_range_type;
		*va_range_handle = va;
	} else {
		return -EINVAL;
	}

	return 0;
}

int amdgpu_va_range_free(amdgpu_va_handle va_range_handle)
{
	if(!va_range_handle || !va_range_handle->address)
		return 0;
	amdgpu_vamgr_free_va(va_range_handle->dev->vamgr, va_range_handle->address,
			va_range_handle->size);
	free(va_range_handle);
	return 0;
}
