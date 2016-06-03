/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright (C) 2013 Rob Clark <robclark@freedesktop.org>
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Rob Clark <robclark@freedesktop.org>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <inttypes.h>

#include "freedreno_ringbuffer.h"
#include "msm_priv.h"

/* represents a single cmd buffer in the submit ioctl.  Each cmd buffer has
 * a backing bo, and a reloc table.
 */
struct msm_cmd {
	struct fd_ringbuffer *ring;
	struct fd_bo *ring_bo;

	/* reloc's table: */
	struct drm_msm_gem_submit_reloc *relocs;
	uint32_t nr_relocs, max_relocs;
};

struct msm_ringbuffer {
	struct fd_ringbuffer base;

	/* submit ioctl related tables:
	 * Note that bos and cmds are tracked by the parent ringbuffer, since
	 * that is global to the submit ioctl call.  The reloc's table is tracked
	 * per cmd-buffer.
	 */
	struct {
		/* bo's table: */
		struct drm_msm_gem_submit_bo *bos;
		uint32_t nr_bos, max_bos;

		/* cmd's table: */
		struct drm_msm_gem_submit_cmd *cmds;
		uint32_t nr_cmds, max_cmds;
	} submit;

	/* should have matching entries in submit.bos: */
	/* Note, only in parent ringbuffer */
	struct fd_bo **bos;
	uint32_t nr_bos, max_bos;

	/* should have matching entries in submit.cmds: */
	struct msm_cmd **cmds;
	uint32_t nr_cmds, max_cmds;

	/* current cmd-buffer: */
	struct msm_cmd *cmd;
};

static pthread_mutex_t idx_lock = PTHREAD_MUTEX_INITIALIZER;
drm_private extern pthread_mutex_t table_lock;

static void ring_bo_del(struct fd_device *dev, struct fd_bo *bo)
{
	int ret;

	pthread_mutex_lock(&table_lock);
	ret = fd_bo_cache_free(&to_msm_device(dev)->ring_cache, bo);
	pthread_mutex_unlock(&table_lock);

	if (ret == 0)
		return;

	fd_bo_del(bo);
}

static struct fd_bo * ring_bo_new(struct fd_device *dev, uint32_t size)
{
	struct fd_bo *bo;

	bo = fd_bo_cache_alloc(&to_msm_device(dev)->ring_cache, &size, 0);
	if (bo)
		return bo;

	bo = fd_bo_new(dev, size, 0);
	if (!bo)
		return NULL;

	/* keep ringbuffer bo's out of the normal bo cache: */
	bo->bo_reuse = FALSE;

	return bo;
}

static void ring_cmd_del(struct msm_cmd *cmd)
{
	if (cmd->ring_bo)
		ring_bo_del(cmd->ring->pipe->dev, cmd->ring_bo);
	free(cmd->relocs);
	free(cmd);
}

static struct msm_cmd * ring_cmd_new(struct fd_ringbuffer *ring, uint32_t size)
{
	struct msm_cmd *cmd = calloc(1, sizeof(*cmd));

	if (!cmd)
		return NULL;

	cmd->ring = ring;
	cmd->ring_bo = ring_bo_new(ring->pipe->dev, size);
	if (!cmd->ring_bo)
		goto fail;

	return cmd;

fail:
	ring_cmd_del(cmd);
	return NULL;
}

static void *grow(void *ptr, uint32_t nr, uint32_t *max, uint32_t sz)
{
	if ((nr + 1) > *max) {
		if ((*max * 2) < (nr + 1))
			*max = nr + 5;
		else
			*max = *max * 2;
		ptr = realloc(ptr, *max * sz);
	}
	return ptr;
}

#define APPEND(x, name) ({ \
	(x)->name = grow((x)->name, (x)->nr_ ## name, &(x)->max_ ## name, sizeof((x)->name[0])); \
	(x)->nr_ ## name ++; \
})

