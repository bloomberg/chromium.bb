/*
 * Copyright 2007 Nouveau Project
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
 * THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#include "nouveau_private.h"

#define PB_BUFMGR_DWORDS   (4096 / 2)
#define PB_MIN_USER_DWORDS  2048

static uint32_t
nouveau_pushbuf_calc_reloc(struct drm_nouveau_gem_pushbuf_bo *pbbo,
			   struct drm_nouveau_gem_pushbuf_reloc *r)
{
	uint32_t push = 0;

	if (r->flags & NOUVEAU_GEM_RELOC_LOW)
		push = (pbbo->presumed_offset + r->data);
	else
	if (r->flags & NOUVEAU_GEM_RELOC_HIGH)
		push = (pbbo->presumed_offset + r->data) >> 32;
	else
		push = r->data;

	if (r->flags & NOUVEAU_GEM_RELOC_OR) {
		if (pbbo->presumed_domain & NOUVEAU_GEM_DOMAIN_VRAM)
			push |= r->vor;
		else
			push |= r->tor;
	}

	return push;
}

int
nouveau_pushbuf_emit_reloc(struct nouveau_channel *chan, void *ptr,
			   struct nouveau_bo *bo, uint32_t data, uint32_t data2,
			   uint32_t flags, uint32_t vor, uint32_t tor)
{
	struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(chan->pushbuf);
	struct nouveau_bo_priv *nvbo = nouveau_bo(bo);
	struct drm_nouveau_gem_pushbuf_reloc *r;
	struct drm_nouveau_gem_pushbuf_bo *pbbo;
	uint32_t domains = 0;

	if (nvpb->nr_relocs >= NOUVEAU_GEM_MAX_RELOCS) {
		fprintf(stderr, "too many relocs!!\n");
		return -ENOMEM;
	}

	if (nvbo->user && (flags & NOUVEAU_BO_WR)) {
		fprintf(stderr, "write to user buffer!!\n");
		return -EINVAL;
	}

	pbbo = nouveau_bo_emit_buffer(chan, bo);
	if (!pbbo) {
		fprintf(stderr, "buffer emit fail :(\n");
		return -ENOMEM;
	}

	nvbo->pending_refcnt++;

	if (flags & NOUVEAU_BO_VRAM)
		domains |= NOUVEAU_GEM_DOMAIN_VRAM;
	if (flags & NOUVEAU_BO_GART)
		domains |= NOUVEAU_GEM_DOMAIN_GART;
	pbbo->valid_domains &= domains;
	assert(pbbo->valid_domains);

	assert(flags & NOUVEAU_BO_RDWR);
	if (flags & NOUVEAU_BO_RD) {
		pbbo->read_domains |= domains;
	}
	if (flags & NOUVEAU_BO_WR) {
		pbbo->write_domains |= domains;
		nvbo->write_marker = 1;
	}

	r = nvpb->relocs + nvpb->nr_relocs++;
	r->bo_index = pbbo - nvpb->buffers;
	r->reloc_index = (uint32_t *)ptr - nvpb->pushbuf;
	r->flags = 0;
	if (flags & NOUVEAU_BO_LOW)
		r->flags |= NOUVEAU_GEM_RELOC_LOW;
	if (flags & NOUVEAU_BO_HIGH)
		r->flags |= NOUVEAU_GEM_RELOC_HIGH;
	if (flags & NOUVEAU_BO_OR)
		r->flags |= NOUVEAU_GEM_RELOC_OR;
	r->data = data;
	r->vor = vor;
	r->tor = tor;

	*(uint32_t *)ptr = (flags & NOUVEAU_BO_DUMMY) ? 0 :
		nouveau_pushbuf_calc_reloc(pbbo, r);
	return 0;
}

static int
nouveau_pushbuf_space_call(struct nouveau_channel *chan, unsigned min)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;
	struct nouveau_bo *bo;
	int ret;

	if (min < PB_MIN_USER_DWORDS)
		min = PB_MIN_USER_DWORDS;

	nvpb->current_offset = nvpb->base.cur - nvpb->pushbuf;
	if (nvpb->current_offset + min + 2 <= nvpb->size)
		return 0;

	nvpb->current++;
	if (nvpb->current == CALPB_BUFFERS)
		nvpb->current = 0;
	bo = nvpb->buffer[nvpb->current];

	ret = nouveau_bo_map(bo, NOUVEAU_BO_WR);
	if (ret)
		return ret;

	nvpb->size = (bo->size - 8) / 4;
	nvpb->pushbuf = bo->map;
	nvpb->current_offset = 0;

	nvpb->base.channel = chan;
	nvpb->base.remaining = nvpb->size;
	nvpb->base.cur = nvpb->pushbuf;

	nouveau_bo_unmap(bo);
	return 0;
}

static int
nouveau_pushbuf_space(struct nouveau_channel *chan, unsigned min)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;

	if (nvpb->use_cal)
		return nouveau_pushbuf_space_call(chan, min);

	if (nvpb->pushbuf) {
		free(nvpb->pushbuf);
		nvpb->pushbuf = NULL;
	}

	nvpb->size = min < PB_MIN_USER_DWORDS ? PB_MIN_USER_DWORDS : min;	
	nvpb->pushbuf = malloc(sizeof(uint32_t) * nvpb->size);

	nvpb->base.channel = chan;
	nvpb->base.remaining = nvpb->size;
	nvpb->base.cur = nvpb->pushbuf;
	
	return 0;
}

static void
nouveau_pushbuf_fini_call(struct nouveau_channel *chan)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;
	int i;

	for (i = 0; i < CALPB_BUFFERS; i++)
		nouveau_bo_ref(NULL, &nvpb->buffer[i]);
	nvpb->use_cal = 0;
	nvpb->pushbuf = NULL;
}

static void
nouveau_pushbuf_init_call(struct nouveau_channel *chan)
{
	struct drm_nouveau_gem_pushbuf_call req;
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;
	struct nouveau_device *dev = chan->device;
	int i, ret;

	req.channel = chan->id;
	req.handle = 0;
	ret = drmCommandWriteRead(nouveau_device(dev)->fd,
				  DRM_NOUVEAU_GEM_PUSHBUF_CALL2,
				  &req, sizeof(req));
	if (ret) {
		ret = drmCommandWriteRead(nouveau_device(dev)->fd,
					  DRM_NOUVEAU_GEM_PUSHBUF_CALL2,
					  &req, sizeof(req));
		if (ret)
			return;

		nvpb->no_aper_update = 1;
	}

	for (i = 0; i < CALPB_BUFFERS; i++) {
		ret = nouveau_bo_new(dev, NOUVEAU_BO_GART | NOUVEAU_BO_MAP,
				     0, CALPB_BUFSZ, &nvpb->buffer[i]);
		if (ret) {
			nouveau_pushbuf_fini_call(chan);
			return;
		}
	}

	nvpb->use_cal = 1;
	nvpb->cal_suffix0 = req.suffix0;
	nvpb->cal_suffix1 = req.suffix1;
}

int
nouveau_pushbuf_init(struct nouveau_channel *chan)
{
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;
	int ret;

	nouveau_pushbuf_init_call(chan);

	ret = nouveau_pushbuf_space(chan, 0);
	if (ret) {
		if (nvpb->use_cal) {
			nouveau_pushbuf_fini_call(chan);
			ret = nouveau_pushbuf_space(chan, 0);
		}

		if (ret)
			return ret;
	}

	nvpb->buffers = calloc(NOUVEAU_GEM_MAX_BUFFERS,
			       sizeof(struct drm_nouveau_gem_pushbuf_bo));
	nvpb->relocs = calloc(NOUVEAU_GEM_MAX_RELOCS,
			      sizeof(struct drm_nouveau_gem_pushbuf_reloc));
	
	chan->pushbuf = &nvpb->base;
	return 0;
}

int
nouveau_pushbuf_flush(struct nouveau_channel *chan, unsigned min)
{
	struct nouveau_device_priv *nvdev = nouveau_device(chan->device);
	struct nouveau_channel_priv *nvchan = nouveau_channel(chan);
	struct nouveau_pushbuf_priv *nvpb = &nvchan->pb;
	unsigned i;
	int ret;

	if (nvpb->base.remaining == nvpb->size)
		return 0;

	if (nvpb->use_cal) {
		struct drm_nouveau_gem_pushbuf_call req;

		*(nvpb->base.cur++) = nvpb->cal_suffix0;
		*(nvpb->base.cur++) = nvpb->cal_suffix1;
		if (nvpb->base.remaining > 2) /* space() will fixup if not */
			nvpb->base.remaining -= 2;

