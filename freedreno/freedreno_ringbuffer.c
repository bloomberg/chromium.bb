/* -*- mode: C; c-file-style: "k&r"; tab-width 4; indent-tabs-mode: t; -*- */

/*
 * Copyright (C) 2012 Rob Clark <robclark@freedesktop.org>
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

#include "freedreno_drmif.h"
#include "freedreno_priv.h"
#include "freedreno_ringbuffer.h"


/* because kgsl tries to validate the gpuaddr on kernel side in ISSUEIBCMDS,
 * we can't use normal gem bo's for ringbuffer..  someday the kernel part
 * needs to be reworked into a single sane drm driver :-/
 */
struct fd_rb_bo {
	struct fd_pipe *pipe;
	void    *hostptr;
	uint32_t gpuaddr;
	uint32_t size;
};

struct fd_ringmarker {
	struct fd_ringbuffer *ring;
	uint32_t *cur;
};

static void fd_rb_bo_del(struct fd_rb_bo *bo)
{
	struct kgsl_sharedmem_free req = {
			.gpuaddr = bo->gpuaddr,
	};
	int ret;

	munmap(bo->hostptr, bo->size);

	ret = ioctl(bo->pipe->fd, IOCTL_KGSL_SHAREDMEM_FREE, &req);
	if (ret) {
		ERROR_MSG("sharedmem free failed: %s", strerror(errno));
	}

	free(bo);
}

static struct fd_rb_bo * fd_rb_bo_new(struct fd_pipe *pipe, uint32_t size)
{
	struct fd_rb_bo *bo;
	struct kgsl_gpumem_alloc req = {
			.size = ALIGN(size, 4096),
			.flags = KGSL_MEMFLAGS_GPUREADONLY,
	};
	int ret;

	bo = calloc(1, sizeof(*bo));
	if (!bo) {
		ERROR_MSG("allocation failed");
		return NULL;
	}
	ret = ioctl(pipe->fd, IOCTL_KGSL_GPUMEM_ALLOC, &req);
	if (ret) {
		ERROR_MSG("gpumem allocation failed: %s", strerror(errno));
		goto fail;
	}

	bo->pipe = pipe;
	bo->gpuaddr = req.gpuaddr;
	bo->size = size;
	bo->hostptr = mmap(NULL, size, PROT_WRITE|PROT_READ,
				MAP_SHARED, pipe->fd, req.gpuaddr);

	return bo;
fail:
	if (bo)
		fd_rb_bo_del(bo);
	return NULL;
}

struct fd_ringbuffer * fd_ringbuffer_new(struct fd_pipe *pipe,
		uint32_t size)
{
	struct fd_ringbuffer *ring = NULL;

	ring = calloc(1, sizeof(*ring));
	if (!ring) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	ring->bo = fd_rb_bo_new(pipe, size);
	if (!ring->bo) {
		ERROR_MSG("ringbuffer allocation failed");
		goto fail;
	}

	ring->size = size;
	ring->pipe = pipe;
	ring->start = ring->bo->hostptr;
	ring->end = &(ring->start[size/4]);

	ring->cur = ring->last_start = ring->start;

	return ring;
fail:
	if (ring)
		fd_ringbuffer_del(ring);
	return NULL;
}

void fd_ringbuffer_del(struct fd_ringbuffer *ring)
{
	if (ring->bo)
		fd_rb_bo_del(ring->bo);
	free(ring);
}

void fd_ringbuffer_reset(struct fd_ringbuffer *ring)
{
	uint32_t *start = ring->start;
	if (ring->pipe->id == FD_PIPE_2D)
		start = &ring->start[0x140];
	ring->cur = ring->last_start = start;
}

