/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2017, 2018 Collabora, Ltd.
 * Copyright © 2017, 2018 General Electric Company
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <assert.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <time.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <gbm.h>
#include <libudev.h>

#include "compositor.h"
#include "compositor-drm.h"
#include "shared/helpers.h"
#include "shared/timespec-util.h"
#include "gl-renderer.h"
#include "weston-egl-ext.h"
#include "pixman-renderer.h"
#include "pixel-formats.h"
#include "libbacklight.h"
#include "libinput-seat.h"
#include "launcher-util.h"
#include "vaapi-recorder.h"
#include "presentation-time-server-protocol.h"
#include "linux-dmabuf.h"
#include "linux-dmabuf-unstable-v1-server-protocol.h"

#ifndef DRM_CLIENT_CAP_ASPECT_RATIO
#define DRM_CLIENT_CAP_ASPECT_RATIO	4
#endif

#ifndef GBM_BO_USE_CURSOR
#define GBM_BO_USE_CURSOR GBM_BO_USE_CURSOR_64X64
#endif

#define MAX_CLONED_CONNECTORS 4

/**
 * aspect ratio info taken from the drmModeModeInfo flag bits 19-22,
 * which should be used to fill the aspect ratio field in weston_mode.
 */
#define DRM_MODE_FLAG_PIC_AR_BITS_POS	19
#ifndef DRM_MODE_FLAG_PIC_AR_MASK
#define DRM_MODE_FLAG_PIC_AR_MASK (0xF << DRM_MODE_FLAG_PIC_AR_BITS_POS)
#endif

/**
 * Represents the values of an enum-type KMS property
 */
struct drm_property_enum_info {
	const char *name; /**< name as string (static, not freed) */
	bool valid; /**< true if value is supported; ignore if false */
	uint64_t value; /**< raw value */
};

/**
 * Holds information on a DRM property, including its ID and the enum
 * values it holds.
 *
 * DRM properties are allocated dynamically, and maintained as DRM objects
 * within the normal object ID space; they thus do not have a stable ID
 * to refer to. This includes enum values, which must be referred to by
 * integer values, but these are not stable.
 *
 * drm_property_info allows a cache to be maintained where Weston can use
 * enum values internally to refer to properties, with the mapping to DRM
 * ID values being maintained internally.
 */
struct drm_property_info {
	const char *name; /**< name as string (static, not freed) */
	uint32_t prop_id; /**< KMS property object ID */
	unsigned int num_enum_values; /**< number of enum values */
	struct drm_property_enum_info *enum_values; /**< array of enum values */
};

/**
 * List of properties attached to DRM planes
 */
enum wdrm_plane_property {
	WDRM_PLANE_TYPE = 0,
	WDRM_PLANE_SRC_X,
	WDRM_PLANE_SRC_Y,
	WDRM_PLANE_SRC_W,
	WDRM_PLANE_SRC_H,
	WDRM_PLANE_CRTC_X,
	WDRM_PLANE_CRTC_Y,
	WDRM_PLANE_CRTC_W,
	WDRM_PLANE_CRTC_H,
	WDRM_PLANE_FB_ID,
	WDRM_PLANE_CRTC_ID,
	WDRM_PLANE_IN_FORMATS,
	WDRM_PLANE__COUNT
};

/**
 * Possible values for the WDRM_PLANE_TYPE property.
 */
enum wdrm_plane_type {
	WDRM_PLANE_TYPE_PRIMARY = 0,
	WDRM_PLANE_TYPE_CURSOR,
	WDRM_PLANE_TYPE_OVERLAY,
	WDRM_PLANE_TYPE__COUNT
};

static struct drm_property_enum_info plane_type_enums[] = {
	[WDRM_PLANE_TYPE_PRIMARY] = {
		.name = "Primary",
	},
	[WDRM_PLANE_TYPE_OVERLAY] = {
		.name = "Overlay",
	},
	[WDRM_PLANE_TYPE_CURSOR] = {
		.name = "Cursor",
	},
};

static const struct drm_property_info plane_props[] = {
	[WDRM_PLANE_TYPE] = {
		.name = "type",
		.enum_values = plane_type_enums,
		.num_enum_values = WDRM_PLANE_TYPE__COUNT,
	},
	[WDRM_PLANE_SRC_X] = { .name = "SRC_X", },
	[WDRM_PLANE_SRC_Y] = { .name = "SRC_Y", },
	[WDRM_PLANE_SRC_W] = { .name = "SRC_W", },
	[WDRM_PLANE_SRC_H] = { .name = "SRC_H", },
	[WDRM_PLANE_CRTC_X] = { .name = "CRTC_X", },
	[WDRM_PLANE_CRTC_Y] = { .name = "CRTC_Y", },
	[WDRM_PLANE_CRTC_W] = { .name = "CRTC_W", },
	[WDRM_PLANE_CRTC_H] = { .name = "CRTC_H", },
	[WDRM_PLANE_FB_ID] = { .name = "FB_ID", },
	[WDRM_PLANE_CRTC_ID] = { .name = "CRTC_ID", },
	[WDRM_PLANE_IN_FORMATS] = { .name = "IN_FORMATS" },
};

/**
 * List of properties attached to a DRM connector
 */
enum wdrm_connector_property {
	WDRM_CONNECTOR_EDID = 0,
	WDRM_CONNECTOR_DPMS,
	WDRM_CONNECTOR_CRTC_ID,
	WDRM_CONNECTOR__COUNT
};

enum wdrm_dpms_state {
	WDRM_DPMS_STATE_OFF = 0,
	WDRM_DPMS_STATE_ON,
	WDRM_DPMS_STATE_STANDBY, /* unused */
	WDRM_DPMS_STATE_SUSPEND, /* unused */
	WDRM_DPMS_STATE__COUNT
};

static struct drm_property_enum_info dpms_state_enums[] = {
	[WDRM_DPMS_STATE_OFF] = {
		.name = "Off",
	},
	[WDRM_DPMS_STATE_ON] = {
		.name = "On",
	},
	[WDRM_DPMS_STATE_STANDBY] = {
		.name = "Standby",
	},
	[WDRM_DPMS_STATE_SUSPEND] = {
		.name = "Suspend",
	},
};

static const struct drm_property_info connector_props[] = {
	[WDRM_CONNECTOR_EDID] = { .name = "EDID" },
	[WDRM_CONNECTOR_DPMS] = {
		.name = "DPMS",
		.enum_values = dpms_state_enums,
		.num_enum_values = WDRM_DPMS_STATE__COUNT,
	},
	[WDRM_CONNECTOR_CRTC_ID] = { .name = "CRTC_ID", },
};

/**
 * List of properties attached to DRM CRTCs
 */
enum wdrm_crtc_property {
	WDRM_CRTC_MODE_ID = 0,
	WDRM_CRTC_ACTIVE,
	WDRM_CRTC__COUNT
};

static const struct drm_property_info crtc_props[] = {
	[WDRM_CRTC_MODE_ID] = { .name = "MODE_ID", },
	[WDRM_CRTC_ACTIVE] = { .name = "ACTIVE", },
};

/**
 * Mode for drm_output_state_duplicate.
 */
enum drm_output_state_duplicate_mode {
	DRM_OUTPUT_STATE_CLEAR_PLANES, /**< reset all planes to off */
	DRM_OUTPUT_STATE_PRESERVE_PLANES, /**< preserve plane state */
};

/**
 * Mode for drm_pending_state_apply and co.
 */
enum drm_state_apply_mode {
	DRM_STATE_APPLY_SYNC, /**< state fully processed */
	DRM_STATE_APPLY_ASYNC, /**< state pending event delivery */
	DRM_STATE_TEST_ONLY, /**< test if the state can be applied */
};

struct drm_backend {
	struct weston_backend base;
	struct weston_compositor *compositor;

	struct udev *udev;
	struct wl_event_source *drm_source;

	struct udev_monitor *udev_monitor;
	struct wl_event_source *udev_drm_source;

	struct {
		int id;
		int fd;
		char *filename;
	} drm;
	struct gbm_device *gbm;
	struct wl_listener session_listener;
	uint32_t gbm_format;

	/* we need these parameters in order to not fail drmModeAddFB2()
	 * due to out of bounds dimensions, and then mistakenly set
	 * sprites_are_broken:
	 */
	int min_width, max_width;
	int min_height, max_height;

	struct wl_list plane_list;
	int sprites_are_broken;
	int sprites_hidden;

	void *repaint_data;

	bool state_invalid;

	/* CRTC IDs not used by any enabled output. */
	struct wl_array unused_crtcs;

	int cursors_are_broken;

	bool universal_planes;
	bool atomic_modeset;

	int use_pixman;
	bool use_pixman_shadow;

	struct udev_input input;

	int32_t cursor_width;
	int32_t cursor_height;

	uint32_t pageflip_timeout;

	bool shutting_down;

	bool aspect_ratio_supported;
};

struct drm_mode {
	struct weston_mode base;
	drmModeModeInfo mode_info;
	uint32_t blob_id;
};

enum drm_fb_type {
	BUFFER_INVALID = 0, /**< never used */
	BUFFER_CLIENT, /**< directly sourced from client */
	BUFFER_DMABUF, /**< imported from linux_dmabuf client */
	BUFFER_PIXMAN_DUMB, /**< internal Pixman rendering */
	BUFFER_GBM_SURFACE, /**< internal EGL rendering */
	BUFFER_CURSOR, /**< internal cursor buffer */
};

struct drm_fb {
	enum drm_fb_type type;

	int refcnt;

	uint32_t fb_id, size;
	uint32_t handles[4];
	uint32_t strides[4];
	uint32_t offsets[4];
	const struct pixel_format_info *format;
	uint64_t modifier;
	int width, height;
	int fd;
	struct weston_buffer_reference buffer_ref;

	/* Used by gbm fbs */
	struct gbm_bo *bo;
	struct gbm_surface *gbm_surface;

	/* Used by dumb fbs */
	void *map;
};

struct drm_edid {
	char eisa_id[13];
	char monitor_name[13];
	char pnp_id[5];
	char serial_number[13];
};

/**
 * Pending state holds one or more drm_output_state structures, collected from
 * performing repaint. This pending state is transient, and only lives between
 * beginning a repaint group and flushing the results: after flush, each
 * output state will complete and be retired separately.
 */
struct drm_pending_state {
	struct drm_backend *backend;
	struct wl_list output_list;
};

/*
 * Output state holds the dynamic state for one Weston output, i.e. a KMS CRTC,
 * plus >= 1 each of encoder/connector/plane. Since everything but the planes
 * is currently statically assigned per-output, we mainly use this to track
 * plane state.
 *
 * pending_state is set when the output state is owned by a pending_state,
 * i.e. when it is being constructed and has not yet been applied. When the
 * output state has been applied, the owning pending_state is freed.
 */
struct drm_output_state {
	struct drm_pending_state *pending_state;
	struct drm_output *output;
	struct wl_list link;
	enum dpms_enum dpms;
	struct wl_list plane_list;
};

/**
 * Plane state holds the dynamic state for a plane: where it is positioned,
 * and which buffer it is currently displaying.
 *
 * The plane state is owned by an output state, except when setting an initial
 * state. See drm_output_state for notes on state object lifetime.
 */
struct drm_plane_state {
	struct drm_plane *plane;
	struct drm_output *output;
	struct drm_output_state *output_state;

	struct drm_fb *fb;

	struct weston_view *ev; /**< maintained for drm_assign_planes only */

	int32_t src_x, src_y;
	uint32_t src_w, src_h;
	int32_t dest_x, dest_y;
	uint32_t dest_w, dest_h;

	bool complete;

	struct wl_list link; /* drm_output_state::plane_list */
};

/**
 * A plane represents one buffer, positioned within a CRTC, and stacked
 * relative to other planes on the same CRTC.
 *
 * Each CRTC has a 'primary plane', which use used to display the classic
 * framebuffer contents, as accessed through the legacy drmModeSetCrtc
 * call (which combines setting the CRTC's actual physical mode, and the
 * properties of the primary plane).
 *
 * The cursor plane also has its own alternate legacy API.
 *
 * Other planes are used opportunistically to display content we do not
 * wish to blit into the primary plane. These non-primary/cursor planes
 * are referred to as 'sprites'.
 */
struct drm_plane {
	struct weston_plane base;

	struct drm_backend *backend;

	enum wdrm_plane_type type;

	uint32_t possible_crtcs;
	uint32_t plane_id;
	uint32_t count_formats;

	struct drm_property_info props[WDRM_PLANE__COUNT];

	/* The last state submitted to the kernel for this plane. */
	struct drm_plane_state *state_cur;

	struct wl_list link;

	struct {
		uint32_t format;
		uint32_t count_modifiers;
		uint64_t *modifiers;
	} formats[];
};

struct drm_head {
	struct weston_head base;
	struct drm_backend *backend;

	drmModeConnector *connector;
	uint32_t connector_id;
	struct drm_edid edid;

	/* Holds the properties for the connector */
	struct drm_property_info props_conn[WDRM_CONNECTOR__COUNT];

	struct backlight *backlight;

	drmModeModeInfo inherited_mode;	/**< Original mode on the connector */
	uint32_t inherited_crtc_id;	/**< Original CRTC assignment */
};

struct drm_output {
	struct weston_output base;

	uint32_t crtc_id; /* object ID to pass to DRM functions */
	int pipe; /* index of CRTC in resource array / bitmasks */

	/* Holds the properties for the CRTC */
	struct drm_property_info props_crtc[WDRM_CRTC__COUNT];

	int vblank_pending;
	int page_flip_pending;
	int atomic_complete_pending;
	int destroy_pending;
	int disable_pending;
	int dpms_off_pending;

	struct drm_fb *gbm_cursor_fb[2];
	struct drm_plane *cursor_plane;
	struct weston_view *cursor_view;
	int current_cursor;

	struct gbm_surface *gbm_surface;
	uint32_t gbm_format;

	/* Plane being displayed directly on the CRTC */
	struct drm_plane *scanout_plane;

	/* The last state submitted to the kernel for this CRTC. */
	struct drm_output_state *state_cur;
	/* The previously-submitted state, where the hardware has not
	 * yet acknowledged completion of state_cur. */
	struct drm_output_state *state_last;

	struct drm_fb *dumb[2];
	pixman_image_t *image[2];
	int current_image;
	pixman_region32_t previous_damage;

	struct vaapi_recorder *recorder;
	struct wl_listener recorder_frame_listener;

	struct wl_event_source *pageflip_timer;
};

static const char *const aspect_ratio_as_string[] = {
	[WESTON_MODE_PIC_AR_NONE] = "",
	[WESTON_MODE_PIC_AR_4_3] = " 4:3",
	[WESTON_MODE_PIC_AR_16_9] = " 16:9",
	[WESTON_MODE_PIC_AR_64_27] = " 64:27",
	[WESTON_MODE_PIC_AR_256_135] = " 256:135",
};

static struct gl_renderer_interface *gl_renderer;

static const char default_seat[] = "seat0";

static void
wl_array_remove_uint32(struct wl_array *array, uint32_t elm)
{
	uint32_t *pos, *end;

	end = (uint32_t *) ((char *) array->data + array->size);

	wl_array_for_each(pos, array) {
		if (*pos != elm)
			continue;

		array->size -= sizeof(*pos);
		if (pos + 1 == end)
			break;

		memmove(pos, pos + 1, (char *) end -  (char *) (pos + 1));
		break;
	}
}

static inline struct drm_head *
to_drm_head(struct weston_head *base)
{
	return container_of(base, struct drm_head, base);
}

static inline struct drm_output *
to_drm_output(struct weston_output *base)
{
	return container_of(base, struct drm_output, base);
}

static inline struct drm_backend *
to_drm_backend(struct weston_compositor *base)
{
	return container_of(base->backend, struct drm_backend, base);
}

static int
pageflip_timeout(void *data) {
	/*
	 * Our timer just went off, that means we're not receiving drm
	 * page flip events anymore for that output. Let's gracefully exit
	 * weston with a return value so devs can debug what's going on.
	 */
	struct drm_output *output = data;
	struct weston_compositor *compositor = output->base.compositor;

	weston_log("Pageflip timeout reached on output %s, your "
	           "driver is probably buggy!  Exiting.\n",
		   output->base.name);
	weston_compositor_exit_with_code(compositor, EXIT_FAILURE);

	return 0;
}

/* Creates the pageflip timer. Note that it isn't armed by default */
static int
drm_output_pageflip_timer_create(struct drm_output *output)
{
	struct wl_event_loop *loop = NULL;
	struct weston_compositor *ec = output->base.compositor;

	loop = wl_display_get_event_loop(ec->wl_display);
	assert(loop);
	output->pageflip_timer = wl_event_loop_add_timer(loop,
	                                                 pageflip_timeout,
	                                                 output);

	if (output->pageflip_timer == NULL) {
		weston_log("creating drm pageflip timer failed: %m\n");
		return -1;
	}

	return 0;
}

static inline struct drm_mode *
to_drm_mode(struct weston_mode *base)
{
	return container_of(base, struct drm_mode, base);
}

/**
 * Get the current value of a KMS property
 *
 * Given a drmModeObjectGetProperties return, as well as the drm_property_info
 * for the target property, return the current value of that property,
 * with an optional default. If the property is a KMS enum type, the return
 * value will be translated into the appropriate internal enum.
 *
 * If the property is not present, the default value will be returned.
 *
 * @param info Internal structure for property to look up
 * @param props Raw KMS properties for the target object
 * @param def Value to return if property is not found
 */
static uint64_t
drm_property_get_value(struct drm_property_info *info,
		       const drmModeObjectProperties *props,
		       uint64_t def)
{
	unsigned int i;

	if (info->prop_id == 0)
		return def;

	for (i = 0; i < props->count_props; i++) {
		unsigned int j;

		if (props->props[i] != info->prop_id)
			continue;

		/* Simple (non-enum) types can return the value directly */
		if (info->num_enum_values == 0)
			return props->prop_values[i];

		/* Map from raw value to enum value */
		for (j = 0; j < info->num_enum_values; j++) {
			if (!info->enum_values[j].valid)
				continue;
			if (info->enum_values[j].value != props->prop_values[i])
				continue;

			return j;
		}

		/* We don't have a mapping for this enum; return default. */
		break;
	}

	return def;
}

/**
 * Cache DRM property values
 *
 * Update a per-object array of drm_property_info structures, given the
 * DRM properties of the object.
 *
 * Call this every time an object newly appears (note that only connectors
 * can be hotplugged), the first time it is seen, or when its status changes
 * in a way which invalidates the potential property values (currently, the
 * only case for this is connector hotplug).
 *
 * This updates the property IDs and enum values within the drm_property_info
 * array.
 *
 * DRM property enum values are dynamic at runtime; the user must query the
 * property to find out the desired runtime value for a requested string
 * name. Using the 'type' field on planes as an example, there is no single
 * hardcoded constant for primary plane types; instead, the property must be
 * queried at runtime to find the value associated with the string "Primary".
 *
 * This helper queries and caches the enum values, to allow us to use a set
 * of compile-time-constant enums portably across various implementations.
 * The values given in enum_names are searched for, and stored in the
 * same-indexed field of the map array.
 *
 * @param b DRM backend object
 * @param src DRM property info array to source from
 * @param info DRM property info array to copy into
 * @param num_infos Number of entries in the source array
 * @param props DRM object properties for the object
 */
static void
drm_property_info_populate(struct drm_backend *b,
		           const struct drm_property_info *src,
			   struct drm_property_info *info,
			   unsigned int num_infos,
			   drmModeObjectProperties *props)
{
	drmModePropertyRes *prop;
	unsigned i, j;

	for (i = 0; i < num_infos; i++) {
		unsigned int j;

		info[i].name = src[i].name;
		info[i].prop_id = 0;
		info[i].num_enum_values = src[i].num_enum_values;

		if (src[i].num_enum_values == 0)
			continue;

		info[i].enum_values =
			malloc(src[i].num_enum_values *
			       sizeof(*info[i].enum_values));
		assert(info[i].enum_values);
		for (j = 0; j < info[i].num_enum_values; j++) {
			info[i].enum_values[j].name = src[i].enum_values[j].name;
			info[i].enum_values[j].valid = false;
		}
	}

	for (i = 0; i < props->count_props; i++) {
		unsigned int k;

		prop = drmModeGetProperty(b->drm.fd, props->props[i]);
		if (!prop)
			continue;

		for (j = 0; j < num_infos; j++) {
			if (!strcmp(prop->name, info[j].name))
				break;
		}

		/* We don't know/care about this property. */
		if (j == num_infos) {
#ifdef DEBUG
			weston_log("DRM debug: unrecognized property %u '%s'\n",
				   prop->prop_id, prop->name);
#endif
			drmModeFreeProperty(prop);
			continue;
		}

		if (info[j].num_enum_values == 0 &&
		    (prop->flags & DRM_MODE_PROP_ENUM)) {
			weston_log("DRM: expected property %s to not be an"
			           " enum, but it is; ignoring\n", prop->name);
			drmModeFreeProperty(prop);
			continue;
		}

		info[j].prop_id = props->props[i];

		if (info[j].num_enum_values == 0) {
			drmModeFreeProperty(prop);
			continue;
		}

		if (!(prop->flags & DRM_MODE_PROP_ENUM)) {
			weston_log("DRM: expected property %s to be an enum,"
				   " but it is not; ignoring\n", prop->name);
			drmModeFreeProperty(prop);
			info[j].prop_id = 0;
			continue;
		}

		for (k = 0; k < info[j].num_enum_values; k++) {
			int l;

			for (l = 0; l < prop->count_enums; l++) {
				if (!strcmp(prop->enums[l].name,
					    info[j].enum_values[k].name))
					break;
			}

			if (l == prop->count_enums)
				continue;

			info[j].enum_values[k].valid = true;
			info[j].enum_values[k].value = prop->enums[l].value;
		}

		drmModeFreeProperty(prop);
	}

#ifdef DEBUG
	for (i = 0; i < num_infos; i++) {
		if (info[i].prop_id == 0)
			weston_log("DRM warning: property '%s' missing\n",
				   info[i].name);
	}
#endif
}

/**
 * Free DRM property information
 *
 * Frees all memory associated with a DRM property info array and zeroes
 * it out, leaving it usable for a further drm_property_info_update() or
 * drm_property_info_free().
 *
 * @param info DRM property info array
 * @param num_props Number of entries in array to free
 */
static void
drm_property_info_free(struct drm_property_info *info, int num_props)
{
	int i;

	for (i = 0; i < num_props; i++)
		free(info[i].enum_values);

	memset(info, 0, sizeof(*info) * num_props);
}

static void
drm_output_set_cursor(struct drm_output_state *output_state);

static void
drm_output_update_msc(struct drm_output *output, unsigned int seq);

static void
drm_output_destroy(struct weston_output *output_base);

/**
 * Returns true if the plane can be used on the given output for its current
 * repaint cycle.
 */
static bool
drm_plane_is_available(struct drm_plane *plane, struct drm_output *output)
{
	assert(plane->state_cur);

	/* The plane still has a request not yet completed by the kernel. */
	if (!plane->state_cur->complete)
		return false;

	/* The plane is still active on another output. */
	if (plane->state_cur->output && plane->state_cur->output != output)
		return false;

	/* Check whether the plane can be used with this CRTC; possible_crtcs
	 * is a bitmask of CRTC indices (pipe), rather than CRTC object ID. */
	return !!(plane->possible_crtcs & (1 << output->pipe));
}

static struct drm_output *
drm_output_find_by_crtc(struct drm_backend *b, uint32_t crtc_id)
{
	struct drm_output *output;

	wl_list_for_each(output, &b->compositor->output_list, base.link) {
		if (output->crtc_id == crtc_id)
			return output;
	}

	return NULL;
}

static struct drm_head *
drm_head_find_by_connector(struct drm_backend *backend, uint32_t connector_id)
{
	struct weston_head *base;
	struct drm_head *head;

	wl_list_for_each(base,
			 &backend->compositor->head_list, compositor_link) {
		head = to_drm_head(base);
		if (head->connector_id == connector_id)
			return head;
	}

	return NULL;
}

static void
drm_fb_destroy(struct drm_fb *fb)
{
	if (fb->fb_id != 0)
		drmModeRmFB(fb->fd, fb->fb_id);
	weston_buffer_reference(&fb->buffer_ref, NULL);
	free(fb);
}

static void
drm_fb_destroy_dumb(struct drm_fb *fb)
{
	struct drm_mode_destroy_dumb destroy_arg;

	assert(fb->type == BUFFER_PIXMAN_DUMB);

	if (fb->map && fb->size > 0)
		munmap(fb->map, fb->size);

	memset(&destroy_arg, 0, sizeof(destroy_arg));
	destroy_arg.handle = fb->handles[0];
	drmIoctl(fb->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);

	drm_fb_destroy(fb);
}

static void
drm_fb_destroy_gbm(struct gbm_bo *bo, void *data)
{
	struct drm_fb *fb = data;

	assert(fb->type == BUFFER_GBM_SURFACE || fb->type == BUFFER_CLIENT ||
	       fb->type == BUFFER_CURSOR);
	drm_fb_destroy(fb);
}

