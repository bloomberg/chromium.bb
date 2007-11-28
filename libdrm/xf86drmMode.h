/*
 * \file xf86drmMode.h
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

#include <drm.h>
#include "xf86mm.h"

/*
 * This is the interface for modesetting for drm.
 *
 * In order to use this interface you must include either <stdint.h> or another
 * header defining uint32_t, int32_t and uint16_t.
 *
 * It aims to provide a randr1.2 compatible interface for modesettings in the
 * kernel, the interface is also ment to be used by libraries like EGL.
 *
 * More information can be found in randrproto.txt which can be found here:
 * http://gitweb.freedesktop.org/?p=xorg/proto/randrproto.git
 *
 * There are some major diffrences to be noted. Unlike the randr1.2 proto you
 * need to create the memory object of the framebuffer yourself with the ttm
 * buffer object interface. This object needs to be pinned.
 */


typedef struct _drmModeRes {

	int count_fbs;
	uint32_t *fbs;

	int count_crtcs;
	uint32_t *crtcs;

	int count_outputs;
	uint32_t *outputs;

} drmModeRes, *drmModeResPtr;

typedef struct drm_mode_fb_cmd drmModeFB, *drmModeFBPtr;

typedef struct _drmModeProperty {
	unsigned int prop_id;
	unsigned int flags;
	unsigned char name[DRM_PROP_NAME_LEN];
	int count_values;
	uint32_t *values;
	int count_enums;
	struct drm_mode_property_enum *enums;

} drmModePropertyRes, *drmModePropertyPtr;

typedef struct _drmModeCrtc {
	unsigned int crtc_id;
	unsigned int buffer_id; /**< FB id to connect to 0 = disconnect*/

	uint32_t x, y; /**< Position on the frameuffer */
	uint32_t width, height;
	int mode_valid;
	struct drm_mode_modeinfo mode;

	int count_outputs;
	uint32_t outputs; /**< Outputs that are connected */

	int count_possibles;
	uint32_t possibles; /**< Outputs that can be connected */

	int gamma_size; /**< Number of gamma stops */

} drmModeCrtc, *drmModeCrtcPtr;

typedef enum {
	DRM_MODE_CONNECTED         = 1,
	DRM_MODE_DISCONNECTED      = 2,
	DRM_MODE_UNKNOWNCONNECTION = 3
} drmModeConnection;

typedef enum {
	DRM_MODE_SUBPIXEL_UNKNOWN        = 1,
	DRM_MODE_SUBPIXEL_HORIZONTAL_RGB = 2,
	DRM_MODE_SUBPIXEL_HORIZONTAL_BGR = 3,
	DRM_MODE_SUBPIXEL_VERTICAL_RGB   = 4,
	DRM_MODE_SUBPIXEL_VERTICAL_BGR   = 5,
	DRM_MODE_SUBPIXEL_NONE           = 6
} drmModeSubPixel;

typedef struct _drmModeOutput {
	unsigned int output_id;

	unsigned int crtc; /**< Crtc currently connected to */
	unsigned char name[DRM_OUTPUT_NAME_LEN];
	drmModeConnection connection;
	uint32_t mmWidth, mmHeight; /**< HxW in millimeters */
	drmModeSubPixel subpixel;

	int count_crtcs;
	uint32_t crtcs; /**< Possible crtc to connect to */

	int count_clones;
	uint32_t clones; /**< Mask of clones */

	int count_modes;
	struct drm_mode_modeinfo *modes;

	int count_props;
	uint32_t *props; /**< List of property ids */
	uint32_t *prop_values; /**< List of property values */

} drmModeOutput, *drmModeOutputPtr;



extern void drmModeFreeModeInfo( struct drm_mode_modeinfo *ptr );
extern void drmModeFreeResources( drmModeResPtr ptr );
extern void drmModeFreeFB( drmModeFBPtr ptr );
extern void drmModeFreeCrtc( drmModeCrtcPtr ptr );
extern void drmModeFreeOutput( drmModeOutputPtr ptr );

/**
 * Retrives all of the resources associated with a card.
 */
extern drmModeResPtr drmModeGetResources(int fd);


/*
 * FrameBuffer manipulation.
 */

/**
 * Retrive information about framebuffer bufferId
 */
extern drmModeFBPtr drmModeGetFB(int fd, uint32_t bufferId);

/**
 * Creates a new framebuffer with an buffer object as its scanout buffer.
 */
extern int drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth,
			uint8_t bpp, uint32_t pitch, drmBO *bo,
			uint32_t *buf_id);
/**
 * Destroies the given framebuffer.
 */
extern int drmModeRmFB(int fd, uint32_t bufferId);


/*
 * Crtc functions
 */

/**
 * Retrive information about the ctrt crtcId
 */
extern drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t crtcId);

/**
 * Set the mode on a crtc crtcId with the given mode modeId.
 */
int drmModeSetCrtc(int fd, uint32_t crtcId, uint32_t bufferId,
                   uint32_t x, uint32_t y, uint32_t *outputs, int count,
		   struct drm_mode_modeinfo *mode);


/*
 * Output manipulation
 */

/**
 * Retrive information about the output outputId.
 */
extern drmModeOutputPtr drmModeGetOutput(int fd,
		uint32_t outputId);

/**
 * Adds a new mode from the given mode info.
 * Name must be unique.
 */
extern uint32_t drmModeAddMode(int fd, struct drm_mode_modeinfo *modeInfo);

/**
 * Removes a mode created with AddMode, must be unused.
 */
extern int drmModeRmMode(int fd, uint32_t modeId);

/**
 * Attaches the given mode to an output.
 */
extern int drmModeAttachMode(int fd, uint32_t outputId, uint32_t modeId);

/**
 * Detaches a mode from the output
 * must be unused, by the given mode.
 */
extern int drmModeDetachMode(int fd, uint32_t outputId, uint32_t modeId);

extern drmModePropertyPtr drmModeGetProperty(int fd, uint32_t propertyId);
extern void drmModeFreeProperty(drmModePropertyPtr ptr);