restart_cal:
		req.channel = chan->id;
		req.handle = nvpb->buffer[nvpb->current]->handle;
		req.offset = nvpb->current_offset * 4;
		req.nr_buffers = nvpb->nr_buffers;
		req.buffers = (uint64_t)(unsigned long)nvpb->buffers;
		req.nr_relocs = nvpb->nr_relocs;
		req.relocs = (uint64_t)(unsigned long)nvpb->relocs;
		req.nr_dwords = (nvpb->base.cur - nvpb->pushbuf) -
				nvpb->current_offset;
		req.suffix0 = nvpb->cal_suffix0;
		req.suffix1 = nvpb->cal_suffix1;
		ret = drmCommandWriteRead(nvdev->fd, nvpb->no_aper_update ?
					  DRM_NOUVEAU_GEM_PUSHBUF_CALL :
					  DRM_NOUVEAU_GEM_PUSHBUF_CALL2,
					  &req, sizeof(req));
		if (ret == -EAGAIN)
			goto restart_cal;
		nvpb->cal_suffix0 = req.suffix0;
		nvpb->cal_suffix1 = req.suffix1;
		if (!nvpb->no_aper_update) {
			nvdev->base.vm_vram_size = req.vram_available;
			nvdev->base.vm_gart_size = req.gart_available;
		}
	} else {
		struct drm_nouveau_gem_pushbuf req;

restart_push:
		req.channel = chan->id;
		req.nr_dwords = nvpb->size - nvpb->base.remaining;
		req.dwords = (uint64_t)(unsigned long)nvpb->pushbuf;
		req.nr_buffers = nvpb->nr_buffers;
		req.buffers = (uint64_t)(unsigned long)nvpb->buffers;
		req.nr_relocs = nvpb->nr_relocs;
		req.relocs = (uint64_t)(unsigned long)nvpb->relocs;
		ret = drmCommandWrite(nvdev->fd, DRM_NOUVEAU_GEM_PUSHBUF,
				      &req, sizeof(req));
		if (ret == -EAGAIN)
			goto restart_push;
	}


	/* Update presumed offset/domain for any buffers that moved.
	 * Dereference all buffers on validate list
	 */
	for (i = 0; i < nvpb->nr_relocs; i++) {
		struct drm_nouveau_gem_pushbuf_reloc *r = &nvpb->relocs[i];
		struct drm_nouveau_gem_pushbuf_bo *pbbo =
			&nvpb->buffers[r->bo_index];
		struct nouveau_bo *bo = (void *)(unsigned long)pbbo->user_priv;
		struct nouveau_bo_priv *nvbo = nouveau_bo(bo);

		if (--nvbo->pending_refcnt)
			continue;

		if (pbbo->presumed_ok == 0) {
			nvbo->domain = pbbo->presumed_domain;
			nvbo->offset = pbbo->presumed_offset;
		}

		nvbo->pending = NULL;
		nouveau_bo_ref(NULL, &bo);
	}

	nvpb->nr_buffers = 0;
	nvpb->nr_relocs = 0;

	/* Allocate space for next push buffer */
	assert(!nouveau_pushbuf_space(chan, min));

	if (chan->flush_notify)
		chan->flush_notify(chan);

	nvpb->marker = 0;
	return ret;
}