static int
drm_fb_addfb(struct drm_fb *fb)
{
	int ret = -EINVAL;
#ifdef HAVE_DRM_ADDFB2_MODIFIERS
	uint64_t mods[4] = { };
	size_t i;
#endif

	/* If we have a modifier set, we must only use the WithModifiers
	 * entrypoint; we cannot import it through legacy ioctls. */
	if (fb->modifier != DRM_FORMAT_MOD_INVALID) {
		/* KMS demands that if a modifier is set, it must be the same
		 * for all planes. */
#ifdef HAVE_DRM_ADDFB2_MODIFIERS
		for (i = 0; i < ARRAY_LENGTH(mods) && fb->handles[i]; i++)
			mods[i] = fb->modifier;
		ret = drmModeAddFB2WithModifiers(fb->fd, fb->width, fb->height,
						 fb->format->format,
						 fb->handles, fb->strides,
						 fb->offsets, mods, &fb->fb_id,
						 DRM_MODE_FB_MODIFIERS);
#endif
		return ret;
	}

	ret = drmModeAddFB2(fb->fd, fb->width, fb->height, fb->format->format,
			    fb->handles, fb->strides, fb->offsets, &fb->fb_id,
			    0);
	if (ret == 0)
		return 0;

	/* Legacy AddFB can't always infer the format from depth/bpp alone, so
	 * check if our format is one of the lucky ones. */
	if (!fb->format->depth || !fb->format->bpp)
		return ret;

	/* Cannot fall back to AddFB for multi-planar formats either. */
	if (fb->handles[1] || fb->handles[2] || fb->handles[3])
		return ret;

	ret = drmModeAddFB(fb->fd, fb->width, fb->height,
			   fb->format->depth, fb->format->bpp,
			   fb->strides[0], fb->handles[0], &fb->fb_id);
	return ret;
}

static struct drm_fb *
drm_fb_create_dumb(struct drm_backend *b, int width, int height,
		   uint32_t format)
{
	struct drm_fb *fb;
	int ret;

	struct drm_mode_create_dumb create_arg;
	struct drm_mode_destroy_dumb destroy_arg;
	struct drm_mode_map_dumb map_arg;

	fb = zalloc(sizeof *fb);
	if (!fb)
		return NULL;
	fb->refcnt = 1;

	fb->format = pixel_format_get_info(format);
	if (!fb->format) {
		weston_log("failed to look up format 0x%lx\n",
			   (unsigned long) format);
		goto err_fb;
	}

	if (!fb->format->depth || !fb->format->bpp) {
		weston_log("format 0x%lx is not compatible with dumb buffers\n",
			   (unsigned long) format);
		goto err_fb;
	}

	memset(&create_arg, 0, sizeof create_arg);
	create_arg.bpp = fb->format->bpp;
	create_arg.width = width;
	create_arg.height = height;

	ret = drmIoctl(b->drm.fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_arg);
	if (ret)
		goto err_fb;

	fb->type = BUFFER_PIXMAN_DUMB;
	fb->modifier = DRM_FORMAT_MOD_INVALID;
	fb->handles[0] = create_arg.handle;
	fb->strides[0] = create_arg.pitch;
	fb->size = create_arg.size;
	fb->width = width;
	fb->height = height;
	fb->fd = b->drm.fd;

	if (drm_fb_addfb(fb) != 0) {
		weston_log("failed to create kms fb: %m\n");
		goto err_bo;
	}

	memset(&map_arg, 0, sizeof map_arg);
	map_arg.handle = fb->handles[0];
	ret = drmIoctl(fb->fd, DRM_IOCTL_MODE_MAP_DUMB, &map_arg);
	if (ret)
		goto err_add_fb;

	fb->map = mmap(NULL, fb->size, PROT_WRITE,
		       MAP_SHARED, b->drm.fd, map_arg.offset);
	if (fb->map == MAP_FAILED)
		goto err_add_fb;

	return fb;

err_add_fb:
	drmModeRmFB(b->drm.fd, fb->fb_id);
err_bo:
	memset(&destroy_arg, 0, sizeof(destroy_arg));
	destroy_arg.handle = create_arg.handle;
	drmIoctl(b->drm.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_arg);
err_fb:
	free(fb);
	return NULL;
}

static struct drm_fb *
drm_fb_ref(struct drm_fb *fb)
{
	fb->refcnt++;
	return fb;
}

static void
drm_fb_destroy_dmabuf(struct drm_fb *fb)
{
	/* We deliberately do not close the GEM handles here; GBM manages
	 * their lifetime through the BO. */
	if (fb->bo)
		gbm_bo_destroy(fb->bo);
	drm_fb_destroy(fb);
}

static struct drm_fb *
drm_fb_get_from_dmabuf(struct linux_dmabuf_buffer *dmabuf,
		       struct drm_backend *backend, bool is_opaque)
{
#ifdef HAVE_GBM_FD_IMPORT
	struct drm_fb *fb;
	struct gbm_import_fd_data import_legacy = {
		.width = dmabuf->attributes.width,
		.height = dmabuf->attributes.height,
		.format = dmabuf->attributes.format,
		.stride = dmabuf->attributes.stride[0],
		.fd = dmabuf->attributes.fd[0],
	};
	struct gbm_import_fd_modifier_data import_mod = {
		.width = dmabuf->attributes.width,
		.height = dmabuf->attributes.height,
		.format = dmabuf->attributes.format,
		.num_fds = dmabuf->attributes.n_planes,
		.modifier = dmabuf->attributes.modifier[0],
	};
	int i;

	/* XXX: TODO:
	 *
	 * Currently the buffer is rejected if any dmabuf attribute
	 * flag is set.  This keeps us from passing an inverted /
	 * interlaced / bottom-first buffer (or any other type that may
	 * be added in the future) through to an overlay.  Ultimately,
	 * these types of buffers should be handled through buffer
	 * transforms and not as spot-checks requiring specific
	 * knowledge. */
	if (dmabuf->attributes.flags)
		return NULL;

	fb = zalloc(sizeof *fb);
	if (fb == NULL)
		return NULL;

	fb->refcnt = 1;
	fb->type = BUFFER_DMABUF;

	static_assert(ARRAY_LENGTH(import_mod.fds) ==
		      ARRAY_LENGTH(dmabuf->attributes.fd),
		      "GBM and linux_dmabuf FD size must match");
	static_assert(sizeof(import_mod.fds) == sizeof(dmabuf->attributes.fd),
		      "GBM and linux_dmabuf FD size must match");
	memcpy(import_mod.fds, dmabuf->attributes.fd, sizeof(import_mod.fds));

	static_assert(ARRAY_LENGTH(import_mod.strides) ==
		      ARRAY_LENGTH(dmabuf->attributes.stride),
		      "GBM and linux_dmabuf stride size must match");
	static_assert(sizeof(import_mod.strides) ==
		      sizeof(dmabuf->attributes.stride),
		      "GBM and linux_dmabuf stride size must match");
	memcpy(import_mod.strides, dmabuf->attributes.stride,
	       sizeof(import_mod.strides));

	static_assert(ARRAY_LENGTH(import_mod.offsets) ==
		      ARRAY_LENGTH(dmabuf->attributes.offset),
		      "GBM and linux_dmabuf offset size must match");
	static_assert(sizeof(import_mod.offsets) ==
		      sizeof(dmabuf->attributes.offset),
		      "GBM and linux_dmabuf offset size must match");
	memcpy(import_mod.offsets, dmabuf->attributes.offset,
	       sizeof(import_mod.offsets));

	/* The legacy FD-import path does not allow us to supply modifiers,
	 * multiple planes, or buffer offsets. */
	if (dmabuf->attributes.modifier[0] != DRM_FORMAT_MOD_INVALID ||
	    import_mod.num_fds > 1 ||
	    import_mod.offsets[0] > 0) {
		fb->bo = gbm_bo_import(backend->gbm, GBM_BO_IMPORT_FD_MODIFIER,
				       &import_mod,
				       GBM_BO_USE_SCANOUT);
	} else {
		fb->bo = gbm_bo_import(backend->gbm, GBM_BO_IMPORT_FD,
				       &import_legacy,
				       GBM_BO_USE_SCANOUT);
	}

	if (!fb->bo)
		goto err_free;

	fb->width = dmabuf->attributes.width;
	fb->height = dmabuf->attributes.height;
	fb->modifier = dmabuf->attributes.modifier[0];
	fb->size = 0;
	fb->fd = backend->drm.fd;

	static_assert(ARRAY_LENGTH(fb->strides) ==
		      ARRAY_LENGTH(dmabuf->attributes.stride),
		      "drm_fb and dmabuf stride size must match");
	static_assert(sizeof(fb->strides) == sizeof(dmabuf->attributes.stride),
		      "drm_fb and dmabuf stride size must match");
	memcpy(fb->strides, dmabuf->attributes.stride, sizeof(fb->strides));
	static_assert(ARRAY_LENGTH(fb->offsets) ==
		      ARRAY_LENGTH(dmabuf->attributes.offset),
		      "drm_fb and dmabuf offset size must match");
	static_assert(sizeof(fb->offsets) == sizeof(dmabuf->attributes.offset),
		      "drm_fb and dmabuf offset size must match");
	memcpy(fb->offsets, dmabuf->attributes.offset, sizeof(fb->offsets));

	fb->format = pixel_format_get_info(dmabuf->attributes.format);
	if (!fb->format) {
		weston_log("couldn't look up format info for 0x%lx\n",
			   (unsigned long) dmabuf->attributes.format);
		goto err_free;
	}

	if (is_opaque)
		fb->format = pixel_format_get_opaque_substitute(fb->format);

	if (backend->min_width > fb->width ||
	    fb->width > backend->max_width ||
	    backend->min_height > fb->height ||
	    fb->height > backend->max_height) {
		weston_log("bo geometry out of bounds\n");
		goto err_free;
	}

	for (i = 0; i < dmabuf->attributes.n_planes; i++) {
		fb->handles[i] = gbm_bo_get_handle_for_plane(fb->bo, i).u32;
		if (!fb->handles[i])
			goto err_free;
	}

	if (drm_fb_addfb(fb) != 0)
		goto err_free;

	return fb;

err_free:
	drm_fb_destroy_dmabuf(fb);
#endif
	return NULL;
}

static struct drm_fb *
drm_fb_get_from_bo(struct gbm_bo *bo, struct drm_backend *backend,
		   bool is_opaque, enum drm_fb_type type)
{
	struct drm_fb *fb = gbm_bo_get_user_data(bo);
#ifdef HAVE_GBM_MODIFIERS
	int i;
#endif

	if (fb) {
		assert(fb->type == type);
		return drm_fb_ref(fb);
	}

	fb = zalloc(sizeof *fb);
	if (fb == NULL)
		return NULL;

	fb->type = type;
	fb->refcnt = 1;
	fb->bo = bo;
	fb->fd = backend->drm.fd;

	fb->width = gbm_bo_get_width(bo);
	fb->height = gbm_bo_get_height(bo);
	fb->format = pixel_format_get_info(gbm_bo_get_format(bo));
	fb->size = 0;

#ifdef HAVE_GBM_MODIFIERS
	fb->modifier = gbm_bo_get_modifier(bo);
	for (i = 0; i < gbm_bo_get_plane_count(bo); i++) {
		fb->strides[i] = gbm_bo_get_stride_for_plane(bo, i);
		fb->handles[i] = gbm_bo_get_handle_for_plane(bo, i).u32;
		fb->offsets[i] = gbm_bo_get_offset(bo, i);
	}
#else
	fb->strides[0] = gbm_bo_get_stride(bo);
	fb->handles[0] = gbm_bo_get_handle(bo).u32;
	fb->modifier = DRM_FORMAT_MOD_INVALID;
#endif

	if (!fb->format) {
		weston_log("couldn't look up format 0x%lx\n",
			   (unsigned long) gbm_bo_get_format(bo));
		goto err_free;
	}

	/* We can scanout an ARGB buffer if the surface's opaque region covers
	 * the whole output, but we have to use XRGB as the KMS format code. */
	if (is_opaque)
		fb->format = pixel_format_get_opaque_substitute(fb->format);

	if (backend->min_width > fb->width ||
	    fb->width > backend->max_width ||
	    backend->min_height > fb->height ||
	    fb->height > backend->max_height) {
		weston_log("bo geometry out of bounds\n");
		goto err_free;
	}

	if (drm_fb_addfb(fb) != 0) {
		if (type == BUFFER_GBM_SURFACE)
			weston_log("failed to create kms fb: %m\n");
		goto err_free;
	}

	gbm_bo_set_user_data(bo, fb, drm_fb_destroy_gbm);

	return fb;

err_free:
	free(fb);
	return NULL;
}

static void
drm_fb_set_buffer(struct drm_fb *fb, struct weston_buffer *buffer)
{
	assert(fb->buffer_ref.buffer == NULL);
	assert(fb->type == BUFFER_CLIENT || fb->type == BUFFER_DMABUF);
	weston_buffer_reference(&fb->buffer_ref, buffer);
}

static void
drm_fb_unref(struct drm_fb *fb)
{
	if (!fb)
		return;

	assert(fb->refcnt > 0);
	if (--fb->refcnt > 0)
		return;

	switch (fb->type) {
	case BUFFER_PIXMAN_DUMB:
		drm_fb_destroy_dumb(fb);
		break;
	case BUFFER_CURSOR:
	case BUFFER_CLIENT:
		gbm_bo_destroy(fb->bo);
		break;
	case BUFFER_GBM_SURFACE:
		gbm_surface_release_buffer(fb->gbm_surface, fb->bo);
		break;
	case BUFFER_DMABUF:
		drm_fb_destroy_dmabuf(fb);
		break;
	default:
		assert(NULL);
		break;
	}
}

/**
 * Allocate a new, empty, plane state.
 */
static struct drm_plane_state *
drm_plane_state_alloc(struct drm_output_state *state_output,
		      struct drm_plane *plane)
{
	struct drm_plane_state *state = zalloc(sizeof(*state));

	assert(state);
	state->output_state = state_output;
	state->plane = plane;

	/* Here we only add the plane state to the desired link, and not
	 * set the member. Having an output pointer set means that the
	 * plane will be displayed on the output; this won't be the case
	 * when we go to disable a plane. In this case, it must be part of
	 * the commit (and thus the output state), but the member must be
	 * NULL, as it will not be on any output when the state takes
	 * effect.
	 */
	if (state_output)
		wl_list_insert(&state_output->plane_list, &state->link);
	else
		wl_list_init(&state->link);

	return state;
}

/**
 * Free an existing plane state. As a special case, the state will not
 * normally be freed if it is the current state; see drm_plane_set_state.
 */
static void
drm_plane_state_free(struct drm_plane_state *state, bool force)
{
	if (!state)
		return;

	wl_list_remove(&state->link);
	wl_list_init(&state->link);
	state->output_state = NULL;

	if (force || state != state->plane->state_cur) {
		drm_fb_unref(state->fb);
		free(state);
	}
}

/**
 * Duplicate an existing plane state into a new plane state, storing it within
 * the given output state. If the output state already contains a plane state
 * for the drm_plane referenced by 'src', that plane state is freed first.
 */
static struct drm_plane_state *
drm_plane_state_duplicate(struct drm_output_state *state_output,
			  struct drm_plane_state *src)
{
	struct drm_plane_state *dst = malloc(sizeof(*dst));
	struct drm_plane_state *old, *tmp;

	assert(src);
	assert(dst);
	*dst = *src;
	wl_list_init(&dst->link);

	wl_list_for_each_safe(old, tmp, &state_output->plane_list, link) {
		/* Duplicating a plane state into the same output state, so
		 * it can replace itself with an identical copy of itself,
		 * makes no sense. */
		assert(old != src);
		if (old->plane == dst->plane)
			drm_plane_state_free(old, false);
	}

	wl_list_insert(&state_output->plane_list, &dst->link);
	if (src->fb)
		dst->fb = drm_fb_ref(src->fb);
	dst->output_state = state_output;
	dst->complete = false;

	return dst;
}

/**
 * Remove a plane state from an output state; if the plane was previously
 * enabled, then replace it with a disabling state. This ensures that the
 * output state was untouched from it was before the plane state was
 * modified by the caller of this function.
 *
 * This is required as drm_output_state_get_plane may either allocate a
 * new plane state, in which case this function will just perform a matching
 * drm_plane_state_free, or it may instead repurpose an existing disabling
 * state (if the plane was previously active), in which case this function
 * will reset it.
 */
static void
drm_plane_state_put_back(struct drm_plane_state *state)
{
	struct drm_output_state *state_output;
	struct drm_plane *plane;

	if (!state)
		return;

	state_output = state->output_state;
	plane = state->plane;
	drm_plane_state_free(state, false);

	/* Plane was previously disabled; no need to keep this temporary
	 * state around. */
	if (!plane->state_cur->fb)
		return;

	(void) drm_plane_state_alloc(state_output, plane);
}

static bool
drm_view_transform_supported(struct weston_view *ev, struct weston_output *output)
{
	struct weston_buffer_viewport *viewport = &ev->surface->buffer_viewport;

	/* This will incorrectly disallow cases where the combination of
	 * buffer and view transformations match the output transform.
	 * Fixing this requires a full analysis of the transformation
	 * chain. */
	if (ev->transform.enabled &&
	    ev->transform.matrix.type >= WESTON_MATRIX_TRANSFORM_ROTATE)
		return false;

	if (viewport->buffer.transform != output->transform)
		return false;

	return true;
}

/**
 * Given a weston_view, fill the drm_plane_state's co-ordinates to display on
 * a given plane.
 */
static bool
drm_plane_state_coords_for_view(struct drm_plane_state *state,
				struct weston_view *ev)
{
	struct drm_output *output = state->output;
	struct weston_buffer *buffer = ev->surface->buffer_ref.buffer;
	pixman_region32_t dest_rect, src_rect;
	pixman_box32_t *box, tbox;
	float sxf1, syf1, sxf2, syf2;

	if (!drm_view_transform_supported(ev, &output->base))
		return false;

	/* Update the base weston_plane co-ordinates. */
	box = pixman_region32_extents(&ev->transform.boundingbox);
	state->plane->base.x = box->x1;
	state->plane->base.y = box->y1;

	/* First calculate the destination co-ordinates by taking the
	 * area of the view which is visible on this output, performing any
	 * transforms to account for output rotation and scale as necessary. */
	pixman_region32_init(&dest_rect);
	pixman_region32_intersect(&dest_rect, &ev->transform.boundingbox,
				  &output->base.region);
	pixman_region32_translate(&dest_rect, -output->base.x, -output->base.y);
	box = pixman_region32_extents(&dest_rect);
	tbox = weston_transformed_rect(output->base.width,
				       output->base.height,
				       output->base.transform,
				       output->base.current_scale,
				       *box);
	state->dest_x = tbox.x1;
	state->dest_y = tbox.y1;
	state->dest_w = tbox.x2 - tbox.x1;
	state->dest_h = tbox.y2 - tbox.y1;
	pixman_region32_fini(&dest_rect);

	/* Now calculate the source rectangle, by finding the extents of the
	 * view, and working backwards to source co-ordinates. */
	pixman_region32_init(&src_rect);
	pixman_region32_intersect(&src_rect, &ev->transform.boundingbox,
				  &output->base.region);
	box = pixman_region32_extents(&src_rect);
	weston_view_from_global_float(ev, box->x1, box->y1, &sxf1, &syf1);
	weston_surface_to_buffer_float(ev->surface, sxf1, syf1, &sxf1, &syf1);
	weston_view_from_global_float(ev, box->x2, box->y2, &sxf2, &syf2);
	weston_surface_to_buffer_float(ev->surface, sxf2, syf2, &sxf2, &syf2);
	pixman_region32_fini(&src_rect);

	/* Buffer transforms may mean that x2 is to the left of x1, and/or that
	 * y2 is above y1. */
	if (sxf2 < sxf1) {
		double tmp = sxf1;
		sxf1 = sxf2;
		sxf2 = tmp;
	}
	if (syf2 < syf1) {
		double tmp = syf1;
		syf1 = syf2;
		syf2 = tmp;
	}

	/* Shift from S23.8 wl_fixed to U16.16 KMS fixed-point encoding. */
	state->src_x = wl_fixed_from_double(sxf1) << 8;
	state->src_y = wl_fixed_from_double(syf1) << 8;
	state->src_w = wl_fixed_from_double(sxf2 - sxf1) << 8;
	state->src_h = wl_fixed_from_double(syf2 - syf1) << 8;

	/* Clamp our source co-ordinates to surface bounds; it's possible
	 * for intermediate translations to give us slightly incorrect
	 * co-ordinates if we have, for example, multiple zooming
	 * transformations. View bounding boxes are also explicitly rounded
	 * greedily. */
	if (state->src_x < 0)
		state->src_x = 0;
	if (state->src_y < 0)
		state->src_y = 0;
	if (state->src_w > (uint32_t) ((buffer->width << 16) - state->src_x))
		state->src_w = (buffer->width << 16) - state->src_x;
	if (state->src_h > (uint32_t) ((buffer->height << 16) - state->src_y))
		state->src_h = (buffer->height << 16) - state->src_y;

	return true;
}

static bool
drm_view_is_opaque(struct weston_view *ev)
{
	pixman_region32_t r;
	bool ret = false;

	pixman_region32_init_rect(&r, 0, 0,
				  ev->surface->width,
				  ev->surface->height);
	pixman_region32_subtract(&r, &r, &ev->surface->opaque);

	if (!pixman_region32_not_empty(&r))
		ret = true;

	pixman_region32_fini(&r);

	return ret;
}

static struct drm_fb *
drm_fb_get_from_view(struct drm_output_state *state, struct weston_view *ev)
{
	struct drm_output *output = state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct weston_buffer *buffer = ev->surface->buffer_ref.buffer;
	bool is_opaque = drm_view_is_opaque(ev);
	struct linux_dmabuf_buffer *dmabuf;
	struct drm_fb *fb;

	if (ev->alpha != 1.0f)
		return NULL;

	if (!drm_view_transform_supported(ev, &output->base))
		return NULL;

	if (!buffer)
		return NULL;

	if (wl_shm_buffer_get(buffer->resource))
		return NULL;

	/* GBM is used for dmabuf import as well as from client wl_buffer. */
	if (!b->gbm)
		return NULL;

	dmabuf = linux_dmabuf_buffer_get(buffer->resource);
	if (dmabuf) {
		fb = drm_fb_get_from_dmabuf(dmabuf, b, is_opaque);
		if (!fb)
			return NULL;
	} else {
		struct gbm_bo *bo;

		bo = gbm_bo_import(b->gbm, GBM_BO_IMPORT_WL_BUFFER,
				   buffer->resource, GBM_BO_USE_SCANOUT);
		if (!bo)
			return NULL;

		fb = drm_fb_get_from_bo(bo, b, is_opaque, BUFFER_CLIENT);
		if (!fb) {
			gbm_bo_destroy(bo);
			return NULL;
		}
	}

	drm_fb_set_buffer(fb, buffer);
	return fb;
}

/**
 * Return a plane state from a drm_output_state.
 */
static struct drm_plane_state *
drm_output_state_get_existing_plane(struct drm_output_state *state_output,
				    struct drm_plane *plane)
{
	struct drm_plane_state *ps;

	wl_list_for_each(ps, &state_output->plane_list, link) {
		if (ps->plane == plane)
			return ps;
	}

	return NULL;
}

/**
 * Return a plane state from a drm_output_state, either existing or
 * freshly allocated.
 */
static struct drm_plane_state *
drm_output_state_get_plane(struct drm_output_state *state_output,
			   struct drm_plane *plane)
{
	struct drm_plane_state *ps;

	ps = drm_output_state_get_existing_plane(state_output, plane);
	if (ps)
		return ps;

	return drm_plane_state_alloc(state_output, plane);
}

/**
 * Allocate a new, empty drm_output_state. This should not generally be used
 * in the repaint cycle; see drm_output_state_duplicate.
 */
static struct drm_output_state *
drm_output_state_alloc(struct drm_output *output,
		       struct drm_pending_state *pending_state)
{
	struct drm_output_state *state = zalloc(sizeof(*state));

	assert(state);
	state->output = output;
	state->dpms = WESTON_DPMS_OFF;
	state->pending_state = pending_state;
	if (pending_state)
		wl_list_insert(&pending_state->output_list, &state->link);
	else
		wl_list_init(&state->link);

	wl_list_init(&state->plane_list);

	return state;
}

/**
 * Duplicate an existing drm_output_state into a new one. This is generally
 * used during the repaint cycle, to capture the existing state of an output
 * and modify it to create a new state to be used.
 *
 * The mode determines whether the output will be reset to an a blank state,
 * or an exact mirror of the current state.
 */
static struct drm_output_state *
drm_output_state_duplicate(struct drm_output_state *src,
			   struct drm_pending_state *pending_state,
			   enum drm_output_state_duplicate_mode plane_mode)
{
	struct drm_output_state *dst = malloc(sizeof(*dst));
	struct drm_plane_state *ps;

	assert(dst);

	/* Copy the whole structure, then individually modify the
	 * pending_state, as well as the list link into our pending
	 * state. */
	*dst = *src;

	dst->pending_state = pending_state;
	if (pending_state)
		wl_list_insert(&pending_state->output_list, &dst->link);
	else
		wl_list_init(&dst->link);

	wl_list_init(&dst->plane_list);

	wl_list_for_each(ps, &src->plane_list, link) {
		/* Don't carry planes which are now disabled; these should be
		 * free for other outputs to reuse. */
		if (!ps->output)
			continue;

		if (plane_mode == DRM_OUTPUT_STATE_CLEAR_PLANES)
			(void) drm_plane_state_alloc(dst, ps->plane);
		else
			(void) drm_plane_state_duplicate(dst, ps);
	}

	return dst;
}

/**
 * Free an unused drm_output_state.
 */
static void
drm_output_state_free(struct drm_output_state *state)
{
	struct drm_plane_state *ps, *next;

	if (!state)
		return;

	wl_list_for_each_safe(ps, next, &state->plane_list, link)
		drm_plane_state_free(ps, false);

	wl_list_remove(&state->link);

	free(state);
}

/**
 * Get output state to disable output
 *
 * Returns a pointer to an output_state object which can be used to disable
 * an output (e.g. DPMS off).
 *
 * @param pending_state The pending state object owning this update
 * @param output The output to disable
 * @returns A drm_output_state to disable the output
 */
static struct drm_output_state *
drm_output_get_disable_state(struct drm_pending_state *pending_state,
			     struct drm_output *output)
{
	struct drm_output_state *output_state;

	output_state = drm_output_state_duplicate(output->state_cur,
						  pending_state,
						  DRM_OUTPUT_STATE_CLEAR_PLANES);
	output_state->dpms = WESTON_DPMS_OFF;

	return output_state;
}

