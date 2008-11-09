/* 
 * Copyright © 2008 Dave Airlie
 * Copyright © 2008 Jérôme Glisse
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS, AUTHORS
 * AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 */
/*
 * Authors:
 *      Dave Airlie
 *      Jérôme Glisse <glisse@freedesktop.org>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "xf86drm.h"
#include "drm.h"
#include "radeon_drm.h"
#include "radeon_bo.h"
#include "radeon_bo_gem.h"

struct radeon_bo_gem {
    struct radeon_bo    base;
    uint32_t            name;
    int                 map_count;
};

struct bo_manager_gem {
    struct radeon_bo_manager    base;
};

static struct radeon_bo *bo_open(struct radeon_bo_manager *bom,
                                 uint32_t handle,
                                 uint32_t size,
                                 uint32_t alignment,
                                 uint32_t domains,
                                 uint32_t flags)
{
    struct radeon_bo_gem *bo;
    int r;

    bo = (struct radeon_bo_gem*)calloc(1, sizeof(struct radeon_bo_gem));
    if (bo == NULL) {
        return NULL;
    }

    bo->base.bom = bom;
    bo->base.handle = 0;
    bo->base.size = size;
    bo->base.alignment = alignment;
    bo->base.domains = domains;
    bo->base.flags = flags;
    bo->base.ptr = NULL;
    bo->map_count = 0;
    if (handle) {
        struct drm_gem_open open_arg;

        memset(&open_arg, 0, sizeof(open_arg));
        open_arg.name = handle;
        r = ioctl(bom->fd, DRM_IOCTL_GEM_OPEN, &open_arg);
        if (r != 0) {
            fprintf(stderr, "GEM open failed: %d (%s)\n",r,strerror(r));
            free(bo);
            return NULL;
        }
        bo->base.handle = open_arg.handle;
        bo->base.size = open_arg.size;
        bo->name = handle;
    } else {
        struct drm_radeon_gem_create args;

        args.size = size;
        args.alignment = alignment;
        args.initial_domain = bo->base.domains;
        args.no_backing_store = 0;
        r = drmCommandWriteRead(bom->fd, DRM_RADEON_GEM_CREATE,
                                &args, sizeof(args));
        bo->base.handle = args.handle;
        if (r) {
            free(bo);
            return NULL;
        }
    }
    radeon_bo_ref(bo);
    return (struct radeon_bo*)bo;
}

static void bo_ref(struct radeon_bo *bo)
{
}

static void bo_unref(struct radeon_bo *bo)
{
    struct radeon_bo_gem *bo_gem = (struct radeon_bo_gem*)bo;
    struct drm_gem_close args;

    if (bo == NULL) {
        return;
    }
    if (bo->cref) {
        return;
    }
    if (bo_gem->map_count) {
        munmap(bo->ptr, bo->size);
    }

    /* close object */
    args.handle = bo->handle;
    ioctl(bo->bom->fd, DRM_IOCTL_GEM_CLOSE, &args);
    free(bo_gem);
}

static int bo_map(struct radeon_bo *bo, int write)
{
    struct radeon_bo_gem *bo_gem = (struct radeon_bo_gem*)bo;
    struct drm_radeon_gem_mmap args;
    int r;
    uint8_t *tt;

    if (bo_gem->map_count++ != 0) {
        return 0;
    }
    bo->ptr = NULL;
    args.handle = bo->handle;
    args.offset = 0;
    args.size = (uint64_t)bo->size;
    r = drmCommandWriteRead(bo->bom->fd,
                            DRM_RADEON_GEM_MMAP,
                            &args,
                            sizeof(args));
    if (!r) {
        bo->ptr = (void *)(unsigned long)args.addr_ptr;
    }
    tt = bo->ptr;
    return r;
}

static int bo_unmap(struct radeon_bo *bo)
{
    struct radeon_bo_gem *bo_gem = (struct radeon_bo_gem*)bo;

    if (--bo_gem->map_count > 0) {
        return 0;
    }
    munmap(bo->ptr, bo->size);
    bo->ptr = NULL;
    return 0;
}

static struct radeon_bo_funcs bo_gem_funcs = {
    bo_open,
    bo_ref,
    bo_unref,
    bo_map,
    bo_unmap
};

struct radeon_bo_manager *radeon_bo_manager_gem(int fd)
{
    struct bo_manager_gem *bomg;

    bomg = (struct bo_manager_gem*)calloc(1, sizeof(struct bo_manager_gem));
    if (bomg == NULL) {
        return NULL;
    }
    bomg->base.funcs = &bo_gem_funcs;
    bomg->base.fd = fd;
    return (struct radeon_bo_manager*)bomg;
}

void radeon_bo_manager_gem_shutdown(struct radeon_bo_manager *bom)
{
    struct bo_manager_gem *bomg = (struct bo_manager_gem*)bom;

    if (bom == NULL) {
        return;
    }
    free(bomg);
}