static inline struct msm_ringbuffer * to_msm_ringbuffer(struct fd_ringbuffer *x)
{
	return (struct msm_ringbuffer *)x;
}

static uint32_t append_bo(struct fd_ringbuffer *ring, struct fd_bo *bo)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	uint32_t idx;

	idx = APPEND(&msm_ring->submit, bos);
	idx = APPEND(msm_ring, bos);

	msm_ring->submit.bos[idx].flags = 0;
	msm_ring->submit.bos[idx].handle = bo->handle;
	msm_ring->submit.bos[idx].presumed = to_msm_bo(bo)->presumed;

	msm_ring->bos[idx] = fd_bo_ref(bo);

	return idx;
}

/* add (if needed) bo, return idx: */
static uint32_t bo2idx(struct fd_ringbuffer *ring, struct fd_bo *bo, uint32_t flags)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	struct msm_bo *msm_bo = to_msm_bo(bo);
	uint32_t idx;
	pthread_mutex_lock(&idx_lock);
	if (!msm_bo->current_ring) {
		idx = append_bo(ring, bo);
		msm_bo->current_ring = ring;
		msm_bo->idx = idx;
	} else if (msm_bo->current_ring == ring) {
		idx = msm_bo->idx;
	} else {
		/* slow-path: */
		for (idx = 0; idx < msm_ring->nr_bos; idx++)
			if (msm_ring->bos[idx] == bo)
				break;
		if (idx == msm_ring->nr_bos) {
			/* not found */
			idx = append_bo(ring, bo);
		}
	}
	pthread_mutex_unlock(&idx_lock);
	if (flags & FD_RELOC_READ)
		msm_ring->submit.bos[idx].flags |= MSM_SUBMIT_BO_READ;
	if (flags & FD_RELOC_WRITE)
		msm_ring->submit.bos[idx].flags |= MSM_SUBMIT_BO_WRITE;
	return idx;
}

static int check_cmd_bo(struct fd_ringbuffer *ring,
		struct drm_msm_gem_submit_cmd *cmd, struct fd_bo *bo)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	return msm_ring->submit.bos[cmd->submit_idx].handle == bo->handle;
}

/* Ensure that submit has corresponding entry in cmds table for the
 * target cmdstream buffer:
 */
static void get_cmd(struct fd_ringbuffer *ring, struct msm_cmd *target_cmd,
		uint32_t submit_offset, uint32_t size, uint32_t type)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	struct drm_msm_gem_submit_cmd *cmd;
	uint32_t i;

	/* figure out if we already have a cmd buf: */
	for (i = 0; i < msm_ring->submit.nr_cmds; i++) {
		cmd = &msm_ring->submit.cmds[i];
		if ((cmd->submit_offset == submit_offset) &&
				(cmd->size == size) &&
				(cmd->type == type) &&
				check_cmd_bo(ring, cmd, target_cmd->ring_bo))
			return;
	}

	/* create cmd buf if not: */
	i = APPEND(&msm_ring->submit, cmds);
	APPEND(msm_ring, cmds);
	msm_ring->cmds[i] = target_cmd;
	cmd = &msm_ring->submit.cmds[i];
	cmd->type = type;
	cmd->submit_idx = bo2idx(ring, target_cmd->ring_bo, FD_RELOC_READ);
	cmd->submit_offset = submit_offset;
	cmd->size = size;
	cmd->pad = 0;
}

static void * msm_ringbuffer_hostptr(struct fd_ringbuffer *ring)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	return fd_bo_map(msm_ring->cmd->ring_bo);
}

static uint32_t find_next_reloc_idx(struct msm_cmd *msm_cmd,
		uint32_t start, uint32_t offset)
{
	uint32_t i;

	/* a binary search would be more clever.. */
	for (i = start; i < msm_cmd->nr_relocs; i++) {
		struct drm_msm_gem_submit_reloc *reloc = &msm_cmd->relocs[i];
		if (reloc->submit_offset >= offset)
			return i;
	}

	return i;
}