/**
 * Allocate a new drm_pending_state
 *
 * Allocate a new, empty, 'pending state' structure to be used across a
 * repaint cycle or similar.
 *
 * @param backend DRM backend
 * @returns Newly-allocated pending state structure
 */
static struct drm_pending_state *
drm_pending_state_alloc(struct drm_backend *backend)
{
	struct drm_pending_state *ret;

	ret = calloc(1, sizeof(*ret));
	if (!ret)
		return NULL;

	ret->backend = backend;
	wl_list_init(&ret->output_list);

	return ret;
}

/**
 * Free a drm_pending_state structure
 *
 * Frees a pending_state structure, as well as any output_states connected
 * to this pending state.
 *
 * @param pending_state Pending state structure to free
 */
static void
drm_pending_state_free(struct drm_pending_state *pending_state)
{
	struct drm_output_state *output_state, *tmp;

	if (!pending_state)
		return;

	wl_list_for_each_safe(output_state, tmp, &pending_state->output_list,
			      link) {
		drm_output_state_free(output_state);
	}

	free(pending_state);
}

/**
 * Find an output state in a pending state
 *
 * Given a pending_state structure, find the output_state for a particular
 * output.
 *
 * @param pending_state Pending state structure to search
 * @param output Output to find state for
 * @returns Output state if present, or NULL if not
 */
static struct drm_output_state *
drm_pending_state_get_output(struct drm_pending_state *pending_state,
			     struct drm_output *output)
{
	struct drm_output_state *output_state;

	wl_list_for_each(output_state, &pending_state->output_list, link) {
		if (output_state->output == output)
			return output_state;
	}

	return NULL;
}

static int drm_pending_state_apply_sync(struct drm_pending_state *state);
static int drm_pending_state_test(struct drm_pending_state *state);

/**
 * Mark a drm_output_state (the output's last state) as complete. This handles
 * any post-completion actions such as updating the repaint timer, disabling the
 * output, and finally freeing the state.
 */
static void
drm_output_update_complete(struct drm_output *output, uint32_t flags,
			   unsigned int sec, unsigned int usec)
{
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane_state *ps;
	struct timespec ts;

	/* Stop the pageflip timer instead of rearming it here */
	if (output->pageflip_timer)
		wl_event_source_timer_update(output->pageflip_timer, 0);

	wl_list_for_each(ps, &output->state_cur->plane_list, link)
		ps->complete = true;

	drm_output_state_free(output->state_last);
	output->state_last = NULL;

	if (output->destroy_pending) {
		output->destroy_pending = 0;
		output->disable_pending = 0;
		output->dpms_off_pending = 0;
		drm_output_destroy(&output->base);
		return;
	} else if (output->disable_pending) {
		output->disable_pending = 0;
		output->dpms_off_pending = 0;
		weston_output_disable(&output->base);
		return;
	} else if (output->dpms_off_pending) {
		struct drm_pending_state *pending = drm_pending_state_alloc(b);
		output->dpms_off_pending = 0;
		drm_output_get_disable_state(pending, output);
		drm_pending_state_apply_sync(pending);
		return;
	} else if (output->state_cur->dpms == WESTON_DPMS_OFF &&
	           output->base.repaint_status != REPAINT_AWAITING_COMPLETION) {
		/* DPMS can happen to us either in the middle of a repaint
		 * cycle (when we have painted fresh content, only to throw it
		 * away for DPMS off), or at any other random point. If the
		 * latter is true, then we cannot go through finish_frame,
		 * because the repaint machinery does not expect this. */
		return;
	}

	ts.tv_sec = sec;
	ts.tv_nsec = usec * 1000;
	weston_output_finish_frame(&output->base, &ts, flags);

	/* We can't call this from frame_notify, because the output's
	 * repaint needed flag is cleared just after that */
	if (output->recorder)
		weston_output_schedule_repaint(&output->base);
}

/**
 * Mark an output state as current on the output, i.e. it has been
 * submitted to the kernel. The mode argument determines whether this
 * update will be applied synchronously (e.g. when calling drmModeSetCrtc),
 * or asynchronously (in which case we wait for events to complete).
 */
static void
drm_output_assign_state(struct drm_output_state *state,
			enum drm_state_apply_mode mode)
{
	struct drm_output *output = state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane_state *plane_state;

	assert(!output->state_last);

	if (mode == DRM_STATE_APPLY_ASYNC)
		output->state_last = output->state_cur;
	else
		drm_output_state_free(output->state_cur);

	wl_list_remove(&state->link);
	wl_list_init(&state->link);
	state->pending_state = NULL;

	output->state_cur = state;

	if (b->atomic_modeset && mode == DRM_STATE_APPLY_ASYNC)
		output->atomic_complete_pending = 1;

	/* Replace state_cur on each affected plane with the new state, being
	 * careful to dispose of orphaned (but only orphaned) previous state.
	 * If the previous state is not orphaned (still has an output_state
	 * attached), it will be disposed of by freeing the output_state. */
	wl_list_for_each(plane_state, &state->plane_list, link) {
		struct drm_plane *plane = plane_state->plane;

		if (plane->state_cur && !plane->state_cur->output_state)
			drm_plane_state_free(plane->state_cur, true);
		plane->state_cur = plane_state;

		if (mode != DRM_STATE_APPLY_ASYNC) {
			plane_state->complete = true;
			continue;
		}

		if (b->atomic_modeset)
			continue;

		if (plane->type == WDRM_PLANE_TYPE_OVERLAY)
			output->vblank_pending++;
		else if (plane->type == WDRM_PLANE_TYPE_PRIMARY)
			output->page_flip_pending = 1;
	}
}

enum drm_output_propose_state_mode {
	DRM_OUTPUT_PROPOSE_STATE_MIXED, /**< mix renderer & planes */
	DRM_OUTPUT_PROPOSE_STATE_RENDERER_ONLY, /**< only assign to renderer & cursor */
	DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY, /**< no renderer use, only planes */
};

static struct drm_plane_state *
drm_output_prepare_scanout_view(struct drm_output_state *output_state,
				struct weston_view *ev,
				enum drm_output_propose_state_mode mode)
{
	struct drm_output *output = output_state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane *scanout_plane = output->scanout_plane;
	struct drm_plane_state *state;
	struct drm_fb *fb;
	pixman_box32_t *extents;

	assert(!b->sprites_are_broken);
	assert(mode == DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY);

	/* Check the view spans exactly the output size, calculated in the
	 * logical co-ordinate space. */
	extents = pixman_region32_extents(&ev->transform.boundingbox);
	if (extents->x1 != output->base.x ||
	    extents->y1 != output->base.y ||
	    extents->x2 != output->base.x + output->base.width ||
	    extents->y2 != output->base.y + output->base.height)
		return NULL;

	if (ev->alpha != 1.0f)
		return NULL;

	fb = drm_fb_get_from_view(output_state, ev);
	if (!fb)
		return NULL;

	/* Can't change formats with just a pageflip */
	if (!b->atomic_modeset && fb->format->format != output->gbm_format) {
		drm_fb_unref(fb);
		return NULL;
	}

	state = drm_output_state_get_plane(output_state, scanout_plane);

	/* The only way we can already have a buffer in the scanout plane is
	 * if we are in mixed mode, or if a client buffer has already been
	 * placed into scanout. The former case will never call into here,
	 * and in the latter case, the view must have been marked as occluded,
	 * meaning we should never have ended up here. */
	assert(!state->fb);
	state->fb = fb;
	state->ev = ev;
	state->output = output;
	if (!drm_plane_state_coords_for_view(state, ev))
		goto err;

	if (state->dest_x != 0 || state->dest_y != 0 ||
	    state->dest_w != (unsigned) output->base.current_mode->width ||
	    state->dest_h != (unsigned) output->base.current_mode->height)
		goto err;

	/* The legacy API does not let us perform cropping or scaling. */
	if (!b->atomic_modeset &&
	    (state->src_x != 0 || state->src_y != 0 ||
	     state->src_w != state->dest_w << 16 ||
	     state->src_h != state->dest_h << 16))
		goto err;

	/* In plane-only mode, we don't need to test the state now, as we
	 * will only test it once at the end. */
	return state;

err:
	drm_plane_state_put_back(state);
	return NULL;
}

static struct drm_fb *
drm_output_render_gl(struct drm_output_state *state, pixman_region32_t *damage)
{
	struct drm_output *output = state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct gbm_bo *bo;
	struct drm_fb *ret;

	output->base.compositor->renderer->repaint_output(&output->base,
							  damage);

	bo = gbm_surface_lock_front_buffer(output->gbm_surface);
	if (!bo) {
		weston_log("failed to lock front buffer: %m\n");
		return NULL;
	}

	/* The renderer always produces an opaque image. */
	ret = drm_fb_get_from_bo(bo, b, true, BUFFER_GBM_SURFACE);
	if (!ret) {
		weston_log("failed to get drm_fb for bo\n");
		gbm_surface_release_buffer(output->gbm_surface, bo);
		return NULL;
	}
	ret->gbm_surface = output->gbm_surface;

	return ret;
}

static struct drm_fb *
drm_output_render_pixman(struct drm_output_state *state,
			 pixman_region32_t *damage)
{
	struct drm_output *output = state->output;
	struct weston_compositor *ec = output->base.compositor;

	output->current_image ^= 1;

	pixman_renderer_output_set_buffer(&output->base,
					  output->image[output->current_image]);
	pixman_renderer_output_set_hw_extra_damage(&output->base,
						   &output->previous_damage);

	ec->renderer->repaint_output(&output->base, damage);

	pixman_region32_copy(&output->previous_damage, damage);

	return drm_fb_ref(output->dumb[output->current_image]);
}

static void
drm_output_render(struct drm_output_state *state, pixman_region32_t *damage)
{
	struct drm_output *output = state->output;
	struct weston_compositor *c = output->base.compositor;
	struct drm_plane_state *scanout_state;
	struct drm_plane *scanout_plane = output->scanout_plane;
	struct drm_backend *b = to_drm_backend(c);
	struct drm_fb *fb;

	/* If we already have a client buffer promoted to scanout, then we don't
	 * want to render. */
	scanout_state = drm_output_state_get_plane(state,
						   output->scanout_plane);
	if (scanout_state->fb)
		return;

	if (!pixman_region32_not_empty(damage) &&
	    scanout_plane->state_cur->fb &&
	    (scanout_plane->state_cur->fb->type == BUFFER_GBM_SURFACE ||
	     scanout_plane->state_cur->fb->type == BUFFER_PIXMAN_DUMB) &&
	    scanout_plane->state_cur->fb->width ==
		output->base.current_mode->width &&
	    scanout_plane->state_cur->fb->height ==
		output->base.current_mode->height) {
		fb = drm_fb_ref(scanout_plane->state_cur->fb);
	} else if (b->use_pixman) {
		fb = drm_output_render_pixman(state, damage);
	} else {
		fb = drm_output_render_gl(state, damage);
	}

	if (!fb) {
		drm_plane_state_put_back(scanout_state);
		return;
	}

	scanout_state->fb = fb;
	scanout_state->output = output;

	scanout_state->src_x = 0;
	scanout_state->src_y = 0;
	scanout_state->src_w = output->base.current_mode->width << 16;
	scanout_state->src_h = output->base.current_mode->height << 16;

	scanout_state->dest_x = 0;
	scanout_state->dest_y = 0;
	scanout_state->dest_w = scanout_state->src_w >> 16;
	scanout_state->dest_h = scanout_state->src_h >> 16;


	pixman_region32_subtract(&c->primary_plane.damage,
				 &c->primary_plane.damage, damage);
}

static void
drm_output_set_gamma(struct weston_output *output_base,
		     uint16_t size, uint16_t *r, uint16_t *g, uint16_t *b)
{
	int rc;
	struct drm_output *output = to_drm_output(output_base);
	struct drm_backend *backend =
		to_drm_backend(output->base.compositor);

	/* check */
	if (output_base->gamma_size != size)
		return;

	rc = drmModeCrtcSetGamma(backend->drm.fd,
				 output->crtc_id,
				 size, r, g, b);
	if (rc)
		weston_log("set gamma failed: %m\n");
}

/* Determine the type of vblank synchronization to use for the output.
 *
 * The pipe parameter indicates which CRTC is in use.  Knowing this, we
 * can determine which vblank sequence type to use for it.  Traditional
 * cards had only two CRTCs, with CRTC 0 using no special flags, and
 * CRTC 1 using DRM_VBLANK_SECONDARY.  The first bit of the pipe
 * parameter indicates this.
 *
 * Bits 1-5 of the pipe parameter are 5 bit wide pipe number between
 * 0-31.  If this is non-zero it indicates we're dealing with a
 * multi-gpu situation and we need to calculate the vblank sync
 * using DRM_BLANK_HIGH_CRTC_MASK.
 */
static unsigned int
drm_waitvblank_pipe(struct drm_output *output)
{
	if (output->pipe > 1)
		return (output->pipe << DRM_VBLANK_HIGH_CRTC_SHIFT) &
				DRM_VBLANK_HIGH_CRTC_MASK;
	else if (output->pipe > 0)
		return DRM_VBLANK_SECONDARY;
	else
		return 0;
}

static int
drm_output_apply_state_legacy(struct drm_output_state *state)
{
	struct drm_output *output = state->output;
	struct drm_backend *backend = to_drm_backend(output->base.compositor);
	struct drm_plane *scanout_plane = output->scanout_plane;
	struct drm_property_info *dpms_prop;
	struct drm_plane_state *scanout_state;
	struct drm_plane_state *ps;
	struct drm_mode *mode;
	struct drm_head *head;
	uint32_t connectors[MAX_CLONED_CONNECTORS];
	int n_conn = 0;
	struct timespec now;
	int ret = 0;

	wl_list_for_each(head, &output->base.head_list, base.output_link) {
		assert(n_conn < MAX_CLONED_CONNECTORS);
		connectors[n_conn++] = head->connector_id;
	}

	/* If disable_planes is set then assign_planes() wasn't
	 * called for this render, so we could still have a stale
	 * cursor plane set up.
	 */
	if (output->base.disable_planes) {
		output->cursor_view = NULL;
		if (output->cursor_plane) {
			output->cursor_plane->base.x = INT32_MIN;
			output->cursor_plane->base.y = INT32_MIN;
		}
	}

	if (state->dpms != WESTON_DPMS_ON) {
		wl_list_for_each(ps, &state->plane_list, link) {
			struct drm_plane *p = ps->plane;
			assert(ps->fb == NULL);
			assert(ps->output == NULL);

			if (p->type != WDRM_PLANE_TYPE_OVERLAY)
				continue;

			ret = drmModeSetPlane(backend->drm.fd, p->plane_id,
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			if (ret)
				weston_log("drmModeSetPlane failed disable: %m\n");
		}

		if (output->cursor_plane) {
			ret = drmModeSetCursor(backend->drm.fd, output->crtc_id,
					       0, 0, 0);
			if (ret)
				weston_log("drmModeSetCursor failed disable: %m\n");
		}

		ret = drmModeSetCrtc(backend->drm.fd, output->crtc_id, 0, 0, 0,
				     NULL, 0, NULL);
		if (ret)
			weston_log("drmModeSetCrtc failed disabling: %m\n");

		drm_output_assign_state(state, DRM_STATE_APPLY_SYNC);
		weston_compositor_read_presentation_clock(output->base.compositor, &now);
		drm_output_update_complete(output,
		                           WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION,
					   now.tv_sec, now.tv_nsec / 1000);

		return 0;
	}

	scanout_state =
		drm_output_state_get_existing_plane(state, scanout_plane);

	/* The legacy SetCrtc API doesn't allow us to do scaling, and the
	 * legacy PageFlip API doesn't allow us to do clipping either. */
	assert(scanout_state->src_x == 0);
	assert(scanout_state->src_y == 0);
	assert(scanout_state->src_w ==
		(unsigned) (output->base.current_mode->width << 16));
	assert(scanout_state->src_h ==
		(unsigned) (output->base.current_mode->height << 16));
	assert(scanout_state->dest_x == 0);
	assert(scanout_state->dest_y == 0);
	assert(scanout_state->dest_w == scanout_state->src_w >> 16);
	assert(scanout_state->dest_h == scanout_state->src_h >> 16);

	mode = to_drm_mode(output->base.current_mode);
	if (backend->state_invalid ||
	    !scanout_plane->state_cur->fb ||
	    scanout_plane->state_cur->fb->strides[0] !=
	    scanout_state->fb->strides[0]) {
		ret = drmModeSetCrtc(backend->drm.fd, output->crtc_id,
				     scanout_state->fb->fb_id,
				     0, 0,
				     connectors, n_conn,
				     &mode->mode_info);
		if (ret) {
			weston_log("set mode failed: %m\n");
			goto err;
		}
	}

	if (drmModePageFlip(backend->drm.fd, output->crtc_id,
			    scanout_state->fb->fb_id,
			    DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
		weston_log("queueing pageflip failed: %m\n");
		goto err;
	}

	assert(!output->page_flip_pending);

	if (output->pageflip_timer)
		wl_event_source_timer_update(output->pageflip_timer,
		                             backend->pageflip_timeout);

	drm_output_set_cursor(state);

	/*
	 * Now, update all the sprite surfaces
	 */
	wl_list_for_each(ps, &state->plane_list, link) {
		uint32_t flags = 0, fb_id = 0;
		drmVBlank vbl = {
			.request.type = DRM_VBLANK_RELATIVE | DRM_VBLANK_EVENT,
			.request.sequence = 1,
		};
		struct drm_plane *p = ps->plane;

		if (p->type != WDRM_PLANE_TYPE_OVERLAY)
			continue;

		assert(p->state_cur->complete);
		assert(!!p->state_cur->output == !!p->state_cur->fb);
		assert(!p->state_cur->output || p->state_cur->output == output);
		assert(!ps->complete);
		assert(!ps->output || ps->output == output);
		assert(!!ps->output == !!ps->fb);

		if (ps->fb && !backend->sprites_hidden)
			fb_id = ps->fb->fb_id;

		ret = drmModeSetPlane(backend->drm.fd, p->plane_id,
				      output->crtc_id, fb_id, flags,
				      ps->dest_x, ps->dest_y,
				      ps->dest_w, ps->dest_h,
				      ps->src_x, ps->src_y,
				      ps->src_w, ps->src_h);
		if (ret)
			weston_log("setplane failed: %d: %s\n",
				ret, strerror(errno));

		vbl.request.type |= drm_waitvblank_pipe(output);

		/*
		 * Queue a vblank signal so we know when the surface
		 * becomes active on the display or has been replaced.
		 */
		vbl.request.signal = (unsigned long) ps;
		ret = drmWaitVBlank(backend->drm.fd, &vbl);
		if (ret) {
			weston_log("vblank event request failed: %d: %s\n",
				ret, strerror(errno));
		}
	}

	if (state->dpms != output->state_cur->dpms) {
		wl_list_for_each(head, &output->base.head_list, base.output_link) {
			dpms_prop = &head->props_conn[WDRM_CONNECTOR_DPMS];
			if (dpms_prop->prop_id == 0)
				continue;

			ret = drmModeConnectorSetProperty(backend->drm.fd,
							  head->connector_id,
							  dpms_prop->prop_id,
							  state->dpms);
			if (ret) {
				weston_log("DRM: DPMS: failed property set for %s\n",
					   head->base.name);
			}
		}
	}

	drm_output_assign_state(state, DRM_STATE_APPLY_ASYNC);

	return 0;

err:
	output->cursor_view = NULL;
	drm_output_state_free(state);
	return -1;
}

#ifdef HAVE_DRM_ATOMIC
static int
crtc_add_prop(drmModeAtomicReq *req, struct drm_output *output,
	      enum wdrm_crtc_property prop, uint64_t val)
{
	struct drm_property_info *info = &output->props_crtc[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, output->crtc_id, info->prop_id,
				       val);
	return (ret <= 0) ? -1 : 0;
}

static int
connector_add_prop(drmModeAtomicReq *req, struct drm_head *head,
		   enum wdrm_connector_property prop, uint64_t val)
{
	struct drm_property_info *info = &head->props_conn[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, head->connector_id,
				       info->prop_id, val);
	return (ret <= 0) ? -1 : 0;
}

static int
plane_add_prop(drmModeAtomicReq *req, struct drm_plane *plane,
	       enum wdrm_plane_property prop, uint64_t val)
{
	struct drm_property_info *info = &plane->props[prop];
	int ret;

	if (info->prop_id == 0)
		return -1;

	ret = drmModeAtomicAddProperty(req, plane->plane_id, info->prop_id,
				       val);
	return (ret <= 0) ? -1 : 0;
}

static int
drm_mode_ensure_blob(struct drm_backend *backend, struct drm_mode *mode)
{
	int ret;

	if (mode->blob_id)
		return 0;

	ret = drmModeCreatePropertyBlob(backend->drm.fd,
					&mode->mode_info,
					sizeof(mode->mode_info),
					&mode->blob_id);
	if (ret != 0)
		weston_log("failed to create mode property blob: %m\n");

	return ret;
}

static int
drm_output_apply_state_atomic(struct drm_output_state *state,
			      drmModeAtomicReq *req,
			      uint32_t *flags)
{
	struct drm_output *output = state->output;
	struct drm_backend *backend = to_drm_backend(output->base.compositor);
	struct drm_plane_state *plane_state;
	struct drm_mode *current_mode = to_drm_mode(output->base.current_mode);
	struct drm_head *head;
	int ret = 0;

	if (state->dpms != output->state_cur->dpms)
		*flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;

	if (state->dpms == WESTON_DPMS_ON) {
		ret = drm_mode_ensure_blob(backend, current_mode);
		if (ret != 0)
			return ret;

		ret |= crtc_add_prop(req, output, WDRM_CRTC_MODE_ID,
				     current_mode->blob_id);
		ret |= crtc_add_prop(req, output, WDRM_CRTC_ACTIVE, 1);

		/* No need for the DPMS property, since it is implicit in
		 * routing and CRTC activity. */
		wl_list_for_each(head, &output->base.head_list, base.output_link) {
			ret |= connector_add_prop(req, head, WDRM_CONNECTOR_CRTC_ID,
						  output->crtc_id);
		}
	} else {
		ret |= crtc_add_prop(req, output, WDRM_CRTC_MODE_ID, 0);
		ret |= crtc_add_prop(req, output, WDRM_CRTC_ACTIVE, 0);

		/* No need for the DPMS property, since it is implicit in
		 * routing and CRTC activity. */
		wl_list_for_each(head, &output->base.head_list, base.output_link)
			ret |= connector_add_prop(req, head, WDRM_CONNECTOR_CRTC_ID, 0);
	}

	if (ret != 0) {
		weston_log("couldn't set atomic CRTC/connector state\n");
		return ret;
	}

	wl_list_for_each(plane_state, &state->plane_list, link) {
		struct drm_plane *plane = plane_state->plane;

		ret |= plane_add_prop(req, plane, WDRM_PLANE_FB_ID,
				      plane_state->fb ? plane_state->fb->fb_id : 0);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_ID,
				      plane_state->fb ? output->crtc_id : 0);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_X,
				      plane_state->src_x);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_Y,
				      plane_state->src_y);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_W,
				      plane_state->src_w);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_SRC_H,
				      plane_state->src_h);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_X,
				      plane_state->dest_x);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_Y,
				      plane_state->dest_y);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_W,
				      plane_state->dest_w);
		ret |= plane_add_prop(req, plane, WDRM_PLANE_CRTC_H,
				      plane_state->dest_h);

		if (ret != 0) {
			weston_log("couldn't set plane state\n");
			return ret;
		}
	}

	return 0;
}

/**
 * Helper function used only by drm_pending_state_apply, with the same
 * guarantees and constraints as that function.
 */
static int
drm_pending_state_apply_atomic(struct drm_pending_state *pending_state,
			       enum drm_state_apply_mode mode)
{
	struct drm_backend *b = pending_state->backend;
	struct drm_output_state *output_state, *tmp;
	struct drm_plane *plane;
	drmModeAtomicReq *req = drmModeAtomicAlloc();
	uint32_t flags;
	int ret = 0;

	if (!req)
		return -1;

	switch (mode) {
	case DRM_STATE_APPLY_SYNC:
		flags = 0;
		break;
	case DRM_STATE_APPLY_ASYNC:
		flags = DRM_MODE_PAGE_FLIP_EVENT | DRM_MODE_ATOMIC_NONBLOCK;
		break;
	case DRM_STATE_TEST_ONLY:
		flags = DRM_MODE_ATOMIC_TEST_ONLY;
		break;
	}

	if (b->state_invalid) {
		struct weston_head *head_base;
		struct drm_head *head;
		uint32_t *unused;
		int err;

		/* If we need to reset all our state (e.g. because we've
		 * just started, or just been VT-switched in), explicitly
		 * disable all the CRTCs and connectors we aren't using. */
		wl_list_for_each(head_base,
				 &b->compositor->head_list, compositor_link) {
			struct drm_property_info *info;

			if (weston_head_is_enabled(head_base))
				continue;

			head = to_drm_head(head_base);

			info = &head->props_conn[WDRM_CONNECTOR_CRTC_ID];
			err = drmModeAtomicAddProperty(req, head->connector_id,
						       info->prop_id, 0);
			if (err <= 0)
				ret = -1;
		}

		wl_array_for_each(unused, &b->unused_crtcs) {
			struct drm_property_info infos[WDRM_CRTC__COUNT];
			struct drm_property_info *info;
			drmModeObjectProperties *props;
			uint64_t active;

			memset(infos, 0, sizeof(infos));

			/* We can't emit a disable on a CRTC that's already
			 * off, as the kernel will refuse to generate an event
			 * for an off->off state and fail the commit.
			 */
			props = drmModeObjectGetProperties(b->drm.fd,
							   *unused,
							   DRM_MODE_OBJECT_CRTC);
			if (!props) {
				ret = -1;
				continue;
			}

			drm_property_info_populate(b, crtc_props, infos,
						   WDRM_CRTC__COUNT,
						   props);

			info = &infos[WDRM_CRTC_ACTIVE];
			active = drm_property_get_value(info, props, 0);
			drmModeFreeObjectProperties(props);
			if (active == 0) {
				drm_property_info_free(infos, WDRM_CRTC__COUNT);
				continue;
			}

			err = drmModeAtomicAddProperty(req, *unused,
						       info->prop_id, 0);
			if (err <= 0)
				ret = -1;

			info = &infos[WDRM_CRTC_MODE_ID];
			err = drmModeAtomicAddProperty(req, *unused,
						       info->prop_id, 0);
			if (err <= 0)
				ret = -1;

			drm_property_info_free(infos, WDRM_CRTC__COUNT);
		}

		/* Disable all the planes; planes which are being used will
		 * override this state in the output-state application. */
		wl_list_for_each(plane, &b->plane_list, link) {
			plane_add_prop(req, plane, WDRM_PLANE_CRTC_ID, 0);
			plane_add_prop(req, plane, WDRM_PLANE_FB_ID, 0);
		}

		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
	}

