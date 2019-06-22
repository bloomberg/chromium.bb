/*
 * Copyright © 2008-2011 Kristian Høgsberg
 * Copyright © 2011 Intel Corporation
 * Copyright © 2017, 2018 Collabora, Ltd.
 * Copyright © 2017, 2018 General Electric Company
 * Copyright (c) 2018 DisplayLink (UK) Ltd.
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

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <libweston/libweston.h>
#include <libweston/backend-drm.h>
#include <libweston/pixel-formats.h>

#include "drm-internal.h"

#include "linux-dmabuf.h"
#include "presentation-time-server-protocol.h"

enum drm_output_propose_state_mode {
	DRM_OUTPUT_PROPOSE_STATE_MIXED, /**< mix renderer & planes */
	DRM_OUTPUT_PROPOSE_STATE_RENDERER_ONLY, /**< only assign to renderer & cursor */
	DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY, /**< no renderer use, only planes */
};

static const char *const drm_output_propose_state_mode_as_string[] = {
	[DRM_OUTPUT_PROPOSE_STATE_MIXED] = "mixed state",
	[DRM_OUTPUT_PROPOSE_STATE_RENDERER_ONLY] = "render-only state",
	[DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY]	= "plane-only state"
};

static const char *
drm_propose_state_mode_to_string(enum drm_output_propose_state_mode mode)
{
	if (mode < 0 || mode >= ARRAY_LENGTH(drm_output_propose_state_mode_as_string))
		return " unknown compositing mode";

	return drm_output_propose_state_mode_as_string[mode];
}

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
	enum {
		NO_PLANES,
		NO_PLANES_WITH_FORMAT,
		NO_PLANES_ACCEPTED,
		PLACED_ON_PLANE,
	} availability = NO_PLANES;

	assert(!b->sprites_are_broken);
	assert(b->atomic_modeset);

	fb = drm_fb_get_from_view(output_state, ev);
	if (!fb) {
		drm_debug(b, "\t\t\t\t[overlay] not placing view %p on overlay: "
			     " couldn't get fb\n", ev);
		return NULL;
	}

	wl_list_for_each(p, &b->plane_list, link) {
		if (p->type != WDRM_PLANE_TYPE_OVERLAY)
			continue;

		if (!drm_plane_is_available(p, output))
			continue;

		state = drm_output_state_get_plane(output_state, p);
		if (state->fb) {
			state = NULL;
			continue;
		}

		if (availability == NO_PLANES)
			availability = NO_PLANES_WITH_FORMAT;

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
		if (i == p->count_formats) {
			drm_plane_state_put_back(state);
			state = NULL;
			continue;
		}

		if (availability == NO_PLANES_WITH_FORMAT)
			availability = NO_PLANES_ACCEPTED;

		state->ev = ev;
		state->output = output;
		if (!drm_plane_state_coords_for_view(state, ev)) {
			drm_debug(b, "\t\t\t\t[overlay] not placing view %p on overlay: "
				     "unsuitable transform\n", ev);
			drm_plane_state_put_back(state);
			state = NULL;
			continue;
		}

		/* If the surface buffer has an in-fence fd, but the plane
		 * doesn't support fences, we can't place the buffer on this
		 * plane. */
		if (ev->surface->acquire_fence_fd >= 0 &&
		     p->props[WDRM_PLANE_IN_FENCE_FD].prop_id == 0) {
			drm_debug(b, "\t\t\t\t[overlay] not placing view %p on overlay: "
				     "no in-fence support\n", ev);
			drm_plane_state_put_back(state);
			state = NULL;
			continue;
		}

		/* We hold one reference for the lifetime of this function;
		 * from calling drm_fb_get_from_view, to the out label where
		 * we unconditionally drop the reference. So, we take another
		 * reference here to live within the state. */
		state->fb = drm_fb_ref(fb);

		state->in_fence_fd = ev->surface->acquire_fence_fd;

		/* In planes-only mode, we don't have an incremental state to
		 * test against, so we just hope it'll work. */
		if (mode == DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY) {
			drm_debug(b, "\t\t\t\t[overlay] provisionally placing "
				     "view %p on overlay %lu in planes-only mode\n",
				  ev, (unsigned long) p->plane_id);
			availability = PLACED_ON_PLANE;
			goto out;
		}

		ret = drm_pending_state_test(output_state->pending_state);
		if (ret == 0) {
			drm_debug(b, "\t\t\t\t[overlay] provisionally placing "
				     "view %p on overlay %d in mixed mode\n",
				  ev, p->plane_id);
			availability = PLACED_ON_PLANE;
			goto out;
		}

		drm_debug(b, "\t\t\t\t[overlay] not placing view %p on overlay %lu "
			     "in mixed mode: kernel test failed\n",
			  ev, (unsigned long) p->plane_id);

		drm_plane_state_put_back(state);
		state = NULL;
	}

	switch (availability) {
	case NO_PLANES:
		drm_debug(b, "\t\t\t\t[overlay] not placing view %p on overlay: "
			     "no free overlay planes\n", ev);
		break;
	case NO_PLANES_WITH_FORMAT:
		drm_debug(b, "\t\t\t\t[overlay] not placing view %p on overlay: "
			     "no free overlay planes matching format %s (0x%lx) "
			     "modifier 0x%llx\n",
			  ev, fb->format->drm_format_name,
			  (unsigned long) fb->format,
			  (unsigned long long) fb->modifier);
		break;
	case NO_PLANES_ACCEPTED:
	case PLACED_ON_PLANE:
		break;
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
		weston_log("failed update cursor: %s\n", strerror(errno));
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

	if (ev->surface->buffer_ref.buffer == NULL) {
		drm_debug(b, "\t\t\t\t[cursor] not assigning view %p to cursor plane "
			     "(no buffer available)\n", ev);
		return NULL;
	}
	shmbuf = wl_shm_buffer_get(ev->surface->buffer_ref.buffer->resource);
	if (!shmbuf) {
		drm_debug(b, "\t\t\t\t[cursor] not assigning view %p to cursor plane "
			     "(buffer isn't SHM)\n", ev);
		return NULL;
	}
	if (wl_shm_buffer_get_format(shmbuf) != WL_SHM_FORMAT_ARGB8888) {
		drm_debug(b, "\t\t\t\t[cursor] not assigning view %p to cursor plane "
			     "(format 0x%lx unsuitable)\n",
			  ev, (unsigned long) wl_shm_buffer_get_format(shmbuf));
		return NULL;
	}

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
	    plane_state->src_h != plane_state->dest_h << 16) {
		drm_debug(b, "\t\t\t\t[cursor] not assigning view %p to cursor plane "
			     "(positioning requires cropping or scaling)\n", ev);
		goto err;
	}

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

	if (needs_update) {
		drm_debug(b, "\t\t\t\t[cursor] copying new content to cursor BO\n");
		cursor_bo_update(plane_state, ev);
	}

	/* The cursor API is somewhat special: in cursor_bo_update(), we upload
	 * a buffer which is always cursor_width x cursor_height, even if the
	 * surface we want to promote is actually smaller than this. Manually
	 * mangle the plane state to deal with this. */
	plane_state->src_w = b->cursor_width << 16;
	plane_state->src_h = b->cursor_height << 16;
	plane_state->dest_w = b->cursor_width;
	plane_state->dest_h = b->cursor_height;

	drm_debug(b, "\t\t\t\t[cursor] provisionally assigned view %p to cursor\n",
		  ev);

	return plane_state;

