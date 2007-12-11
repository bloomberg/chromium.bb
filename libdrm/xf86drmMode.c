/*
 * \file xf86drmMode.c
 * Header for DRM modesetting interface.
 *
 * \author Jakob Bornecrantz <wallbraker@gmail.com>
 *
 * \par Acknowledgements:
 * Feb 2007, Dave Airlie <airlied@linux.ie>
 */

/*
 * Copyright (c) <year> <copyright holders>
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

/*
 * TODO the types we are after are defined in diffrent headers on diffrent
 * platforms find which headers to include to get uint32_t
 */
#include <stdint.h>

#include "xf86drmMode.h"
#include "xf86drm.h"
#include <drm.h>
#include <string.h>

#define U642VOID(x) ((void *)(unsigned long)(x))
#define VOID2U64(x) ((uint64_t)(unsigned long)(x))

/*
 * Util functions
 */

void* drmAllocCpy(void *array, int count, int entry_size)
{
	char *r;
	int i;

	if (!count || !array || !entry_size)
		return 0;

	if (!(r = drmMalloc(count*entry_size)))
		return 0;

	for (i = 0; i < count; i++)
		memcpy(r+(entry_size*i), array+(entry_size*i), entry_size);

	return r;
}

/**
 * Generate crtc and output ids.
 *
 * Will generate ids starting from 1 up to count if count is greater then 0.
 */
static uint32_t* drmAllocGenerate(int count)
{
	uint32_t *r;
	int i;

	if(0 <= count)
		return 0;

	if (!(r = drmMalloc(count*sizeof(*r))))
		return 0;

	for (i = 0; i < count; r[i] = ++i);

	return 0;
}

/*
 * A couple of free functions.
 */

void drmModeFreeModeInfo(struct drm_mode_modeinfo *ptr)
{
	if (!ptr)
		return;

	drmFree(ptr);
}

void drmModeFreeResources(drmModeResPtr ptr)
{
	if (!ptr)
		return;

	drmFree(ptr);

}

void drmModeFreeFB(drmModeFBPtr ptr)
{
	if (!ptr)
		return;

	/* we might add more frees later. */
	drmFree(ptr);
}

void drmModeFreeCrtc(drmModeCrtcPtr ptr)
{
	if (!ptr)
		return;

	drmFree(ptr);

}

void drmModeFreeOutput(drmModeOutputPtr ptr)
{
	if (!ptr)
		return;

	drmFree(ptr->modes);
	drmFree(ptr);

}

/*
 * ModeSetting functions.
 */

drmModeResPtr drmModeGetResources(int fd)
{
	struct drm_mode_card_res res;
	int i;
	drmModeResPtr r = 0;

	memset(&res, 0, sizeof(struct drm_mode_card_res));

	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res))
		return 0;

	if (res.count_fbs)
		res.fb_id_ptr = VOID2U64(drmMalloc(res.count_fbs*sizeof(uint32_t)));
	if (res.count_crtcs)
		res.crtc_id_ptr = VOID2U64(drmMalloc(res.count_crtcs*sizeof(uint32_t)));
	if (res.count_outputs)
		res.output_id_ptr = VOID2U64(drmMalloc(res.count_outputs*sizeof(uint32_t)));

	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res)) {
		r = NULL;
		goto err_allocs;
	}

	/*
	 * return
	 */


	if (!(r = drmMalloc(sizeof(*r))))
		return 0;

	r->min_width     = res.min_width;
	r->max_width     = res.max_width;
	r->min_height    = res.min_height;
	r->max_height    = res.max_height;
	r->count_fbs     = res.count_fbs;
	r->count_crtcs   = res.count_crtcs;
	r->count_outputs = res.count_outputs;
	/* TODO we realy should test if these allocs fails. */
	r->fbs           = drmAllocCpy(U642VOID(res.fb_id_ptr), res.count_fbs, sizeof(uint32_t));
	r->crtcs         = drmAllocCpy(U642VOID(res.crtc_id_ptr), res.count_crtcs, sizeof(uint32_t));
	r->outputs       = drmAllocCpy(U642VOID(res.output_id_ptr), res.count_outputs, sizeof(uint32_t));