	wl_list_for_each(output_state, &pending_state->output_list, link) {
		if (mode == DRM_STATE_APPLY_SYNC)
			assert(output_state->dpms == WESTON_DPMS_OFF);
		ret |= drm_output_apply_state_atomic(output_state, req, &flags);
	}

	if (ret != 0) {
		weston_log("atomic: couldn't compile atomic state\n");
		goto out;
	}

	ret = drmModeAtomicCommit(b->drm.fd, req, flags, b);

	/* Test commits do not take ownership of the state; return
	 * without freeing here. */
	if (mode == DRM_STATE_TEST_ONLY) {
		drmModeAtomicFree(req);
		return ret;
	}

	if (ret != 0) {
		weston_log("atomic: couldn't commit new state: %m\n");
		goto out;
	}

	wl_list_for_each_safe(output_state, tmp, &pending_state->output_list,
			      link)
		drm_output_assign_state(output_state, mode);

	b->state_invalid = false;

	assert(wl_list_empty(&pending_state->output_list));

out:
	drmModeAtomicFree(req);
	drm_pending_state_free(pending_state);
	return ret;
}
#endif

/**
 * Tests a pending state, to see if the kernel will accept the update as
 * constructed.
 *
 * Using atomic modesetting, the kernel performs the same checks as it would
 * on a real commit, returning success or failure without actually modifying
 * the running state. It does not return -EBUSY if there are pending updates
 * in flight, so states may be tested at any point, however this means a
 * state which passed testing may fail on a real commit if the timing is not
 * respected (e.g. committing before the previous commit has completed).
 *
 * Without atomic modesetting, we have no way to check, so we optimistically
 * claim it will work.
 *
 * Unlike drm_pending_state_apply() and drm_pending_state_apply_sync(), this
 * function does _not_ take ownership of pending_state, nor does it clear
 * state_invalid.
 */
static int
drm_pending_state_test(struct drm_pending_state *pending_state)
{
#ifdef HAVE_DRM_ATOMIC
	struct drm_backend *b = pending_state->backend;

	if (b->atomic_modeset)
		return drm_pending_state_apply_atomic(pending_state,
						      DRM_STATE_TEST_ONLY);
#endif

	/* We have no way to test state before application on the legacy
	 * modesetting API, so just claim it succeeded. */
	return 0;
}

/**
 * Applies all of a pending_state asynchronously: the primary entry point for
 * applying KMS state to a device. Updates the state for all outputs in the
 * pending_state, as well as disabling any unclaimed outputs.
 *
 * Unconditionally takes ownership of pending_state, and clears state_invalid.
 */
static int
drm_pending_state_apply(struct drm_pending_state *pending_state)
{
	struct drm_backend *b = pending_state->backend;
	struct drm_output_state *output_state, *tmp;
	uint32_t *unused;

#ifdef HAVE_DRM_ATOMIC
	if (b->atomic_modeset)
		return drm_pending_state_apply_atomic(pending_state,
						      DRM_STATE_APPLY_ASYNC);
#endif

	if (b->state_invalid) {
		/* If we need to reset all our state (e.g. because we've
		 * just started, or just been VT-switched in), explicitly
		 * disable all the CRTCs we aren't using. This also disables
		 * all connectors on these CRTCs, so we don't need to do that
		 * separately with the pre-atomic API. */
		wl_array_for_each(unused, &b->unused_crtcs)
			drmModeSetCrtc(b->drm.fd, *unused, 0, 0, 0, NULL, 0,
				       NULL);
	}

	wl_list_for_each_safe(output_state, tmp, &pending_state->output_list,
			      link) {
		struct drm_output *output = output_state->output;
		int ret;

		ret = drm_output_apply_state_legacy(output_state);
		if (ret != 0) {
			weston_log("Couldn't apply state for output %s\n",
				   output->base.name);
		}
	}

	b->state_invalid = false;

	assert(wl_list_empty(&pending_state->output_list));

	drm_pending_state_free(pending_state);

	return 0;
}

/**
 * The synchronous version of drm_pending_state_apply. May only be used to
 * disable outputs. Does so synchronously: the request is guaranteed to have
 * completed on return, and the output will not be touched afterwards.
 *
 * Unconditionally takes ownership of pending_state, and clears state_invalid.
 */
static int
drm_pending_state_apply_sync(struct drm_pending_state *pending_state)
{
	struct drm_backend *b = pending_state->backend;
	struct drm_output_state *output_state, *tmp;
	uint32_t *unused;

#ifdef HAVE_DRM_ATOMIC
	if (b->atomic_modeset)
		return drm_pending_state_apply_atomic(pending_state,
						      DRM_STATE_APPLY_SYNC);
#endif

	if (b->state_invalid) {
		/* If we need to reset all our state (e.g. because we've
		 * just started, or just been VT-switched in), explicitly
		 * disable all the CRTCs we aren't using. This also disables
		 * all connectors on these CRTCs, so we don't need to do that
		 * separately with the pre-atomic API. */
		wl_array_for_each(unused, &b->unused_crtcs)
			drmModeSetCrtc(b->drm.fd, *unused, 0, 0, 0, NULL, 0,
				       NULL);
	}

	wl_list_for_each_safe(output_state, tmp, &pending_state->output_list,
			      link) {
		int ret;

		assert(output_state->dpms == WESTON_DPMS_OFF);
		ret = drm_output_apply_state_legacy(output_state);
		if (ret != 0) {
			weston_log("Couldn't apply state for output %s\n",
				   output_state->output->base.name);
		}
	}

	b->state_invalid = false;

	assert(wl_list_empty(&pending_state->output_list));

	drm_pending_state_free(pending_state);

	return 0;
}

static int
drm_output_repaint(struct weston_output *output_base,
		   pixman_region32_t *damage,
		   void *repaint_data)
{
	struct drm_pending_state *pending_state = repaint_data;
	struct drm_output *output = to_drm_output(output_base);
	struct drm_output_state *state = NULL;
	struct drm_plane_state *scanout_state;

	if (output->disable_pending || output->destroy_pending)
		goto err;

	assert(!output->state_last);

	/* If planes have been disabled in the core, we might not have
	 * hit assign_planes at all, so might not have valid output state
	 * here. */
	state = drm_pending_state_get_output(pending_state, output);
	if (!state)
		state = drm_output_state_duplicate(output->state_cur,
						   pending_state,
						   DRM_OUTPUT_STATE_CLEAR_PLANES);
	state->dpms = WESTON_DPMS_ON;

	drm_output_render(state, damage);
	scanout_state = drm_output_state_get_plane(state,
						   output->scanout_plane);
	if (!scanout_state || !scanout_state->fb)
		goto err;

	return 0;

err:
	drm_output_state_free(state);
	return -1;
}

static void
drm_output_start_repaint_loop(struct weston_output *output_base)
{
	struct drm_output *output = to_drm_output(output_base);
	struct drm_pending_state *pending_state;
	struct drm_plane *scanout_plane = output->scanout_plane;
	struct drm_backend *backend =
		to_drm_backend(output_base->compositor);
	struct timespec ts, tnow;
	struct timespec vbl2now;
	int64_t refresh_nsec;
	int ret;
	drmVBlank vbl = {
		.request.type = DRM_VBLANK_RELATIVE,
		.request.sequence = 0,
		.request.signal = 0,
	};

	if (output->disable_pending || output->destroy_pending)
		return;

	if (!output->scanout_plane->state_cur->fb) {
		/* We can't page flip if there's no mode set */
		goto finish_frame;
	}

	/* Need to smash all state in from scratch; current timings might not
	 * be what we want, page flip might not work, etc.
	 */
	if (backend->state_invalid)
		goto finish_frame;

	assert(scanout_plane->state_cur->output == output);

	/* Try to get current msc and timestamp via instant query */
	vbl.request.type |= drm_waitvblank_pipe(output);
	ret = drmWaitVBlank(backend->drm.fd, &vbl);

	/* Error ret or zero timestamp means failure to get valid timestamp */
	if ((ret == 0) && (vbl.reply.tval_sec > 0 || vbl.reply.tval_usec > 0)) {
		ts.tv_sec = vbl.reply.tval_sec;
		ts.tv_nsec = vbl.reply.tval_usec * 1000;

		/* Valid timestamp for most recent vblank - not stale?
		 * Stale ts could happen on Linux 3.17+, so make sure it
		 * is not older than 1 refresh duration since now.
		 */
		weston_compositor_read_presentation_clock(backend->compositor,
							  &tnow);
		timespec_sub(&vbl2now, &tnow, &ts);
		refresh_nsec =
			millihz_to_nsec(output->base.current_mode->refresh);
		if (timespec_to_nsec(&vbl2now) < refresh_nsec) {
			drm_output_update_msc(output, vbl.reply.sequence);
			weston_output_finish_frame(output_base, &ts,
						WP_PRESENTATION_FEEDBACK_INVALID);
			return;
		}
	}

	/* Immediate query didn't provide valid timestamp.
	 * Use pageflip fallback.
	 */

	assert(!output->page_flip_pending);
	assert(!output->state_last);

	pending_state = drm_pending_state_alloc(backend);
	drm_output_state_duplicate(output->state_cur, pending_state,
				   DRM_OUTPUT_STATE_PRESERVE_PLANES);

	ret = drm_pending_state_apply(pending_state);
	if (ret != 0) {
		weston_log("applying repaint-start state failed: %m\n");
		goto finish_frame;
	}

	return;

finish_frame:
	/* if we cannot page-flip, immediately finish frame */
	weston_output_finish_frame(output_base, NULL,
				   WP_PRESENTATION_FEEDBACK_INVALID);
}

static void
drm_output_update_msc(struct drm_output *output, unsigned int seq)
{
	uint64_t msc_hi = output->base.msc >> 32;

	if (seq < (output->base.msc & 0xffffffff))
		msc_hi++;

	output->base.msc = (msc_hi << 32) + seq;
}

static void
vblank_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec,
	       void *data)
{
	struct drm_plane_state *ps = (struct drm_plane_state *) data;
	struct drm_output_state *os = ps->output_state;
	struct drm_output *output = os->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;

	assert(!b->atomic_modeset);

	drm_output_update_msc(output, frame);
	output->vblank_pending--;
	assert(output->vblank_pending >= 0);

	assert(ps->fb);

	if (output->page_flip_pending || output->vblank_pending)
		return;

	drm_output_update_complete(output, flags, sec, usec);
}

static void
page_flip_handler(int fd, unsigned int frame,
		  unsigned int sec, unsigned int usec, void *data)
{
	struct drm_output *output = data;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;

	drm_output_update_msc(output, frame);

	assert(!b->atomic_modeset);
	assert(output->page_flip_pending);
	output->page_flip_pending = 0;

	if (output->vblank_pending)
		return;

	drm_output_update_complete(output, flags, sec, usec);
}

/**
 * Begin a new repaint cycle
 *
 * Called by the core compositor at the beginning of a repaint cycle. Creates
 * a new pending_state structure to own any output state created by individual
 * output repaint functions until the repaint is flushed or cancelled.
 */
static void *
drm_repaint_begin(struct weston_compositor *compositor)
{
	struct drm_backend *b = to_drm_backend(compositor);
	struct drm_pending_state *ret;

	ret = drm_pending_state_alloc(b);
	b->repaint_data = ret;

	return ret;
}

/**
 * Flush a repaint set
 *
 * Called by the core compositor when a repaint cycle has been completed
 * and should be flushed. Frees the pending state, transitioning ownership
 * of the output state from the pending state, to the update itself. When
 * the update completes (see drm_output_update_complete), the output
 * state will be freed.
 */
static void
drm_repaint_flush(struct weston_compositor *compositor, void *repaint_data)
{
	struct drm_backend *b = to_drm_backend(compositor);
	struct drm_pending_state *pending_state = repaint_data;

	drm_pending_state_apply(pending_state);
	b->repaint_data = NULL;
}

/**
 * Cancel a repaint set
 *
 * Called by the core compositor when a repaint has finished, so the data
 * held across the repaint cycle should be discarded.
 */
static void
drm_repaint_cancel(struct weston_compositor *compositor, void *repaint_data)
{
	struct drm_backend *b = to_drm_backend(compositor);
	struct drm_pending_state *pending_state = repaint_data;

	drm_pending_state_free(pending_state);
	b->repaint_data = NULL;
}

#ifdef HAVE_DRM_ATOMIC
static void
atomic_flip_handler(int fd, unsigned int frame, unsigned int sec,
		    unsigned int usec, unsigned int crtc_id, void *data)
{
	struct drm_backend *b = data;
	struct drm_output *output = drm_output_find_by_crtc(b, crtc_id);
	uint32_t flags = WP_PRESENTATION_FEEDBACK_KIND_VSYNC |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION |
			 WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK;

	/* During the initial modeset, we can disable CRTCs which we don't
	 * actually handle during normal operation; this will give us events
	 * for unknown outputs. Ignore them. */
	if (!output || !output->base.enabled)
		return;

	drm_output_update_msc(output, frame);

	assert(b->atomic_modeset);
	assert(output->atomic_complete_pending);
	output->atomic_complete_pending = 0;

	drm_output_update_complete(output, flags, sec, usec);
}
#endif

static struct drm_plane_state *
drm_output_prepare_overlay_view(struct drm_output_state *output_state,
				struct weston_view *ev,
				enum drm_output_propose_state_mode mode)
{
	struct drm_output *output = output_state->output;
	struct weston_compositor *ec = output->base.compositor;
	struct drm_backend *b = to_drm_backend(ec);
	struct drm_plane *p;
	struct drm_plane_state *state = NULL;
	struct drm_fb *fb;
	unsigned int i;
	int ret;

	assert(!b->sprites_are_broken);

	fb = drm_fb_get_from_view(output_state, ev);
	if (!fb)
		return NULL;

	wl_list_for_each(p, &b->plane_list, link) {
		if (p->type != WDRM_PLANE_TYPE_OVERLAY)
			continue;

		if (!drm_plane_is_available(p, output))
			continue;

		/* Check whether the format is supported */
		for (i = 0; i < p->count_formats; i++) {
			unsigned int j;

			if (p->formats[i].format != fb->format->format)
				continue;

			if (fb->modifier == DRM_FORMAT_MOD_INVALID)
				break;

			for (j = 0; j < p->formats[i].count_modifiers; j++) {
				if (p->formats[i].modifiers[j] == fb->modifier)
					break;
			}
			if (j != p->formats[i].count_modifiers)
				break;
		}
		if (i == p->count_formats)
			continue;

		state = drm_output_state_get_plane(output_state, p);
		if (state->fb) {
			state = NULL;
			continue;
		}

		state->ev = ev;
		state->output = output;
		if (!drm_plane_state_coords_for_view(state, ev)) {
			drm_plane_state_put_back(state);
			state = NULL;
			continue;
		}
		if (!b->atomic_modeset &&
		    (state->src_w != state->dest_w << 16 ||
		     state->src_h != state->dest_h << 16)) {
			drm_plane_state_put_back(state);
			state = NULL;
			continue;
		}

		/* We hold one reference for the lifetime of this function;
		 * from calling drm_fb_get_from_view, to the out label where
		 * we unconditionally drop the reference. So, we take another
		 * reference here to live within the state. */
		state->fb = drm_fb_ref(fb);

		/* In planes-only mode, we don't have an incremental state to
		 * test against, so we just hope it'll work. */
		if (mode == DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY)
			goto out;

		ret = drm_pending_state_test(output_state->pending_state);
		if (ret == 0)
			goto out;

		drm_plane_state_put_back(state);
		state = NULL;
	}

out:
	drm_fb_unref(fb);
	return state;
}

/**
 * Update the image for the current cursor surface
 *
 * @param plane_state DRM cursor plane state
 * @param ev Source view for cursor
 */
static void
cursor_bo_update(struct drm_plane_state *plane_state, struct weston_view *ev)
{
	struct drm_backend *b = plane_state->plane->backend;
	struct gbm_bo *bo = plane_state->fb->bo;
	struct weston_buffer *buffer = ev->surface->buffer_ref.buffer;
	uint32_t buf[b->cursor_width * b->cursor_height];
	int32_t stride;
	uint8_t *s;
	int i;

	assert(buffer && buffer->shm_buffer);
	assert(buffer->shm_buffer == wl_shm_buffer_get(buffer->resource));
	assert(buffer->width <= b->cursor_width);
	assert(buffer->height <= b->cursor_height);

	memset(buf, 0, sizeof buf);
	stride = wl_shm_buffer_get_stride(buffer->shm_buffer);
	s = wl_shm_buffer_get_data(buffer->shm_buffer);

	wl_shm_buffer_begin_access(buffer->shm_buffer);
	for (i = 0; i < buffer->height; i++)
		memcpy(buf + i * b->cursor_width,
		       s + i * stride,
		       buffer->width * 4);
	wl_shm_buffer_end_access(buffer->shm_buffer);

	if (gbm_bo_write(bo, buf, sizeof buf) < 0)
		weston_log("failed update cursor: %m\n");
}

static struct drm_plane_state *
drm_output_prepare_cursor_view(struct drm_output_state *output_state,
			       struct weston_view *ev)
{
	struct drm_output *output = output_state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane *plane = output->cursor_plane;
	struct drm_plane_state *plane_state;
	struct wl_shm_buffer *shmbuf;
	bool needs_update = false;

	assert(!b->cursors_are_broken);

	if (!plane)
		return NULL;

	if (!plane->state_cur->complete)
		return NULL;

	if (plane->state_cur->output && plane->state_cur->output != output)
		return NULL;

	/* We use GBM to import SHM buffers. */
	if (b->gbm == NULL)
		return NULL;

	if (ev->surface->buffer_ref.buffer == NULL)
		return NULL;
	shmbuf = wl_shm_buffer_get(ev->surface->buffer_ref.buffer->resource);
	if (!shmbuf)
		return NULL;
	if (wl_shm_buffer_get_format(shmbuf) != WL_SHM_FORMAT_ARGB8888)
		return NULL;

	plane_state =
		drm_output_state_get_plane(output_state, output->cursor_plane);

	if (plane_state && plane_state->fb)
		return NULL;

	/* We can't scale with the legacy API, and we don't try to account for
	 * simple cropping/translation in cursor_bo_update. */
	plane_state->output = output;
	if (!drm_plane_state_coords_for_view(plane_state, ev))
		goto err;

	if (plane_state->src_x != 0 || plane_state->src_y != 0 ||
	    plane_state->src_w > (unsigned) b->cursor_width << 16 ||
	    plane_state->src_h > (unsigned) b->cursor_height << 16 ||
	    plane_state->src_w != plane_state->dest_w << 16 ||
	    plane_state->src_h != plane_state->dest_h << 16)
		goto err;

	/* Since we're setting plane state up front, we need to work out
	 * whether or not we need to upload a new cursor. We can't use the
	 * plane damage, since the planes haven't actually been calculated
	 * yet: instead try to figure it out directly. KMS cursor planes are
	 * pretty unique here, in that they lie partway between a Weston plane
	 * (direct scanout) and a renderer. */
	if (ev != output->cursor_view ||
	    pixman_region32_not_empty(&ev->surface->damage)) {
		output->current_cursor++;
		output->current_cursor =
			output->current_cursor %
				ARRAY_LENGTH(output->gbm_cursor_fb);
		needs_update = true;
	}

	output->cursor_view = ev;
	plane_state->ev = ev;

	plane_state->fb =
		drm_fb_ref(output->gbm_cursor_fb[output->current_cursor]);

	if (needs_update)
		cursor_bo_update(plane_state, ev);

	/* The cursor API is somewhat special: in cursor_bo_update(), we upload
	 * a buffer which is always cursor_width x cursor_height, even if the
	 * surface we want to promote is actually smaller than this. Manually
	 * mangle the plane state to deal with this. */
	plane_state->src_w = b->cursor_width << 16;
	plane_state->src_h = b->cursor_height << 16;
	plane_state->dest_w = b->cursor_width;
	plane_state->dest_h = b->cursor_height;

	return plane_state;

err:
	drm_plane_state_put_back(plane_state);
	return NULL;
}

static void
drm_output_set_cursor(struct drm_output_state *output_state)
{
	struct drm_output *output = output_state->output;
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_plane *plane = output->cursor_plane;
	struct drm_plane_state *state;
	EGLint handle;
	struct gbm_bo *bo;

	if (!plane)
		return;

	state = drm_output_state_get_existing_plane(output_state, plane);
	if (!state)
		return;

	if (!state->fb) {
		pixman_region32_fini(&plane->base.damage);
		pixman_region32_init(&plane->base.damage);
		drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);
		return;
	}

	assert(state->fb == output->gbm_cursor_fb[output->current_cursor]);
	assert(!plane->state_cur->output || plane->state_cur->output == output);

	if (plane->state_cur->fb != state->fb) {
		bo = state->fb->bo;
		handle = gbm_bo_get_handle(bo).s32;
		if (drmModeSetCursor(b->drm.fd, output->crtc_id, handle,
				     b->cursor_width, b->cursor_height)) {
			weston_log("failed to set cursor: %m\n");
			goto err;
		}
	}

	pixman_region32_fini(&plane->base.damage);
	pixman_region32_init(&plane->base.damage);

	if (drmModeMoveCursor(b->drm.fd, output->crtc_id,
	                      state->dest_x, state->dest_y)) {
		weston_log("failed to move cursor: %m\n");
		goto err;
	}

	return;

err:
	b->cursors_are_broken = 1;
	drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);
}

static struct drm_output_state *
drm_output_propose_state(struct weston_output *output_base,
			 struct drm_pending_state *pending_state,
			 enum drm_output_propose_state_mode mode)
{
	struct drm_output *output = to_drm_output(output_base);
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	struct drm_output_state *state;
	struct drm_plane_state *scanout_state = NULL;
	struct weston_view *ev;
	pixman_region32_t surface_overlap, renderer_region, occluded_region;
	bool planes_ok = (mode != DRM_OUTPUT_PROPOSE_STATE_RENDERER_ONLY);
	bool renderer_ok = (mode != DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY);
	int ret;

	assert(!output->state_last);
	state = drm_output_state_duplicate(output->state_cur,
					   pending_state,
					   DRM_OUTPUT_STATE_CLEAR_PLANES);

	/* We implement mixed mode by progressively creating and testing
	 * incremental states, of scanout + overlay + cursor. Since we
	 * walk our views top to bottom, the scanout plane is last, however
	 * we always need it in our scene for the test modeset to be
	 * meaningful. To do this, we steal a reference to the last
	 * renderer framebuffer we have, if we think it's basically
	 * compatible. If we don't have that, then we conservatively fall
	 * back to only using the renderer for this repaint. */
	if (mode == DRM_OUTPUT_PROPOSE_STATE_MIXED) {
		struct drm_plane *plane = output->scanout_plane;
		struct drm_fb *scanout_fb = plane->state_cur->fb;

		if (!scanout_fb ||
		    (scanout_fb->type != BUFFER_GBM_SURFACE &&
		     scanout_fb->type != BUFFER_PIXMAN_DUMB)) {
			drm_output_state_free(state);
			return NULL;
		}

		if (scanout_fb->width != output_base->current_mode->width ||
		    scanout_fb->height != output_base->current_mode->height) {
			drm_output_state_free(state);
			return NULL;
		}

		scanout_state = drm_plane_state_duplicate(state,
							  plane->state_cur);
	}

	/*
	 * Find a surface for each sprite in the output using some heuristics:
	 * 1) size
	 * 2) frequency of update
	 * 3) opacity (though some hw might support alpha blending)
	 * 4) clipping (this can be fixed with color keys)
	 *
	 * The idea is to save on blitting since this should save power.
	 * If we can get a large video surface on the sprite for example,
	 * the main display surface may not need to update at all, and
	 * the client buffer can be used directly for the sprite surface
	 * as we do for flipping full screen surfaces.
	 */
	pixman_region32_init(&renderer_region);
	pixman_region32_init(&occluded_region);