err:
	drm_plane_state_put_back(plane_state);
	return NULL;
}

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
	assert(b->atomic_modeset);
	assert(mode == DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY);

	/* Check the view spans exactly the output size, calculated in the
	 * logical co-ordinate space. */
	extents = pixman_region32_extents(&ev->transform.boundingbox);
	if (extents->x1 != output->base.x ||
	    extents->y1 != output->base.y ||
	    extents->x2 != output->base.x + output->base.width ||
	    extents->y2 != output->base.y + output->base.height)
		return NULL;

	/* If the surface buffer has an in-fence fd, but the plane doesn't
	 * support fences, we can't place the buffer on this plane. */
	if (ev->surface->acquire_fence_fd >= 0 &&
	    scanout_plane->props[WDRM_PLANE_IN_FENCE_FD].prop_id == 0)
		return NULL;

	fb = drm_fb_get_from_view(output_state, ev);
	if (!fb) {
		drm_debug(b, "\t\t\t\t[scanout] not placing view %p on scanout: "
			     " couldn't get fb\n", ev);
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

	state->in_fence_fd = ev->surface->acquire_fence_fd;

	/* In plane-only mode, we don't need to test the state now, as we
	 * will only test it once at the end. */
	return state;

err:
	drm_plane_state_put_back(state);
	return NULL;
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
			drm_debug(b, "\t\t[state] cannot propose mixed mode: "
			             "for output %s (%lu): no previous renderer "
			             "fb\n",
				  output->base.name,
				  (unsigned long) output->base.id);
			drm_output_state_free(state);
			return NULL;
		}

		if (scanout_fb->width != output_base->current_mode->width ||
		    scanout_fb->height != output_base->current_mode->height) {
			drm_debug(b, "\t\t[state] cannot propose mixed mode "
			             "for output %s (%lu): previous fb has "
				     "different size\n",
				  output->base.name,
				  (unsigned long) output->base.id);
			drm_output_state_free(state);
			return NULL;
		}

		scanout_state = drm_plane_state_duplicate(state,
							  plane->state_cur);
		drm_debug(b, "\t\t[state] using renderer FB ID %lu for mixed "
			     "mode for output %s (%lu)\n",
			  (unsigned long) scanout_fb->fb_id, output->base.name,
			  (unsigned long) output->base.id);
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

		drm_debug(b, "\t\t\t[view] evaluating view %p for "
		             "output %s (%lu)\n",
		          ev, output->base.name,
			  (unsigned long) output->base.id);

		/* If this view doesn't touch our output at all, there's no
		 * reason to do anything with it. */
		if (!(ev->output_mask & (1u << output->base.id))) {
			drm_debug(b, "\t\t\t\t[view] ignoring view %p "
			             "(not on our output)\n", ev);
			continue;
		}

		/* We only assign planes to views which are exclusively present
		 * on our output. */
		if (ev->output_mask != (1u << output->base.id)) {
			drm_debug(b, "\t\t\t\t[view] not assigning view %p to plane "
			             "(on multiple outputs)\n", ev);
			force_renderer = true;
		}

		if (!ev->surface->buffer_ref.buffer) {
			drm_debug(b, "\t\t\t\t[view] not assigning view %p to plane "
			             "(no buffer available)\n", ev);
			force_renderer = true;
		}

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
			drm_debug(b, "\t\t\t\t[view] ignoring view %p "
			             "(occluded on our output)\n", ev);
			pixman_region32_fini(&surface_overlap);
			pixman_region32_fini(&clipped_view);
			continue;
		}

		/* Since we process views from top to bottom, we know that if
		 * the view intersects the calculated renderer region, it must
		 * be part of, or occluded by, it, and cannot go on a plane. */
		pixman_region32_intersect(&surface_overlap, &renderer_region,
					  &clipped_view);
		if (pixman_region32_not_empty(&surface_overlap)) {
			drm_debug(b, "\t\t\t\t[view] not assigning view %p to plane "
			             "(occluded by renderer views)\n", ev);
			force_renderer = true;
		}

		/* We do not control the stacking order of overlay planes;
		 * the scanout plane is strictly stacked bottom and the cursor
		 * plane top, but the ordering of overlay planes with respect
		 * to each other is undefined. Make sure we do not have two
		 * planes overlapping each other. */
		pixman_region32_intersect(&surface_overlap, &occluded_region,
					  &clipped_view);
		if (pixman_region32_not_empty(&surface_overlap)) {
			drm_debug(b, "\t\t\t\t[view] not assigning view %p to plane "
			             "(occluded by other overlay planes)\n", ev);
			overlay_occluded = true;
		}
		pixman_region32_fini(&surface_overlap);

		/* The cursor plane is 'special' in the sense that we can still
		 * place it in the legacy API, and we gate that with a separate
		 * cursors_are_broken flag. */
		if (!force_renderer && !overlay_occluded && !b->cursors_are_broken)
			ps = drm_output_prepare_cursor_view(state, ev);

		/* If sprites are disabled or the view is not fully opaque, we
		 * must put the view into the renderer - unless it has already
		 * been placed in the cursor plane, which can handle alpha. */
		if (!ps && !planes_ok) {
			drm_debug(b, "\t\t\t\t[view] not assigning view %p to plane "
			             "(precluded by mode)\n", ev);
			force_renderer = true;
		}
		if (!ps && !weston_view_is_opaque(ev, &clipped_view)) {
			drm_debug(b, "\t\t\t\t[view] not assigning view %p to plane "
			             "(view not fully opaque)\n", ev);
			force_renderer = true;
		}

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
			drm_debug(b, "\t\t[view] failing state generation: "
				      "placing view %p to renderer not allowed\n",
				  ev);
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
	if (ret != 0) {
		drm_debug(b, "\t\t[view] failing state generation: "
			     "atomic test not OK\n");
		goto err;
	}

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

