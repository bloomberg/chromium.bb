/* 
 * Copyright © 2008 Jérôme Glisse
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */
/*
 * Authors:
 *      Jérôme Glisse <glisse@freedesktop.org>
 */
#ifndef RADEON_BO_H
#define RADEON_BO_H

#include <stdint.h>

/* bo object */
#define RADEON_BO_FLAGS_MACRO_TILE  1
#define RADEON_BO_FLAGS_MICRO_TILE  2

struct radeon_bo_manager;

struct radeon_bo {
    uint32_t                    alignment;
    uint32_t                    handle;
    uint32_t                    size;
    uint32_t                    flags;
    void                        *ptr;
    struct radeon_bo_manager    *bom;
};

/* bo functions */
struct radeon_bo_funcs {
    struct radeon_bo *(*bo_open)(struct radeon_bo_manager *bom,
                                 uint32_t handle,
                                 uint32_t size,
                                 uint32_t alignment,
                                 uint32_t flags);
    void (*bo_close)(struct radeon_bo *bo);
    void (*bo_pin)(struct radeon_bo *bo);
    void (*bo_unpin)(struct radeon_bo *bo);
    int (*bo_map)(struct radeon_bo *bo, unsigned int flags);
    int (*bo_unmap)(struct radeon_bo *bo);
};

struct radeon_bo_manager {
    struct radeon_bo_funcs  *funcs;
    int                     fd;
};

static inline struct radeon_bo *radeon_bo_open(struct radeon_bo_manager *bom,
                                               uint32_t handle,
                                               uint32_t size,
                                               uint32_t alignment,
                                               uint32_t flags)
{
    return bom->funcs->bo_open(bom, handle, size, alignment, flags);
}

static inline void radeon_bo_close(struct radeon_bo *bo)
{
    return bo->bom->funcs->bo_close(bo);
}

static inline void radeon_bo_pin(struct radeon_bo *bo)
{
    return bo->bom->funcs->bo_pin(bo);
}

static inline void radeon_bo_unpin(struct radeon_bo *bo)
{
    return bo->bom->funcs->bo_unpin(bo);
}

static inline int radeon_bo_map(struct radeon_bo *bo, unsigned int flags)
{
    return bo->bom->funcs->bo_map(bo, flags);
}

static inline int radeon_bo_unmap(struct radeon_bo *bo)
{
    return bo->bom->funcs->bo_unmap(bo);
}

#endif
