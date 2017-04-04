/*
 * Copyright © 2016 Collabora, Ltd.
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Daniel Stone <daniels@collabora.com>
 */

#include "config.h"

#include <endian.h>
#include <inttypes.h>
#include <stdbool.h>
#include <unistd.h>
#include <drm_fourcc.h>

#include "helpers.h"
#include "wayland-util.h"
#include "pixel-formats.h"

#if ENABLE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define GL_FORMAT(fmt) .gl_format = (fmt)
#define GL_TYPE(type) .gl_type = (type)
#define SAMPLER_TYPE(type) .sampler_type = (type)
#else
#define GL_FORMAT(fmt) .gl_format = 0
#define GL_TYPE(type) .gl_type = 0
#define SAMPLER_TYPE(type) .sampler_type = 0
#endif

#include "weston-egl-ext.h"

/**
 * Table of DRM formats supported by Weston; RGB, ARGB and YUV formats are
 * supported. Indexed/greyscale formats, and formats not containing complete
 * colour channels, are not supported.
 */
static const struct pixel_format_info pixel_format_table[] = {
	{
		.format = DRM_FORMAT_XRGB4444,
	},
	{
		.format = DRM_FORMAT_ARGB4444,
		.opaque_substitute = DRM_FORMAT_XRGB4444,
	},
	{
		.format = DRM_FORMAT_XBGR4444,
	},
	{
		.format = DRM_FORMAT_ABGR4444,
		.opaque_substitute = DRM_FORMAT_XBGR4444,
	},
	{
		.format = DRM_FORMAT_RGBX4444,
# if __BYTE_ORDER == __LITTLE_ENDIAN
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_SHORT_4_4_4_4),
#endif
	},
	{
		.format = DRM_FORMAT_RGBA4444,
		.opaque_substitute = DRM_FORMAT_RGBX4444,
# if __BYTE_ORDER == __LITTLE_ENDIAN
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_SHORT_4_4_4_4),
#endif
	},
	{
		.format = DRM_FORMAT_BGRX4444,
	},
	{
		.format = DRM_FORMAT_BGRA4444,
		.opaque_substitute = DRM_FORMAT_BGRX4444,
	},
	{
		.format = DRM_FORMAT_XRGB1555,
		.depth = 15,
		.bpp = 16,
	},
	{
		.format = DRM_FORMAT_ARGB1555,
		.opaque_substitute = DRM_FORMAT_XRGB1555,
	},
	{
		.format = DRM_FORMAT_XBGR1555,
	},
	{
		.format = DRM_FORMAT_ABGR1555,
		.opaque_substitute = DRM_FORMAT_XBGR1555,
	},
	{
		.format = DRM_FORMAT_RGBX5551,
# if __BYTE_ORDER == __LITTLE_ENDIAN
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_SHORT_5_5_5_1),
#endif
	},
	{
		.format = DRM_FORMAT_RGBA5551,
		.opaque_substitute = DRM_FORMAT_RGBX5551,
# if __BYTE_ORDER == __LITTLE_ENDIAN
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_SHORT_5_5_5_1),
#endif
	},
	{
		.format = DRM_FORMAT_BGRX5551,
	},
	{
		.format = DRM_FORMAT_BGRA5551,
		.opaque_substitute = DRM_FORMAT_BGRX5551,
	},
	{
		.format = DRM_FORMAT_RGB565,
		.depth = 16,
		.bpp = 16,
# if __BYTE_ORDER == __LITTLE_ENDIAN
		GL_FORMAT(GL_RGB),
		GL_TYPE(GL_UNSIGNED_SHORT_5_6_5),
#endif
	},
	{
		.format = DRM_FORMAT_BGR565,
	},
	{
		.format = DRM_FORMAT_RGB888,
	},
	{
		.format = DRM_FORMAT_BGR888,
		GL_FORMAT(GL_RGB),
		GL_TYPE(GL_UNSIGNED_BYTE),
	},
	{
		.format = DRM_FORMAT_XRGB8888,
		.depth = 24,
		.bpp = 32,
		GL_FORMAT(GL_BGRA_EXT),
		GL_TYPE(GL_UNSIGNED_BYTE),
	},
	{
		.format = DRM_FORMAT_ARGB8888,
		.opaque_substitute = DRM_FORMAT_XRGB8888,
		.depth = 32,
		.bpp = 32,
		GL_FORMAT(GL_BGRA_EXT),
		GL_TYPE(GL_UNSIGNED_BYTE),
	},
	{
		.format = DRM_FORMAT_XBGR8888,
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_BYTE),
	},
	{
		.format = DRM_FORMAT_ABGR8888,
		.opaque_substitute = DRM_FORMAT_XBGR8888,
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_BYTE),
	},
	{
		.format = DRM_FORMAT_RGBX8888,
	},
	{
		.format = DRM_FORMAT_RGBA8888,
		.opaque_substitute = DRM_FORMAT_RGBX8888,
	},
	{
		.format = DRM_FORMAT_BGRX8888,
	},
	{
		.format = DRM_FORMAT_BGRA8888,
		.opaque_substitute = DRM_FORMAT_BGRX8888,
	},
	{
		.format = DRM_FORMAT_XRGB2101010,
		.depth = 30,
		.bpp = 32,
	},
	{
		.format = DRM_FORMAT_ARGB2101010,
		.opaque_substitute = DRM_FORMAT_XRGB2101010,
	},
	{
		.format = DRM_FORMAT_XBGR2101010,
# if __BYTE_ORDER == __LITTLE_ENDIAN
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_INT_2_10_10_10_REV_EXT),
#endif
	},
	{
		.format = DRM_FORMAT_ABGR2101010,
		.opaque_substitute = DRM_FORMAT_XBGR2101010,
# if __BYTE_ORDER == __LITTLE_ENDIAN
		GL_FORMAT(GL_RGBA),
		GL_TYPE(GL_UNSIGNED_INT_2_10_10_10_REV_EXT),
#endif
	},
	{
		.format = DRM_FORMAT_RGBX1010102,
	},
	{
		.format = DRM_FORMAT_RGBA1010102,
		.opaque_substitute = DRM_FORMAT_RGBX1010102,
	},
	{
		.format = DRM_FORMAT_BGRX1010102,
	},
	{
		.format = DRM_FORMAT_BGRA1010102,
		.opaque_substitute = DRM_FORMAT_BGRX1010102,
	},
	{
		.format = DRM_FORMAT_YUYV,
		SAMPLER_TYPE(EGL_TEXTURE_Y_XUXV_WL),
		.num_planes = 1,
		.hsub = 2,
	},
	{
		.format = DRM_FORMAT_YVYU,
		SAMPLER_TYPE(EGL_TEXTURE_Y_XUXV_WL),
		.num_planes = 1,
		.chroma_order = ORDER_VU,
		.hsub = 2,
	},
	{
		.format = DRM_FORMAT_UYVY,
		SAMPLER_TYPE(EGL_TEXTURE_Y_XUXV_WL),
		.num_planes = 1,
		.luma_chroma_order = ORDER_CHROMA_LUMA,
		.hsub = 2,
	},
	{
		.format = DRM_FORMAT_VYUY,
		SAMPLER_TYPE(EGL_TEXTURE_Y_XUXV_WL),
		.num_planes = 1,
		.luma_chroma_order = ORDER_CHROMA_LUMA,
		.chroma_order = ORDER_VU,
		.hsub = 2,
	},
	{
		.format = DRM_FORMAT_NV12,
		SAMPLER_TYPE(EGL_TEXTURE_Y_UV_WL),
		.num_planes = 2,
		.hsub = 2,
		.vsub = 2,
	},
	{
		.format = DRM_FORMAT_NV21,
		SAMPLER_TYPE(EGL_TEXTURE_Y_UV_WL),
		.num_planes = 2,
		.chroma_order = ORDER_VU,
		.hsub = 2,
		.vsub = 2,
	},
	{
		.format = DRM_FORMAT_NV16,
		SAMPLER_TYPE(EGL_TEXTURE_Y_UV_WL),
		.num_planes = 2,
		.hsub = 2,
		.vsub = 1,
	},
	{
		.format = DRM_FORMAT_NV61,
		SAMPLER_TYPE(EGL_TEXTURE_Y_UV_WL),
		.num_planes = 2,
		.chroma_order = ORDER_VU,
		.hsub = 2,
		.vsub = 1,
	},
	{
		.format = DRM_FORMAT_NV24,
		SAMPLER_TYPE(EGL_TEXTURE_Y_UV_WL),
		.num_planes = 2,
	},
	{
		.format = DRM_FORMAT_NV42,
		SAMPLER_TYPE(EGL_TEXTURE_Y_UV_WL),
		.num_planes = 2,
		.chroma_order = ORDER_VU,
	},
	{
		.format = DRM_FORMAT_YUV410,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.hsub = 4,
		.vsub = 4,
	},
	{
		.format = DRM_FORMAT_YVU410,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.chroma_order = ORDER_VU,
		.hsub = 4,
		.vsub = 4,
	},
	{
		.format = DRM_FORMAT_YUV411,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.hsub = 4,
		.vsub = 1,
	},
	{
		.format = DRM_FORMAT_YVU411,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.chroma_order = ORDER_VU,
		.hsub = 4,
		.vsub = 1,
	},
	{
		.format = DRM_FORMAT_YUV420,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.hsub = 2,
		.vsub = 2,
	},
	{
		.format = DRM_FORMAT_YVU420,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.chroma_order = ORDER_VU,
		.hsub = 2,
		.vsub = 2,
	},
	{
		.format = DRM_FORMAT_YUV422,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.hsub = 2,
		.vsub = 1,
	},
	{
		.format = DRM_FORMAT_YVU422,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.chroma_order = ORDER_VU,
		.hsub = 2,
		.vsub = 1,
	},
	{
		.format = DRM_FORMAT_YUV444,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
	},
	{
		.format = DRM_FORMAT_YVU444,
		SAMPLER_TYPE(EGL_TEXTURE_Y_U_V_WL),
		.num_planes = 3,
		.chroma_order = ORDER_VU,
	},
};