void
drm_assign_planes(struct weston_output *output_base, void *repaint_data)
{
	struct drm_backend *b = to_drm_backend(output_base->compositor);
	struct drm_pending_state *pending_state = repaint_data;
	struct drm_output *output = to_drm_output(output_base);
	struct drm_output_state *state = NULL;
	struct drm_plane_state *plane_state;
	struct weston_view *ev;
	struct weston_plane *primary = &output_base->compositor->primary_plane;
	enum drm_output_propose_state_mode mode = DRM_OUTPUT_PROPOSE_STATE_PLANES_ONLY;

	drm_debug(b, "\t[repaint] preparing state for output %s (%lu)\n",
		  output_base->name, (unsigned long) output_base->id);

	if (!b->sprites_are_broken && !output->virtual) {
		drm_debug(b, "\t[repaint] trying planes-only build state\n");
		state = drm_output_propose_state(output_base, pending_state, mode);
		if (!state) {
			drm_debug(b, "\t[repaint] could not build planes-only "
				     "state, trying mixed\n");
			mode = DRM_OUTPUT_PROPOSE_STATE_MIXED;
			state = drm_output_propose_state(output_base,
							 pending_state,
							 mode);
		}
		if (!state) {
			drm_debug(b, "\t[repaint] could not build mixed-mode "
				     "state, trying renderer-only\n");
		}
	} else {
		drm_debug(b, "\t[state] no overlay plane support\n");
	}

	if (!state) {
		mode = DRM_OUTPUT_PROPOSE_STATE_RENDERER_ONLY;
		state = drm_output_propose_state(output_base, pending_state,
						 mode);
	}

	assert(state);
	drm_debug(b, "\t[repaint] Using %s composition\n",
		  drm_propose_state_mode_to_string(mode));

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

		if (target_plane) {
			drm_debug(b, "\t[repaint] view %p on %s plane %lu\n",
				  ev, plane_type_enums[target_plane->type].name,
				  (unsigned long) target_plane->plane_id);
			weston_view_move_to_plane(ev, &target_plane->base);
		} else {
			drm_debug(b, "\t[repaint] view %p using renderer "
				     "composition\n", ev);
			weston_view_move_to_plane(ev, primary);
		}

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
