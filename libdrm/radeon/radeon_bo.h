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

#include <stdio.h>
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
    unsigned                    cref;
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
    void (*bo_ref)(struct radeon_bo *bo);
    void (*bo_unref)(struct radeon_bo *bo);
    int (*bo_map)(struct radeon_bo *bo, int write);
    int (*bo_unmap)(struct radeon_bo *bo);
};

struct radeon_bo_manager {
    struct radeon_bo_funcs  *funcs;
    int                     fd;
};

static inline struct radeon_bo *_radeon_bo_open(struct radeon_bo_manager *bom,
                                                uint32_t handle,
                                                uint32_t size,
                                                uint32_t alignment,
                                                uint32_t flags,
                                                const char *file,
                                                const char *func,
                                                int line)
{
    struct radeon_bo *bo;
    bo = bom->funcs->bo_open(bom, handle, size, alignment, flags);
#ifdef RADEON_BO_TRACK_OPEN
    if (bo) {
        fprintf(stderr, "+open (%p, %d, %d) at (%s, %s, %d)\n",
                bo, bo->size, bo->cref, file, func, line);
    }
#endif
    return bo;
}

static inline void _radeon_bo_ref(struct radeon_bo *bo,
                                  const char *file,
                                  const char *func,
                                  int line)
{
    bo->cref++;
#ifdef RADEON_BO_TRACK_REF
    fprintf(stderr, "+ref  (%p, %d, %d) at (%s, %s, %d)\n",
            bo, bo->size, bo->cref, file, func, line);
#endif
    bo->bom->funcs->bo_ref(bo);
}

static inline void _radeon_bo_unref(struct radeon_bo *bo,
                                    const char *file,
                                    const char *func,
                                    int line)
{
    bo->cref--;
#ifdef RADEON_BO_TRACK_REF
    fprintf(stderr, "-unref(%p, %d, %d) at (%s, %s, %d)\n",
            bo, bo->size, bo->cref, file, func, line);
#endif
    bo->bom->funcs->bo_unref(bo);
}

static inline int _radeon_bo_map(struct radeon_bo *bo,
                                 int write,
                                 const char *file,
                                 const char *func,
                                 int line)
{
#ifdef RADEON_BO_TRACK_MAP
    fprintf(stderr, "+map  (%p, %d, %d) at (%s, %s, %d)\n",
            bo, bo->size, bo->cref, file, func, line);
#endif
    return bo->bom->funcs->bo_map(bo, write);
}

static inline int _radeon_bo_unmap(struct radeon_bo *bo,
                                   const char *file,
                                   const char *func,
                                   int line)
{
#ifdef RADEON_BO_TRACK_MAP
    fprintf(stderr, "-unmap(%p, %d, %d) at (%s, %s, %d)\n",
            bo, bo->size, bo->cref, file, func, line);
#endif
    return bo->bom->funcs->bo_unmap(bo);
}

#define radeon_bo_open(bom, h, s, a, f)\
    _radeon_bo_open(bom, h, s, a, f, __FILE__, __FUNCTION__, __LINE__)
#define radeon_bo_ref(bo)\
    _radeon_bo_ref(bo, __FILE__, __FUNCTION__, __LINE__)
#define radeon_bo_unref(bo)\
    _radeon_bo_unref(bo, __FILE__, __FUNCTION__, __LINE__)
#define radeon_bo_map(bo, w)\
    _radeon_bo_map(bo, w, __FILE__, __FUNCTION__, __LINE__)
#define radeon_bo_unmap(bo)\
    _radeon_bo_unmap(bo, __FILE__, __FUNCTION__, __LINE__)

#endif
