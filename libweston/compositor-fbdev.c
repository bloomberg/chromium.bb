/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2012 Raspberry Pi Foundation
 * Copyright © 2013 Philip Withnall
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
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/fb.h>
#include <linux/input.h>

#include <libudev.h>

#include "shared/helpers.h"
#include "compositor.h"
#include "compositor-fbdev.h"
#include "launcher-util.h"
#include "pixman-renderer.h"
#include "libinput-seat.h"
#include "presentation-time-server-protocol.h"

struct fbdev_backend {
	struct weston_backend base;
	struct weston_compositor *compositor;
	uint32_t prev_state;

	struct udev *udev;
	struct udev_input input;
	uint32_t output_transform;
	struct wl_listener session_listener;
};

struct fbdev_screeninfo {
	unsigned int x_resolution; /* pixels, visible area */
	unsigned int y_resolution; /* pixels, visible area */
	unsigned int width_mm; /* visible screen width in mm */
	unsigned int height_mm; /* visible screen height in mm */
	unsigned int bits_per_pixel;

	size_t buffer_length; /* length of frame buffer memory in bytes */
	size_t line_length; /* length of a line in bytes */
	char id[16]; /* screen identifier */

	pixman_format_code_t pixel_format; /* frame buffer pixel format */
	unsigned int refresh_rate; /* Hertz */
};

struct fbdev_output {
	struct fbdev_backend *backend;
	struct weston_output base;

	struct weston_mode mode;
	struct wl_event_source *finish_frame_timer;

	/* Frame buffer details. */
	char *device;
	struct fbdev_screeninfo fb_info;
	void *fb; /* length is fb_info.buffer_length */

	/* pixman details. */
	pixman_image_t *hw_surface;
	uint8_t depth;
};

static const char default_seat[] = "seat0";

static inline struct fbdev_output *
to_fbdev_output(struct weston_output *base)
{
	return container_of(base, struct fbdev_output, base);
}

static inline struct fbdev_backend *
to_fbdev_backend(struct weston_compositor *base)
{
	return container_of(base->backend, struct fbdev_backend, base);
}

static void
fbdev_output_start_repaint_loop(struct weston_output *output)
{
	struct timespec ts;

	weston_compositor_read_presentation_clock(output->compositor, &ts);
	weston_output_finish_frame(output, &ts, WP_PRESENTATION_FEEDBACK_INVALID);
}

static int
fbdev_output_repaint(struct weston_output *base, pixman_region32_t *damage,
		     void *repaint_data)
{
	struct fbdev_output *output = to_fbdev_output(base);
	struct weston_compositor *ec = output->base.compositor;

	/* Repaint the damaged region onto the back buffer. */
	pixman_renderer_output_set_buffer(base, output->hw_surface);
	ec->renderer->repaint_output(base, damage);

	/* Update the damage region. */
	pixman_region32_subtract(&ec->primary_plane.damage,
	                         &ec->primary_plane.damage, damage);

	/* Schedule the end of the frame. We do not sync this to the frame
	 * buffer clock because users who want that should be using the DRM
	 * compositor. FBIO_WAITFORVSYNC blocks and FB_ACTIVATE_VBL requires
	 * panning, which is broken in most kernel drivers.
	 *
	 * Finish the frame synchronised to the specified refresh rate. The
	 * refresh rate is given in mHz and the interval in ms. */
	wl_event_source_timer_update(output->finish_frame_timer,
	                             1000000 / output->mode.refresh);

	return 0;
}

static int
finish_frame_handler(void *data)
{
	struct fbdev_output *output = data;
	struct timespec ts;

	weston_compositor_read_presentation_clock(output->base.compositor, &ts);
	weston_output_finish_frame(&output->base, &ts, 0);

	return 1;
}

