/* 
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
 *      Aapo Tahkola <aet@rasterburn.org>
 *      Nicolai Haehnle <prefect_@gmx.net>
 *      Jérôme Glisse <glisse@freedesktop.org>
 */
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "radeon_cs.h"
#include "radeon_cs_gem.h"
#include "radeon_bo_gem.h"
#include "drm.h"
#include "xf86drm.h"
#include "radeon_drm.h"

#pragma pack(1)
struct cs_reloc_gem {
    uint32_t    handle;
    uint32_t    start_offset;
    uint32_t    end_offset;
    uint32_t    read_domain;
    uint32_t    write_domain;
    uint32_t    flags;
};
#pragma pack()

struct cs_gem {
    struct radeon_cs            base;
    struct drm_radeon_cs2       cs;
    struct drm_radeon_cs_chunk  chunks[2];
    unsigned                    nrelocs;
    uint32_t                    *relocs;
    struct radeon_bo            **relocs_bo;
};

static struct radeon_cs *cs_gem_create(struct radeon_cs_manager *csm,
                                       uint32_t ndw)
{
    struct cs_gem *csg;

    /* max cmd buffer size is 64Kb */
    if (ndw > (64 * 1024 / 4)) {
        return NULL;
    }
    csg = (struct cs_gem*)calloc(1, sizeof(struct cs_gem));
    if (csg == NULL) {
        return NULL;
    }
    csg->base.csm = csm;
    csg->base.ndw = 64 * 1024 / 4;
    csg->base.packets = (uint32_t*)calloc(1, 64 * 1024);
    if (csg->base.packets == NULL) {
        free(csg);
        return NULL;
    }
    csg->base.relocs_total_size = 0;
    csg->base.crelocs = 0;
    csg->nrelocs = 4096 / (4 * 4) ;
    csg->relocs_bo = (struct radeon_bo**)calloc(1,
                                                csg->nrelocs*sizeof(void*));
    if (csg->relocs_bo == NULL) {
        free(csg->base.packets);
        free(csg);
        return NULL;
    }
    csg->base.relocs = csg->relocs = (uint32_t*)calloc(1, 4096);
    if (csg->relocs == NULL) {
        free(csg->relocs_bo);
        free(csg->base.packets);
        free(csg);
        return NULL;
    }
    csg->chunks[0].chunk_id = RADEON_CHUNK_ID_IB;
    csg->chunks[0].length_dw = 0;
    csg->chunks[0].chunk_data = (uint64_t)(intptr_t)csg->base.packets;
    csg->chunks[1].chunk_id = RADEON_CHUNK_ID_RELOCS;
    csg->chunks[1].length_dw = 0;
    csg->chunks[1].chunk_data = (uint64_t)(intptr_t)csg->relocs;
    return (struct radeon_cs*)csg;
}

static int cs_gem_write_dword(struct radeon_cs *cs, uint32_t dword)
{
    struct cs_gem *csg = (struct cs_gem*)cs;
    if (cs->cdw >= cs->ndw) {
        uint32_t tmp, *ptr;
        tmp = (cs->cdw + 1 + 0x3FF) & (~0x3FF);
        ptr = (uint32_t*)realloc(cs->packets, 4 * tmp);
        if (ptr == NULL) {
            return -ENOMEM;
        }
        cs->packets = ptr;
        cs->ndw = tmp;
        csg->chunks[0].chunk_data = (uint64_t)(intptr_t)csg->base.packets;
    }
    cs->packets[cs->cdw++] = dword;
    csg->chunks[0].length_dw += 1;
    return 0;
}