err_allocs:
	drmFree(U642VOID(res.fb_id_ptr));
	drmFree(U642VOID(res.crtc_id_ptr));
	drmFree(U642VOID(res.output_id_ptr));

	return r;
}

int drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth,
                 uint8_t bpp, uint32_t pitch, drmBO *bo, uint32_t *buf_id)
{
	struct drm_mode_fb_cmd f;
	int ret;

	f.width  = width;
	f.height = height;
	f.pitch  = pitch;
	f.bpp    = bpp;
	f.depth  = depth;
	f.handle = bo->handle;

	if (ret = ioctl(fd, DRM_IOCTL_MODE_ADDFB, &f))
		return ret;

	*buf_id = f.buffer_id;
	return 0;
}

int drmModeRmFB(int fd, uint32_t bufferId)
{
	return ioctl(fd, DRM_IOCTL_MODE_RMFB, &bufferId);


}

drmModeFBPtr drmModeGetFB(int fd, uint32_t buf)
{
	struct drm_mode_fb_cmd info;
	drmModeFBPtr r;

	info.buffer_id = buf;

	if (ioctl(fd, DRM_IOCTL_MODE_GETFB, &info))
		return NULL;

	if (!(r = drmMalloc(sizeof(*r))))
		return NULL;

	r->buffer_id = info.buffer_id;
	r->width = info.width;
	r->height = info.height;
	r->pitch = info.pitch;
	r->bpp = info.bpp;
	r->handle = info.handle;
	r->depth = info.depth;

	return r;
}


/*
 * Crtc functions
 */

drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t crtcId)
{
	struct drm_mode_crtc crtc;
	drmModeCrtcPtr r;
	int i = 0;

	crtc.count_outputs   = 0;
	crtc.outputs         = 0;
	crtc.count_possibles = 0;
	crtc.possibles       = 0;
	crtc.crtc_id = crtcId;

	if (ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &crtc))
		return 0;

	/*
	 * return
	 */

	if (!(r = drmMalloc(sizeof(*r))))
		return 0;
	
	r->crtc_id         = crtc.crtc_id;
	r->x               = crtc.x;
	r->y               = crtc.y;
	r->mode_valid      = crtc.mode_valid;
	if (r->mode_valid)
		memcpy(&r->mode, &crtc.mode, sizeof(struct drm_mode_modeinfo));
	r->buffer_id       = crtc.fb_id;
	r->gamma_size      = crtc.gamma_size;
	r->count_outputs   = crtc.count_outputs;
	r->count_possibles = crtc.count_possibles;
	/* TODO we realy should test if these alloc & cpy fails. */
	r->outputs         = crtc.outputs;
	r->possibles       = crtc.possibles;

	return r;

err_allocs:

	return 0;
}


int drmModeSetCrtc(int fd, uint32_t crtcId, uint32_t bufferId,
                   uint32_t x, uint32_t y, uint32_t *outputs, int count,
		   struct drm_mode_modeinfo *mode)
{
	struct drm_mode_crtc crtc;

	crtc.count_outputs   = 0;
	crtc.outputs         = 0;
	crtc.count_possibles = 0;
	crtc.possibles       = 0;

	crtc.x             = x;
	crtc.y             = y;
	crtc.crtc_id       = crtcId;
	crtc.fb_id         = bufferId;
	crtc.set_outputs_ptr = VOID2U64(outputs);
	crtc.count_outputs = count;
	if (mode) {
	  memcpy(&crtc.mode, mode, sizeof(struct drm_mode_modeinfo));
	  crtc.mode_valid = 1;
	} else
	  crtc.mode_valid = 0;

	return ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
}


