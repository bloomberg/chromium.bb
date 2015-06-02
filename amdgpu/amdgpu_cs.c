/*
 * Copyright 2014 Advanced Micro Devices, Inc.
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
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/ioctl.h>

#include "xf86drm.h"
#include "amdgpu_drm.h"
#include "amdgpu_internal.h"

/**
 * Create command submission context
 *
 * \param   dev - \c [in] amdgpu device handle
 * \param   context - \c [out] amdgpu context handle
 *
 * \return  0 on success otherwise POSIX Error code
*/
int amdgpu_cs_ctx_create(amdgpu_device_handle dev,
			 amdgpu_context_handle *context)
{
	struct amdgpu_bo_alloc_request alloc_buffer = {};
	struct amdgpu_bo_alloc_result info = {};
	struct amdgpu_context *gpu_context;
	union drm_amdgpu_ctx args;
	int r;

	if (NULL == dev)
		return -EINVAL;
	if (NULL == context)
		return -EINVAL;

	gpu_context = calloc(1, sizeof(struct amdgpu_context));
	if (NULL == gpu_context)
		return -ENOMEM;

	gpu_context->dev = dev;

	r = pthread_mutex_init(&gpu_context->sequence_mutex, NULL);
	if (r)
		goto error_mutex;

	/* Create the fence BO */
	alloc_buffer.alloc_size = 4 * 1024;
	alloc_buffer.phys_alignment = 4 * 1024;
	alloc_buffer.preferred_heap = AMDGPU_GEM_DOMAIN_GTT;

	r = amdgpu_bo_alloc(dev, &alloc_buffer, &info);
	if (r)
		goto error_fence_alloc;
	gpu_context->fence_bo = info.buf_handle;

	r = amdgpu_bo_cpu_map(gpu_context->fence_bo, &gpu_context->fence_cpu);
	if (r)
		goto error_fence_map;

	/* Create the context */
	memset(&args, 0, sizeof(args));
	args.in.op = AMDGPU_CTX_OP_ALLOC_CTX;
	r = drmCommandWriteRead(dev->fd, DRM_AMDGPU_CTX, &args, sizeof(args));
	if (r)
		goto error_kernel;

	gpu_context->id = args.out.alloc.ctx_id;
	*context = (amdgpu_context_handle)gpu_context;

	return 0;

error_kernel:
	amdgpu_bo_cpu_unmap(gpu_context->fence_bo);

error_fence_map:
	amdgpu_bo_free(gpu_context->fence_bo);

error_fence_alloc:
	pthread_mutex_destroy(&gpu_context->sequence_mutex);

error_mutex:
	free(gpu_context);
	return r;
}

/**
 * Release command submission context
 *
 * \param   dev - \c [in] amdgpu device handle
 * \param   context - \c [in] amdgpu context handle
 *
 * \return  0 on success otherwise POSIX Error code
*/
int amdgpu_cs_ctx_free(amdgpu_context_handle context)
{
	union drm_amdgpu_ctx args;
	int r;

	if (NULL == context)
		return -EINVAL;

	r = amdgpu_bo_cpu_unmap(context->fence_bo);
	if (r)
		return r;

	r = amdgpu_bo_free(context->fence_bo);
	if (r)
		return r;

	pthread_mutex_destroy(&context->sequence_mutex);

	/* now deal with kernel side */
	memset(&args, 0, sizeof(args));
	args.in.op = AMDGPU_CTX_OP_FREE_CTX;
	args.in.ctx_id = context->id;
	r = drmCommandWriteRead(context->dev->fd, DRM_AMDGPU_CTX,
				&args, sizeof(args));

	free(context);

	return r;
}

int amdgpu_cs_query_reset_state(amdgpu_context_handle context,
				uint32_t *state, uint32_t *hangs)
{
	union drm_amdgpu_ctx args;
	int r;

	if (!context)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.in.op = AMDGPU_CTX_OP_QUERY_STATE;
	args.in.ctx_id = context->id;
	r = drmCommandWriteRead(context->dev->fd, DRM_AMDGPU_CTX,
				&args, sizeof(args));
	if (!r) {
		*state = args.out.state.reset_status;
		*hangs = args.out.state.hangs;
	}
	return r;
}

static uint32_t amdgpu_cs_fence_index(unsigned ip, unsigned ring)
{
	return ip * AMDGPU_CS_MAX_RINGS + ring;
}