static pixman_format_code_t
calculate_pixman_format(struct fb_var_screeninfo *vinfo,
                        struct fb_fix_screeninfo *finfo)
{
	/* Calculate the pixman format supported by the frame buffer from the
	 * buffer's metadata. Return 0 if no known pixman format is supported
	 * (since this has depth 0 it's guaranteed to not conflict with any
	 * actual pixman format).
	 *
	 * Documentation on the vinfo and finfo structures:
	 *    http://www.mjmwired.net/kernel/Documentation/fb/api.txt
	 *
	 * TODO: Try a bit harder to support other formats, including setting
	 * the preferred format in the hardware. */
	int type;

	weston_log("Calculating pixman format from:\n"
	           STAMP_SPACE " - type: %i (aux: %i)\n"
	           STAMP_SPACE " - visual: %i\n"
	           STAMP_SPACE " - bpp: %i (grayscale: %i)\n"
	           STAMP_SPACE " - red: offset: %i, length: %i, MSB: %i\n"
	           STAMP_SPACE " - green: offset: %i, length: %i, MSB: %i\n"
	           STAMP_SPACE " - blue: offset: %i, length: %i, MSB: %i\n"
	           STAMP_SPACE " - transp: offset: %i, length: %i, MSB: %i\n",
	           finfo->type, finfo->type_aux, finfo->visual,
	           vinfo->bits_per_pixel, vinfo->grayscale,
	           vinfo->red.offset, vinfo->red.length, vinfo->red.msb_right,
	           vinfo->green.offset, vinfo->green.length,
	           vinfo->green.msb_right,
	           vinfo->blue.offset, vinfo->blue.length,
	           vinfo->blue.msb_right,
	           vinfo->transp.offset, vinfo->transp.length,
	           vinfo->transp.msb_right);

	/* We only handle packed formats at the moment. */
	if (finfo->type != FB_TYPE_PACKED_PIXELS)
		return 0;

	/* We only handle true-colour frame buffers at the moment. */
	switch(finfo->visual) {
		case FB_VISUAL_TRUECOLOR:
		case FB_VISUAL_DIRECTCOLOR:
			if (vinfo->grayscale != 0)
				return 0;
		break;
		default:
			return 0;
	}

	/* We only support formats with MSBs on the left. */
	if (vinfo->red.msb_right != 0 || vinfo->green.msb_right != 0 ||
	    vinfo->blue.msb_right != 0)
		return 0;

	/* Work out the format type from the offsets. We only support RGBA and
	 * ARGB at the moment. */
	type = PIXMAN_TYPE_OTHER;

	if ((vinfo->transp.offset >= vinfo->red.offset ||
	     vinfo->transp.length == 0) &&
	    vinfo->red.offset >= vinfo->green.offset &&
	    vinfo->green.offset >= vinfo->blue.offset)
		type = PIXMAN_TYPE_ARGB;
	else if (vinfo->red.offset >= vinfo->green.offset &&
	         vinfo->green.offset >= vinfo->blue.offset &&
	         vinfo->blue.offset >= vinfo->transp.offset)
		type = PIXMAN_TYPE_RGBA;

	if (type == PIXMAN_TYPE_OTHER)
		return 0;

	/* Build the format. */
	return PIXMAN_FORMAT(vinfo->bits_per_pixel, type,
	                     vinfo->transp.length,
	                     vinfo->red.length,
	                     vinfo->green.length,
	                     vinfo->blue.length);
}

static int
calculate_refresh_rate(struct fb_var_screeninfo *vinfo)
{
	uint64_t quot;

	/* Calculate monitor refresh rate. Default is 60 Hz. Units are mHz. */
	quot = (vinfo->upper_margin + vinfo->lower_margin + vinfo->yres);
	quot *= (vinfo->left_margin + vinfo->right_margin + vinfo->xres);
	quot *= vinfo->pixclock;

	if (quot > 0) {
		uint64_t refresh_rate;

		refresh_rate = 1000000000000000LLU / quot;
		if (refresh_rate > 200000)
			refresh_rate = 200000; /* cap at 200 Hz */

		if (refresh_rate >= 1000) /* at least 1 Hz */
			return refresh_rate;
	}

	return 60 * 1000; /* default to 60 Hz */
}