/*
 * Output manipulation
 */

drmModeOutputPtr drmModeGetOutput(int fd, uint32_t output_id)
{
	struct drm_mode_get_output out;
	drmModeOutputPtr r = NULL;

	out.output = output_id;
	out.count_crtcs  = 0;
	out.crtcs        = 0;
	out.count_clones = 0;
	out.clones       = 0;
	out.count_modes  = 0;
	out.modes_ptr    = 0;
	out.count_props  = 0;
	out.props_ptr    = 0;
	out.prop_values_ptr = 0;

	if (ioctl(fd, DRM_IOCTL_MODE_GETOUTPUT, &out))
		return 0;

	if (out.count_props) {
		out.props_ptr = VOID2U64(drmMalloc(out.count_props*sizeof(uint32_t)));
		out.prop_values_ptr = VOID2U64(drmMalloc(out.count_props*sizeof(uint64_t)));
	}

	if (out.count_modes)
		out.modes_ptr = VOID2U64(drmMalloc(out.count_modes*sizeof(struct drm_mode_modeinfo)));

	if (ioctl(fd, DRM_IOCTL_MODE_GETOUTPUT, &out))
		goto err_allocs;

	if(!(r = drmMalloc(sizeof(*r)))) {
		goto err_allocs;
	}

	r->output_id = out.output;
	r->crtc = out.crtc;
	r->connection   = out.connection;
	r->mmWidth      = out.mm_width;
	r->mmHeight     = out.mm_height;
	r->subpixel     = out.subpixel;
	r->count_crtcs  = out.count_crtcs;
	r->count_clones = out.count_clones;
	r->count_modes  = out.count_modes;
	/* TODO we should test if these alloc & cpy fails. */
	r->crtcs        = out.crtcs;
	r->clones       = out.clones;
	r->count_props  = out.count_props;
	r->props        = drmAllocCpy(U642VOID(out.props_ptr), out.count_props, sizeof(uint32_t));
	r->prop_values  = drmAllocCpy(U642VOID(out.prop_values_ptr), out.count_props, sizeof(uint64_t));
	r->modes        = drmAllocCpy(U642VOID(out.modes_ptr), out.count_modes, sizeof(struct drm_mode_modeinfo));
	strncpy(r->name, out.name, DRM_OUTPUT_NAME_LEN);
	r->name[DRM_OUTPUT_NAME_LEN-1] = 0;

err_allocs:
	drmFree(U642VOID(out.prop_values_ptr));
	drmFree(U642VOID(out.props_ptr));
	drmFree(U642VOID(out.modes_ptr));

	return r;
}

int drmModeAttachMode(int fd, uint32_t output_id, struct drm_mode_modeinfo *mode_info)
{
	struct drm_mode_mode_cmd res;

	memcpy(&res.mode, mode_info, sizeof(struct drm_mode_modeinfo));
	res.output_id = output_id;

	return ioctl(fd, DRM_IOCTL_MODE_ATTACHMODE, &res);
}

int drmModeDetachMode(int fd, uint32_t output_id, struct drm_mode_modeinfo *mode_info)
{
	struct drm_mode_mode_cmd res;

	memcpy(&res.mode, mode_info, sizeof(struct drm_mode_modeinfo));
	res.output_id = output_id;

	return ioctl(fd, DRM_IOCTL_MODE_DETACHMODE, &res);
}