/**
 * Submit command to kernel DRM
 * \param   dev - \c [in]  Device handle
 * \param   context - \c [in]  GPU Context
 * \param   ibs_request - \c [in]  Pointer to submission requests
 * \param   fence - \c [out] return fence for this submission
 *
 * \return  0 on success otherwise POSIX Error code
 * \sa amdgpu_cs_submit()
*/
static int amdgpu_cs_submit_one(amdgpu_context_handle context,
				struct amdgpu_cs_request *ibs_request,
				uint64_t *fence)
{
	int r = 0;
	uint32_t i, size;
	union drm_amdgpu_cs cs;
	uint64_t *chunk_array;
	struct drm_amdgpu_cs_chunk *chunks;
	struct drm_amdgpu_cs_chunk_data *chunk_data;

	if (ibs_request->ip_type >= AMDGPU_HW_IP_NUM)
		return -EINVAL;
	if (ibs_request->ring >= AMDGPU_CS_MAX_RINGS)
		return -EINVAL;
	if (ibs_request->number_of_ibs > AMDGPU_CS_MAX_IBS_PER_SUBMIT)
		return -EINVAL;

	size = (ibs_request->number_of_ibs + 1) * (
		sizeof(uint64_t) +
		sizeof(struct drm_amdgpu_cs_chunk) +
		sizeof(struct drm_amdgpu_cs_chunk_data));

	chunk_array = calloc(1, size);
	if (NULL == chunk_array)
		return -ENOMEM;

	chunks = (struct drm_amdgpu_cs_chunk *)(chunk_array + ibs_request->number_of_ibs + 1);
	chunk_data = (struct drm_amdgpu_cs_chunk_data *)(chunks + ibs_request->number_of_ibs + 1);

	memset(&cs, 0, sizeof(cs));
	cs.in.chunks = (uint64_t)(uintptr_t)chunk_array;
	cs.in.ctx_id = context->id;
	if (ibs_request->resources)
		cs.in.bo_list_handle = ibs_request->resources->handle;
	cs.in.num_chunks = ibs_request->number_of_ibs;
	/* IB chunks */
	for (i = 0; i < ibs_request->number_of_ibs; i++) {
		struct amdgpu_cs_ib_info *ib;
		chunk_array[i] = (uint64_t)(uintptr_t)&chunks[i];
		chunks[i].chunk_id = AMDGPU_CHUNK_ID_IB;
		chunks[i].length_dw = sizeof(struct drm_amdgpu_cs_chunk_ib) / 4;
		chunks[i].chunk_data = (uint64_t)(uintptr_t)&chunk_data[i];

		ib = &ibs_request->ibs[i];

		chunk_data[i].ib_data._pad = 0;
		chunk_data[i].ib_data.va_start = ib->ib_mc_address;
		chunk_data[i].ib_data.ib_bytes = ib->size * 4;
		chunk_data[i].ib_data.ip_type = ibs_request->ip_type;
		chunk_data[i].ib_data.ip_instance = ibs_request->ip_instance;
		chunk_data[i].ib_data.ring = ibs_request->ring;
		chunk_data[i].ib_data.flags = ib->flags;
	}

	pthread_mutex_lock(&context->sequence_mutex);

	if (ibs_request->ip_type != AMDGPU_HW_IP_UVD &&
	    ibs_request->ip_type != AMDGPU_HW_IP_VCE) {
		i = cs.in.num_chunks++;

		/* fence chunk */
		chunk_array[i] = (uint64_t)(uintptr_t)&chunks[i];
		chunks[i].chunk_id = AMDGPU_CHUNK_ID_FENCE;
		chunks[i].length_dw = sizeof(struct drm_amdgpu_cs_chunk_fence) / 4;
		chunks[i].chunk_data = (uint64_t)(uintptr_t)&chunk_data[i];

		/* fence bo handle */
		chunk_data[i].fence_data.handle = context->fence_bo->handle;
		/* offset */
		chunk_data[i].fence_data.offset = amdgpu_cs_fence_index(
			ibs_request->ip_type, ibs_request->ring);
		chunk_data[i].fence_data.offset *= sizeof(uint64_t);
	}

	r = drmCommandWriteRead(context->dev->fd, DRM_AMDGPU_CS,
				&cs, sizeof(cs));
	if (r)
		goto error_unlock;

	*fence = cs.out.handle;

error_unlock:
	pthread_mutex_unlock(&context->sequence_mutex);
	free(chunk_array);
	return r;
}