int
nouveau_pushbuf_marker_emit(struct nouveau_channel *chan,
			    unsigned wait_dwords, unsigned wait_relocs)
{
	struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(chan->pushbuf);

	if (AVAIL_RING(chan) < wait_dwords)
		return nouveau_pushbuf_flush(chan, wait_dwords);

	if (nvpb->nr_relocs + wait_relocs >= NOUVEAU_GEM_MAX_RELOCS)
		return nouveau_pushbuf_flush(chan, wait_dwords);

	nvpb->marker = nvpb->base.cur - nvpb->pushbuf;
	nvpb->marker_relocs = nvpb->nr_relocs;
	return 0;
}

void
nouveau_pushbuf_marker_undo(struct nouveau_channel *chan)
{
	struct nouveau_pushbuf_priv *nvpb = nouveau_pushbuf(chan->pushbuf);
	unsigned i;

	if (!nvpb->marker)
		return;

	/* undo any relocs/buffers added to the list since last marker */
	for (i = nvpb->marker_relocs; i < nvpb->nr_relocs; i++) {
		struct drm_nouveau_gem_pushbuf_reloc *r = &nvpb->relocs[i];
		struct drm_nouveau_gem_pushbuf_bo *pbbo =
			&nvpb->buffers[r->bo_index];
		struct nouveau_bo *bo = (void *)(unsigned long)pbbo->user_priv;
		struct nouveau_bo_priv *nvbo = nouveau_bo(bo);

		if (--nvbo->pending_refcnt)
			continue;

		nvbo->pending = NULL;
		nouveau_bo_ref(NULL, &bo);
		nvpb->nr_buffers--;
	}
	nvpb->nr_relocs = nvpb->marker_relocs;

	/* reset pushbuf back to last marker */
	nvpb->base.cur = nvpb->pushbuf + nvpb->marker;
	nvpb->base.remaining = nvpb->size - nvpb->marker;
	nvpb->marker = 0;
}


