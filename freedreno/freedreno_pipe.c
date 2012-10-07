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


static int getprop(int fd, enum kgsl_property_type type,
		void *value, int sizebytes)
{
	struct kgsl_device_getproperty req = {
			.type = type,
			.value = value,
			.sizebytes = sizebytes,
	};
	return ioctl(fd, IOCTL_KGSL_DEVICE_GETPROPERTY, &req);
}

#define GETPROP(fd, prop, x) do { \
	if (getprop((fd), KGSL_PROP_##prop, &(x), sizeof(x))) {     \
		ERROR_MSG("failed to get property: " #prop);            \
		goto fail;                                              \
	} } while (0)


struct fd_pipe * fd_pipe_new(struct fd_device *dev, enum fd_pipe_id id)
{
	static const char *paths[] = {
			[FD_PIPE_3D] = "/dev/kgsl-3d0",
			[FD_PIPE_2D] = "/dev/kgsl-2d0",
	};
	struct kgsl_drawctxt_create req = {
			.flags = 0x2000, /* ??? */
	};
	struct fd_pipe *pipe = NULL;
	int ret, fd;

	if (id > FD_PIPE_MAX) {
		ERROR_MSG("invalid pipe id: %d", id);
		goto fail;
	}

	fd = open(paths[id], O_RDWR);
	if (fd < 0) {
		ERROR_MSG("could not open %s device: %d (%s)",
				paths[id], fd, strerror(errno));
		goto fail;
	}

	ret = ioctl(fd, IOCTL_KGSL_DRAWCTXT_CREATE, &req);
	if (ret) {
		ERROR_MSG("failed to allocate context: %d (%s)",
				ret, strerror(errno));
		goto fail;
	}

	pipe = calloc(1, sizeof(*pipe));
	if (!pipe) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	pipe->dev = dev;
	pipe->id = id;
	pipe->fd = fd;
	pipe->drawctxt_id = req.drawctxt_id;

	list_inithead(&pipe->submit_list);
	list_inithead(&pipe->pending_list);

	GETPROP(fd, VERSION,     pipe->version);
	GETPROP(fd, DEVICE_INFO, pipe->devinfo);

	INFO_MSG("Pipe Info:");
	INFO_MSG(" Device:          %s", paths[id]);
	INFO_MSG(" Chip-id:         %d.%d.%d.%d",
			(pipe->devinfo.chip_id >> 24) & 0xff,
			(pipe->devinfo.chip_id >> 16) & 0xff,
			(pipe->devinfo.chip_id >>  8) & 0xff,
			(pipe->devinfo.chip_id >>  0) & 0xff);
	INFO_MSG(" Device-id:       %d", pipe->devinfo.device_id);
	INFO_MSG(" GPU-id:          %d", pipe->devinfo.gpu_id);
	INFO_MSG(" MMU enabled:     %d", pipe->devinfo.mmu_enabled);
	INFO_MSG(" GMEM Base addr:  0x%08x", pipe->devinfo.gmem_gpubaseaddr);
	INFO_MSG(" GMEM size:       0x%08x", pipe->devinfo.gmem_sizebytes);
	INFO_MSG(" Driver version:  %d.%d",
			pipe->version.drv_major, pipe->version.drv_minor);
	INFO_MSG(" Device version:  %d.%d",
			pipe->version.dev_major, pipe->version.dev_minor);

	return pipe;
fail:
	if (pipe)
		fd_pipe_del(pipe);
	return NULL;
}

void fd_pipe_del(struct fd_pipe *pipe)
{
	struct kgsl_drawctxt_destroy req = {
			.drawctxt_id = pipe->drawctxt_id,
	};

	if (pipe->drawctxt_id)
		ioctl(pipe->fd, IOCTL_KGSL_DRAWCTXT_DESTROY, &req);

	if (pipe->fd)
		close(pipe->fd);

	free(pipe);
}

int fd_pipe_get_param(struct fd_pipe *pipe, enum fd_param_id param,
		uint64_t *value)
{
	switch (param) {
	case FD_DEVICE_ID:
		*value = pipe->devinfo.device_id;
		return 0;
	case FD_GMEM_SIZE:
		*value = pipe->devinfo.gmem_sizebytes;
		return 0;
	default:
		ERROR_MSG("invalid param id: %d", param);
		return -1;
	}
}

int fd_pipe_wait(struct fd_pipe *pipe, uint32_t timestamp)
{
	struct kgsl_device_waittimestamp req = {
			.timestamp = timestamp,
			.timeout   = 5000,
	};
	int ret;

	do {
		ret = ioctl(pipe->fd, IOCTL_KGSL_DEVICE_WAITTIMESTAMP, &req);
	} while ((ret == -1) && ((errno == EINTR) || (errno == EAGAIN)));
	if (ret)
		ERROR_MSG("waittimestamp failed! %d (%s)", ret, strerror(errno));
	else
		fd_pipe_process_pending(pipe, timestamp);
	return ret;
}

int fd_pipe_timestamp(struct fd_pipe *pipe, uint32_t *timestamp)
{
	struct kgsl_cmdstream_readtimestamp req = {
			.type = KGSL_TIMESTAMP_RETIRED
	};
	int ret = ioctl(pipe->fd, IOCTL_KGSL_CMDSTREAM_READTIMESTAMP, &req);
	if (ret) {
		ERROR_MSG("readtimestamp failed! %d (%s)",
				ret, strerror(errno));
		return ret;
	}
	*timestamp = req.timestamp;
	return 0;
}

/* add buffer to submit list when it is referenced in cmdstream: */
void fd_pipe_add_submit(struct fd_pipe *pipe, struct fd_bo *bo)
{
	struct list_head *list = &bo->list[pipe->id];
	if (LIST_IS_EMPTY(list)) {
		fd_bo_ref(bo);
	} else {
		list_del(list);
	}
	list_addtail(list, &pipe->submit_list);
}

/* process buffers on submit list after flush: */
void fd_pipe_process_submit(struct fd_pipe *pipe, uint32_t timestamp)
{
	struct fd_bo *bo, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(bo, tmp, &pipe->submit_list, list[pipe->id]) {
		struct list_head *list = &bo->list[pipe->id];
		list_del(list);
		bo->timestamp[pipe->id] = timestamp;
		list_addtail(list, &pipe->pending_list);
	}

	if (!fd_pipe_timestamp(pipe, &timestamp))
		fd_pipe_process_pending(pipe, timestamp);
}

void fd_pipe_process_pending(struct fd_pipe *pipe, uint32_t timestamp)
{
	struct fd_bo *bo, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(bo, tmp, &pipe->pending_list, list[pipe->id]) {
		struct list_head *list = &bo->list[pipe->id];
		if (bo->timestamp[pipe->id] > timestamp)
			return;
		list_delinit(list);
		bo->timestamp[pipe->id] = 0;
		fd_bo_del(bo);
	}
}