static int cs_gem_write_reloc(struct radeon_cs *cs,
                              struct radeon_bo *bo,
                              uint32_t start_offset,
                              uint32_t end_offset,
                              uint32_t read_domain,
                              uint32_t write_domain,
                              uint32_t flags)
{
    struct cs_gem *csg = (struct cs_gem*)cs;
    struct cs_reloc_gem *reloc;
    uint32_t idx;
    unsigned i;

    /* check domains */
    if ((read_domain && write_domain) || (!read_domain && !write_domain)) {
        /* in one CS a bo can only be in read or write domain but not
         * in read & write domain at the same sime
         */
        return -EINVAL;
    }
    if (read_domain == RADEON_GEM_DOMAIN_CPU) {
        return -EINVAL;
    }
    if (write_domain == RADEON_GEM_DOMAIN_CPU) {
        return -EINVAL;
    }
    /* check reloc window */
    if (end_offset > bo->size) {
        return -EINVAL;
    }
    if (start_offset > end_offset) {
        return -EINVAL;
    }
    /* check if bo is already referenced */
    for(i = 0; i < cs->crelocs; i++) {
        idx = i * 6;
        reloc = (struct cs_reloc_gem*)&csg->relocs[idx];
        if (reloc->handle == bo->handle) {
            /* Check domains must be in read or write. As we check already
             * checked that in argument one of the read or write domain was
             * set we only need to check that if previous reloc as the read
             * domain set then the read_domain should also be set for this
             * new relocation.
             */
            if (reloc->read_domain && !read_domain) {
                return -EINVAL;
            }
            if (reloc->write_domain && !write_domain) {
                return -EINVAL;
            }
            reloc->read_domain |= read_domain;
            reloc->write_domain |= write_domain;
            /* update start and end offset */
            if (start_offset < reloc->start_offset) {
                reloc->start_offset = start_offset;
            }
            if (end_offset > reloc->end_offset) {
                reloc->end_offset = end_offset;
            }
            /* update flags */
            reloc->flags |= (flags & reloc->flags);
            /* write relocation packet */
            cs_gem_write_dword(cs, 0xc0001000);
            cs_gem_write_dword(cs, idx);
            return 0;
        }
    }
    /* new relocation */
    if (csg->base.crelocs >= csg->nrelocs) {
        /* allocate more memory (TODO: should use a slab allocatore maybe) */
        uint32_t *tmp, size;
        size = ((csg->nrelocs + 1) * sizeof(struct radeon_bo*));
        tmp = (uint32_t*)realloc(csg->relocs_bo, size);
        if (tmp == NULL) {
            return -ENOMEM;
        }
        csg->relocs_bo = (struct radeon_bo**)tmp;
        size = ((csg->nrelocs + 1) * 6 * 4);
        tmp = (uint32_t*)realloc(csg->relocs, size);
        if (tmp == NULL) {
            return -ENOMEM;
        }
        cs->relocs = csg->relocs = tmp;
        csg->nrelocs += 1;
        csg->chunks[1].chunk_data = (uint64_t)(intptr_t)csg->relocs;
    }
    csg->relocs_bo[csg->base.crelocs] = bo;
    idx = (csg->base.crelocs++) * 6;
    reloc = (struct cs_reloc_gem*)&csg->relocs[idx];
    reloc->handle = bo->handle;
    reloc->start_offset = start_offset;
    reloc->end_offset = end_offset;
    reloc->read_domain = read_domain;
    reloc->write_domain = write_domain;
    reloc->flags = flags;
    csg->chunks[1].length_dw += 6;
    radeon_bo_ref(bo);
    cs->relocs_total_size += bo->size;
    cs_gem_write_dword(cs, 0xc0001000);
    cs_gem_write_dword(cs, idx);
    return 0;
}

static int cs_gem_begin(struct radeon_cs *cs,
                        uint32_t ndw,
                        const char *file,
                        const char *func,
                        int line)
{
    return 0;
}

static int cs_gem_end(struct radeon_cs *cs,
                      const char *file,
                      const char *func,
                      int line)

{
    cs->section = 0;
    return 0;
}

static int cs_gem_emit(struct radeon_cs *cs)
{
    struct cs_gem *csg = (struct cs_gem*)cs;
    uint64_t chunk_array[2];
    int r;

    chunk_array[0] = (uint64_t)(intptr_t)&csg->chunks[0];
    chunk_array[1] = (uint64_t)(intptr_t)&csg->chunks[1];

    csg->cs.num_chunks = 2;
    csg->cs.chunks = (uint64_t)(intptr_t)chunk_array;

    r = drmCommandWriteRead(cs->csm->fd, DRM_RADEON_CS2,
                            &csg->cs, sizeof(struct drm_radeon_cs2));
    if (r) {
        return r;
    }
    return 0;
}

static int cs_gem_destroy(struct radeon_cs *cs)
{
    struct cs_gem *csg = (struct cs_gem*)cs;

    free(csg->relocs_bo);
    free(cs->relocs);
    free(cs->packets);
    free(cs);
    return 0;
}

static int cs_gem_erase(struct radeon_cs *cs)
{
    struct cs_gem *csg = (struct cs_gem*)cs;

    cs->relocs_total_size = 0;
    cs->cdw = 0;
    cs->section = 0;
    cs->crelocs = 0;
    csg->chunks[0].length_dw = 0;
    csg->chunks[1].length_dw = 0;
    return 0;
}

static int cs_gem_need_flush(struct radeon_cs *cs)
{
    return (cs->relocs_total_size > (16*1024*1024));
}

static struct radeon_cs_funcs radeon_cs_gem_funcs = {
    cs_gem_create,
    cs_gem_write_dword,
    cs_gem_write_reloc,
    cs_gem_begin,
    cs_gem_end,
    cs_gem_emit,
    cs_gem_destroy,
    cs_gem_erase,
    cs_gem_need_flush
};

struct radeon_cs_manager *radeon_cs_manager_gem(int fd)
{
    struct radeon_cs_manager *csm;

    csm = (struct radeon_cs_manager*)calloc(1,
                                            sizeof(struct radeon_cs_manager));
    if (csm == NULL) {
        return NULL;
    }
    csm->funcs = &radeon_cs_gem_funcs;
    csm->fd = fd;
    return csm;
}

void radeon_cs_manager_gem_shutdown(struct radeon_cs_manager *csm)
{
    free(csm);
}