int amdgpu_cs_submit(amdgpu_context_handle context,
		     uint64_t flags,
		     struct amdgpu_cs_request *ibs_request,
		     uint32_t number_of_requests,
		     uint64_t *fences)
{
	uint32_t i;
	int r;

	if (NULL == context)
		return -EINVAL;
	if (NULL == ibs_request)
		return -EINVAL;
	if (NULL == fences)
		return -EINVAL;

	r = 0;
	for (i = 0; i < number_of_requests; i++) {
		r = amdgpu_cs_submit_one(context, ibs_request, fences);
		if (r)
			break;
		fences++;
		ibs_request++;
	}

	return r;
}

/**
 * Calculate absolute timeout.
 *
 * \param   timeout - \c [in] timeout in nanoseconds.
 *
 * \return  absolute timeout in nanoseconds
*/
uint64_t amdgpu_cs_calculate_timeout(uint64_t timeout)
{
	int r;

	if (timeout != AMDGPU_TIMEOUT_INFINITE) {
		struct timespec current;
		r = clock_gettime(CLOCK_MONOTONIC, &current);
		if (r)
			return r;

		timeout += ((uint64_t)current.tv_sec) * 1000000000ull;
		timeout += current.tv_nsec;
	}
	return timeout;
}

static int amdgpu_ioctl_wait_cs(amdgpu_context_handle context,
				unsigned ip,
				unsigned ip_instance,
				uint32_t ring,
				uint64_t handle,
				uint64_t timeout_ns,
				bool *busy)
{
	amdgpu_device_handle dev = context->dev;
	union drm_amdgpu_wait_cs args;
	int r;

	memset(&args, 0, sizeof(args));
	args.in.handle = handle;
	args.in.ip_type = ip;
	args.in.ip_instance = ip_instance;
	args.in.ring = ring;
	args.in.timeout = amdgpu_cs_calculate_timeout(timeout_ns);
	args.in.ctx_id = context->id;

	/* Handle errors manually here because of timeout */
	r = ioctl(dev->fd, DRM_IOCTL_AMDGPU_WAIT_CS, &args);
	if (r == -1 && (errno == EINTR || errno == EAGAIN)) {
		*busy = true;
		return 0;
	} else if (r)
		return -errno;

	*busy = args.out.status;
	return 0;
}

int amdgpu_cs_query_fence_status(struct amdgpu_cs_query_fence *fence,
				 uint32_t *expired)
{
	amdgpu_context_handle context;
	uint64_t *signaled_fence;
	uint64_t *expired_fence;
	unsigned ip_type, ip_instance;
	uint32_t ring;
	bool busy = true;
	int r;

	if (NULL == fence)
		return -EINVAL;
	if (NULL == expired)
		return -EINVAL;
	if (NULL == fence->context)
		return -EINVAL;
	if (fence->ip_type >= AMDGPU_HW_IP_NUM)
		return -EINVAL;
	if (fence->ring >= AMDGPU_CS_MAX_RINGS)
		return -EINVAL;

	context = fence->context;
	ip_type = fence->ip_type;
	ip_instance = fence->ip_instance;
	ring = fence->ring;
	signaled_fence = context->fence_cpu;
	signaled_fence += amdgpu_cs_fence_index(ip_type, ring);
	expired_fence = &context->expired_fences[ip_type][ip_instance][ring];
	*expired = false;

	pthread_mutex_lock(&context->sequence_mutex);
	if (fence->fence <= *expired_fence) {
		/* This fence value is expired already. */
		pthread_mutex_unlock(&context->sequence_mutex);
		*expired = true;
		return 0;
	}

	if (fence->fence <= *signaled_fence) {
		/* This fence value is signaled already. */
		*expired_fence = *signaled_fence;
		pthread_mutex_unlock(&context->sequence_mutex);
		*expired = true;
		return 0;
	}

	pthread_mutex_unlock(&context->sequence_mutex);

	r = amdgpu_ioctl_wait_cs(context, ip_type, ip_instance, ring,
				 fence->fence, fence->timeout_ns, &busy);
	if (!r && !busy) {
		*expired = true;
		pthread_mutex_lock(&context->sequence_mutex);
		/* The thread doesn't hold sequence_mutex. Other thread could
		   update *expired_fence already. Check whether there is a
		   newerly expired fence. */
		if (fence->fence > *expired_fence)
			*expired_fence = fence->fence;
		pthread_mutex_unlock(&context->sequence_mutex);
	}

	return r;
}