static int flush_impl(struct fd_ringbuffer *ring, uint32_t *last_start)
{
	uint32_t offset = (uint8_t *)last_start - (uint8_t *)ring->start;
	struct kgsl_ibdesc ibdesc = {
			.gpuaddr     = ring->bo->gpuaddr + offset,
			.hostptr     = last_start,
			.sizedwords  = ring->cur - last_start,
	};
	struct kgsl_ringbuffer_issueibcmds req = {
			.drawctxt_id = ring->pipe->drawctxt_id,
			.ibdesc_addr = (unsigned long)&ibdesc,
			.numibs      = 1,
			.flags       = KGSL_CONTEXT_SUBMIT_IB_LIST,
	};
	int ret;

	fd_pipe_pre_submit(ring->pipe);

	/* z180_cmdstream_issueibcmds() is made of fail: */
	if (ring->pipe->id == FD_PIPE_2D) {
		/* fix up size field in last cmd packet */
		uint32_t last_size = (uint32_t)(ring->cur - last_start);
		/* 5 is length of first packet, 2 for the two 7f000000's */
		last_start[2] = last_size - (5 + 2);
		ibdesc.gpuaddr = ring->bo->gpuaddr;
		ibdesc.hostptr = ring->bo->hostptr;
		ibdesc.sizedwords = 0x145;
		req.timestamp = (uint32_t)ring->bo->hostptr;
	}

	do {
		ret = ioctl(ring->pipe->fd, IOCTL_KGSL_RINGBUFFER_ISSUEIBCMDS, &req);
	} while ((ret == -1) && ((errno == EINTR) || (errno == EAGAIN)));
	if (ret)
		ERROR_MSG("issueibcmds failed!  %d (%s)", ret, strerror(errno));

	ring->last_timestamp = req.timestamp;
	ring->last_start = ring->cur;

	fd_pipe_post_submit(ring->pipe, req.timestamp);

	return ret;
}

/* maybe get rid of this and use fd_ringmarker_flush() from DDX too? */
int fd_ringbuffer_flush(struct fd_ringbuffer *ring)
{
	return flush_impl(ring, ring->last_start);
}

uint32_t fd_ringbuffer_timestamp(struct fd_ringbuffer *ring)
{
	return ring->last_timestamp;
}

void fd_ringbuffer_emit_reloc(struct fd_ringbuffer *ring,
		struct fd_bo *bo, uint32_t offset, uint32_t or)
{
	(*ring->cur++) = fd_bo_gpuaddr(bo, offset) | or;
	fd_pipe_add_submit(ring->pipe, bo);
}

void fd_ringbuffer_emit_reloc_shift(struct fd_ringbuffer *ring,
		struct fd_bo *bo, uint32_t offset, uint32_t or, int32_t shift)
{
	uint32_t addr = fd_bo_gpuaddr(bo, offset);
	if (shift < 0)
		addr >>= -shift;
	else
		addr <<= shift;
	(*ring->cur++) = addr | or;
	fd_pipe_add_submit(ring->pipe, bo);
}

void fd_ringbuffer_emit_reloc_ring(struct fd_ringbuffer *ring,
		struct fd_ringmarker *target)
{
	(*ring->cur++) = target->ring->bo->gpuaddr +
			(uint8_t *)target->cur - (uint8_t *)target->ring->start;
}

struct fd_ringmarker * fd_ringmarker_new(struct fd_ringbuffer *ring)
{
	struct fd_ringmarker *marker = NULL;

	marker = calloc(1, sizeof(*marker));
	if (!marker) {
		ERROR_MSG("allocation failed");
		return NULL;
	}

	marker->ring = ring;

	fd_ringmarker_mark(marker);

	return marker;
}

void fd_ringmarker_del(struct fd_ringmarker *marker)
{
	free(marker);
}

void fd_ringmarker_mark(struct fd_ringmarker *marker)
{
	marker->cur = marker->ring->cur;
}

uint32_t fd_ringmarker_dwords(struct fd_ringmarker *start,
		struct fd_ringmarker *end)
{
	return end->cur - start->cur;
}

int fd_ringmarker_flush(struct fd_ringmarker *marker)
{
	return flush_impl(marker->ring, marker->cur);
}