WL_EXPORT const struct pixel_format_info *
pixel_format_get_info(uint32_t format)
{
	unsigned int i;

	for (i = 0; i < ARRAY_LENGTH(pixel_format_table); i++) {
		if (pixel_format_table[i].format == format)
			return &pixel_format_table[i];
	}

	return NULL;
}

WL_EXPORT unsigned int
pixel_format_get_plane_count(const struct pixel_format_info *info)
{
	return info->num_planes ? info->num_planes : 1;
}

WL_EXPORT bool
pixel_format_is_opaque(const struct pixel_format_info *info)
{
	return !info->opaque_substitute;
}

WL_EXPORT const struct pixel_format_info *
pixel_format_get_opaque_substitute(const struct pixel_format_info *info)
{
	if (!info->opaque_substitute)
		return info;
	else
		return pixel_format_get_info(info->opaque_substitute);
}

WL_EXPORT unsigned int
pixel_format_width_for_plane(const struct pixel_format_info *info,
			     unsigned int plane,
			     unsigned int width)
{
	/* We don't support any formats where the first plane is subsampled. */
	if (plane == 0 || !info->hsub)
		return width;

	return width / info->hsub;
}

WL_EXPORT unsigned int
pixel_format_height_for_plane(const struct pixel_format_info *info,
			      unsigned int plane,
			      unsigned int height)
{
	/* We don't support any formats where the first plane is subsampled. */
	if (plane == 0 || !info->vsub)
		return height;

	return height / info->vsub;
}