static int
fbdev_query_screen_info(struct fbdev_output *output, int fd,
                        struct fbdev_screeninfo *info)
{
	struct fb_var_screeninfo varinfo;
	struct fb_fix_screeninfo fixinfo;

	/* Probe the device for screen information. */
	if (ioctl(fd, FBIOGET_FSCREENINFO, &fixinfo) < 0 ||
	    ioctl(fd, FBIOGET_VSCREENINFO, &varinfo) < 0) {
		return -1;
	}

	/* Store the pertinent data. */
	info->x_resolution = varinfo.xres;
	info->y_resolution = varinfo.yres;
	info->width_mm = varinfo.width;
	info->height_mm = varinfo.height;
	info->bits_per_pixel = varinfo.bits_per_pixel;

	info->buffer_length = fixinfo.smem_len;
	info->line_length = fixinfo.line_length;
	strncpy(info->id, fixinfo.id, sizeof(info->id));
	info->id[sizeof(info->id)-1] = '\0';

	info->pixel_format = calculate_pixman_format(&varinfo, &fixinfo);
	info->refresh_rate = calculate_refresh_rate(&varinfo);

	if (info->pixel_format == 0) {
		weston_log("Frame buffer uses an unsupported format.\n");
		return -1;
	}

	return 1;
}

static int
fbdev_set_screen_info(struct fbdev_output *output, int fd,
                      struct fbdev_screeninfo *info)
{
	struct fb_var_screeninfo varinfo;

	/* Grab the current screen information. */
	if (ioctl(fd, FBIOGET_VSCREENINFO, &varinfo) < 0) {
		return -1;
	}

	/* Update the information. */
	varinfo.xres = info->x_resolution;
	varinfo.yres = info->y_resolution;
	varinfo.width = info->width_mm;
	varinfo.height = info->height_mm;
	varinfo.bits_per_pixel = info->bits_per_pixel;

	/* Try to set up an ARGB (x8r8g8b8) pixel format. */
	varinfo.grayscale = 0;
	varinfo.transp.offset = 24;
	varinfo.transp.length = 0;
	varinfo.transp.msb_right = 0;
	varinfo.red.offset = 16;
	varinfo.red.length = 8;
	varinfo.red.msb_right = 0;
	varinfo.green.offset = 8;
	varinfo.green.length = 8;
	varinfo.green.msb_right = 0;
	varinfo.blue.offset = 0;
	varinfo.blue.length = 8;
	varinfo.blue.msb_right = 0;

	/* Set the device's screen information. */
	if (ioctl(fd, FBIOPUT_VSCREENINFO, &varinfo) < 0) {
		return -1;
	}

	return 1;
}

static void fbdev_frame_buffer_destroy(struct fbdev_output *output);

/* Returns an FD for the frame buffer device. */
static int
fbdev_frame_buffer_open(struct fbdev_output *output, const char *fb_dev,
                        struct fbdev_screeninfo *screen_info)
{
	int fd = -1;

	weston_log("Opening fbdev frame buffer.\n");

	/* Open the frame buffer device. */
	fd = open(fb_dev, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		weston_log("Failed to open frame buffer device ‘%s’: %s\n",
		           fb_dev, strerror(errno));
		return -1;
	}

	/* Grab the screen info. */
	if (fbdev_query_screen_info(output, fd, screen_info) < 0) {
		weston_log("Failed to get frame buffer info: %s\n",
		           strerror(errno));

		close(fd);
		return -1;
	}

	return fd;
}

/* Closes the FD on success or failure. */
static int
fbdev_frame_buffer_map(struct fbdev_output *output, int fd)
{
	int retval = -1;

	weston_log("Mapping fbdev frame buffer.\n");

	/* Map the frame buffer. Write-only mode, since we don't want to read
	 * anything back (because it's slow). */
	output->fb = mmap(NULL, output->fb_info.buffer_length,
	                  PROT_WRITE, MAP_SHARED, fd, 0);
	if (output->fb == MAP_FAILED) {
		weston_log("Failed to mmap frame buffer: %s\n",
		           strerror(errno));
		goto out_close;
	}

	/* Create a pixman image to wrap the memory mapped frame buffer. */
	output->hw_surface =
		pixman_image_create_bits(output->fb_info.pixel_format,
		                         output->fb_info.x_resolution,
		                         output->fb_info.y_resolution,
		                         output->fb,
		                         output->fb_info.line_length);
	if (output->hw_surface == NULL) {
		weston_log("Failed to create surface for frame buffer.\n");
		goto out_unmap;
	}

	/* Success! */
	retval = 0;

out_unmap:
	if (retval != 0 && output->fb != NULL)
		fbdev_frame_buffer_destroy(output);

out_close:
	if (fd >= 0)
		close(fd);

	return retval;
}