	wl_list_for_each(ev, &output_base->compositor->view_list, link) {
		struct drm_plane_state *ps = NULL;
		bool force_renderer = false;
		pixman_region32_t clipped_view;
		bool totally_occluded = false;
		bool overlay_occluded = false;

		/* If this view doesn't touch our output at all, there's no
		 * reason to do anything with it. */
		if (!(ev->output_mask & (1u << output->base.id)))
			continue;

		/* We only assign planes to views which are exclusively present
		 * on our output. */
		if (ev->output_mask != (1u << output->base.id))
			force_renderer = true;

		if (!ev->surface->buffer_ref.buffer)
			force_renderer = true;

		/* Ignore views we know to be totally occluded. */
		pixman_region32_init(&clipped_view);
		pixman_region32_intersect(&clipped_view,
					  &ev->transform.boundingbox,
					  &output->base.region);

		pixman_region32_init(&surface_overlap);
		pixman_region32_subtract(&surface_overlap, &clipped_view,
					 &occluded_region);
		totally_occluded = !pixman_region32_not_empty(&surface_overlap);
		if (totally_occluded) {
			pixman_region32_fini(&surface_overlap);
			pixman_region32_fini(&clipped_view);
			continue;
		}

		/* Since we process views from top to bottom, we know that if
		 * the view intersects the calculated renderer region, it must
		 * be part of, or occluded by, it, and cannot go on a plane. */
		pixman_region32_intersect(&surface_overlap, &renderer_region,
					  &clipped_view);
		if (pixman_region32_not_empty(&surface_overlap))
			force_renderer = true;

		/* We do not control the stacking order of overlay planes;
		 * the scanout plane is strictly stacked bottom and the cursor
		 * plane top, but the ordering of overlay planes with respect
		 * to each other is undefined. Make sure we do not have two
		 * planes overlapping each other. */
		pixman_region32_intersect(&surface_overlap, &occluded_region,
					  &clipped_view);
		if (pixman_region32_not_empty(&surface_overlap))
			overlay_occluded = true;
		pixman_region32_fini(&surface_overlap);

		/* The cursor plane is 'special' in the sense that we can still
		 * place it in the legacy API, and we gate that with a separate
		 * cursors_are_broken flag. */
		if (!force_renderer && !overlay_occluded && !b->cursors_are_broken)
			ps = drm_output_prepare_cursor_view(state, ev);

		/* If sprites are disabled or the view is not fully opaque, we
		 * must put the view into the renderer - unless it has already
		 * been placed in the cursor plane, which can handle alpha. */
		if (!ps && !planes_ok)
			force_renderer = true;
		if (!ps && !drm_view_is_opaque(ev))
			force_renderer = true;

		/* Only try to place scanout surfaces in planes-only mode; in
		 * mixed mode, we have already failed to place a view on the
		 * scanout surface, forcing usage of the renderer on the
		 * scanout plane. */
		if (!ps && !force_renderer && !renderer_ok)
			ps = drm_output_prepare_scanout_view(state, ev, mode);

		if (!ps && !overlay_occluded && !force_renderer)
			ps = drm_output_prepare_overlay_view(state, ev, mode);

		if (ps) {
			/* If we have been assigned to an overlay or scanout
			 * plane, add this area to the occluded region, so
			 * other views are known to be behind it. The cursor
			 * plane, however, is special, in that it blends with
			 * the content underneath it: the area should neither
			 * be added to the renderer region nor the occluded
			 * region. */
			if (ps->plane->type != WDRM_PLANE_TYPE_CURSOR) {
				pixman_region32_union(&occluded_region,
						      &occluded_region,
						      &clipped_view);
				pixman_region32_fini(&clipped_view);
			}
			continue;
		}

		/* We have been assigned to the primary (renderer) plane:
		 * check if this is OK, and add ourselves to the renderer
		 * region if so. */
		if (!renderer_ok) {
			pixman_region32_fini(&clipped_view);
			goto err_region;
		}

		pixman_region32_union(&renderer_region,
				      &renderer_region,
				      &clipped_view);
		pixman_region32_fini(&clipped_view);
	}
	pixman_region32_fini(&renderer_region);
	pixman_region32_fini(&occluded_region);

	/* In renderer-only mode, we can't test the state as we don't have a
	 * renderer buffer yet. */
	if (mode == DRM_OUTPUT_PROPOSE_STATE_RENDERER_ONLY)
		return state;

	/* Check to see if this state will actually work. */
	ret = drm_pending_state_test(state->pending_state);
	if (ret != 0)
		goto err;

	/* Counterpart to duplicating scanout state at the top of this
	 * function: if we have taken a renderer framebuffer and placed it in
	 * the pending state in order to incrementally test overlay planes,
	 * remove it now. */
	if (mode == DRM_OUTPUT_PROPOSE_STATE_MIXED) {
		assert(scanout_state->fb->type == BUFFER_GBM_SURFACE ||
		       scanout_state->fb->type == BUFFER_PIXMAN_DUMB);
		drm_plane_state_put_back(scanout_state);
	}

	return state;

err_region:
	pixman_region32_fini(&renderer_region);
	pixman_region32_fini(&occluded_region);
err:
	drm_output_state_free(state);
	return NULL;
}

static void
drm_assign_planes(struct weston_output *output_base, void *repaint_data)
{
	struct drm_backend *b = to_drm_backend(output_base->compositor);
	struct drm_pending_state *pending_state = repaint_data;
	struct drm_output *output = to_drm_output(output_base);
	struct drm_output_state *state = NULL;
	struct drm_plane_state *plane_state;
	struct weston_view *ev;
	struct weston_plane *primary = &output_base->compositor->primary_plane;

	if (!b->sprites_are_broken) {
		state = drm_output_propose_state(output_base, pending_state,
						 DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY);
		if (!state)
			state = drm_output_propose_state(output_base, pending_state,
							 DRM_OUTPUT_PROPOSE_STATE_MIXED);
	}

	if (!state)
		state = drm_output_propose_state(output_base, pending_state,
						 DRM_OUTPUT_PROPOSE_STATE_RENDERER_ONLY);

	assert(state);

	wl_list_for_each(ev, &output_base->compositor->view_list, link) {
		struct drm_plane *target_plane = NULL;

		/* If this view doesn't touch our output at all, there's no
		 * reason to do anything with it. */
		if (!(ev->output_mask & (1u << output->base.id)))
			continue;

		/* Test whether this buffer can ever go into a plane:
		 * non-shm, or small enough to be a cursor.
		 *
		 * Also, keep a reference when using the pixman renderer.
		 * That makes it possible to do a seamless switch to the GL
		 * renderer and since the pixman renderer keeps a reference
		 * to the buffer anyway, there is no side effects.
		 */
		if (b->use_pixman ||
		    (ev->surface->buffer_ref.buffer &&
		    (!wl_shm_buffer_get(ev->surface->buffer_ref.buffer->resource) ||
		     (ev->surface->width <= b->cursor_width &&
		      ev->surface->height <= b->cursor_height))))
			ev->surface->keep_buffer = true;
		else
			ev->surface->keep_buffer = false;

		/* This is a bit unpleasant, but lacking a temporary place to
		 * hang a plane off the view, we have to do a nested walk.
		 * Our first-order iteration has to be planes rather than
		 * views, because otherwise we won't reset views which were
		 * previously on planes to being on the primary plane. */
		wl_list_for_each(plane_state, &state->plane_list, link) {
			if (plane_state->ev == ev) {
				plane_state->ev = NULL;
				target_plane = plane_state->plane;
				break;
			}
		}

		if (target_plane)
			weston_view_move_to_plane(ev, &target_plane->base);
		else
			weston_view_move_to_plane(ev, primary);

		if (!target_plane ||
		    target_plane->type == WDRM_PLANE_TYPE_CURSOR) {
			/* cursor plane & renderer involve a copy */
			ev->psf_flags = 0;
		} else {
			/* All other planes are a direct scanout of a
			 * single client buffer.
			 */
			ev->psf_flags = WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY;
		}
	}

	/* We rely on output->cursor_view being both an accurate reflection of
	 * the cursor plane's state, but also being maintained across repaints
	 * to avoid unnecessary damage uploads, per the comment in
	 * drm_output_prepare_cursor_view. In the event that we go from having
	 * a cursor view to not having a cursor view, we need to clear it. */
	if (output->cursor_view) {
		plane_state =
			drm_output_state_get_existing_plane(state,
							    output->cursor_plane);
		if (!plane_state || !plane_state->fb)
			output->cursor_view = NULL;
	}
}

/*
 * Get the aspect-ratio from drmModeModeInfo mode flags.
 *
 * @param drm_mode_flags- flags from drmModeModeInfo structure.
 * @returns aspect-ratio as encoded in enum 'weston_mode_aspect_ratio'.
 */
static enum weston_mode_aspect_ratio
drm_to_weston_mode_aspect_ratio(uint32_t drm_mode_flags)
{
	return (drm_mode_flags & DRM_MODE_FLAG_PIC_AR_MASK) >>
		DRM_MODE_FLAG_PIC_AR_BITS_POS;
}

static const char *
aspect_ratio_to_string(enum weston_mode_aspect_ratio ratio)
{
	if (ratio < 0 || ratio >= ARRAY_LENGTH(aspect_ratio_as_string) ||
	    !aspect_ratio_as_string[ratio])
		return " (unknown aspect ratio)";

	return aspect_ratio_as_string[ratio];
}

/**
 * Find the closest-matching mode for a given target
 *
 * Given a target mode, find the most suitable mode amongst the output's
 * current mode list to use, preferring the current mode if possible, to
 * avoid an expensive mode switch.
 *
 * @param output DRM output
 * @param target_mode Mode to attempt to match
 * @returns Pointer to a mode from the output's mode list
 */
static struct drm_mode *
choose_mode (struct drm_output *output, struct weston_mode *target_mode)
{
	struct drm_mode *tmp_mode = NULL, *mode_fall_back = NULL, *mode;
	enum weston_mode_aspect_ratio src_aspect = WESTON_MODE_PIC_AR_NONE;
	enum weston_mode_aspect_ratio target_aspect = WESTON_MODE_PIC_AR_NONE;
	struct drm_backend *b;

	b = to_drm_backend(output->base.compositor);
	target_aspect = target_mode->aspect_ratio;
	src_aspect = output->base.current_mode->aspect_ratio;
	if (output->base.current_mode->width == target_mode->width &&
	    output->base.current_mode->height == target_mode->height &&
	    (output->base.current_mode->refresh == target_mode->refresh ||
	     target_mode->refresh == 0)) {
		if (!b->aspect_ratio_supported || src_aspect == target_aspect)
			return to_drm_mode(output->base.current_mode);
	}

	wl_list_for_each(mode, &output->base.mode_list, base.link) {

		src_aspect = mode->base.aspect_ratio;
		if (mode->mode_info.hdisplay == target_mode->width &&
		    mode->mode_info.vdisplay == target_mode->height) {
			if (mode->base.refresh == target_mode->refresh ||
			    target_mode->refresh == 0) {
				if (!b->aspect_ratio_supported ||
				    src_aspect == target_aspect)
					return mode;
				else if (!mode_fall_back)
					mode_fall_back = mode;
			} else if (!tmp_mode) {
				tmp_mode = mode;
			}
		}
	}

	if (mode_fall_back)
		return mode_fall_back;

	return tmp_mode;
}

static int
drm_output_init_egl(struct drm_output *output, struct drm_backend *b);
static void
drm_output_fini_egl(struct drm_output *output);
static int
drm_output_init_pixman(struct drm_output *output, struct drm_backend *b);
static void
drm_output_fini_pixman(struct drm_output *output);

static int
drm_output_switch_mode(struct weston_output *output_base, struct weston_mode *mode)
{
	struct drm_output *output = to_drm_output(output_base);
	struct drm_backend *b = to_drm_backend(output_base->compositor);
	struct drm_mode *drm_mode = choose_mode(output, mode);

	if (!drm_mode) {
		weston_log("%s: invalid resolution %dx%d\n",
			   output_base->name, mode->width, mode->height);
		return -1;
	}

	if (&drm_mode->base == output->base.current_mode)
		return 0;

	output->base.current_mode->flags = 0;

	output->base.current_mode = &drm_mode->base;
	output->base.current_mode->flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;

	/* XXX: This drops our current buffer too early, before we've started
	 *      displaying it. Ideally this should be much more atomic and
	 *      integrated with a full repaint cycle, rather than doing a
	 *      sledgehammer modeswitch first, and only later showing new
	 *      content.
	 */
	b->state_invalid = true;

	if (b->use_pixman) {
		drm_output_fini_pixman(output);
		if (drm_output_init_pixman(output, b) < 0) {
			weston_log("failed to init output pixman state with "
				   "new mode\n");
			return -1;
		}
	} else {
		drm_output_fini_egl(output);
		if (drm_output_init_egl(output, b) < 0) {
			weston_log("failed to init output egl state with "
				   "new mode");
			return -1;
		}
	}

	return 0;
}

static int
on_drm_input(int fd, uint32_t mask, void *data)
{
#ifdef HAVE_DRM_ATOMIC
	struct drm_backend *b = data;
#endif
	drmEventContext evctx;

	memset(&evctx, 0, sizeof evctx);
#ifndef HAVE_DRM_ATOMIC
	evctx.version = 2;
#else
	evctx.version = 3;
	if (b->atomic_modeset)
		evctx.page_flip_handler2 = atomic_flip_handler;
	else
#endif
		evctx.page_flip_handler = page_flip_handler;
	evctx.vblank_handler = vblank_handler;
	drmHandleEvent(fd, &evctx);

	return 1;
}

