/*
 * Copyright (C) 2015 Etnaviv Project
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
 *    Christian Gmeiner <christian.gmeiner@gmail.com>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "etnaviv_priv.h"
#include "etnaviv_drmif.h"

static uint64_t get_param(struct etna_device *dev, uint32_t core, uint32_t param)
{
	struct drm_etnaviv_param req = {
		.pipe = core,
		.param = param,
	};
	int ret;

	ret = drmCommandWriteRead(dev->fd, DRM_ETNAVIV_GET_PARAM, &req, sizeof(req));
	if (ret) {
		ERROR_MSG("get-param (%x) failed! %d (%s)", param, ret, strerror(errno));
		return 0;
	}

	return req.value;
}

struct etna_gpu *etna_gpu_new(struct etna_device *dev, unsigned int core)
{
	struct etna_gpu *gpu;

	gpu = calloc(1, sizeof(*gpu));
	if (!gpu) {
		ERROR_MSG("allocation failed");
		goto fail;
	}

	gpu->dev = dev;
	gpu->core = core;

	/* get specs from kernel space */
	gpu->specs.model    	= get_param(dev, core, ETNAVIV_PARAM_GPU_MODEL);
	gpu->specs.revision 	= get_param(dev, core, ETNAVIV_PARAM_GPU_REVISION);
	gpu->specs.features[0] = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_0);
	gpu->specs.features[1] = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_1);
	gpu->specs.features[2] = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_2);
	gpu->specs.features[3] = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_3);
	gpu->specs.features[4] = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_4);
	gpu->specs.features[5] = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_5);
	gpu->specs.features[6] = get_param(dev, core, ETNAVIV_PARAM_GPU_FEATURES_6);
	gpu->specs.stream_count = get_param(dev, core, ETNA_GPU_STREAM_COUNT);
	gpu->specs.register_max = get_param(dev, core, ETNA_GPU_REGISTER_MAX);
	gpu->specs.thread_count = get_param(dev, core, ETNA_GPU_THREAD_COUNT);
	gpu->specs.vertex_cache_size = get_param(dev, core, ETNA_GPU_VERTEX_CACHE_SIZE);
	gpu->specs.shader_core_count = get_param(dev, core, ETNA_GPU_SHADER_CORE_COUNT);
	gpu->specs.pixel_pipes = get_param(dev, core, ETNA_GPU_PIXEL_PIPES);
	gpu->specs.vertex_output_buffer_size = get_param(dev, core, ETNA_GPU_VERTEX_OUTPUT_BUFFER_SIZE);
	gpu->specs.buffer_size = get_param(dev, core, ETNA_GPU_BUFFER_SIZE);
	gpu->specs.instruction_count = get_param(dev, core, ETNA_GPU_INSTRUCTION_COUNT);
	gpu->specs.num_constants = get_param(dev, core, ETNA_GPU_NUM_CONSTANTS);
	gpu->specs.num_varyings = get_param(dev, core, ETNA_GPU_NUM_VARYINGS);

	if (!gpu->specs.model)
		goto fail;

	INFO_MSG(" GPU model:          0x%x (rev %x)", gpu->specs.model, gpu->specs.revision);

	return gpu;
fail:
	if (gpu)
		etna_gpu_del(gpu);

	return NULL;
}

void etna_gpu_del(struct etna_gpu *gpu)
{
	free(gpu);
}

int etna_gpu_get_param(struct etna_gpu *gpu, enum etna_param_id param,
		uint64_t *value)
{
	switch(param) {
	case ETNA_GPU_MODEL:
		*value = gpu->specs.model;
		return 0;
	case ETNA_GPU_REVISION:
		*value = gpu->specs.revision;
		return 0;
	case ETNA_GPU_FEATURES_0:
		*value = gpu->specs.features[0];
		return 0;
	case ETNA_GPU_FEATURES_1:
		*value = gpu->specs.features[1];
		return 0;
	case ETNA_GPU_FEATURES_2:
		*value = gpu->specs.features[2];
		return 0;
	case ETNA_GPU_FEATURES_3:
		*value = gpu->specs.features[3];
		return 0;
	case ETNA_GPU_FEATURES_4:
		*value = gpu->specs.features[4];
		return 0;
	case ETNA_GPU_FEATURES_5:
		*value = gpu->specs.features[5];
		return 0;
	case ETNA_GPU_FEATURES_6:
		*value = gpu->specs.features[6];
		return 0;
	case ETNA_GPU_STREAM_COUNT:
		*value = gpu->specs.stream_count;
		return 0;
	case ETNA_GPU_REGISTER_MAX:
		*value = gpu->specs.register_max;
		return 0;
	case ETNA_GPU_THREAD_COUNT:
		*value = gpu->specs.thread_count;
		return 0;
	case ETNA_GPU_VERTEX_CACHE_SIZE:
		*value = gpu->specs.vertex_cache_size;
		return 0;
	case ETNA_GPU_SHADER_CORE_COUNT:
		*value = gpu->specs.shader_core_count;
		return 0;
	case ETNA_GPU_PIXEL_PIPES:
		*value = gpu->specs.pixel_pipes;
		return 0;
	case ETNA_GPU_VERTEX_OUTPUT_BUFFER_SIZE:
		*value = gpu->specs.vertex_output_buffer_size;
		return 0;
	case ETNA_GPU_BUFFER_SIZE:
		*value = gpu->specs.buffer_size;
		return 0;
	case ETNA_GPU_INSTRUCTION_COUNT:
		*value = gpu->specs.instruction_count;
		return 0;
	case ETNA_GPU_NUM_CONSTANTS:
		*value = gpu->specs.num_constants;
		return 0;
	case ETNA_GPU_NUM_VARYINGS:
		*value = gpu->specs.num_varyings;
		return 0;

	default:
		ERROR_MSG("invalid param id: %d", param);
		return -1;
	}

	return 0;
}