static void
fbdev_frame_buffer_destroy(struct fbdev_output *output)
{
	weston_log("Destroying fbdev frame buffer.\n");

	if (munmap(output->fb, output->fb_info.buffer_length) < 0)
		weston_log("Failed to munmap frame buffer: %s\n",
		           strerror(errno));

	output->fb = NULL;
}

static void fbdev_output_destroy(struct weston_output *base);
static void fbdev_output_disable(struct weston_output *base);

static int
fbdev_output_enable(struct weston_output *base)
{
	struct fbdev_output *output = to_fbdev_output(base);
	struct fbdev_backend *backend = to_fbdev_backend(base->compositor);
	int fb_fd;
	struct wl_event_loop *loop;

	/* Create the frame buffer. */
	fb_fd = fbdev_frame_buffer_open(output, output->device, &output->fb_info);
	if (fb_fd < 0) {
		weston_log("Creating frame buffer failed.\n");
		return -1;
	}

	if (fbdev_frame_buffer_map(output, fb_fd) < 0) {
		weston_log("Mapping frame buffer failed.\n");
		return -1;
	}

	output->base.start_repaint_loop = fbdev_output_start_repaint_loop;
	output->base.repaint = fbdev_output_repaint;

	if (pixman_renderer_output_create(&output->base) < 0)
		goto out_hw_surface;

	loop = wl_display_get_event_loop(backend->compositor->wl_display);
	output->finish_frame_timer =
		wl_event_loop_add_timer(loop, finish_frame_handler, output);

	weston_log("fbdev output %d×%d px\n",
	           output->mode.width, output->mode.height);
	weston_log_continue(STAMP_SPACE "guessing %d Hz and 96 dpi\n",
	                    output->mode.refresh / 1000);

	return 0;

out_hw_surface:
	pixman_image_unref(output->hw_surface);
	output->hw_surface = NULL;
	fbdev_frame_buffer_destroy(output);

	return -1;
}

static int
fbdev_output_disable_handler(struct weston_output *base)
{
	if (!base->enabled)
		return 0;

	/* Close the frame buffer. */
	fbdev_output_disable(base);

	if (base->renderer_state != NULL)
		pixman_renderer_output_destroy(base);

	return 0;
}

static int
fbdev_output_create(struct fbdev_backend *backend,
                    const char *device)
{
	struct fbdev_output *output;
	int fb_fd;

	weston_log("Creating fbdev output.\n");

	output = zalloc(sizeof *output);
	if (output == NULL)
		return -1;

	output->backend = backend;
	output->device = strdup(device);

	/* Create the frame buffer. */
	fb_fd = fbdev_frame_buffer_open(output, device, &output->fb_info);
	if (fb_fd < 0) {
		weston_log("Creating frame buffer failed.\n");
		goto out_free;
	}

	output->base.name = strdup("fbdev");
	output->base.destroy = fbdev_output_destroy;
	output->base.disable = fbdev_output_disable_handler;
	output->base.enable = fbdev_output_enable;

	weston_output_init(&output->base, backend->compositor);

	/* only one static mode in list */
	output->mode.flags =
		WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED;
	output->mode.width = output->fb_info.x_resolution;
	output->mode.height = output->fb_info.y_resolution;
	output->mode.refresh = output->fb_info.refresh_rate;
	wl_list_init(&output->base.mode_list);
	wl_list_insert(&output->base.mode_list, &output->mode.link);

	output->base.current_mode = &output->mode;
	output->base.subpixel = WL_OUTPUT_SUBPIXEL_UNKNOWN;
	output->base.make = "unknown";
	output->base.model = output->fb_info.id;

	output->base.mm_width = output->fb_info.width_mm;
	output->base.mm_height = output->fb_info.height_mm;

	close(fb_fd);

	weston_compositor_add_pending_output(&output->base, backend->compositor);

	return 0;

out_free:
	free(output->device);
	free(output);

	return -1;
}

