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

#ifndef MSM_PRIV_H_
#define MSM_PRIV_H_

#include "freedreno_priv.h"

#ifndef __user
#  define __user
#endif

#include "msm_drm.h"

struct msm_device {
	struct fd_device base;
};

static inline struct msm_device * to_msm_device(struct fd_device *x)
{
	return (struct msm_device *)x;
}

struct fd_device * msm_device_new(int fd);

struct msm_pipe {
	struct fd_pipe base;
	uint32_t pipe;
	uint32_t gpu_id;
	uint32_t gmem;
	uint32_t chip_id;
};

static inline struct msm_pipe * to_msm_pipe(struct fd_pipe *x)
{
	return (struct msm_pipe *)x;
}

struct fd_pipe * msm_pipe_new(struct fd_device *dev, enum fd_pipe_id id);

struct fd_ringbuffer * msm_ringbuffer_new(struct fd_pipe *pipe,
		uint32_t size);

struct msm_bo {
	struct fd_bo base;
	uint64_t offset;
	uint64_t presumed;
	uint32_t indexp1[FD_PIPE_MAX]; /* index plus 1 */
	struct list_head list[FD_PIPE_MAX];
};

static inline struct msm_bo * to_msm_bo(struct fd_bo *x)
{
	return (struct msm_bo *)x;
}

int msm_bo_new_handle(struct fd_device *dev,
		uint32_t size, uint32_t flags, uint32_t *handle);
struct fd_bo * msm_bo_from_handle(struct fd_device *dev,
		uint32_t size, uint32_t handle);

static inline void get_abs_timeout(struct drm_msm_timespec *tv, uint32_t ms)
{
	struct timespec t;
	uint32_t s = ms / 1000;
	clock_gettime(CLOCK_MONOTONIC, &t);
	tv->tv_sec = t.tv_sec + s;
	tv->tv_nsec = t.tv_nsec + ((ms - (s * 1000)) * 1000000);
}

#endif /* MSM_PRIV_H_ */
