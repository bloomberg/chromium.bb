/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "../cros_gralloc_driver.h"

#include <hardware/gralloc.h>
#include <memory.h>

struct gralloc0_module {
	gralloc_module_t base;
	std::unique_ptr<alloc_device_t> alloc;
	std::unique_ptr<cros_gralloc_driver> driver;
};

/* This enumeration must match the one in <gralloc_drm.h>.
 * The functions supported by this gralloc's temporary private API are listed
 * below. Use of these functions is highly discouraged and should only be
 * reserved for cases where no alternative to get same information (such as
 * querying ANativeWindow) exists.
 */
// clang-format off
enum {
	GRALLOC_DRM_GET_STRIDE,
	GRALLOC_DRM_GET_FORMAT,
	GRALLOC_DRM_GET_DIMENSIONS,
	GRALLOC_DRM_GET_BACKING_STORE,
};
// clang-format on

static int64_t gralloc0_convert_flags(int flags)
{
	uint64_t usage = BO_USE_NONE;

	if (flags & GRALLOC_USAGE_CURSOR)
		usage |= BO_USE_NONE;
	if ((flags & GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_RARELY)
		usage |= BO_USE_SW_READ_RARELY;
	if ((flags & GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_OFTEN)
		usage |= BO_USE_SW_READ_OFTEN;
	if ((flags & GRALLOC_USAGE_SW_WRITE_MASK) == GRALLOC_USAGE_SW_WRITE_RARELY)
		usage |= BO_USE_SW_WRITE_RARELY;
	if ((flags & GRALLOC_USAGE_SW_WRITE_MASK) == GRALLOC_USAGE_SW_WRITE_OFTEN)
		usage |= BO_USE_SW_WRITE_OFTEN;
	if (flags & GRALLOC_USAGE_HW_TEXTURE)
		usage |= BO_USE_TEXTURE;
	if (flags & GRALLOC_USAGE_HW_RENDER)
		usage |= BO_USE_RENDERING;
	if (flags & GRALLOC_USAGE_HW_2D)
		usage |= BO_USE_RENDERING;
	if (flags & GRALLOC_USAGE_HW_COMPOSER)
		/* HWC wants to use display hardware, but can defer to OpenGL. */
		usage |= BO_USE_SCANOUT | BO_USE_TEXTURE;
	if (flags & GRALLOC_USAGE_HW_FB)
		usage |= BO_USE_NONE;
	if (flags & GRALLOC_USAGE_EXTERNAL_DISP)
		/*
		 * This flag potentially covers external display for the normal drivers (i915,
		 * rockchip) and usb monitors (evdi/udl). It's complicated so ignore it.
		 * */
		usage |= BO_USE_NONE;
	if (flags & GRALLOC_USAGE_PROTECTED)
		usage |= BO_USE_PROTECTED;
	if (flags & GRALLOC_USAGE_HW_VIDEO_ENCODER)
		/*HACK: See b/30054495 */
		usage |= BO_USE_SW_READ_OFTEN;
	if (flags & GRALLOC_USAGE_HW_CAMERA_WRITE)
		usage |= BO_USE_HW_CAMERA_WRITE;
	if (flags & GRALLOC_USAGE_HW_CAMERA_READ)
		usage |= BO_USE_HW_CAMERA_READ;
	if (flags & GRALLOC_USAGE_HW_CAMERA_ZSL)
		usage |= BO_USE_HW_CAMERA_ZSL;
	if (flags & GRALLOC_USAGE_RENDERSCRIPT)
		/* We use CPU for compute. */
		usage |= BO_USE_LINEAR;

	return usage;
}

static int gralloc0_alloc(alloc_device_t *dev, int w, int h, int format, int usage,
			  buffer_handle_t *handle, int *stride)
{
	int32_t ret;
	bool supported;
	struct cros_gralloc_buffer_descriptor descriptor;
	auto mod = (struct gralloc0_module *)dev->common.module;

	descriptor.width = w;
	descriptor.height = h;
	descriptor.droid_format = format;
	descriptor.producer_usage = descriptor.consumer_usage = usage;
	descriptor.drm_format = cros_gralloc_convert_format(format);
	descriptor.drv_usage = gralloc0_convert_flags(usage);

	supported = mod->driver->is_supported(&descriptor);
	if (!supported && (usage & GRALLOC_USAGE_HW_COMPOSER)) {
		descriptor.drv_usage &= ~BO_USE_SCANOUT;
		supported = mod->driver->is_supported(&descriptor);
	}

	if (!supported) {
		cros_gralloc_error("Unsupported combination -- HAL format: %u, HAL flags: %u, "
				   "drv_format: %4.4s, drv_flags: %llu",
				   format, usage, reinterpret_cast<char *>(descriptor.drm_format),
				   static_cast<unsigned long long>(descriptor.drv_usage));
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	ret = mod->driver->allocate(&descriptor, handle);
	if (ret)
		return ret;

	auto hnd = cros_gralloc_convert_handle(*handle);
	*stride = hnd->pixel_stride;

	return CROS_GRALLOC_ERROR_NONE;
}

static int gralloc0_free(alloc_device_t *dev, buffer_handle_t handle)
{
	auto mod = (struct gralloc0_module *)dev->common.module;
	return mod->driver->release(handle);
}

static int gralloc0_close(struct hw_device_t *dev)
{
	/* Memory is freed by managed pointers on process close. */
	return CROS_GRALLOC_ERROR_NONE;
}

static int gralloc0_open(const struct hw_module_t *mod, const char *name, struct hw_device_t **dev)
{
	auto module = (struct gralloc0_module *)mod;

	if (module->alloc) {
		*dev = &module->alloc->common;
		return CROS_GRALLOC_ERROR_NONE;
	}

	if (strcmp(name, GRALLOC_HARDWARE_GPU0)) {
		cros_gralloc_error("Incorrect device name - %s.", name);
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	module->driver = std::make_unique<cros_gralloc_driver>();
	if (module->driver->init()) {
		cros_gralloc_error("Failed to initialize driver.");
		return CROS_GRALLOC_ERROR_NO_RESOURCES;
	}

	module->alloc = std::make_unique<alloc_device_t>();

	module->alloc->alloc = gralloc0_alloc;
	module->alloc->free = gralloc0_free;
	module->alloc->common.tag = HARDWARE_DEVICE_TAG;
	module->alloc->common.version = 0;
	module->alloc->common.module = (hw_module_t *)mod;
	module->alloc->common.close = gralloc0_close;

	*dev = &module->alloc->common;
	return CROS_GRALLOC_ERROR_NONE;
}

static int gralloc0_register_buffer(struct gralloc_module_t const *module, buffer_handle_t handle)
{
	auto mod = (struct gralloc0_module *)module;

	if (!mod->driver) {
		mod->driver = std::make_unique<cros_gralloc_driver>();
		if (mod->driver->init()) {
			cros_gralloc_error("Failed to initialize driver.");
			return CROS_GRALLOC_ERROR_NO_RESOURCES;
		}
	}

	return mod->driver->retain(handle);
}

static int gralloc0_unregister_buffer(struct gralloc_module_t const *module, buffer_handle_t handle)
{
	auto mod = (struct gralloc0_module *)module;
	return mod->driver->release(handle);
}

static int gralloc0_lock(struct gralloc_module_t const *module, buffer_handle_t handle, int usage,
			 int l, int t, int w, int h, void **vaddr)
{
	int32_t ret, fence;
	uint64_t flags;
	uint8_t *addr[DRV_MAX_PLANES];
	auto mod = (struct gralloc0_module *)module;

	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if ((hnd->droid_format == HAL_PIXEL_FORMAT_YCbCr_420_888)) {
		cros_gralloc_error("HAL_PIXEL_FORMAT_YCbCr_*_888 format not compatible.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	fence = -1;
	flags = gralloc0_convert_flags(usage);
	ret = mod->driver->lock(handle, fence, flags, addr);
	*vaddr = addr[0];
	return ret;
}

static int gralloc0_unlock(struct gralloc_module_t const *module, buffer_handle_t handle)
{
	auto mod = (struct gralloc0_module *)module;
	return mod->driver->unlock(handle);
}

static int gralloc0_perform(struct gralloc_module_t const *module, int op, ...)
{
	va_list args;
	int32_t *out_format, ret;
	uint64_t *out_store;
	buffer_handle_t handle;
	uint32_t *out_width, *out_height, *out_stride;
	auto mod = (struct gralloc0_module *)module;

	switch (op) {
	case GRALLOC_DRM_GET_STRIDE:
	case GRALLOC_DRM_GET_FORMAT:
	case GRALLOC_DRM_GET_DIMENSIONS:
	case GRALLOC_DRM_GET_BACKING_STORE:
		break;
	default:
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	va_start(args, op);

	ret = CROS_GRALLOC_ERROR_NONE;
	handle = va_arg(args, buffer_handle_t);
	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	switch (op) {
	case GRALLOC_DRM_GET_STRIDE:
		out_stride = va_arg(args, uint32_t *);
		*out_stride = hnd->pixel_stride;
		break;
	case GRALLOC_DRM_GET_FORMAT:
		out_format = va_arg(args, int32_t *);
		*out_format = hnd->droid_format;
		break;
	case GRALLOC_DRM_GET_DIMENSIONS:
		out_width = va_arg(args, uint32_t *);
		out_height = va_arg(args, uint32_t *);
		*out_width = hnd->width;
		*out_height = hnd->height;
		break;
	case GRALLOC_DRM_GET_BACKING_STORE:
		out_store = va_arg(args, uint64_t *);
		ret = mod->driver->get_backing_store(handle, out_store);
		break;
	default:
		ret = CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	va_end(args);

	return ret;
}

static int gralloc0_lock_ycbcr(struct gralloc_module_t const *module, buffer_handle_t handle,
			       int usage, int l, int t, int w, int h, struct android_ycbcr *ycbcr)
{
	uint64_t flags;
	int32_t fence, ret;
	uint8_t *addr[DRV_MAX_PLANES] = { nullptr, nullptr, nullptr, nullptr };
	auto mod = (struct gralloc0_module *)module;

	auto hnd = cros_gralloc_convert_handle(handle);
	if (!hnd) {
		cros_gralloc_error("Invalid handle.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	if ((hnd->droid_format != HAL_PIXEL_FORMAT_YCbCr_420_888) &&
	    (hnd->droid_format != HAL_PIXEL_FORMAT_YV12)) {
		cros_gralloc_error("Non-YUV format not compatible.");
		return CROS_GRALLOC_ERROR_BAD_HANDLE;
	}

	fence = -1;
	flags = gralloc0_convert_flags(usage);
	ret = mod->driver->lock(handle, fence, flags, addr);

	switch (hnd->format) {
	case DRM_FORMAT_NV12:
		ycbcr->y = addr[0];
		ycbcr->cb = addr[1];
		ycbcr->cr = addr[1] + 1;
		ycbcr->ystride = hnd->strides[0];
		ycbcr->cstride = hnd->strides[1];
		ycbcr->chroma_step = 2;
		break;
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YVU420_ANDROID:
		ycbcr->y = addr[0];
		ycbcr->cb = addr[2];
		ycbcr->cr = addr[1];
		ycbcr->ystride = hnd->strides[0];
		ycbcr->cstride = hnd->strides[1];
		ycbcr->chroma_step = 1;
		break;
	default:
		return CROS_GRALLOC_ERROR_UNSUPPORTED;
	}

	return ret;
}

static struct hw_module_methods_t gralloc0_module_methods = {.open = gralloc0_open };

struct gralloc0_module HAL_MODULE_INFO_SYM = {
	.base =
	    {
		.common =
		    {
			.tag = HARDWARE_MODULE_TAG,
			.module_api_version = GRALLOC_MODULE_API_VERSION_0_2,
			.hal_api_version = 0,
			.id = GRALLOC_HARDWARE_MODULE_ID,
			.name = "CrOS Gralloc",
			.author = "Chrome OS",
			.methods = &gralloc0_module_methods,
		    },

		.registerBuffer = gralloc0_register_buffer,
		.unregisterBuffer = gralloc0_unregister_buffer,
		.lock = gralloc0_lock,
		.unlock = gralloc0_unlock,
		.perform = gralloc0_perform,
		.lock_ycbcr = gralloc0_lock_ycbcr,
	    },

	.alloc = nullptr,
	.driver = nullptr,
};