static int
init_kms_caps(struct drm_backend *b)
{
	uint64_t cap;
	int ret;
	clockid_t clk_id;

	weston_log("using %s\n", b->drm.filename);

	ret = drmGetCap(b->drm.fd, DRM_CAP_TIMESTAMP_MONOTONIC, &cap);
	if (ret == 0 && cap == 1)
		clk_id = CLOCK_MONOTONIC;
	else
		clk_id = CLOCK_REALTIME;

	if (weston_compositor_set_presentation_clock(b->compositor, clk_id) < 0) {
		weston_log("Error: failed to set presentation clock %d.\n",
			   clk_id);
		return -1;
	}

	ret = drmGetCap(b->drm.fd, DRM_CAP_CURSOR_WIDTH, &cap);
	if (ret == 0)
		b->cursor_width = cap;
	else
		b->cursor_width = 64;

	ret = drmGetCap(b->drm.fd, DRM_CAP_CURSOR_HEIGHT, &cap);
	if (ret == 0)
		b->cursor_height = cap;
	else
		b->cursor_height = 64;

	if (!getenv("WESTON_DISABLE_UNIVERSAL_PLANES")) {
		ret = drmSetClientCap(b->drm.fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
		b->universal_planes = (ret == 0);
	}
	weston_log("DRM: %s universal planes\n",
		   b->universal_planes ? "supports" : "does not support");

#ifdef HAVE_DRM_ATOMIC
	if (b->universal_planes && !getenv("WESTON_DISABLE_ATOMIC")) {
		ret = drmGetCap(b->drm.fd, DRM_CAP_CRTC_IN_VBLANK_EVENT, &cap);
		if (ret != 0)
			cap = 0;
		ret = drmSetClientCap(b->drm.fd, DRM_CLIENT_CAP_ATOMIC, 1);
		b->atomic_modeset = ((ret == 0) && (cap == 1));
	}
#endif
	weston_log("DRM: %s atomic modesetting\n",
		   b->atomic_modeset ? "supports" : "does not support");

	/*
	 * KMS support for hardware planes cannot properly synchronize
	 * without nuclear page flip. Without nuclear/atomic, hw plane
	 * and cursor plane updates would either tear or cause extra
	 * waits for vblanks which means dropping the compositor framerate
	 * to a fraction. For cursors, it's not so bad, so they are
	 * enabled.
	 */
	if (!b->atomic_modeset)
		b->sprites_are_broken = 1;

	ret = drmSetClientCap(b->drm.fd, DRM_CLIENT_CAP_ASPECT_RATIO, 1);
	b->aspect_ratio_supported = (ret == 0);
	weston_log("DRM: %s picture aspect ratio\n",
		   b->aspect_ratio_supported ? "supports" : "does not support");

	return 0;
}

static struct gbm_device *
create_gbm_device(int fd)
{
	struct gbm_device *gbm;

	gl_renderer = weston_load_module("gl-renderer.so",
					 "gl_renderer_interface");
	if (!gl_renderer)
		return NULL;

	/* GBM will load a dri driver, but even though they need symbols from
	 * libglapi, in some version of Mesa they are not linked to it. Since
	 * only the gl-renderer module links to it, the call above won't make
	 * these symbols globally available, and loading the DRI driver fails.
	 * Workaround this by dlopen()'ing libglapi with RTLD_GLOBAL. */
	dlopen("libglapi.so.0", RTLD_LAZY | RTLD_GLOBAL);

	gbm = gbm_create_device(fd);

	return gbm;
}

/* When initializing EGL, if the preferred buffer format isn't available
 * we may be able to substitute an ARGB format for an XRGB one.
 *
 * This returns 0 if substitution isn't possible, but 0 might be a
 * legitimate format for other EGL platforms, so the caller is
 * responsible for checking for 0 before calling gl_renderer->create().
 *
 * This works around https://bugs.freedesktop.org/show_bug.cgi?id=89689
 * but it's entirely possible we'll see this again on other implementations.
 */
static int
fallback_format_for(uint32_t format)
{
	switch (format) {
	case GBM_FORMAT_XRGB8888:
		return GBM_FORMAT_ARGB8888;
	case GBM_FORMAT_XRGB2101010:
		return GBM_FORMAT_ARGB2101010;
	default:
		return 0;
	}
}

static int
drm_backend_create_gl_renderer(struct drm_backend *b)
{
	EGLint format[3] = {
		b->gbm_format,
		fallback_format_for(b->gbm_format),
		0,
	};
	int n_formats = 2;

	if (format[1])
		n_formats = 3;
	if (gl_renderer->display_create(b->compositor,
					EGL_PLATFORM_GBM_KHR,
					(void *)b->gbm,
					NULL,
					gl_renderer->opaque_attribs,
					format,
					n_formats) < 0) {
		return -1;
	}

	return 0;
}

static int
init_egl(struct drm_backend *b)
{
	b->gbm = create_gbm_device(b->drm.fd);

	if (!b->gbm)
		return -1;

	if (drm_backend_create_gl_renderer(b) < 0) {
		gbm_device_destroy(b->gbm);
		return -1;
	}

	return 0;
}

static int
init_pixman(struct drm_backend *b)
{
	return pixman_renderer_init(b->compositor);
}

#ifdef HAVE_DRM_FORMATS_BLOB
static inline uint32_t *
formats_ptr(struct drm_format_modifier_blob *blob)
{
	return (uint32_t *)(((char *)blob) + blob->formats_offset);
}

static inline struct drm_format_modifier *
modifiers_ptr(struct drm_format_modifier_blob *blob)
{
	return (struct drm_format_modifier *)
		(((char *)blob) + blob->modifiers_offset);
}
#endif

/**
 * Populates the plane's formats array, using either the IN_FORMATS blob
 * property (if available), or the plane's format list if not.
 */
static int
drm_plane_populate_formats(struct drm_plane *plane, const drmModePlane *kplane,
			   const drmModeObjectProperties *props)
{
	unsigned i;
#ifdef HAVE_DRM_FORMATS_BLOB
	drmModePropertyBlobRes *blob;
	struct drm_format_modifier_blob *fmt_mod_blob;
	struct drm_format_modifier *blob_modifiers;
	uint32_t *blob_formats;
	uint32_t blob_id;

	blob_id = drm_property_get_value(&plane->props[WDRM_PLANE_IN_FORMATS],
				         props,
				         0);
	if (blob_id == 0)
		goto fallback;

	blob = drmModeGetPropertyBlob(plane->backend->drm.fd, blob_id);
	if (!blob)
		goto fallback;

	fmt_mod_blob = blob->data;
	blob_formats = formats_ptr(fmt_mod_blob);
	blob_modifiers = modifiers_ptr(fmt_mod_blob);

	if (plane->count_formats != fmt_mod_blob->count_formats) {
		weston_log("DRM backend: format count differs between "
		           "plane (%d) and IN_FORMATS (%d)\n",
			   plane->count_formats,
			   fmt_mod_blob->count_formats);
		weston_log("This represents a kernel bug; Weston is "
			   "unable to continue.\n");
		abort();
	}

	for (i = 0; i < fmt_mod_blob->count_formats; i++) {
		uint32_t count_modifiers = 0;
		uint64_t *modifiers = NULL;
		unsigned j;

		for (j = 0; j < fmt_mod_blob->count_modifiers; j++) {
			struct drm_format_modifier *mod = &blob_modifiers[j];

			if ((i < mod->offset) || (i > mod->offset + 63))
				continue;
			if (!(mod->formats & (1 << (i - mod->offset))))
				continue;

			modifiers = realloc(modifiers,
					    (count_modifiers + 1) *
					     sizeof(modifiers[0]));
			assert(modifiers);
			modifiers[count_modifiers++] = mod->modifier;
		}

		plane->formats[i].format = blob_formats[i];
		plane->formats[i].modifiers = modifiers;
		plane->formats[i].count_modifiers = count_modifiers;
	}

	drmModeFreePropertyBlob(blob);

	return 0;

fallback:
#endif
	/* No IN_FORMATS blob available, so just use the old. */
	assert(plane->count_formats == kplane->count_formats);
	for (i = 0; i < kplane->count_formats; i++)
		plane->formats[i].format = kplane->formats[i];

	return 0;
}

/**
 * Create a drm_plane for a hardware plane
 *
 * Creates one drm_plane structure for a hardware plane, and initialises its
 * properties and formats.
 *
 * In the absence of universal plane support, where KMS does not explicitly
 * expose the primary and cursor planes to userspace, this may also create
 * an 'internal' plane for internal management.
 *
 * This function does not add the plane to the list of usable planes in Weston
 * itself; the caller is responsible for this.
 *
 * Call drm_plane_destroy to clean up the plane.
 *
 * @sa drm_output_find_special_plane
 * @param b DRM compositor backend
 * @param kplane DRM plane to create, or NULL if creating internal plane
 * @param output Output to create internal plane for, or NULL
 * @param type Type to use when creating internal plane, or invalid
 * @param format Format to use for internal planes, or 0
 */
static struct drm_plane *
drm_plane_create(struct drm_backend *b, const drmModePlane *kplane,
		 struct drm_output *output, enum wdrm_plane_type type,
		 uint32_t format)
{
	struct drm_plane *plane;
	drmModeObjectProperties *props;
	uint32_t num_formats = (kplane) ? kplane->count_formats : 1;

	plane = zalloc(sizeof(*plane) +
		       (sizeof(plane->formats[0]) * num_formats));
	if (!plane) {
		weston_log("%s: out of memory\n", __func__);
		return NULL;
	}

	plane->backend = b;
	plane->count_formats = num_formats;
	plane->state_cur = drm_plane_state_alloc(NULL, plane);
	plane->state_cur->complete = true;

	if (kplane) {
		plane->possible_crtcs = kplane->possible_crtcs;
		plane->plane_id = kplane->plane_id;

		props = drmModeObjectGetProperties(b->drm.fd, kplane->plane_id,
						   DRM_MODE_OBJECT_PLANE);
		if (!props) {
			weston_log("couldn't get plane properties\n");
			goto err;
		}
		drm_property_info_populate(b, plane_props, plane->props,
					   WDRM_PLANE__COUNT, props);
		plane->type =
			drm_property_get_value(&plane->props[WDRM_PLANE_TYPE],
					       props,
					       WDRM_PLANE_TYPE__COUNT);

		if (drm_plane_populate_formats(plane, kplane, props) < 0) {
			drmModeFreeObjectProperties(props);
			goto err;
		}

		drmModeFreeObjectProperties(props);
	}
	else {
		plane->possible_crtcs = (1 << output->pipe);
		plane->plane_id = 0;
		plane->count_formats = 1;
		plane->formats[0].format = format;
		plane->type = type;
	}

	if (plane->type == WDRM_PLANE_TYPE__COUNT)
		goto err_props;

	/* With universal planes, everything is a DRM plane; without
	 * universal planes, the only DRM planes are overlay planes.
	 * Everything else is a fake plane. */
	if (b->universal_planes) {
		assert(kplane);
	} else {
		if (kplane)
			assert(plane->type == WDRM_PLANE_TYPE_OVERLAY);
		else
			assert(plane->type != WDRM_PLANE_TYPE_OVERLAY &&
			       output);
	}

	weston_plane_init(&plane->base, b->compositor, 0, 0);
	wl_list_insert(&b->plane_list, &plane->link);

	return plane;

err_props:
	drm_property_info_free(plane->props, WDRM_PLANE__COUNT);
err:
	drm_plane_state_free(plane->state_cur, true);
	free(plane);
	return NULL;
}

/**
 * Find, or create, a special-purpose plane
 *
 * Primary and cursor planes are a special case, in that before universal
 * planes, they are driven by non-plane API calls. Without universal plane
 * support, the only way to configure a primary plane is via drmModeSetCrtc,
 * and the only way to configure a cursor plane is drmModeSetCursor2.
 *
 * Although they may actually be regular planes in the hardware, without
 * universal plane support, these planes are not actually exposed to
 * userspace in the regular plane list.
 *
 * However, for ease of internal tracking, we want to manage all planes
 * through the same drm_plane structures. Therefore, when we are running
 * without universal plane support, we create fake drm_plane structures
 * to track these planes.
 *
 * @param b DRM backend
 * @param output Output to use for plane
 * @param type Type of plane
 */
static struct drm_plane *
drm_output_find_special_plane(struct drm_backend *b, struct drm_output *output,
			      enum wdrm_plane_type type)
{
	struct drm_plane *plane;

	if (!b->universal_planes) {
		uint32_t format;

		switch (type) {
		case WDRM_PLANE_TYPE_CURSOR:
			format = GBM_FORMAT_ARGB8888;
			break;
		case WDRM_PLANE_TYPE_PRIMARY:
			/* We don't know what formats the primary plane supports
			 * before universal planes, so we just assume that the
			 * GBM format works; however, this isn't set until after
			 * the output is created. */
			format = 0;
			break;
		default:
			assert(!"invalid type in drm_output_find_special_plane");
			break;
		}

		return drm_plane_create(b, NULL, output, type, format);
	}

	wl_list_for_each(plane, &b->plane_list, link) {
		struct drm_output *tmp;
		bool found_elsewhere = false;

		if (plane->type != type)
			continue;
		if (!drm_plane_is_available(plane, output))
			continue;

		/* On some platforms, primary/cursor planes can roam
		 * between different CRTCs, so make sure we don't claim the
		 * same plane for two outputs. */
		wl_list_for_each(tmp, &b->compositor->output_list,
				 base.link) {
			if (tmp->cursor_plane == plane ||
			    tmp->scanout_plane == plane) {
				found_elsewhere = true;
				break;
			}
		}

		if (found_elsewhere)
			continue;

		plane->possible_crtcs = (1 << output->pipe);
		return plane;
	}

	return NULL;
}

/**
 * Destroy one DRM plane
 *
 * Destroy a DRM plane, removing it from screen and releasing its retained
 * buffers in the process. The counterpart to drm_plane_create.
 *
 * @param plane Plane to deallocate (will be freed)
 */
static void
drm_plane_destroy(struct drm_plane *plane)
{
	if (plane->type == WDRM_PLANE_TYPE_OVERLAY)
		drmModeSetPlane(plane->backend->drm.fd, plane->plane_id,
				0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	drm_plane_state_free(plane->state_cur, true);
	drm_property_info_free(plane->props, WDRM_PLANE__COUNT);
	weston_plane_release(&plane->base);
	wl_list_remove(&plane->link);
	free(plane);
}

/**
 * Initialise sprites (overlay planes)
 *
 * Walk the list of provided DRM planes, and add overlay planes.
 *
 * Call destroy_sprites to free these planes.
 *
 * @param b DRM compositor backend
 */
static void
create_sprites(struct drm_backend *b)
{
	drmModePlaneRes *kplane_res;
	drmModePlane *kplane;
	struct drm_plane *drm_plane;
	uint32_t i;
	kplane_res = drmModeGetPlaneResources(b->drm.fd);
	if (!kplane_res) {
		weston_log("failed to get plane resources: %s\n",
			strerror(errno));
		return;
	}

	for (i = 0; i < kplane_res->count_planes; i++) {
		kplane = drmModeGetPlane(b->drm.fd, kplane_res->planes[i]);
		if (!kplane)
			continue;

		drm_plane = drm_plane_create(b, kplane, NULL,
		                             WDRM_PLANE_TYPE__COUNT, 0);
		drmModeFreePlane(kplane);
		if (!drm_plane)
			continue;

		if (drm_plane->type == WDRM_PLANE_TYPE_OVERLAY)
			weston_compositor_stack_plane(b->compositor,
						      &drm_plane->base,
						      &b->compositor->primary_plane);
	}

	drmModeFreePlaneResources(kplane_res);
}

/**
 * Clean up sprites (overlay planes)
 *
 * The counterpart to create_sprites.
 *
 * @param b DRM compositor backend
 */
static void
destroy_sprites(struct drm_backend *b)
{
	struct drm_plane *plane, *next;

	wl_list_for_each_safe(plane, next, &b->plane_list, link)
		drm_plane_destroy(plane);
}

static uint32_t
drm_refresh_rate_mHz(const drmModeModeInfo *info)
{
	uint64_t refresh;

	/* Calculate higher precision (mHz) refresh rate */
	refresh = (info->clock * 1000000LL / info->htotal +
		   info->vtotal / 2) / info->vtotal;

	if (info->flags & DRM_MODE_FLAG_INTERLACE)
		refresh *= 2;
	if (info->flags & DRM_MODE_FLAG_DBLSCAN)
		refresh /= 2;
	if (info->vscan > 1)
	    refresh /= info->vscan;

	return refresh;
}

/**
 * Add a mode to output's mode list
 *
 * Copy the supplied DRM mode into a Weston mode structure, and add it to the
 * output's mode list.
 *
 * @param output DRM output to add mode to
 * @param info DRM mode structure to add
 * @returns Newly-allocated Weston/DRM mode structure
 */
static struct drm_mode *
drm_output_add_mode(struct drm_output *output, const drmModeModeInfo *info)
{
	struct drm_mode *mode;

	mode = malloc(sizeof *mode);
	if (mode == NULL)
		return NULL;

	mode->base.flags = 0;
	mode->base.width = info->hdisplay;
	mode->base.height = info->vdisplay;

	mode->base.refresh = drm_refresh_rate_mHz(info);
	mode->mode_info = *info;
	mode->blob_id = 0;

	if (info->type & DRM_MODE_TYPE_PREFERRED)
		mode->base.flags |= WL_OUTPUT_MODE_PREFERRED;

	mode->base.aspect_ratio = drm_to_weston_mode_aspect_ratio(info->flags);

	wl_list_insert(output->base.mode_list.prev, &mode->base.link);

	return mode;
}

/**
 * Destroys a mode, and removes it from the list.
 */
static void
drm_output_destroy_mode(struct drm_backend *backend, struct drm_mode *mode)
{
	if (mode->blob_id)
		drmModeDestroyPropertyBlob(backend->drm.fd, mode->blob_id);
	wl_list_remove(&mode->base.link);
	free(mode);
}

/** Destroy a list of drm_modes
 *
 * @param backend The backend for releasing mode property blobs.
 * @param mode_list The list linked by drm_mode::base.link.
 */
static void
drm_mode_list_destroy(struct drm_backend *backend, struct wl_list *mode_list)
{
	struct drm_mode *mode, *next;

	wl_list_for_each_safe(mode, next, mode_list, base.link)
		drm_output_destroy_mode(backend, mode);
}

static int
drm_subpixel_to_wayland(int drm_value)
{
	switch (drm_value) {
	default:
	case DRM_MODE_SUBPIXEL_UNKNOWN:
		return WL_OUTPUT_SUBPIXEL_UNKNOWN;
	case DRM_MODE_SUBPIXEL_NONE:
		return WL_OUTPUT_SUBPIXEL_NONE;
	case DRM_MODE_SUBPIXEL_HORIZONTAL_RGB:
		return WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB;
	case DRM_MODE_SUBPIXEL_HORIZONTAL_BGR:
		return WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR;
	case DRM_MODE_SUBPIXEL_VERTICAL_RGB:
		return WL_OUTPUT_SUBPIXEL_VERTICAL_RGB;
	case DRM_MODE_SUBPIXEL_VERTICAL_BGR:
		return WL_OUTPUT_SUBPIXEL_VERTICAL_BGR;
	}
}

/* returns a value between 0-255 range, where higher is brighter */
static uint32_t
drm_get_backlight(struct drm_head *head)
{
	long brightness, max_brightness, norm;

	brightness = backlight_get_brightness(head->backlight);
	max_brightness = backlight_get_max_brightness(head->backlight);

	/* convert it on a scale of 0 to 255 */
	norm = (brightness * 255)/(max_brightness);

	return (uint32_t) norm;
}

/* values accepted are between 0-255 range */
static void
drm_set_backlight(struct weston_output *output_base, uint32_t value)
{
	struct drm_output *output = to_drm_output(output_base);
	struct drm_head *head;
	long max_brightness, new_brightness;

	if (value > 255)
		return;

	wl_list_for_each(head, &output->base.head_list, base.output_link) {
		if (!head->backlight)
			return;

		max_brightness = backlight_get_max_brightness(head->backlight);

		/* get denormalized value */
		new_brightness = (value * max_brightness) / 255;

		backlight_set_brightness(head->backlight, new_brightness);
	}
}

static void
drm_output_init_backlight(struct drm_output *output)
{
	struct weston_head *base;
	struct drm_head *head;

	output->base.set_backlight = NULL;

	wl_list_for_each(base, &output->base.head_list, output_link) {
		head = to_drm_head(base);

		if (head->backlight) {
			weston_log("Initialized backlight for head '%s', device %s\n",
				   head->base.name, head->backlight->path);

			if (!output->base.set_backlight) {
				output->base.set_backlight = drm_set_backlight;
				output->base.backlight_current =
							drm_get_backlight(head);
			}
		}
	}

	if (!output->base.set_backlight) {
		weston_log("No backlight control for output '%s'\n",
			   output->base.name);
	}
}

/**
 * Power output on or off
 *
 * The DPMS/power level of an output is used to switch it on or off. This
 * is DRM's hook for doing so, which can called either as part of repaint,
 * or independently of the repaint loop.
 *
 * If we are called as part of repaint, we simply set the relevant bit in
 * state and return.
 */
static void
drm_set_dpms(struct weston_output *output_base, enum dpms_enum level)
{
	struct drm_output *output = to_drm_output(output_base);
	struct drm_backend *b = to_drm_backend(output_base->compositor);
	struct drm_pending_state *pending_state = b->repaint_data;
	struct drm_output_state *state;
	int ret;

	if (output->state_cur->dpms == level)
		return;

	/* If we're being called during the repaint loop, then this is
	 * simple: discard any previously-generated state, and create a new
	 * state where we disable everything. When we come to flush, this
	 * will be applied.
	 *
	 * However, we need to be careful: we can be called whilst another
	 * output is in its repaint cycle (pending_state exists), but our
	 * output still has an incomplete state application outstanding.
	 * In that case, we need to wait until that completes. */
	if (pending_state && !output->state_last) {
		/* The repaint loop already sets DPMS on; we don't need to
		 * explicitly set it on here, as it will already happen
		 * whilst applying the repaint state. */
		if (level == WESTON_DPMS_ON)
			return;

		state = drm_pending_state_get_output(pending_state, output);
		if (state)
			drm_output_state_free(state);
		state = drm_output_get_disable_state(pending_state, output);
		return;
	}

	/* As we throw everything away when disabling, just send us back through
	 * a repaint cycle. */
	if (level == WESTON_DPMS_ON) {
		if (output->dpms_off_pending)
			output->dpms_off_pending = 0;
		weston_output_schedule_repaint(output_base);
		return;
	}

	/* If we've already got a request in the pipeline, then we need to
	 * park our DPMS request until that request has quiesced. */
	if (output->state_last) {
		output->dpms_off_pending = 1;
		return;
	}

	pending_state = drm_pending_state_alloc(b);
	drm_output_get_disable_state(pending_state, output);
	ret = drm_pending_state_apply_sync(pending_state);
	if (ret != 0)
		weston_log("drm_set_dpms: couldn't disable output?\n");
}

static const char * const connector_type_names[] = {
	[DRM_MODE_CONNECTOR_Unknown]     = "Unknown",
	[DRM_MODE_CONNECTOR_VGA]         = "VGA",
	[DRM_MODE_CONNECTOR_DVII]        = "DVI-I",
	[DRM_MODE_CONNECTOR_DVID]        = "DVI-D",
	[DRM_MODE_CONNECTOR_DVIA]        = "DVI-A",
	[DRM_MODE_CONNECTOR_Composite]   = "Composite",
	[DRM_MODE_CONNECTOR_SVIDEO]      = "SVIDEO",
	[DRM_MODE_CONNECTOR_LVDS]        = "LVDS",
	[DRM_MODE_CONNECTOR_Component]   = "Component",
	[DRM_MODE_CONNECTOR_9PinDIN]     = "DIN",
	[DRM_MODE_CONNECTOR_DisplayPort] = "DP",
	[DRM_MODE_CONNECTOR_HDMIA]       = "HDMI-A",
	[DRM_MODE_CONNECTOR_HDMIB]       = "HDMI-B",
	[DRM_MODE_CONNECTOR_TV]          = "TV",
	[DRM_MODE_CONNECTOR_eDP]         = "eDP",
#ifdef DRM_MODE_CONNECTOR_DSI
	[DRM_MODE_CONNECTOR_VIRTUAL]     = "Virtual",
	[DRM_MODE_CONNECTOR_DSI]         = "DSI",
#endif
#ifdef DRM_MODE_CONNECTOR_DPI
	[DRM_MODE_CONNECTOR_DPI]         = "DPI",
#endif
};

/** Create a name given a DRM connector
 *
 * \param con The DRM connector whose type and id form the name.
 * \return A newly allocate string, or NULL on error. Must be free()'d
 * after use.
 *
 * The name does not identify the DRM display device.
 */
static char *
make_connector_name(const drmModeConnector *con)
{
	char *name;
	const char *type_name = NULL;
	int ret;

	if (con->connector_type < ARRAY_LENGTH(connector_type_names))
		type_name = connector_type_names[con->connector_type];

	if (!type_name)
		type_name = "UNNAMED";

	ret = asprintf(&name, "%s-%d", type_name, con->connector_type_id);
	if (ret < 0)
		return NULL;

	return name;
}

static void drm_output_fini_cursor_egl(struct drm_output *output)
{
	unsigned int i;

	for (i = 0; i < ARRAY_LENGTH(output->gbm_cursor_fb); i++) {
		drm_fb_unref(output->gbm_cursor_fb[i]);
		output->gbm_cursor_fb[i] = NULL;
	}
}

static int
drm_output_init_cursor_egl(struct drm_output *output, struct drm_backend *b)
{
	unsigned int i;

	/* No point creating cursors if we don't have a plane for them. */
	if (!output->cursor_plane)
		return 0;

	for (i = 0; i < ARRAY_LENGTH(output->gbm_cursor_fb); i++) {
		struct gbm_bo *bo;

		bo = gbm_bo_create(b->gbm, b->cursor_width, b->cursor_height,
				   GBM_FORMAT_ARGB8888,
				   GBM_BO_USE_CURSOR | GBM_BO_USE_WRITE);
		if (!bo)
			goto err;

		output->gbm_cursor_fb[i] =
			drm_fb_get_from_bo(bo, b, false, BUFFER_CURSOR);
		if (!output->gbm_cursor_fb[i]) {
			gbm_bo_destroy(bo);
			goto err;
		}
	}

	return 0;

err:
	weston_log("cursor buffers unavailable, using gl cursors\n");
	b->cursors_are_broken = 1;
	drm_output_fini_cursor_egl(output);
	return -1;
}

/* Init output state that depends on gl or gbm */
static int
drm_output_init_egl(struct drm_output *output, struct drm_backend *b)
{
	EGLint format[2] = {
		output->gbm_format,
		fallback_format_for(output->gbm_format),
	};
	int n_formats = 1;
	struct weston_mode *mode = output->base.current_mode;
	struct drm_plane *plane = output->scanout_plane;
	unsigned int i;

	for (i = 0; i < plane->count_formats; i++) {
		if (plane->formats[i].format == output->gbm_format)
			break;
	}

	if (i == plane->count_formats) {
		weston_log("format 0x%x not supported by output %s\n",
			   output->gbm_format, output->base.name);
		return -1;
	}

#ifdef HAVE_GBM_MODIFIERS
	if (plane->formats[i].count_modifiers > 0) {
		output->gbm_surface =
			gbm_surface_create_with_modifiers(b->gbm,
							  mode->width,
							  mode->height,
							  output->gbm_format,
							  plane->formats[i].modifiers,
							  plane->formats[i].count_modifiers);
	} else
#endif
	{
		output->gbm_surface =
		    gbm_surface_create(b->gbm, mode->width, mode->height,
				       output->gbm_format,
				       GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT);
	}

	if (!output->gbm_surface) {
		weston_log("failed to create gbm surface\n");
		return -1;
	}

	if (format[1])
		n_formats = 2;
	if (gl_renderer->output_window_create(&output->base,
					      (EGLNativeWindowType)output->gbm_surface,
					      output->gbm_surface,
					      gl_renderer->opaque_attribs,
					      format,
					      n_formats) < 0) {
		weston_log("failed to create gl renderer output state\n");
		gbm_surface_destroy(output->gbm_surface);
		return -1;
	}

	drm_output_init_cursor_egl(output, b);

	return 0;
}

static void
drm_output_fini_egl(struct drm_output *output)
{
	struct drm_backend *b = to_drm_backend(output->base.compositor);

	/* Destroying the GBM surface will destroy all our GBM buffers,
	 * regardless of refcount. Ensure we destroy them here. */
	if (!b->shutting_down &&
	    output->scanout_plane->state_cur->fb &&
	    output->scanout_plane->state_cur->fb->type == BUFFER_GBM_SURFACE) {
		drm_plane_state_free(output->scanout_plane->state_cur, true);
		output->scanout_plane->state_cur =
			drm_plane_state_alloc(NULL, output->scanout_plane);
		output->scanout_plane->state_cur->complete = true;
	}

	gl_renderer->output_destroy(&output->base);
	gbm_surface_destroy(output->gbm_surface);
	drm_output_fini_cursor_egl(output);
}

static int
drm_output_init_pixman(struct drm_output *output, struct drm_backend *b)
{
	int w = output->base.current_mode->width;
	int h = output->base.current_mode->height;
	uint32_t format = output->gbm_format;
	uint32_t pixman_format;
	unsigned int i;
	uint32_t flags = 0;

	switch (format) {
		case GBM_FORMAT_XRGB8888:
			pixman_format = PIXMAN_x8r8g8b8;
			break;
		case GBM_FORMAT_RGB565:
			pixman_format = PIXMAN_r5g6b5;
			break;
		default:
			weston_log("Unsupported pixman format 0x%x\n", format);
			return -1;
	}

	/* FIXME error checking */
	for (i = 0; i < ARRAY_LENGTH(output->dumb); i++) {
		output->dumb[i] = drm_fb_create_dumb(b, w, h, format);
		if (!output->dumb[i])
			goto err;

		output->image[i] =
			pixman_image_create_bits(pixman_format, w, h,
						 output->dumb[i]->map,
						 output->dumb[i]->strides[0]);
		if (!output->image[i])
			goto err;
	}

	if (b->use_pixman_shadow)
		flags |= PIXMAN_RENDERER_OUTPUT_USE_SHADOW;

	if (pixman_renderer_output_create(&output->base, flags) < 0)
 		goto err;

	weston_log("DRM: output %s %s shadow framebuffer.\n", output->base.name,
		   b->use_pixman_shadow ? "uses" : "does not use");

	pixman_region32_init_rect(&output->previous_damage,
				  output->base.x, output->base.y, output->base.width, output->base.height);

	return 0;

err:
	for (i = 0; i < ARRAY_LENGTH(output->dumb); i++) {
		if (output->dumb[i])
			drm_fb_unref(output->dumb[i]);
		if (output->image[i])
			pixman_image_unref(output->image[i]);

		output->dumb[i] = NULL;
		output->image[i] = NULL;
	}

	return -1;
}

static void
drm_output_fini_pixman(struct drm_output *output)
{
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	unsigned int i;

	/* Destroying the Pixman surface will destroy all our buffers,
	 * regardless of refcount. Ensure we destroy them here. */
	if (!b->shutting_down &&
	    output->scanout_plane->state_cur->fb &&
	    output->scanout_plane->state_cur->fb->type == BUFFER_PIXMAN_DUMB) {
		drm_plane_state_free(output->scanout_plane->state_cur, true);
		output->scanout_plane->state_cur =
			drm_plane_state_alloc(NULL, output->scanout_plane);
		output->scanout_plane->state_cur->complete = true;
	}

	pixman_renderer_output_destroy(&output->base);
	pixman_region32_fini(&output->previous_damage);

	for (i = 0; i < ARRAY_LENGTH(output->dumb); i++) {
		pixman_image_unref(output->image[i]);
		drm_fb_unref(output->dumb[i]);
		output->dumb[i] = NULL;
		output->image[i] = NULL;
	}
}

static void
edid_parse_string(const uint8_t *data, char text[])
{
	int i;
	int replaced = 0;

	/* this is always 12 bytes, but we can't guarantee it's null
	 * terminated or not junk. */
	strncpy(text, (const char *) data, 12);

	/* guarantee our new string is null-terminated */
	text[12] = '\0';

	/* remove insane chars */
	for (i = 0; text[i] != '\0'; i++) {
		if (text[i] == '\n' ||
		    text[i] == '\r') {
			text[i] = '\0';
			break;
		}
	}

	/* ensure string is printable */
	for (i = 0; text[i] != '\0'; i++) {
		if (!isprint(text[i])) {
			text[i] = '-';
			replaced++;
		}
	}

	/* if the string is random junk, ignore the string */
	if (replaced > 4)
		text[0] = '\0';
}

#define EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING	0xfe
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME		0xfc
#define EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER	0xff
#define EDID_OFFSET_DATA_BLOCKS				0x36
#define EDID_OFFSET_LAST_BLOCK				0x6c
#define EDID_OFFSET_PNPID				0x08
#define EDID_OFFSET_SERIAL				0x0c

static int
edid_parse(struct drm_edid *edid, const uint8_t *data, size_t length)
{
	int i;
	uint32_t serial_number;

	/* check header */
	if (length < 128)
		return -1;
	if (data[0] != 0x00 || data[1] != 0xff)
		return -1;

	/* decode the PNP ID from three 5 bit words packed into 2 bytes
	 * /--08--\/--09--\
	 * 7654321076543210
	 * |\---/\---/\---/
	 * R  C1   C2   C3 */
	edid->pnp_id[0] = 'A' + ((data[EDID_OFFSET_PNPID + 0] & 0x7c) / 4) - 1;
	edid->pnp_id[1] = 'A' + ((data[EDID_OFFSET_PNPID + 0] & 0x3) * 8) + ((data[EDID_OFFSET_PNPID + 1] & 0xe0) / 32) - 1;
	edid->pnp_id[2] = 'A' + (data[EDID_OFFSET_PNPID + 1] & 0x1f) - 1;
	edid->pnp_id[3] = '\0';

	/* maybe there isn't a ASCII serial number descriptor, so use this instead */
	serial_number = (uint32_t) data[EDID_OFFSET_SERIAL + 0];
	serial_number += (uint32_t) data[EDID_OFFSET_SERIAL + 1] * 0x100;
	serial_number += (uint32_t) data[EDID_OFFSET_SERIAL + 2] * 0x10000;
	serial_number += (uint32_t) data[EDID_OFFSET_SERIAL + 3] * 0x1000000;
	if (serial_number > 0)
		sprintf(edid->serial_number, "%lu", (unsigned long) serial_number);

	/* parse EDID data */
	for (i = EDID_OFFSET_DATA_BLOCKS;
	     i <= EDID_OFFSET_LAST_BLOCK;
	     i += 18) {
		/* ignore pixel clock data */
		if (data[i] != 0)
			continue;
		if (data[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (data[i+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
			edid_parse_string(&data[i+5],
					  edid->monitor_name);
		} else if (data[i+3] == EDID_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
			edid_parse_string(&data[i+5],
					  edid->serial_number);
		} else if (data[i+3] == EDID_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
			edid_parse_string(&data[i+5],
					  edid->eisa_id);
		}
	}
	return 0;
}

/** Parse monitor make, model and serial from EDID
 *
 * \param head The head whose \c drm_edid to fill in.
 * \param props The DRM connector properties to get the EDID from.
 * \param make[out] The monitor make (PNP ID).
 * \param model[out] The monitor model (name).
 * \param serial_number[out] The monitor serial number.
 *
 * Each of \c *make, \c *model and \c *serial_number are set only if the
 * information is found in the EDID. The pointers they are set to must not
 * be free()'d explicitly, instead they get implicitly freed when the
 * \c drm_head is destroyed.
 */
static void
find_and_parse_output_edid(struct drm_head *head,
			   drmModeObjectPropertiesPtr props,
			   const char **make,
			   const char **model,
			   const char **serial_number)
{
	drmModePropertyBlobPtr edid_blob = NULL;
	uint32_t blob_id;
	int rc;

	blob_id =
		drm_property_get_value(&head->props_conn[WDRM_CONNECTOR_EDID],
				       props, 0);
	if (!blob_id)
		return;

	edid_blob = drmModeGetPropertyBlob(head->backend->drm.fd, blob_id);
	if (!edid_blob)
		return;

	rc = edid_parse(&head->edid,
			edid_blob->data,
			edid_blob->length);
	if (!rc) {
		if (head->edid.pnp_id[0] != '\0')
			*make = head->edid.pnp_id;
		if (head->edid.monitor_name[0] != '\0')
			*model = head->edid.monitor_name;
		if (head->edid.serial_number[0] != '\0')
			*serial_number = head->edid.serial_number;
	}
	drmModeFreePropertyBlob(edid_blob);
}

static int
parse_modeline(const char *s, drmModeModeInfo *mode)
{
	char hsync[16];
	char vsync[16];
	float fclock;

	memset(mode, 0, sizeof *mode);

	mode->type = DRM_MODE_TYPE_USERDEF;
	mode->hskew = 0;
	mode->vscan = 0;
	mode->vrefresh = 0;
	mode->flags = 0;

	if (sscanf(s, "%f %hd %hd %hd %hd %hd %hd %hd %hd %15s %15s",
		   &fclock,
		   &mode->hdisplay,
		   &mode->hsync_start,
		   &mode->hsync_end,
		   &mode->htotal,
		   &mode->vdisplay,
		   &mode->vsync_start,
		   &mode->vsync_end,
		   &mode->vtotal, hsync, vsync) != 11)
		return -1;

	mode->clock = fclock * 1000;
	if (strcasecmp(hsync, "+hsync") == 0)
		mode->flags |= DRM_MODE_FLAG_PHSYNC;
	else if (strcasecmp(hsync, "-hsync") == 0)
		mode->flags |= DRM_MODE_FLAG_NHSYNC;
	else
		return -1;

	if (strcasecmp(vsync, "+vsync") == 0)
		mode->flags |= DRM_MODE_FLAG_PVSYNC;
	else if (strcasecmp(vsync, "-vsync") == 0)
		mode->flags |= DRM_MODE_FLAG_NVSYNC;
	else
		return -1;

	snprintf(mode->name, sizeof mode->name, "%dx%d@%.3f",
		 mode->hdisplay, mode->vdisplay, fclock);

	return 0;
}

static void
setup_output_seat_constraint(struct drm_backend *b,
			     struct weston_output *output,
			     const char *s)
{
	if (strcmp(s, "") != 0) {
		struct weston_pointer *pointer;
		struct udev_seat *seat;

		seat = udev_seat_get_named(&b->input, s);
		if (!seat)
			return;

		seat->base.output = output;

		pointer = weston_seat_get_pointer(&seat->base);
		if (pointer)
			weston_pointer_clamp(pointer,
					     &pointer->x,
					     &pointer->y);
	}
}

static int
drm_output_attach_head(struct weston_output *output_base,
		       struct weston_head *head_base)
{
	struct drm_backend *b = to_drm_backend(output_base->compositor);

	if (wl_list_length(&output_base->head_list) >= MAX_CLONED_CONNECTORS)
		return -1;

	if (!output_base->enabled)
		return 0;

	/* XXX: ensure the configuration will work.
	 * This is actually impossible without major infrastructure
	 * work. */

	/* Need to go through modeset to add connectors. */
	/* XXX: Ideally we'd do this per-output, not globally. */
	/* XXX: Doing it globally, what guarantees another output's update
	 * will not clear the flag before this output is updated?
	 */
	b->state_invalid = true;

	weston_output_schedule_repaint(output_base);

	return 0;
}

static void
drm_output_detach_head(struct weston_output *output_base,
		       struct weston_head *head_base)
{
	struct drm_backend *b = to_drm_backend(output_base->compositor);

	if (!output_base->enabled)
		return;

	/* Need to go through modeset to drop connectors that should no longer
	 * be driven. */
	/* XXX: Ideally we'd do this per-output, not globally. */
	b->state_invalid = true;

	weston_output_schedule_repaint(output_base);
}

static int
parse_gbm_format(const char *s, uint32_t default_value, uint32_t *gbm_format)
{
	int ret = 0;

	if (s == NULL)
		*gbm_format = default_value;
	else if (strcmp(s, "xrgb8888") == 0)
		*gbm_format = GBM_FORMAT_XRGB8888;
	else if (strcmp(s, "rgb565") == 0)
		*gbm_format = GBM_FORMAT_RGB565;
	else if (strcmp(s, "xrgb2101010") == 0)
		*gbm_format = GBM_FORMAT_XRGB2101010;
	else {
		weston_log("fatal: unrecognized pixel format: %s\n", s);
		ret = -1;
	}

	return ret;
}

static uint32_t
u32distance(uint32_t a, uint32_t b)
{
	if (a < b)
		return b - a;
	else
		return a - b;
}

/** Choose equivalent mode
 *
 * If the two modes are not equivalent, return NULL.
 * Otherwise return the mode that is more likely to work in place of both.
 *
 * None of the fuzzy matching criteria in this function have any justification.
 *
 * typedef struct _drmModeModeInfo {
 *         uint32_t clock;
 *         uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
 *         uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;
 *
 *         uint32_t vrefresh;
 *
 *         uint32_t flags;
 *         uint32_t type;
 *         char name[DRM_DISPLAY_MODE_LEN];
 * } drmModeModeInfo, *drmModeModeInfoPtr;
 */
static const drmModeModeInfo *
drm_mode_pick_equivalent(const drmModeModeInfo *a, const drmModeModeInfo *b)
{
	uint32_t refresh_a, refresh_b;

	if (a->hdisplay != b->hdisplay || a->vdisplay != b->vdisplay)
		return NULL;

	if (a->flags != b->flags)
		return NULL;

	/* kHz */
	if (u32distance(a->clock, b->clock) > 500)
		return NULL;

	refresh_a = drm_refresh_rate_mHz(a);
	refresh_b = drm_refresh_rate_mHz(b);
	if (u32distance(refresh_a, refresh_b) > 50)
		return NULL;

	if ((a->type ^ b->type) & DRM_MODE_TYPE_PREFERRED) {
		if (a->type & DRM_MODE_TYPE_PREFERRED)
			return a;
		else
			return b;
	}

	return a;
}

/* If the given mode info is not already in the list, add it.
 * If it is in the list, either keep the existing or replace it,
 * depending on which one is "better".
 */
static int
drm_output_try_add_mode(struct drm_output *output, const drmModeModeInfo *info)
{
	struct weston_mode *base;
	struct drm_mode *mode;
	struct drm_backend *backend;
	const drmModeModeInfo *chosen = NULL;

	assert(info);

	wl_list_for_each(base, &output->base.mode_list, link) {
		mode = to_drm_mode(base);
		chosen = drm_mode_pick_equivalent(&mode->mode_info, info);
		if (chosen)
			break;
	}

	if (chosen == info) {
		backend = to_drm_backend(output->base.compositor);
		drm_output_destroy_mode(backend, mode);
		chosen = NULL;
	}

	if (!chosen) {
		mode = drm_output_add_mode(output, info);
		if (!mode)
			return -1;
	}
	/* else { the equivalent mode is already in the list } */

	return 0;
}

/** Rewrite the output's mode list
 *
 * @param output The output.
 * @return 0 on success, -1 on failure.
 *
 * Destroy all existing modes in the list, and reconstruct a new list from
 * scratch, based on the currently attached heads.
 *
 * On failure the output's mode list may contain some modes.
 */
static int
drm_output_update_modelist_from_heads(struct drm_output *output)
{
	struct drm_backend *backend = to_drm_backend(output->base.compositor);
	struct weston_head *head_base;
	struct drm_head *head;
	int i;
	int ret;

	assert(!output->base.enabled);

	drm_mode_list_destroy(backend, &output->base.mode_list);

	wl_list_for_each(head_base, &output->base.head_list, output_link) {
		head = to_drm_head(head_base);
		for (i = 0; i < head->connector->count_modes; i++) {
			ret = drm_output_try_add_mode(output,
						&head->connector->modes[i]);
			if (ret < 0)
				return -1;
		}
	}

	return 0;
}

/**
 * Choose suitable mode for an output
 *
 * Find the most suitable mode to use for initial setup (or reconfiguration on
 * hotplug etc) for a DRM output.
 *
 * @param output DRM output to choose mode for
 * @param kind Strategy and preference to use when choosing mode
 * @param width Desired width for this output
 * @param height Desired height for this output
 * @param current_mode Mode currently being displayed on this output
 * @param modeline Manually-entered mode (may be NULL)
 * @returns A mode from the output's mode list, or NULL if none available
 */
static struct drm_mode *
drm_output_choose_initial_mode(struct drm_backend *backend,
			       struct drm_output *output,
			       enum weston_drm_backend_output_mode mode,
			       const char *modeline,
			       const drmModeModeInfo *current_mode)
{
	struct drm_mode *preferred = NULL;
	struct drm_mode *current = NULL;
	struct drm_mode *configured = NULL;
	struct drm_mode *config_fall_back = NULL;
	struct drm_mode *best = NULL;
	struct drm_mode *drm_mode;
	drmModeModeInfo drm_modeline;
	int32_t width = 0;
	int32_t height = 0;
	uint32_t refresh = 0;
	uint32_t aspect_width = 0;
	uint32_t aspect_height = 0;
	enum weston_mode_aspect_ratio aspect_ratio = WESTON_MODE_PIC_AR_NONE;
	int n;

	if (mode == WESTON_DRM_BACKEND_OUTPUT_PREFERRED && modeline) {
		n = sscanf(modeline, "%dx%d@%d %u:%u", &width, &height,
			   &refresh, &aspect_width, &aspect_height);
		if (backend->aspect_ratio_supported && n == 5) {
			if (aspect_width == 4 && aspect_height == 3)
				aspect_ratio = WESTON_MODE_PIC_AR_4_3;
			else if (aspect_width == 16 && aspect_height == 9)
				aspect_ratio = WESTON_MODE_PIC_AR_16_9;
			else if (aspect_width == 64 && aspect_height == 27)
				aspect_ratio = WESTON_MODE_PIC_AR_64_27;
			else if (aspect_width == 256 && aspect_height == 135)
				aspect_ratio = WESTON_MODE_PIC_AR_256_135;
			else
				weston_log("Invalid modeline \"%s\" for output %s\n",
					   modeline, output->base.name);
		}
		if (n != 2 && n != 3 && n != 5) {
			width = -1;

			if (parse_modeline(modeline, &drm_modeline) == 0) {
				configured = drm_output_add_mode(output, &drm_modeline);
				if (!configured)
					return NULL;
			} else {
				weston_log("Invalid modeline \"%s\" for output %s\n",
					   modeline, output->base.name);
			}
		}
	}

	wl_list_for_each_reverse(drm_mode, &output->base.mode_list, base.link) {
		if (width == drm_mode->base.width &&
		    height == drm_mode->base.height &&
		    (refresh == 0 || refresh == drm_mode->mode_info.vrefresh)) {
			if (!backend->aspect_ratio_supported ||
			    aspect_ratio == drm_mode->base.aspect_ratio)
				configured = drm_mode;
			else
				config_fall_back = drm_mode;
		}

		if (memcmp(current_mode, &drm_mode->mode_info,
			   sizeof *current_mode) == 0)
			current = drm_mode;

		if (drm_mode->base.flags & WL_OUTPUT_MODE_PREFERRED)
			preferred = drm_mode;

		best = drm_mode;
	}

	if (current == NULL && current_mode->clock != 0) {
		current = drm_output_add_mode(output, current_mode);
		if (!current)
			return NULL;
	}

	if (mode == WESTON_DRM_BACKEND_OUTPUT_CURRENT)
		configured = current;

	if (configured)
		return configured;

	if (config_fall_back)
		return config_fall_back;

	if (preferred)
		return preferred;

	if (current)
		return current;

	if (best)
		return best;

	weston_log("no available modes for %s\n", output->base.name);
	return NULL;
}

static int
drm_head_read_current_setup(struct drm_head *head, struct drm_backend *backend)
{
	int drm_fd = backend->drm.fd;
	drmModeEncoder *encoder;
	drmModeCrtc *crtc;

	/* Get the current mode on the crtc that's currently driving
	 * this connector. */
	encoder = drmModeGetEncoder(drm_fd, head->connector->encoder_id);
	if (encoder != NULL) {
		head->inherited_crtc_id = encoder->crtc_id;

		crtc = drmModeGetCrtc(drm_fd, encoder->crtc_id);
		drmModeFreeEncoder(encoder);

		if (crtc == NULL)
			return -1;
		if (crtc->mode_valid)
			head->inherited_mode = crtc->mode;
		drmModeFreeCrtc(crtc);
	}

	return 0;
}

static int
drm_output_set_mode(struct weston_output *base,
		    enum weston_drm_backend_output_mode mode,
		    const char *modeline)
{
	struct drm_output *output = to_drm_output(base);
	struct drm_backend *b = to_drm_backend(base->compositor);
	struct drm_head *head = to_drm_head(weston_output_get_first_head(base));

	struct drm_mode *current;

	if (drm_output_update_modelist_from_heads(output) < 0)
		return -1;

	current = drm_output_choose_initial_mode(b, output, mode, modeline,
						 &head->inherited_mode);
	if (!current)
		return -1;

	output->base.current_mode = &current->base;
	output->base.current_mode->flags |= WL_OUTPUT_MODE_CURRENT;

	/* Set native_ fields, so weston_output_mode_switch_to_native() works */
	output->base.native_mode = output->base.current_mode;
	output->base.native_scale = output->base.current_scale;

	return 0;
}

static void
drm_output_set_gbm_format(struct weston_output *base,
			  const char *gbm_format)
{
	struct drm_output *output = to_drm_output(base);
	struct drm_backend *b = to_drm_backend(base->compositor);

	if (parse_gbm_format(gbm_format, b->gbm_format, &output->gbm_format) == -1)
		output->gbm_format = b->gbm_format;

	/* Without universal planes, we can't discover which formats are
	 * supported by the primary plane; we just hope that the GBM format
	 * works. */
	if (!b->universal_planes)
		output->scanout_plane->formats[0].format = output->gbm_format;
}

static void
drm_output_set_seat(struct weston_output *base,
		    const char *seat)
{
	struct drm_output *output = to_drm_output(base);
	struct drm_backend *b = to_drm_backend(base->compositor);

	setup_output_seat_constraint(b, &output->base,
				     seat ? seat : "");
}

static int
drm_output_init_gamma_size(struct drm_output *output)
{
	struct drm_backend *backend = to_drm_backend(output->base.compositor);
	drmModeCrtc *crtc;

	assert(output->base.compositor);
	assert(output->crtc_id != 0);
	crtc = drmModeGetCrtc(backend->drm.fd, output->crtc_id);
	if (!crtc)
		return -1;

	output->base.gamma_size = crtc->gamma_size;

	drmModeFreeCrtc(crtc);

	return 0;
}

static uint32_t
drm_head_get_possible_crtcs_mask(struct drm_head *head)
{
	uint32_t possible_crtcs = 0;
	drmModeEncoder *encoder;
	int i;

	for (i = 0; i < head->connector->count_encoders; i++) {
		encoder = drmModeGetEncoder(head->backend->drm.fd,
					    head->connector->encoders[i]);
		if (!encoder)
			continue;

		possible_crtcs |= encoder->possible_crtcs;
		drmModeFreeEncoder(encoder);
	}

	return possible_crtcs;
}

static int
drm_crtc_get_index(drmModeRes *resources, uint32_t crtc_id)
{
	int i;

	for (i = 0; i < resources->count_crtcs; i++) {
		if (resources->crtcs[i] == crtc_id)
			return i;
	}

	assert(0 && "unknown crtc id");
	return -1;
}

/** Pick a CRTC that might be able to drive all attached connectors
 *
 * @param output The output whose attached heads to include.
 * @param resources The DRM KMS resources.
 * @return CRTC index, or -1 on failure or not found.
 */
static int
drm_output_pick_crtc(struct drm_output *output, drmModeRes *resources)
{
	struct drm_backend *backend;
	struct weston_head *base;
	struct drm_head *head;
	uint32_t possible_crtcs = 0xffffffff;
	int existing_crtc[32];
	unsigned j, n = 0;
	uint32_t crtc_id;
	int best_crtc_index = -1;
	int fallback_crtc_index = -1;
	int i;
	bool match;

	backend = to_drm_backend(output->base.compositor);

	/* This algorithm ignores drmModeEncoder::possible_clones restriction,
	 * because it is more often set wrong than not in the kernel. */

	/* Accumulate a mask of possible crtcs and find existing routings. */
	wl_list_for_each(base, &output->base.head_list, output_link) {
		head = to_drm_head(base);

		possible_crtcs &= drm_head_get_possible_crtcs_mask(head);

		crtc_id = head->inherited_crtc_id;
		if (crtc_id > 0 && n < ARRAY_LENGTH(existing_crtc))
			existing_crtc[n++] = drm_crtc_get_index(resources,
								crtc_id);
	}

	/* Find a crtc that could drive each connector individually at least,
	 * and prefer existing routings. */
	for (i = 0; i < resources->count_crtcs; i++) {
		crtc_id = resources->crtcs[i];

		/* Could the crtc not drive each connector? */
		if (!(possible_crtcs & (1 << i)))
			continue;

		/* Is the crtc already in use? */
		if (drm_output_find_by_crtc(backend, crtc_id))
			continue;

		/* Try to preserve the existing CRTC -> connector routing;
		 * it makes initialisation faster, and also since we have a
		 * very dumb picking algorithm, may preserve a better
		 * choice. */
		for (j = 0; j < n; j++) {
			if (existing_crtc[j] == i)
				return i;
		}

		/* Check if any other head had existing routing to this CRTC.
		 * If they did, this is not the best CRTC as it might be needed
		 * for another output we haven't enabled yet. */
		match = false;
		wl_list_for_each(base, &backend->compositor->head_list,
				 compositor_link) {
			head = to_drm_head(base);

			if (head->base.output == &output->base)
				continue;

			if (weston_head_is_enabled(&head->base))
				continue;

			if (head->inherited_crtc_id == crtc_id) {
				match = true;
				break;
			}
		}
		if (!match)
			best_crtc_index = i;

		fallback_crtc_index = i;
	}

	if (best_crtc_index != -1)
		return best_crtc_index;

	if (fallback_crtc_index != -1)
		return fallback_crtc_index;

	/* Likely possible_crtcs was empty due to asking for clones,
	 * but since the DRM documentation says the kernel lies, let's
	 * pick one crtc anyway. Trial and error is the only way to
	 * be sure if something doesn't work. */

	/* First pick any existing assignment. */
	for (j = 0; j < n; j++) {
		crtc_id = resources->crtcs[existing_crtc[j]];
		if (!drm_output_find_by_crtc(backend, crtc_id))
			return existing_crtc[j];
	}

	/* Otherwise pick any available crtc. */
	for (i = 0; i < resources->count_crtcs; i++) {
		crtc_id = resources->crtcs[i];

		if (!drm_output_find_by_crtc(backend, crtc_id))
			return i;
	}

	return -1;
}

/** Allocate a CRTC for the output
 *
 * @param output The output with no allocated CRTC.
 * @param resources DRM KMS resources.
 * @return 0 on success, -1 on failure.
 *
 * Finds a free CRTC that might drive the attached connectors, reserves the CRTC
 * for the output, and loads the CRTC properties.
 *
 * Populates the cursor and scanout planes.
 *
 * On failure, the output remains without a CRTC.
 */
static int
drm_output_init_crtc(struct drm_output *output, drmModeRes *resources)
{
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	drmModeObjectPropertiesPtr props;
	int i;

	assert(output->crtc_id == 0);

	i = drm_output_pick_crtc(output, resources);
	if (i < 0) {
		weston_log("Output '%s': No available CRTCs.\n",
			   output->base.name);
		return -1;
	}

	output->crtc_id = resources->crtcs[i];
	output->pipe = i;

	props = drmModeObjectGetProperties(b->drm.fd, output->crtc_id,
					   DRM_MODE_OBJECT_CRTC);
	if (!props) {
		weston_log("failed to get CRTC properties\n");
		goto err_crtc;
	}
	drm_property_info_populate(b, crtc_props, output->props_crtc,
				   WDRM_CRTC__COUNT, props);
	drmModeFreeObjectProperties(props);

	output->scanout_plane =
		drm_output_find_special_plane(b, output,
					      WDRM_PLANE_TYPE_PRIMARY);
	if (!output->scanout_plane) {
		weston_log("Failed to find primary plane for output %s\n",
			   output->base.name);
		goto err_crtc;
	}

	/* Failing to find a cursor plane is not fatal, as we'll fall back
	 * to software cursor. */
	output->cursor_plane =
		drm_output_find_special_plane(b, output,
					      WDRM_PLANE_TYPE_CURSOR);

	wl_array_remove_uint32(&b->unused_crtcs, output->crtc_id);

	return 0;

err_crtc:
	output->crtc_id = 0;
	output->pipe = 0;

	return -1;
}

/** Free the CRTC from the output
 *
 * @param output The output whose CRTC to deallocate.
 *
 * The CRTC reserved for the given output becomes free to use again.
 */
static void
drm_output_fini_crtc(struct drm_output *output)
{
	struct drm_backend *b = to_drm_backend(output->base.compositor);
	uint32_t *unused;

	if (!b->universal_planes && !b->shutting_down) {
		/* With universal planes, the 'special' planes are allocated at
		 * startup, freed at shutdown, and live on the plane list in
		 * between. We want the planes to continue to exist and be freed
		 * up for other outputs.
		 *
		 * Without universal planes, our special planes are
		 * pseudo-planes allocated at output creation, freed at output
		 * destruction, and not usable by other outputs.
		 *
		 * On the other hand, if the compositor is already shutting down,
		 * the plane has already been destroyed.
		 */
		if (output->cursor_plane)
			drm_plane_destroy(output->cursor_plane);
		if (output->scanout_plane)
			drm_plane_destroy(output->scanout_plane);
	}

	drm_property_info_free(output->props_crtc, WDRM_CRTC__COUNT);

	assert(output->crtc_id != 0);

	unused = wl_array_add(&b->unused_crtcs, sizeof(*unused));
	*unused = output->crtc_id;

	/* Force resetting unused CRTCs */
	b->state_invalid = true;

	output->crtc_id = 0;
	output->cursor_plane = NULL;
	output->scanout_plane = NULL;
}

static void
drm_output_print_modes(struct drm_output *output)
{
	struct weston_mode *m;
	struct drm_mode *dm;
	const char *aspect_ratio;

	wl_list_for_each(m, &output->base.mode_list, link) {
		dm = to_drm_mode(m);

		aspect_ratio = aspect_ratio_to_string(m->aspect_ratio);
		weston_log_continue(STAMP_SPACE "%dx%d@%.1f%s%s%s, %.1f MHz\n",
				    m->width, m->height, m->refresh / 1000.0,
				    aspect_ratio,
				    m->flags & WL_OUTPUT_MODE_PREFERRED ?
				    ", preferred" : "",
				    m->flags & WL_OUTPUT_MODE_CURRENT ?
				    ", current" : "",
				    dm->mode_info.clock / 1000.0);
	}
}

static int
drm_output_enable(struct weston_output *base)
{
	struct drm_output *output = to_drm_output(base);
	struct drm_backend *b = to_drm_backend(base->compositor);
	drmModeRes *resources;
	int ret;

	resources = drmModeGetResources(b->drm.fd);
	if (!resources) {
		weston_log("drmModeGetResources failed\n");
		return -1;
	}
	ret = drm_output_init_crtc(output, resources);
	drmModeFreeResources(resources);
	if (ret < 0)
		return -1;

	if (drm_output_init_gamma_size(output) < 0)
		goto err;

	if (b->pageflip_timeout)
		drm_output_pageflip_timer_create(output);

	if (b->use_pixman) {
		if (drm_output_init_pixman(output, b) < 0) {
			weston_log("Failed to init output pixman state\n");
			goto err;
		}
	} else if (drm_output_init_egl(output, b) < 0) {
		weston_log("Failed to init output gl state\n");
		goto err;
	}

	drm_output_init_backlight(output);

	output->base.start_repaint_loop = drm_output_start_repaint_loop;
	output->base.repaint = drm_output_repaint;
	output->base.assign_planes = drm_assign_planes;
	output->base.set_dpms = drm_set_dpms;
	output->base.switch_mode = drm_output_switch_mode;
	output->base.set_gamma = drm_output_set_gamma;

	if (output->cursor_plane)
		weston_compositor_stack_plane(b->compositor,
					      &output->cursor_plane->base,
					      NULL);
	else
		b->cursors_are_broken = 1;

	weston_compositor_stack_plane(b->compositor,
				      &output->scanout_plane->base,
				      &b->compositor->primary_plane);

	weston_log("Output %s (crtc %d) video modes:\n",
		   output->base.name, output->crtc_id);
	drm_output_print_modes(output);

	return 0;

err:
	drm_output_fini_crtc(output);

	return -1;
}

static void
drm_output_deinit(struct weston_output *base)
{
	struct drm_output *output = to_drm_output(base);
	struct drm_backend *b = to_drm_backend(base->compositor);

	if (b->use_pixman)
		drm_output_fini_pixman(output);
	else
		drm_output_fini_egl(output);

	/* Since our planes are no longer in use anywhere, remove their base
	 * weston_plane's link from the plane stacking list, unless we're
	 * shutting down, in which case the plane has already been
	 * destroyed. */
	if (!b->shutting_down) {
		wl_list_remove(&output->scanout_plane->base.link);
		wl_list_init(&output->scanout_plane->base.link);

		if (output->cursor_plane) {
			wl_list_remove(&output->cursor_plane->base.link);
			wl_list_init(&output->cursor_plane->base.link);
			/* Turn off hardware cursor */
			drmModeSetCursor(b->drm.fd, output->crtc_id, 0, 0, 0);
		}
	}

	drm_output_fini_crtc(output);
}

static void
drm_head_destroy(struct drm_head *head);

static void
drm_output_destroy(struct weston_output *base)
{
	struct drm_output *output = to_drm_output(base);
	struct drm_backend *b = to_drm_backend(base->compositor);

	if (output->page_flip_pending || output->vblank_pending ||
	    output->atomic_complete_pending) {
		output->destroy_pending = 1;
		weston_log("destroy output while page flip pending\n");
		return;
	}

	if (output->base.enabled)
		drm_output_deinit(&output->base);

	drm_mode_list_destroy(b, &output->base.mode_list);

	if (output->pageflip_timer)
		wl_event_source_remove(output->pageflip_timer);

	weston_output_release(&output->base);

	assert(!output->state_last);
	drm_output_state_free(output->state_cur);

	free(output);
}

static int
drm_output_disable(struct weston_output *base)
{
	struct drm_output *output = to_drm_output(base);

	if (output->page_flip_pending || output->vblank_pending ||
	    output->atomic_complete_pending) {
		output->disable_pending = 1;
		return -1;
	}

	weston_log("Disabling output %s\n", output->base.name);

	if (output->base.enabled)
		drm_output_deinit(&output->base);

	output->disable_pending = 0;

	return 0;
}

/**
 * Update the list of unused connectors and CRTCs
 *
 * This keeps the unused_crtc arrays up to date.
 *
 * @param b Weston backend structure
 * @param resources DRM resources for this device
 */
static void
drm_backend_update_unused_outputs(struct drm_backend *b, drmModeRes *resources)
{
	int i;

	wl_array_release(&b->unused_crtcs);
	wl_array_init(&b->unused_crtcs);

	for (i = 0; i < resources->count_crtcs; i++) {
		struct drm_output *output;
		uint32_t *crtc_id;

		output = drm_output_find_by_crtc(b, resources->crtcs[i]);
		if (output && output->base.enabled)
			continue;

		crtc_id = wl_array_add(&b->unused_crtcs, sizeof(*crtc_id));
		*crtc_id = resources->crtcs[i];
	}
}

/** Replace connector data and monitor information
 *
 * @param head The head to update.
 * @param connector The connector data to be owned by the head, must match
 * the head's connector ID.
 * @return 0 on success, -1 on failure.
 *
 * Takes ownership of @c connector on success, not on failure.
 *
 * May schedule a heads changed call.
 */
static int
drm_head_assign_connector_info(struct drm_head *head,
			       drmModeConnector *connector)
{
	drmModeObjectProperties *props;
	const char *make = "unknown";
	const char *model = "unknown";
	const char *serial_number = "unknown";

	assert(connector);
	assert(head->connector_id == connector->connector_id);

	props = drmModeObjectGetProperties(head->backend->drm.fd,
					   head->connector_id,
					   DRM_MODE_OBJECT_CONNECTOR);
	if (!props) {
		weston_log("Error: failed to get connector '%s' properties\n",
			   head->base.name);
		return -1;
	}

	if (head->connector)
		drmModeFreeConnector(head->connector);
	head->connector = connector;

	drm_property_info_populate(head->backend, connector_props,
				   head->props_conn,
				   WDRM_CONNECTOR__COUNT, props);
	find_and_parse_output_edid(head, props, &make, &model, &serial_number);
	weston_head_set_monitor_strings(&head->base, make, model, serial_number);
	weston_head_set_subpixel(&head->base,
		drm_subpixel_to_wayland(head->connector->subpixel));

	weston_head_set_physical_size(&head->base, head->connector->mmWidth,
				      head->connector->mmHeight);

	drmModeFreeObjectProperties(props);

	/* Unknown connection status is assumed disconnected. */
	weston_head_set_connection_status(&head->base,
			head->connector->connection == DRM_MODE_CONNECTED);

	return 0;
}

static void
drm_head_log_info(struct drm_head *head, const char *msg)
{
	if (head->base.connected) {
		weston_log("DRM: head '%s' %s, connector %d is connected, "
			   "EDID make '%s', model '%s', serial '%s'\n",
			   head->base.name, msg, head->connector_id,
			   head->base.make, head->base.model,
			   head->base.serial_number ?: "");
	} else {
		weston_log("DRM: head '%s' %s, connector %d is disconnected.\n",
			   head->base.name, msg, head->connector_id);
	}
}

/** Update connector and monitor information
 *
 * @param head The head to update.
 *
 * Re-reads the DRM property lists for the connector and updates monitor
 * information and connection status. This may schedule a heads changed call
 * to the user.
 */
static void
drm_head_update_info(struct drm_head *head)
{
	drmModeConnector *connector;

	connector = drmModeGetConnector(head->backend->drm.fd,
					head->connector_id);
	if (!connector) {
		weston_log("DRM: getting connector info for '%s' failed.\n",
			   head->base.name);
		return;
	}

	if (drm_head_assign_connector_info(head, connector) < 0)
		drmModeFreeConnector(connector);

	if (head->base.device_changed)
		drm_head_log_info(head, "updated");
}

/**
 * Create a Weston head for a connector
 *
 * Given a DRM connector, create a matching drm_head structure and add it
 * to Weston's head list.
 *
 * @param b Weston backend structure
 * @param connector_id DRM connector ID for the head
 * @param drm_device udev device pointer
 * @returns The new head, or NULL on failure.
 */
static struct drm_head *
drm_head_create(struct drm_backend *backend, uint32_t connector_id,
		struct udev_device *drm_device)
{
	struct drm_head *head;
	drmModeConnector *connector;
	char *name;

	head = zalloc(sizeof *head);
	if (!head)
		return NULL;

	connector = drmModeGetConnector(backend->drm.fd, connector_id);
	if (!connector)
		goto err_alloc;

	name = make_connector_name(connector);
	if (!name)
		goto err_alloc;

	weston_head_init(&head->base, name);
	free(name);

	head->connector_id = connector_id;
	head->backend = backend;

	head->backlight = backlight_init(drm_device, connector->connector_type);

	if (drm_head_assign_connector_info(head, connector) < 0)
		goto err_init;

	if (head->connector->connector_type == DRM_MODE_CONNECTOR_LVDS ||
	    head->connector->connector_type == DRM_MODE_CONNECTOR_eDP)
		weston_head_set_internal(&head->base);

	if (drm_head_read_current_setup(head, backend) < 0) {
		weston_log("Failed to retrieve current mode from connector %d.\n",
			   head->connector_id);
		/* Not fatal. */
	}

	weston_compositor_add_head(backend->compositor, &head->base);
	drm_head_log_info(head, "found");

	return head;

err_init:
	weston_head_release(&head->base);

err_alloc:
	if (connector)
		drmModeFreeConnector(connector);

	free(head);

	return NULL;
}

static void
drm_head_destroy(struct drm_head *head)
{
	weston_head_release(&head->base);

	drm_property_info_free(head->props_conn, WDRM_CONNECTOR__COUNT);
	drmModeFreeConnector(head->connector);

	if (head->backlight)
		backlight_destroy(head->backlight);

	free(head);
}

/**
 * Create a Weston output structure
 *
 * Create an "empty" drm_output. This is the implementation of
 * weston_backend::create_output.
 *
 * Creating an output is usually followed by drm_output_attach_head()
 * and drm_output_enable() to make use of it.
 *
 * @param compositor The compositor instance.
 * @param name Name for the new output.
 * @returns The output, or NULL on failure.
 */
static struct weston_output *
drm_output_create(struct weston_compositor *compositor, const char *name)
{
	struct drm_backend *b = to_drm_backend(compositor);
	struct drm_output *output;

	output = zalloc(sizeof *output);
	if (output == NULL)
		return NULL;

	weston_output_init(&output->base, compositor, name);

	output->base.enable = drm_output_enable;
	output->base.destroy = drm_output_destroy;
	output->base.disable = drm_output_disable;
	output->base.attach_head = drm_output_attach_head;
	output->base.detach_head = drm_output_detach_head;

	output->destroy_pending = 0;
	output->disable_pending = 0;

	output->state_cur = drm_output_state_alloc(output, NULL);

	weston_compositor_add_pending_output(&output->base, b->compositor);

	return &output->base;
}

static int
drm_backend_create_heads(struct drm_backend *b, struct udev_device *drm_device)
{
	struct drm_head *head;
	drmModeRes *resources;
	int i;

	resources = drmModeGetResources(b->drm.fd);
	if (!resources) {
		weston_log("drmModeGetResources failed\n");
		return -1;
	}

	b->min_width  = resources->min_width;
	b->max_width  = resources->max_width;
	b->min_height = resources->min_height;
	b->max_height = resources->max_height;

	for (i = 0; i < resources->count_connectors; i++) {
		uint32_t connector_id = resources->connectors[i];

		head = drm_head_create(b, connector_id, drm_device);
		if (!head) {
			weston_log("DRM: failed to create head for connector %d.\n",
				   connector_id);
		}
	}

	drm_backend_update_unused_outputs(b, resources);

	drmModeFreeResources(resources);

	return 0;
}

static void
drm_backend_update_heads(struct drm_backend *b, struct udev_device *drm_device)
{
	drmModeRes *resources;
	struct weston_head *base, *next;
	struct drm_head *head;
	int i;

	resources = drmModeGetResources(b->drm.fd);
	if (!resources) {
		weston_log("drmModeGetResources failed\n");
		return;
	}

	/* collect new connectors that have appeared, e.g. MST */
	for (i = 0; i < resources->count_connectors; i++) {
		uint32_t connector_id = resources->connectors[i];

		head = drm_head_find_by_connector(b, connector_id);
		if (head) {
			drm_head_update_info(head);
		} else {
			head = drm_head_create(b, connector_id, drm_device);
			if (!head)
				weston_log("DRM: failed to create head for hot-added connector %d.\n",
					   connector_id);
		}
	}

	/* Remove connectors that have disappeared. */
	wl_list_for_each_safe(base, next,
			      &b->compositor->head_list, compositor_link) {
		bool removed = true;

		head = to_drm_head(base);

		for (i = 0; i < resources->count_connectors; i++) {
			if (resources->connectors[i] == head->connector_id) {
				removed = false;
				break;
			}
		}

		if (!removed)
			continue;

		weston_log("DRM: head '%s' (connector %d) disappeared.\n",
			   head->base.name, head->connector_id);
		drm_head_destroy(head);
	}

	drm_backend_update_unused_outputs(b, resources);

	drmModeFreeResources(resources);
}

static int
udev_event_is_hotplug(struct drm_backend *b, struct udev_device *device)
{
	const char *sysnum;
	const char *val;

	sysnum = udev_device_get_sysnum(device);
	if (!sysnum || atoi(sysnum) != b->drm.id)
		return 0;

	val = udev_device_get_property_value(device, "HOTPLUG");
	if (!val)
		return 0;

	return strcmp(val, "1") == 0;
}

static int
udev_drm_event(int fd, uint32_t mask, void *data)
{
	struct drm_backend *b = data;
	struct udev_device *event;

	event = udev_monitor_receive_device(b->udev_monitor);

	if (udev_event_is_hotplug(b, event))
		drm_backend_update_heads(b, event);

	udev_device_unref(event);

	return 1;
}

static void
drm_destroy(struct weston_compositor *ec)
{
	struct drm_backend *b = to_drm_backend(ec);
	struct weston_head *base, *next;

	udev_input_destroy(&b->input);

	wl_event_source_remove(b->udev_drm_source);
	wl_event_source_remove(b->drm_source);

	b->shutting_down = true;

	destroy_sprites(b);

	weston_compositor_shutdown(ec);

	wl_list_for_each_safe(base, next, &ec->head_list, compositor_link)
		drm_head_destroy(to_drm_head(base));

	if (b->gbm)
		gbm_device_destroy(b->gbm);

	udev_monitor_unref(b->udev_monitor);
	udev_unref(b->udev);

	weston_launcher_destroy(ec->launcher);

	wl_array_release(&b->unused_crtcs);

	close(b->drm.fd);
	free(b->drm.filename);
	free(b);
}

static void
session_notify(struct wl_listener *listener, void *data)
{
	struct weston_compositor *compositor = data;
	struct drm_backend *b = to_drm_backend(compositor);
	struct drm_plane *plane;
	struct drm_output *output;

	if (compositor->session_active) {
		weston_log("activating session\n");
		weston_compositor_wake(compositor);
		weston_compositor_damage_all(compositor);
		b->state_invalid = true;
		udev_input_enable(&b->input);
	} else {
		weston_log("deactivating session\n");
		udev_input_disable(&b->input);

		weston_compositor_offscreen(compositor);

		/* If we have a repaint scheduled (either from a
		 * pending pageflip or the idle handler), make sure we
		 * cancel that so we don't try to pageflip when we're
		 * vt switched away.  The OFFSCREEN state will prevent
		 * further attempts at repainting.  When we switch
		 * back, we schedule a repaint, which will process
		 * pending frame callbacks. */

		wl_list_for_each(output, &compositor->output_list, base.link) {
			output->base.repaint_needed = false;
			if (output->cursor_plane)
				drmModeSetCursor(b->drm.fd, output->crtc_id,
						 0, 0, 0);
		}

		output = container_of(compositor->output_list.next,
				      struct drm_output, base.link);

		wl_list_for_each(plane, &b->plane_list, link) {
			if (plane->type != WDRM_PLANE_TYPE_OVERLAY)
				continue;

			drmModeSetPlane(b->drm.fd,
					plane->plane_id,
					output->crtc_id, 0, 0,
					0, 0, 0, 0, 0, 0, 0, 0);
		}
	}
}

/**
 * Determines whether or not a device is capable of modesetting. If successful,
 * sets b->drm.fd and b->drm.filename to the opened device.
 */
static bool
drm_device_is_kms(struct drm_backend *b, struct udev_device *device)
{
	const char *filename = udev_device_get_devnode(device);
	const char *sysnum = udev_device_get_sysnum(device);
	drmModeRes *res;
	int id, fd;

	if (!filename)
		return false;

	fd = weston_launcher_open(b->compositor->launcher, filename, O_RDWR);
	if (fd < 0)
		return false;

	res = drmModeGetResources(fd);
	if (!res)
		goto out_fd;

	if (res->count_crtcs <= 0 || res->count_connectors <= 0 ||
	    res->count_encoders <= 0)
		goto out_res;

	if (sysnum)
		id = atoi(sysnum);
	if (!sysnum || id < 0) {
		weston_log("couldn't get sysnum for device %s\n", filename);
		goto out_res;
	}

	/* We can be called successfully on multiple devices; if we have,
	 * clean up old entries. */
	if (b->drm.fd >= 0)
		weston_launcher_close(b->compositor->launcher, b->drm.fd);
	free(b->drm.filename);

	b->drm.fd = fd;
	b->drm.id = id;
	b->drm.filename = strdup(filename);

	drmModeFreeResources(res);

	return true;

out_res:
	drmModeFreeResources(res);
out_fd:
	weston_launcher_close(b->compositor->launcher, fd);
	return false;
}

/*
 * Find primary GPU
 * Some systems may have multiple DRM devices attached to a single seat. This
 * function loops over all devices and tries to find a PCI device with the
 * boot_vga sysfs attribute set to 1.
 * If no such device is found, the first DRM device reported by udev is used.
 * Devices are also vetted to make sure they are are capable of modesetting,
 * rather than pure render nodes (GPU with no display), or pure
 * memory-allocation devices (VGEM).
 */
static struct udev_device*
find_primary_gpu(struct drm_backend *b, const char *seat)
{
	struct udev_enumerate *e;
	struct udev_list_entry *entry;
	const char *path, *device_seat, *id;
	struct udev_device *device, *drm_device, *pci;

	e = udev_enumerate_new(b->udev);
	udev_enumerate_add_match_subsystem(e, "drm");
	udev_enumerate_add_match_sysname(e, "card[0-9]*");

	udev_enumerate_scan_devices(e);
	drm_device = NULL;
	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(e)) {
		bool is_boot_vga = false;

		path = udev_list_entry_get_name(entry);
		device = udev_device_new_from_syspath(b->udev, path);
		if (!device)
			continue;
		device_seat = udev_device_get_property_value(device, "ID_SEAT");
		if (!device_seat)
			device_seat = default_seat;
		if (strcmp(device_seat, seat)) {
			udev_device_unref(device);
			continue;
		}

		pci = udev_device_get_parent_with_subsystem_devtype(device,
								"pci", NULL);
		if (pci) {
			id = udev_device_get_sysattr_value(pci, "boot_vga");
			if (id && !strcmp(id, "1"))
				is_boot_vga = true;
		}

		/* If we already have a modesetting-capable device, and this
		 * device isn't our boot-VGA device, we aren't going to use
		 * it. */
		if (!is_boot_vga && drm_device) {
			udev_device_unref(device);
			continue;
		}

		/* Make sure this device is actually capable of modesetting;
		 * if this call succeeds, b->drm.{fd,filename} will be set,
		 * and any old values freed. */
		if (!drm_device_is_kms(b, device)) {
			udev_device_unref(device);
			continue;
		}

		/* There can only be one boot_vga device, and we try to use it
		 * at all costs. */
		if (is_boot_vga) {
			if (drm_device)
				udev_device_unref(drm_device);
			drm_device = device;
			break;
		}

		/* Per the (!is_boot_vga && drm_device) test above, we only
		 * trump existing saved devices with boot-VGA devices, so if
		 * we end up here, this must be the first device we've seen. */
		assert(!drm_device);
		drm_device = device;
	}

	/* If we're returning a device to use, we must have an open FD for
	 * it. */
	assert(!!drm_device == (b->drm.fd >= 0));

	udev_enumerate_unref(e);
	return drm_device;
}

static struct udev_device *
open_specific_drm_device(struct drm_backend *b, const char *name)
{
	struct udev_device *device;

	device = udev_device_new_from_subsystem_sysname(b->udev, "drm", name);
	if (!device) {
		weston_log("ERROR: could not open DRM device '%s'\n", name);
		return NULL;
	}

	if (!drm_device_is_kms(b, device)) {
		udev_device_unref(device);
		weston_log("ERROR: DRM device '%s' is not a KMS device.\n", name);
		return NULL;
	}

	/* If we're returning a device to use, we must have an open FD for
	 * it. */
	assert(b->drm.fd >= 0);

	return device;
}

static void
planes_binding(struct weston_keyboard *keyboard, const struct timespec *time,
	       uint32_t key, void *data)
{
	struct drm_backend *b = data;

	switch (key) {
	case KEY_C:
		b->cursors_are_broken ^= 1;
		break;
	case KEY_V:
		b->sprites_are_broken ^= 1;
		break;
	case KEY_O:
		b->sprites_hidden ^= 1;
		break;
	default:
		break;
	}
}

#ifdef BUILD_VAAPI_RECORDER
static void
recorder_destroy(struct drm_output *output)
{
	vaapi_recorder_destroy(output->recorder);
	output->recorder = NULL;

	output->base.disable_planes--;

	wl_list_remove(&output->recorder_frame_listener.link);
	weston_log("[libva recorder] done\n");
}

static void
recorder_frame_notify(struct wl_listener *listener, void *data)
{
	struct drm_output *output;
	struct drm_backend *b;
	int fd, ret;

	output = container_of(listener, struct drm_output,
			      recorder_frame_listener);
	b = to_drm_backend(output->base.compositor);

	if (!output->recorder)
		return;

	ret = drmPrimeHandleToFD(b->drm.fd,
				 output->scanout_plane->state_cur->fb->handles[0],
				 DRM_CLOEXEC, &fd);
	if (ret) {
		weston_log("[libva recorder] "
			   "failed to create prime fd for front buffer\n");
		return;
	}

	ret = vaapi_recorder_frame(output->recorder, fd,
				   output->scanout_plane->state_cur->fb->strides[0]);
	if (ret < 0) {
		weston_log("[libva recorder] aborted: %m\n");
		recorder_destroy(output);
	}
}

static void *
create_recorder(struct drm_backend *b, int width, int height,
		const char *filename)
{
	int fd;
	drm_magic_t magic;

	fd = open(b->drm.filename, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		return NULL;

	drmGetMagic(fd, &magic);
	drmAuthMagic(b->drm.fd, magic);

	return vaapi_recorder_create(fd, width, height, filename);
}

static void
recorder_binding(struct weston_keyboard *keyboard, const struct timespec *time,
		 uint32_t key, void *data)
{
	struct drm_backend *b = data;
	struct drm_output *output;
	int width, height;

	output = container_of(b->compositor->output_list.next,
			      struct drm_output, base.link);

	if (!output->recorder) {
		if (output->gbm_format != GBM_FORMAT_XRGB8888) {
			weston_log("failed to start vaapi recorder: "
				   "output format not supported\n");
			return;
		}

		width = output->base.current_mode->width;
		height = output->base.current_mode->height;

		output->recorder =
			create_recorder(b, width, height, "capture.h264");
		if (!output->recorder) {
			weston_log("failed to create vaapi recorder\n");
			return;
		}

		output->base.disable_planes++;

		output->recorder_frame_listener.notify = recorder_frame_notify;
		wl_signal_add(&output->base.frame_signal,
			      &output->recorder_frame_listener);

		weston_output_schedule_repaint(&output->base);

		weston_log("[libva recorder] initialized\n");
	} else {
		recorder_destroy(output);
	}
}
#else
static void
recorder_binding(struct weston_keyboard *keyboard, const struct timespec *time,
		 uint32_t key, void *data)
{
	weston_log("Compiled without libva support\n");
}
#endif

static void
switch_to_gl_renderer(struct drm_backend *b)
{
	struct drm_output *output;
	bool dmabuf_support_inited;

	if (!b->use_pixman)
		return;

	dmabuf_support_inited = !!b->compositor->renderer->import_dmabuf;

	weston_log("Switching to GL renderer\n");

	b->gbm = create_gbm_device(b->drm.fd);
	if (!b->gbm) {
		weston_log("Failed to create gbm device. "
			   "Aborting renderer switch\n");
		return;
	}

	wl_list_for_each(output, &b->compositor->output_list, base.link)
		pixman_renderer_output_destroy(&output->base);

	b->compositor->renderer->destroy(b->compositor);

	if (drm_backend_create_gl_renderer(b) < 0) {
		gbm_device_destroy(b->gbm);
		weston_log("Failed to create GL renderer. Quitting.\n");
		/* FIXME: we need a function to shutdown cleanly */
		assert(0);
	}

	wl_list_for_each(output, &b->compositor->output_list, base.link)
		drm_output_init_egl(output, b);

	b->use_pixman = 0;

	if (!dmabuf_support_inited && b->compositor->renderer->import_dmabuf) {
		if (linux_dmabuf_setup(b->compositor) < 0)
			weston_log("Error: initializing dmabuf "
				   "support failed.\n");
	}
}

static void
renderer_switch_binding(struct weston_keyboard *keyboard,
			const struct timespec *time, uint32_t key, void *data)
{
	struct drm_backend *b =
		to_drm_backend(keyboard->seat->compositor);

	switch_to_gl_renderer(b);
}

static const struct weston_drm_output_api api = {
	drm_output_set_mode,
	drm_output_set_gbm_format,
	drm_output_set_seat,
};

static struct drm_backend *
drm_backend_create(struct weston_compositor *compositor,
		   struct weston_drm_backend_config *config)
{
	struct drm_backend *b;
	struct udev_device *drm_device;
	struct wl_event_loop *loop;
	const char *seat_id = default_seat;
	const char *session_seat;
	int ret;

	session_seat = getenv("XDG_SEAT");
	if (session_seat)
		seat_id = session_seat;

	if (config->seat_id)
		seat_id = config->seat_id;

	weston_log("initializing drm backend\n");

	b = zalloc(sizeof *b);
	if (b == NULL)
		return NULL;

	b->state_invalid = true;
	b->drm.fd = -1;
	wl_array_init(&b->unused_crtcs);

	b->compositor = compositor;
	b->use_pixman = config->use_pixman;
	b->pageflip_timeout = config->pageflip_timeout;
	b->use_pixman_shadow = config->use_pixman_shadow;

	compositor->backend = &b->base;

	if (parse_gbm_format(config->gbm_format, GBM_FORMAT_XRGB8888, &b->gbm_format) < 0)
		goto err_compositor;

	/* Check if we run drm-backend using weston-launch */
	compositor->launcher = weston_launcher_connect(compositor, config->tty,
						       seat_id, true);
	if (compositor->launcher == NULL) {
		weston_log("fatal: drm backend should be run using "
			   "weston-launch binary, or your system should "
			   "provide the logind D-Bus API.\n");
		goto err_compositor;
	}

	b->udev = udev_new();
	if (b->udev == NULL) {
		weston_log("failed to initialize udev context\n");
		goto err_launcher;
	}

	b->session_listener.notify = session_notify;
	wl_signal_add(&compositor->session_signal, &b->session_listener);

	if (config->specific_device)
		drm_device = open_specific_drm_device(b, config->specific_device);
	else
		drm_device = find_primary_gpu(b, seat_id);
	if (drm_device == NULL) {
		weston_log("no drm device found\n");
		goto err_udev;
	}

	if (init_kms_caps(b) < 0) {
		weston_log("failed to initialize kms\n");
		goto err_udev_dev;
	}

	if (b->use_pixman) {
		if (init_pixman(b) < 0) {
			weston_log("failed to initialize pixman renderer\n");
			goto err_udev_dev;
		}
	} else {
		if (init_egl(b) < 0) {
			weston_log("failed to initialize egl\n");
			goto err_udev_dev;
		}
	}

	b->base.destroy = drm_destroy;
	b->base.repaint_begin = drm_repaint_begin;
	b->base.repaint_flush = drm_repaint_flush;
	b->base.repaint_cancel = drm_repaint_cancel;
	b->base.create_output = drm_output_create;

	weston_setup_vt_switch_bindings(compositor);

	wl_list_init(&b->plane_list);
	create_sprites(b);

	if (udev_input_init(&b->input,
			    compositor, b->udev, seat_id,
			    config->configure_device) < 0) {
		weston_log("failed to create input devices\n");
		goto err_sprite;
	}

	if (drm_backend_create_heads(b, drm_device) < 0) {
		weston_log("Failed to create heads for %s\n", b->drm.filename);
		goto err_udev_input;
	}

	/* A this point we have some idea of whether or not we have a working
	 * cursor plane. */
	if (!b->cursors_are_broken)
		compositor->capabilities |= WESTON_CAP_CURSOR_PLANE;

	loop = wl_display_get_event_loop(compositor->wl_display);
	b->drm_source =
		wl_event_loop_add_fd(loop, b->drm.fd,
				     WL_EVENT_READABLE, on_drm_input, b);

	b->udev_monitor = udev_monitor_new_from_netlink(b->udev, "udev");
	if (b->udev_monitor == NULL) {
		weston_log("failed to initialize udev monitor\n");
		goto err_drm_source;
	}
	udev_monitor_filter_add_match_subsystem_devtype(b->udev_monitor,
							"drm", NULL);
	b->udev_drm_source =
		wl_event_loop_add_fd(loop,
				     udev_monitor_get_fd(b->udev_monitor),
				     WL_EVENT_READABLE, udev_drm_event, b);

	if (udev_monitor_enable_receiving(b->udev_monitor) < 0) {
		weston_log("failed to enable udev-monitor receiving\n");
		goto err_udev_monitor;
	}

	udev_device_unref(drm_device);

	weston_compositor_add_debug_binding(compositor, KEY_O,
					    planes_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_C,
					    planes_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_V,
					    planes_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_Q,
					    recorder_binding, b);
	weston_compositor_add_debug_binding(compositor, KEY_W,
					    renderer_switch_binding, b);

	if (compositor->renderer->import_dmabuf) {
		if (linux_dmabuf_setup(compositor) < 0)
			weston_log("Error: initializing dmabuf "
				   "support failed.\n");
	}

	ret = weston_plugin_api_register(compositor, WESTON_DRM_OUTPUT_API_NAME,
					 &api, sizeof(api));

	if (ret < 0) {
		weston_log("Failed to register output API.\n");
		goto err_udev_monitor;
	}

	return b;

err_udev_monitor:
	wl_event_source_remove(b->udev_drm_source);
	udev_monitor_unref(b->udev_monitor);
err_drm_source:
	wl_event_source_remove(b->drm_source);
err_udev_input:
	udev_input_destroy(&b->input);
err_sprite:
	if (b->gbm)
		gbm_device_destroy(b->gbm);
	destroy_sprites(b);
err_udev_dev:
	udev_device_unref(drm_device);
err_launcher:
	weston_launcher_destroy(compositor->launcher);
err_udev:
	udev_unref(b->udev);
err_compositor:
	weston_compositor_shutdown(compositor);
	free(b);
	return NULL;
}

static void
config_init_to_defaults(struct weston_drm_backend_config *config)
{
	config->use_pixman_shadow = true;
}

WL_EXPORT int
weston_backend_init(struct weston_compositor *compositor,
		    struct weston_backend_config *config_base)
{
	struct drm_backend *b;
	struct weston_drm_backend_config config = {{ 0, }};

	if (config_base == NULL ||
	    config_base->struct_version != WESTON_DRM_BACKEND_CONFIG_VERSION ||
	    config_base->struct_size > sizeof(struct weston_drm_backend_config)) {
		weston_log("drm backend config structure is invalid\n");
		return -1;
	}

	config_init_to_defaults(&config);
	memcpy(&config, config_base, config_base->struct_size);

	b = drm_backend_create(compositor, &config);
	if (b == NULL)
		return -1;

	return 0;
}