static void
fbdev_output_destroy(struct weston_output *base)
{
	struct fbdev_output *output = to_fbdev_output(base);

	weston_log("Destroying fbdev output.\n");

	fbdev_output_disable_handler(base);

	/* Remove the output. */
	weston_output_destroy(&output->base);

	free(output->device);
	free(output);
}

/* strcmp()-style return values. */
static int
compare_screen_info (const struct fbdev_screeninfo *a,
                     const struct fbdev_screeninfo *b)
{
	if (a->x_resolution == b->x_resolution &&
	    a->y_resolution == b->y_resolution &&
	    a->width_mm == b->width_mm &&
	    a->height_mm == b->height_mm &&
	    a->bits_per_pixel == b->bits_per_pixel &&
	    a->pixel_format == b->pixel_format &&
	    a->refresh_rate == b->refresh_rate)
		return 0;

	return 1;
}

static int
fbdev_output_reenable(struct fbdev_backend *backend,
                      struct weston_output *base)
{
	struct fbdev_output *output = to_fbdev_output(base);
	struct fbdev_screeninfo new_screen_info;
	int fb_fd;
	char *device;

	weston_log("Re-enabling fbdev output.\n");

	/* Create the frame buffer. */
	fb_fd = fbdev_frame_buffer_open(output, output->device,
	                                &new_screen_info);
	if (fb_fd < 0) {
		weston_log("Creating frame buffer failed.\n");
		goto err;
	}

	/* Check whether the frame buffer details have changed since we were
	 * disabled. */
	if (compare_screen_info (&output->fb_info, &new_screen_info) != 0) {
		/* Perform a mode-set to restore the old mode. */
		if (fbdev_set_screen_info(output, fb_fd,
		                          &output->fb_info) < 0) {
			weston_log("Failed to restore mode settings. "
			           "Attempting to re-open output anyway.\n");
		}

		close(fb_fd);

		/* Remove and re-add the output so that resources depending on
		 * the frame buffer X/Y resolution (such as the shadow buffer)
		 * are re-initialised. */
		device = strdup(output->device);
		fbdev_output_destroy(&output->base);
		fbdev_output_create(backend, device);
		free(device);

		return 0;
	}

	/* Map the device if it has the same details as before. */
	if (fbdev_frame_buffer_map(output, fb_fd) < 0) {
		weston_log("Mapping frame buffer failed.\n");
		goto err;
	}

	return 0;

err:
	return -1;
}

/* NOTE: This leaves output->fb_info populated, caching data so that if
 * fbdev_output_reenable() is called again, it can determine whether a mode-set
 * is needed. */
static void
fbdev_output_disable(struct weston_output *base)
{
	struct fbdev_output *output = to_fbdev_output(base);

	weston_log("Disabling fbdev output.\n");

	if (output->hw_surface != NULL) {
		pixman_image_unref(output->hw_surface);
		output->hw_surface = NULL;
	}

	fbdev_frame_buffer_destroy(output);
}

static void
fbdev_backend_destroy(struct weston_compositor *base)
{
	struct fbdev_backend *backend = to_fbdev_backend(base);

	udev_input_destroy(&backend->input);

	/* Destroy the output. */
	weston_compositor_shutdown(base);

	/* Chain up. */
	weston_launcher_destroy(base->launcher);

	free(backend);
}