static void flush_reset(struct fd_ringbuffer *ring)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	unsigned i;

	for (i = 0; i < msm_ring->nr_bos; i++) {
		struct msm_bo *msm_bo = to_msm_bo(msm_ring->bos[i]);
		msm_bo->current_ring = NULL;
		fd_bo_del(&msm_bo->base);
	}

	/* for each of the cmd buffers, clear their reloc's: */
	for (i = 0; i < msm_ring->submit.nr_cmds; i++) {
		struct msm_cmd *target_cmd = msm_ring->cmds[i];
		target_cmd->nr_relocs = 0;
	}

	msm_ring->cmd->nr_relocs = 0;
	msm_ring->submit.nr_cmds = 0;
	msm_ring->submit.nr_bos = 0;
	msm_ring->nr_cmds = 0;
	msm_ring->nr_bos = 0;
}

static int msm_ringbuffer_flush(struct fd_ringbuffer *ring, uint32_t *last_start)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	struct drm_msm_gem_submit req = {
			.pipe = to_msm_pipe(ring->pipe)->pipe,
	};
	uint32_t i, j, submit_offset, size;
	int ret;

	submit_offset = offset_bytes(last_start, ring->start);
	size = offset_bytes(ring->cur, last_start);

	get_cmd(ring, msm_ring->cmd, submit_offset, size, MSM_SUBMIT_CMD_BUF);

	/* needs to be after get_cmd() as that could create bos/cmds table: */
	req.bos = VOID2U64(msm_ring->submit.bos),
	req.nr_bos = msm_ring->submit.nr_bos;
	req.cmds = VOID2U64(msm_ring->submit.cmds),
	req.nr_cmds = msm_ring->submit.nr_cmds;

	/* for each of the cmd's fix up their reloc's: */
	for (i = 0; i < msm_ring->submit.nr_cmds; i++) {
		struct drm_msm_gem_submit_cmd *cmd = &msm_ring->submit.cmds[i];
		struct msm_cmd *msm_cmd = msm_ring->cmds[i];
		uint32_t a = find_next_reloc_idx(msm_cmd, 0, cmd->submit_offset);
		uint32_t b = find_next_reloc_idx(msm_cmd, a, cmd->submit_offset + cmd->size);
		cmd->relocs = VOID2U64(&msm_cmd->relocs[a]);
		cmd->nr_relocs = (b > a) ? b - a : 0;
	}

	DEBUG_MSG("nr_cmds=%u, nr_bos=%u", req.nr_cmds, req.nr_bos);

	ret = drmCommandWriteRead(ring->pipe->dev->fd, DRM_MSM_GEM_SUBMIT,
			&req, sizeof(req));
	if (ret) {
		ERROR_MSG("submit failed: %d (%s)", ret, strerror(errno));
		ERROR_MSG("  pipe:  %u", req.pipe);
		for (i = 0; i < msm_ring->submit.nr_bos; i++) {
			struct drm_msm_gem_submit_bo *bo = &msm_ring->submit.bos[i];
			ERROR_MSG("  bos[%d]: handle=%u, flags=%x", i, bo->handle, bo->flags);
		}
		for (i = 0; i < msm_ring->submit.nr_cmds; i++) {
			struct drm_msm_gem_submit_cmd *cmd = &msm_ring->submit.cmds[i];
			struct drm_msm_gem_submit_reloc *relocs = U642VOID(cmd->relocs);
			ERROR_MSG("  cmd[%d]: type=%u, submit_idx=%u, submit_offset=%u, size=%u",
					i, cmd->type, cmd->submit_idx, cmd->submit_offset, cmd->size);
			for (j = 0; j < cmd->nr_relocs; j++) {
				struct drm_msm_gem_submit_reloc *r = &relocs[j];
				ERROR_MSG("    reloc[%d]: submit_offset=%u, or=%08x, shift=%d, reloc_idx=%u"
						", reloc_offset=%"PRIu64, j, r->submit_offset, r->or, r->shift,
						r->reloc_idx, r->reloc_offset);
			}
		}
	} else if (!ret) {
		/* update timestamp on all rings associated with submit: */
		for (i = 0; i < msm_ring->submit.nr_cmds; i++) {
			struct msm_cmd *msm_cmd = msm_ring->cmds[i];
			msm_cmd->ring->last_timestamp = req.fence;
		}
	}

	flush_reset(ring);

	return ret;
}