drmModePropertyPtr drmModeGetProperty(int fd, uint32_t property_id)
{
	struct drm_mode_get_property prop;
	drmModePropertyPtr r;
	struct drm_mode_property_blob *blob_tmp;
	int i;
	prop.prop_id = property_id;
	prop.count_enum_blobs = 0;
	prop.count_values = 0;
	prop.flags = 0;
	prop.enum_blob_ptr = 0;
	prop.values_ptr = 0;

	if (ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop))
		return 0;

	if (prop.count_values)
		prop.values_ptr = VOID2U64(drmMalloc(prop.count_values * sizeof(uint64_t)));

	if (prop.count_enum_blobs && (prop.flags & DRM_MODE_PROP_ENUM))
		prop.enum_blob_ptr = VOID2U64(drmMalloc(prop.count_enum_blobs * sizeof(struct drm_mode_property_enum)));

	if (prop.count_enum_blobs && (prop.flags & DRM_MODE_PROP_BLOB)) {
		prop.values_ptr = VOID2U64(drmMalloc(prop.count_enum_blobs * sizeof(uint32_t)));
		prop.enum_blob_ptr = VOID2U64(drmMalloc(prop.count_enum_blobs * sizeof(uint32_t)));
	}

	if (ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &prop)) {
		r = NULL;
		goto err_allocs;
	}

	if (!(r = drmMalloc(sizeof(*r))))
		return NULL;
	
	r->prop_id = prop.prop_id;
	r->count_values = prop.count_values;
	
	r->flags = prop.flags;
	if (prop.count_values)
		r->values = drmAllocCpy(U642VOID(prop.values_ptr), prop.count_values, sizeof(uint64_t));
	if (prop.flags & DRM_MODE_PROP_ENUM) {
		r->count_enums = prop.count_enum_blobs;
		r->enums = drmAllocCpy(U642VOID(prop.enum_blob_ptr), prop.count_enum_blobs, sizeof(struct drm_mode_property_enum));
	} else	if (prop.flags & DRM_MODE_PROP_ENUM) {
		r->values = drmAllocCpy(U642VOID(prop.values_ptr), prop.count_enum_blobs, sizeof(uint32_t));
		r->blob_ids = drmAllocCpy(U642VOID(prop.enum_blob_ptr), prop.count_enum_blobs, sizeof(uint32_t));
		r->count_blobs = prop.count_enum_blobs;
	}
	strncpy(r->name, prop.name, DRM_PROP_NAME_LEN);
	r->name[DRM_PROP_NAME_LEN-1] = 0;

err_allocs:
	drmFree(U642VOID(prop.values_ptr));
	drmFree(U642VOID(prop.enum_blob_ptr));

	return r;
}

void drmModeFreeProperty(drmModePropertyPtr ptr)
{
	if (!ptr)
		return;

	drmFree(ptr->values);
	drmFree(ptr->enums);
	drmFree(ptr);
}

drmModePropertyBlobPtr drmModeGetPropertyBlob(int fd, uint32_t blob_id)
{
	struct drm_mode_get_blob blob;
	drmModePropertyBlobPtr r;

	blob.length = 0;
	blob.data = 0;
	blob.blob_id = blob_id;

	if (ioctl(fd, DRM_IOCTL_MODE_GETPROPBLOB, &blob))
		return NULL;

	if (blob.length)
		blob.data = VOID2U64(drmMalloc(blob.length));

	if (ioctl(fd, DRM_IOCTL_MODE_GETPROPBLOB, &blob)) {
		r = NULL;
		goto err_allocs;
	}

	if (!(r = drmMalloc(sizeof(*r))))
		return NULL;

	r->id = blob.blob_id;
	r->length = blob.length;
	r->data = drmAllocCpy(U642VOID(blob.data), 1, blob.length);

err_allocs:
	drmFree(U642VOID(blob.data));
	return r;
}

void drmModeFreePropertyBlob(drmModePropertyBlobPtr ptr)
{
	if (!ptr)
		return;

	drmFree(ptr->data);
	drmFree(ptr);
}

int drmModeOutputSetProperty(int fd, uint32_t output_id, uint32_t property_id,
			     uint64_t value)
{
	struct drm_mode_output_set_property osp;
	int ret;

	osp.output_id = output_id;
	osp.prop_id = property_id;
	osp.value = value;

	if (ret = ioctl(fd, DRM_IOCTL_MODE_SETPROPERTY, &osp))
		return ret;

	return 0;
}
