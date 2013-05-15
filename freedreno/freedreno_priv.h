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

#ifndef FREEDRENO_PRIV_H_
#define FREEDRENO_PRIV_H_

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include "xf86drm.h"
#include "xf86atomic.h"

#include "list.h"

#include "freedreno_drmif.h"
#include "msm_kgsl.h"
#include "kgsl_drm.h"

struct fd_device {
	int fd;
	atomic_t refcnt;

	/* tables to keep track of bo's, to avoid "evil-twin" fd_bo objects:
	 *
	 *   handle_table: maps handle to fd_bo
	 *   name_table: maps flink name to fd_bo
	 *
	 * We end up needing two tables, because DRM_IOCTL_GEM_OPEN always
	 * returns a new handle.  So we need to figure out if the bo is already
	 * open in the process first, before calling gem-open.
	 */
	void *handle_table, *name_table;
};

struct fd_pipe {
	struct fd_device *dev;
	enum fd_pipe_id id;
	int fd;
	uint32_t drawctxt_id;

	/* device properties: */
	struct kgsl_version version;
	struct kgsl_devinfo devinfo;

	/* list of bo's that are referenced in ringbuffer but not
	 * submitted yet:
	 */
	struct list_head submit_list;

	/* list of bo's that have been submitted but timestamp has
	 * not passed yet (so still ref'd in active cmdstream)
	 */
	struct list_head pending_list;

	/* if we are the 2d pipe, and want to wait on a timestamp
	 * from 3d, we need to also internally open the 3d pipe:
	 */
	struct fd_pipe *p3d;
};

void fd_pipe_add_submit(struct fd_pipe *pipe, struct fd_bo *bo);
void fd_pipe_pre_submit(struct fd_pipe *pipe);
void fd_pipe_post_submit(struct fd_pipe *pipe, uint32_t timestamp);
void fd_pipe_process_pending(struct fd_pipe *pipe, uint32_t timestamp);

struct fd_bo {
	struct fd_device *dev;
	uint32_t size;
	uint32_t handle;
	uint32_t name;
	uint32_t gpuaddr;
	void *map;
	uint64_t offset;
	/* timestamp (per pipe) for bo's in a pipe's pending_list: */
	uint32_t timestamp[FD_PIPE_MAX];
	/* list-node for pipe's submit_list or pending_list */
	struct list_head list[FD_PIPE_MAX];
	atomic_t refcnt;
};

/* not exposed publicly, because won't be needed when we have
 * a proper kernel driver
 */
uint32_t fd_bo_gpuaddr(struct fd_bo *bo, uint32_t offset);
void fb_bo_set_timestamp(struct fd_bo *bo, uint32_t timestamp);
uint32_t fd_bo_get_timestamp(struct fd_bo *bo);

#define ALIGN(v,a) (((v) + (a) - 1) & ~((a) - 1))
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define enable_debug 1  /* TODO make dynamic */

#define INFO_MSG(fmt, ...) \
		do { drmMsg("[I] "fmt " (%s:%d)\n", \
				##__VA_ARGS__, __FUNCTION__, __LINE__); } while (0)
#define DEBUG_MSG(fmt, ...) \
		do if (enable_debug) { drmMsg("[D] "fmt " (%s:%d)\n", \
				##__VA_ARGS__, __FUNCTION__, __LINE__); } while (0)
#define WARN_MSG(fmt, ...) \
		do { drmMsg("[W] "fmt " (%s:%d)\n", \
				##__VA_ARGS__, __FUNCTION__, __LINE__); } while (0)
#define ERROR_MSG(fmt, ...) \
		do { drmMsg("[E] " fmt " (%s:%d)\n", \
				##__VA_ARGS__, __FUNCTION__, __LINE__); } while (0)

#endif /* FREEDRENO_PRIV_H_ */
