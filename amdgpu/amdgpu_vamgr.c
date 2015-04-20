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
#include "amdgpu.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"
#include "util_math.h"

void amdgpu_vamgr_init(struct amdgpu_device *dev)
{
	struct amdgpu_bo_va_mgr *vamgr = &dev->vamgr;

	vamgr->va_offset = dev->dev_info.virtual_address_offset;
	vamgr->va_alignment = dev->dev_info.virtual_address_alignment;

	list_inithead(&vamgr->va_holes);
	pthread_mutex_init(&vamgr->bo_va_mutex, NULL);
}

uint64_t amdgpu_vamgr_find_va(struct amdgpu_bo_va_mgr *mgr,
                               uint64_t size, uint64_t alignment)
{
        struct amdgpu_bo_va_hole *hole, *n;
        uint64_t offset = 0, waste = 0;

        alignment = MAX2(alignment, mgr->va_alignment);
        size = ALIGN(size, mgr->va_alignment);

        pthread_mutex_lock(&mgr->bo_va_mutex);
        /* TODO: using more appropriate way to track the holes */
        /* first look for a hole */
        LIST_FOR_EACH_ENTRY_SAFE(hole, n, &mgr->va_holes, list) {
                offset = hole->offset;
                waste = offset % alignment;
                waste = waste ? alignment - waste : 0;
                offset += waste;
                if (offset >= (hole->offset + hole->size)) {
                        continue;
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
                                n = calloc(1,
                                           sizeof(struct amdgpu_bo_va_hole));
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

        offset = mgr->va_offset;
        waste = offset % alignment;
        waste = waste ? alignment - waste : 0;
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

void amdgpu_vamgr_free_va(struct amdgpu_bo_va_mgr *mgr, uint64_t va,
                           uint64_t size)
{
        struct amdgpu_bo_va_hole *hole;

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