static void msm_ringbuffer_reset(struct fd_ringbuffer *ring)
{
	flush_reset(ring);
}

static void msm_ringbuffer_emit_reloc(struct fd_ringbuffer *ring,
		const struct fd_reloc *r)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	struct fd_ringbuffer *parent = ring->parent ? ring->parent : ring;
	struct msm_bo *msm_bo = to_msm_bo(r->bo);
	struct drm_msm_gem_submit_reloc *reloc;
	uint32_t idx = APPEND(msm_ring->cmd, relocs);
	uint32_t addr;

	reloc = &msm_ring->cmd->relocs[idx];

	reloc->reloc_idx = bo2idx(parent, r->bo, r->flags);
	reloc->reloc_offset = r->offset;
	reloc->or = r->or;
	reloc->shift = r->shift;
	reloc->submit_offset = offset_bytes(ring->cur, ring->start);

	addr = msm_bo->presumed;
	if (r->shift < 0)
		addr >>= -r->shift;
	else
		addr <<= r->shift;
	(*ring->cur++) = addr | r->or;
}

static void msm_ringbuffer_emit_reloc_ring(struct fd_ringbuffer *ring,
		struct fd_ringbuffer *target,
		uint32_t submit_offset, uint32_t size)
{
	struct msm_cmd *cmd = to_msm_ringbuffer(target)->cmd;

	get_cmd(ring, cmd, submit_offset, size, MSM_SUBMIT_CMD_IB_TARGET_BUF);

	msm_ringbuffer_emit_reloc(ring, &(struct fd_reloc){
		.bo = cmd->ring_bo,
		.flags = FD_RELOC_READ,
		.offset = submit_offset,
	});
}

static void msm_ringbuffer_destroy(struct fd_ringbuffer *ring)
{
	struct msm_ringbuffer *msm_ring = to_msm_ringbuffer(ring);
	if (msm_ring->cmd)
		ring_cmd_del(msm_ring->cmd);
	free(msm_ring->submit.cmds);
	free(msm_ring->submit.bos);
	free(msm_ring->bos);
	free(msm_ring->cmds);
	free(msm_ring);
}

static const struct fd_ringbuffer_funcs funcs = {
		.hostptr = msm_ringbuffer_hostptr,
		.flush = msm_ringbuffer_flush,
		.reset = msm_ringbuffer_reset,
		.emit_reloc = msm_ringbuffer_emit_reloc,
		.emit_reloc_ring = msm_ringbuffer_emit_reloc_ring,
		.destroy = msm_ringbuffer_destroy,
};

drm_private struct fd_ringbuffer * msm_ringbuffer_new(struct fd_pipe *pipe,
		uint32_t size)
{
	struct msm_ringbuffer *msm_ring;
	struct fd_ringbuffer *ring = NULL;

	msm_ring = calloc(1, sizeof(*msm_ring));
	if (!msm_ring) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	ring = &msm_ring->base;
	ring->funcs = &funcs;
	ring->pipe = pipe;   /* needed in ring_cmd_new() */

	msm_ring->cmd = ring_cmd_new(ring, size);
	if (!msm_ring->cmd) {
		ERROR_MSG("command buffer allocation failed");
		goto fail;
	}

	return ring;
fail:
	if (ring)
		fd_ringbuffer_del(ring);
	return NULL;
}