static void
session_notify(struct wl_listener *listener, void *data)
{
	struct weston_compositor *compositor = data;
	struct fbdev_backend *backend = to_fbdev_backend(compositor);
	struct weston_output *output;

	if (compositor->session_active) {
		weston_log("entering VT\n");
		compositor->state = backend->prev_state;

		wl_list_for_each(output, &compositor->output_list, link) {
			fbdev_output_reenable(backend, output);
		}

		weston_compositor_damage_all(compositor);

		udev_input_enable(&backend->input);
	} else {
		weston_log("leaving VT\n");
		udev_input_disable(&backend->input);

		wl_list_for_each(output, &compositor->output_list, link) {
			fbdev_output_disable(output);
		}

		backend->prev_state = compositor->state;
		weston_compositor_offscreen(compositor);

		/* If we have a repaint scheduled (from the idle handler), make
		 * sure we cancel that so we don't try to pageflip when we're
		 * vt switched away.  The OFFSCREEN state will prevent
		 * further attempts at repainting.  When we switch
		 * back, we schedule a repaint, which will process
		 * pending frame callbacks. */

		wl_list_for_each(output,
				 &compositor->output_list, link) {
			output->repaint_needed = false;
		}
	}
}

static void
fbdev_restore(struct weston_compositor *compositor)
{
	weston_launcher_restore(compositor->launcher);
}

static struct fbdev_backend *
fbdev_backend_create(struct weston_compositor *compositor,
                     struct weston_fbdev_backend_config *param)
{
	struct fbdev_backend *backend;
	const char *seat_id = default_seat;

	weston_log("initializing fbdev backend\n");

	backend = zalloc(sizeof *backend);
	if (backend == NULL)
		return NULL;

	backend->compositor = compositor;
	if (weston_compositor_set_presentation_clock_software(
							compositor) < 0)
		goto out_compositor;

	backend->udev = udev_new();
	if (backend->udev == NULL) {
		weston_log("Failed to initialize udev context.\n");
		goto out_compositor;
	}

	/* Set up the TTY. */
	backend->session_listener.notify = session_notify;
	wl_signal_add(&compositor->session_signal,
		      &backend->session_listener);
	compositor->launcher =
		weston_launcher_connect(compositor, param->tty, "seat0", false);
	if (!compositor->launcher) {
		weston_log("fatal: fbdev backend should be run "
			   "using weston-launch binary or as root\n");
		goto out_udev;
	}

	backend->base.destroy = fbdev_backend_destroy;
	backend->base.restore = fbdev_restore;

	backend->prev_state = WESTON_COMPOSITOR_ACTIVE;

	weston_setup_vt_switch_bindings(compositor);

	if (pixman_renderer_init(compositor) < 0)
		goto out_launcher;

	if (fbdev_output_create(backend, param->device) < 0)
		goto out_launcher;

	udev_input_init(&backend->input, compositor, backend->udev,
			seat_id, param->configure_device);

	compositor->backend = &backend->base;
	return backend;

out_launcher:
	weston_launcher_destroy(compositor->launcher);

out_udev:
	udev_unref(backend->udev);

out_compositor:
	weston_compositor_shutdown(compositor);
	free(backend);

	return NULL;
}

static void
config_init_to_defaults(struct weston_fbdev_backend_config *config)
{
	/* TODO: Ideally, available frame buffers should be enumerated using
	 * udev, rather than passing a device node in as a parameter. */
	config->tty = 0; /* default to current tty */
	config->device = "/dev/fb0"; /* default frame buffer */
}

WL_EXPORT int
weston_backend_init(struct weston_compositor *compositor,
		    struct weston_backend_config *config_base)
{
	struct fbdev_backend *b;
	struct weston_fbdev_backend_config config = {{ 0, }};

	if (config_base == NULL ||
	    config_base->struct_version != WESTON_FBDEV_BACKEND_CONFIG_VERSION ||
	    config_base->struct_size > sizeof(struct weston_fbdev_backend_config)) {
		weston_log("fbdev backend config structure is invalid\n");
		return -1;
	}

	config_init_to_defaults(&config);
	memcpy(&config, config_base, config_base->struct_size);

	b = fbdev_backend_create(compositor, &config);
	if (b == NULL)
		return -1;
	return 0;
}
