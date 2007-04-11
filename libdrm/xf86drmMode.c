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

	drmFree(ptr->modes);
	drmFree(ptr);

}

void drmModeFreeFrameBuffer(drmModeFrameBufferPtr ptr)
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
		res.fb_id = drmMalloc(res.count_fbs*sizeof(uint32_t));
	if (res.count_crtcs)
		res.crtc_id = drmMalloc(res.count_crtcs*sizeof(uint32_t));
	if (res.count_outputs)
		res.output_id = drmMalloc(res.count_outputs*sizeof(uint32_t));
	if (res.count_modes)
		res.modes = drmMalloc(res.count_modes*sizeof(*res.modes));

	if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res)) {
		r = NULL;
		goto err_allocs;
	}

	/*
	 * return
	 */


	if (!(r = drmMalloc(sizeof(*r))))
		return 0;

	r->count_fbs     = res.count_fbs;
	r->count_crtcs   = res.count_crtcs;
	r->count_outputs = res.count_outputs;
	r->count_modes   = res.count_modes;
	/* TODO we realy should test if these allocs fails. */
	r->fbs           = drmAllocCpy(res.fb_id, res.count_fbs, sizeof(uint32_t));
	r->crtcs         = drmAllocCpy(res.crtc_id, res.count_crtcs, sizeof(uint32_t));
	r->outputs       = drmAllocCpy(res.output_id, res.count_outputs, sizeof(uint32_t));
	r->modes         = drmAllocCpy(res.modes, res.count_modes, sizeof(struct drm_mode_modeinfo));

err_allocs:
	drmFree(res.fb_id);
	drmFree(res.crtc_id);
	drmFree(res.output_id);
	drmFree(res.modes);

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
	return ioctl(fd, DRM_IOCTL_MODE_RMFB, bufferId);
}

#if 0
int drmModeForceProbe(int fd, uint32_t outputId)
{
	/* TODO impl/keep? */
}

drmModeFrameBufferPtr drmModeGetFrameBuffer(int fd, uint32_t buf)
{
//	struct drm_mode_fb_cmd info;
	drmModeFrameBufferPtr r;

	//	if (ioctl(fd, DRM_IOCTL_MODE_GETFRAMEBUFFER, &info))
		return 0;

	if (!(r = drmMalloc(sizeof(*r))))
		return 0;

	/* TODO change to new code
	r->minWidth  = info.minWidth;
	r->maxWidth  = info.maxWidth;
	r->minHeight = info.minHeight;
	r->maxHeight = info.maxHeight;*/

	return r;
}

uint32_t drmModeNewFrameBuffer(int fd, uint32_t width, uint32_t height,
		uint8_t bpp, uint32_t pitch, drmBO *bo)
{
	drm_mode_fb_cmd_t f;

	f.handle = bo->handle;
	f.width  = width;
	f.height = height;
	f.pitch  = pitch;

	//	if (ioctl(fd, DRM_IOCTL_MODE_NEWFRAMEBUFFER, &f))
		return 0;

	return f.bufferId;
}

int drmModeDesFrameBuffer(int fd, uint32_t bufferId)
{
  //	return ioctl(fd, DRM_IOCTL_MODE_DESFRAMEBUFFER, bufferId);
}

#endif
/*
 * Crtc function.
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

	r->x               = crtc.x;
	r->y               = crtc.y;
	r->mode            = crtc.mode;
//	r->width           = crtc.width;
//	r->height          = crtc.height;
	r->bufferId        = crtc.fb_id;
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
                   uint32_t x, uint32_t y, uint32_t modeId,
                   uint32_t *outputs, int count)
{
	struct drm_mode_crtc crtc;

	crtc.count_outputs   = 0;
	crtc.outputs         = 0;
	crtc.count_possibles = 0;
	crtc.possibles       = 0;

	crtc.x             = x;
	crtc.y             = y;
	crtc.crtc_id        = crtcId;
	crtc.fb_id      = bufferId;
	crtc.set_outputs   = outputs;
	crtc.count_outputs = count;
	crtc.mode          = modeId;

	return ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc);
}

#if 0
drmModeGammaTriplePtr drmModeGetCrtcGamma(int fd, uint32_t crtc, int *count)
{
	/* TODO impl */
}

int drmModeSetCrtcGamma(int fd, uint32_t crtcId,
		drmModeGammaTriplePtr ptr, int count)
{
	/* TODO impl */
}

#endif
/*
 * Output manipulation
 */
drmModeOutputPtr drmModeGetOutput(int fd, uint32_t output_id)
{
	struct drm_mode_get_output out;
	drmModeOutputPtr r = 0;

	out.output = output_id;
	out.count_crtcs  = 0;
	out.crtcs        = 0;
	out.count_clones = 0;
	out.clones       = 0;
	out.count_modes  = 0;
	out.modes        = 0;

	if (ioctl(fd, DRM_IOCTL_MODE_GETOUTPUT, &out))
		return 0;

	if (out.count_modes)
		out.modes = drmMalloc(out.count_modes*sizeof(uint32_t));

	if (ioctl(fd, DRM_IOCTL_MODE_GETOUTPUT, &out))
		goto err_allocs;

	if(!(r = drmMalloc(sizeof(*r))))
		return 0;

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
	r->modes        = drmAllocCpy(out.modes, out.count_modes, sizeof(uint32_t));
	strncpy(r->name, out.name, DRM_OUTPUT_NAME_LEN);
	r->name[DRM_OUTPUT_NAME_LEN-1] = 0;
	return r;

err_allocs:
	drmFree(out.modes);

	return 0;
}

#if 0
uint32_t drmModeNewMode(int fd, struct drm_mode_modeinfo *modeInfo)
{
	/* TODO impl */
}

int drmModeDesMode(int fd, uint32_t modeId)
{
  //	return ioctl(fd, DRM_IOCTL_MODE_DESMODE, modeId);
}

int drmModeAddMode(int fd, uint32_t outputId, uint32_t modeId)
{

	drm_mode_outputmode_t res;

	res.outputId = outputId;
	res.modeId = modeId;

	//	return ioctl(fd, DRM_IOCTL_MODE_ADDMODE, &res);
}

int drmModeDelMode(int fd, uint32_t outputId, uint32_t modeId)
{
	drm_mode_outputmode_t res;

	res.outputId = outputId;
	res.modeId = modeId;

	//	return ioctl(fd, DRM_IOCTL_MODE_DELMODE, &res);
}

#endif

