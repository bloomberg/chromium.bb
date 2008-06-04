/* (c) 2006-2007 Intel Corporation
 * Copyright (c) 2007 Dave Airlie <airlied@linux.ie>
 *
 * DRM core CRTC related functions
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 *
 * Authors:
 *      Keith Packard
 *	Eric Anholt <eric@anholt.net>
 *      Dave Airlie <airlied@linux.ie>
 *      Jesse Barnes <jesse.barnes@intel.com>
 */

#include "drmP.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

/*
 * Detailed mode info for a standard 640x480@60Hz monitor
 */
static struct drm_display_mode std_mode[] = {
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DEFAULT, 25200, 640, 656,
		   752, 800, 0, 480, 490, 492, 525, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 640x480@60Hz */
};

/**
 * drm_helper_probe_connector_modes - get complete set of display modes
 * @dev: DRM device
 * @maxX: max width for modes
 * @maxY: max height for modes
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Based on @dev's mode_config layout, scan all the connectors and try to detect
 * modes on them.  Modes will first be added to the connector's probed_modes
 * list, then culled (based on validity and the @maxX, @maxY parameters) and
 * put into the normal modes list.
 *
 * Intended to be used either at bootup time or when major configuration
 * changes have occurred.
 *
 * FIXME: take into account monitor limits
 */
void drm_helper_probe_single_connector_modes(struct drm_connector *connector, uint32_t maxX, uint32_t maxY)
{
	struct drm_device *dev = connector->dev;
	struct drm_display_mode *mode, *t;
	struct drm_connector_helper_funcs *connector_funcs = connector->helper_private;
	int ret;

	/* set all modes to the unverified state */
	list_for_each_entry_safe(mode, t, &connector->modes, head)
		mode->status = MODE_UNVERIFIED;
		
	connector->status = (*connector->funcs->detect)(connector);
	
	if (connector->status == connector_status_disconnected) {
		DRM_DEBUG("%s is disconnected\n", drm_get_connector_name(connector));
		/* TODO set EDID to NULL */
		return;
	}
	
	ret = (*connector_funcs->get_modes)(connector);
	
	if (ret) {
		drm_mode_connector_list_update(connector);
	}
	
	if (maxX && maxY)
		drm_mode_validate_size(dev, &connector->modes, maxX,
				       maxY, 0);
	list_for_each_entry_safe(mode, t, &connector->modes, head) {
		if (mode->status == MODE_OK)
			mode->status = (*connector_funcs->mode_valid)(connector,mode);
	}
	
	
	drm_mode_prune_invalid(dev, &connector->modes, TRUE);
	
	if (list_empty(&connector->modes)) {
		struct drm_display_mode *stdmode;
		
		DRM_DEBUG("No valid modes on %s\n", drm_get_connector_name(connector));
		
		/* Should we do this here ???
		 * When no valid EDID modes are available we end up
		 * here and bailed in the past, now we add a standard
		 * 640x480@60Hz mode and carry on.
		 */
		stdmode = drm_mode_duplicate(dev, &std_mode[0]);
		drm_mode_probed_add(connector, stdmode);
		drm_mode_list_concat(&connector->probed_modes,
				     &connector->modes);
		
		DRM_DEBUG("Adding standard 640x480 @ 60Hz to %s\n",
			  drm_get_connector_name(connector));
	}
	
	drm_mode_sort(&connector->modes);
	
	DRM_DEBUG("Probed modes for %s\n", drm_get_connector_name(connector));
	list_for_each_entry_safe(mode, t, &connector->modes, head) {
		mode->vrefresh = drm_mode_vrefresh(mode);
		
		drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
		drm_mode_debug_printmodeline(mode);
	}
}
EXPORT_SYMBOL(drm_helper_probe_single_connector_modes);

void drm_helper_probe_connector_modes(struct drm_device *dev, uint32_t maxX, uint32_t maxY)
{
	struct drm_connector *connector;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		drm_helper_probe_single_connector_modes(connector, maxX, maxY);
	}
}
EXPORT_SYMBOL(drm_helper_probe_connector_modes);


/**
 * drm_helper_crtc_in_use - check if a given CRTC is in a mode_config
 * @crtc: CRTC to check
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Walk @crtc's DRM device's mode_config and see if it's in use.
 *
 * RETURNS:
 * True if @crtc is part of the mode_config, false otherwise.
 */
bool drm_helper_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_encoder *encoder;
	struct drm_device *dev = crtc->dev;
	/* FIXME: Locking around list access? */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head)
		if (encoder->crtc == crtc)
			return true;
	return false;
}
EXPORT_SYMBOL(drm_helper_crtc_in_use);

/**
 * drm_disable_unused_functions - disable unused objects
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * If an connector or CRTC isn't part of @dev's mode_config, it can be disabled
 * by calling its dpms function, which should power it off.
 */
void drm_helper_disable_unused_functions(struct drm_device *dev)
{
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_crtc *crtc;

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		encoder_funcs = encoder->helper_private;
		if (!encoder->crtc)
			(*encoder_funcs->dpms)(encoder, DPMSModeOff);
	}

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
		if (!crtc->enabled)
			crtc_funcs->dpms(crtc, DPMSModeOff);
	}
}
EXPORT_SYMBOL(drm_helper_disable_unused_functions);

/**
 * drm_pick_crtcs - pick crtcs for connector devices
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 */
static void drm_pick_crtcs (struct drm_device *dev)
{
	int c, o, assigned;
	struct drm_connector *connector, *connector_equal;
	struct drm_encoder *encoder, *encoder_equal;
	struct drm_crtc   *crtc;
	struct drm_display_mode *des_mode = NULL, *modes, *modes_equal;
	struct drm_connector_helper_funcs *connector_funcs;
	int found;

	/* clean out all the encoder/crtc combos */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		encoder->crtc = NULL;
	}
	
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		connector_funcs = connector->helper_private;
		connector->encoder = NULL;

    		/* Don't hook up connectors that are disconnected ??
		 *
		 * This is debateable. Do we want fixed /dev/fbX or
		 * dynamic on hotplug (need mode code for that though) ?
		 *
		 * If we don't hook up connectors now, then we only create
		 * /dev/fbX for the connector that's enabled, that's good as
		 * the users console will be on that connector.
		 *
		 * If we do hook up connectors that are disconnected now, then
		 * the user may end up having to muck about with the fbcon
		 * map flags to assign his console to the enabled connector. Ugh.
		 */
    		if (connector->status != connector_status_connected)
			continue;

		if (list_empty(&connector->modes))
			continue;

		des_mode = NULL;
		found = 0;
		list_for_each_entry(des_mode, &connector->modes, head) {
			if (des_mode->type & DRM_MODE_TYPE_PREFERRED) {
				found = 1;
				break;
			}
		}

		/* No preferred mode, let's just select the first available */
		if (!found) {
			des_mode = NULL;
			list_for_each_entry(des_mode, &connector->modes, head) {
				break;
			}
		}

		encoder = connector_funcs->best_encoder(connector);
		if (!encoder)
			continue;

		connector->encoder = encoder;

		c = -1;
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			assigned = 0;

			c++;
			if ((encoder->possible_crtcs & (1 << c)) == 0)
		    		continue;
	
			list_for_each_entry(encoder_equal, &dev->mode_config.encoder_list, head) {
				if (encoder->base.id == encoder_equal->base.id)
					continue;

				/* Find out if crtc has been assigned before */
				if (encoder_equal->crtc == crtc)
					assigned = 1;
			}

#if 1 /* continue for now */
			if (assigned)
				continue;
#endif

			o = -1;
			list_for_each_entry(connector_equal, &dev->mode_config.connector_list, head) {
				o++;
				if (connector->base.id == connector_equal->base.id)
					continue;

				encoder_equal = connector_equal->encoder;

				if (!encoder_equal)
					continue;

				list_for_each_entry(modes, &connector->modes, head) {
					list_for_each_entry(modes_equal, &connector_equal->modes, head) {
						if (drm_mode_equal (modes, modes_equal)) {
							if ((encoder->possible_clones & encoder_equal->possible_clones) && (connector_equal->encoder->crtc == crtc)) {
								printk("Cloning %s (0x%x) to %s (0x%x)\n",drm_get_connector_name(connector),encoder->possible_clones,drm_get_connector_name(connector_equal),encoder_equal->possible_clones);
								des_mode = modes;
								assigned = 0;
								goto clone;
							}
						}
					}
				}
			}

clone:
			/* crtc has been assigned skip it */
			if (assigned)
				continue;

			/* Found a CRTC to attach to, do it ! */
			encoder->crtc = crtc;
			encoder->crtc->desired_mode = des_mode;
			connector->initial_x = 0;
			connector->initial_y = 0;
			DRM_DEBUG("Desired mode for CRTC %d is 0x%x:%s\n",c,des_mode->base.id, des_mode->name);
			break;
    		}
	}
}
EXPORT_SYMBOL(drm_pick_crtcs);

/**
 * drm_crtc_set_mode - set a mode
 * @crtc: CRTC to program
 * @mode: mode to use
 * @x: width of mode
 * @y: height of mode
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Try to set @mode on @crtc.  Give @crtc and its associated connectors a chance
 * to fixup or reject the mode prior to trying to set it.
 *
 * RETURNS:
 * True if the mode was set successfully, or false otherwise.
 */
bool drm_crtc_helper_set_mode(struct drm_crtc *crtc, struct drm_display_mode *mode,
			      int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct drm_display_mode *adjusted_mode, saved_mode;
	struct drm_crtc_helper_funcs *crtc_funcs = crtc->helper_private;
	struct drm_encoder_helper_funcs *encoder_funcs;
	int saved_x, saved_y;
	struct drm_encoder *encoder;
	bool ret = true;

	adjusted_mode = drm_mode_duplicate(dev, mode);

	crtc->enabled = drm_helper_crtc_in_use(crtc);

	if (!crtc->enabled)
		return true;

	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;
	
	/* Update crtc values up front so the driver can rely on them for mode
	 * setting.
	 */
	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;

	if (drm_mode_equal(&saved_mode, &crtc->mode)) {
		if (saved_x != crtc->x || saved_y != crtc->y) {
			crtc_funcs->mode_set_base(crtc, crtc->x, crtc->y);
			goto done;
		}
	}

	/* Pass our mode to the connectors and the CRTC to give them a chance to
	 * adjust it according to limitations or connector properties, and also
	 * a chance to reject the mode entirely.
	 */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		
		if (encoder->crtc != crtc)
			continue;
 		encoder_funcs = encoder->helper_private;
		if (!(ret = encoder_funcs->mode_fixup(encoder, mode, adjusted_mode))) {
			goto done;
		}
	}
	
	if (!(ret = crtc_funcs->mode_fixup(crtc, mode, adjusted_mode))) {
		goto done;
	}

	/* Prepare the encoders and CRTCs before setting the mode. */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		encoder_funcs = encoder->helper_private;
		/* Disable the encoders as the first thing we do. */
		encoder_funcs->prepare(encoder);
	}
	
	crtc_funcs->prepare(crtc);
	
	/* Set up the DPLL and any encoders state that needs to adjust or depend
	 * on the DPLL.
	 */
	crtc_funcs->mode_set(crtc, mode, adjusted_mode, x, y);

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;
		
		DRM_INFO("%s: set mode %s %x\n", drm_get_encoder_name(encoder), mode->name, mode->base.id);
		encoder_funcs = encoder->helper_private;
		encoder_funcs->mode_set(encoder, mode, adjusted_mode);
	}
	
	/* Now, enable the clocks, plane, pipe, and connectors that we set up. */
	crtc_funcs->commit(crtc);

	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {

		if (encoder->crtc != crtc)
			continue;

		encoder_funcs = encoder->helper_private;
		encoder_funcs->commit(encoder);

	}
	
	/* XXX free adjustedmode */
	drm_mode_destroy(dev, adjusted_mode);
	/* TODO */
//	if (scrn->pScreen)
//		drm_crtc_set_screen_sub_pixel_order(dev);

done:
	if (!ret) { 
		crtc->mode = saved_mode;
		crtc->x = saved_x;
		crtc->y = saved_y;
	}

	return ret;
}
EXPORT_SYMBOL(drm_crtc_helper_set_mode);


/**
 * drm_crtc_helper_set_config - set a new config from userspace
 * @crtc: CRTC to setup
 * @crtc_info: user provided configuration
 * @new_mode: new mode to set
 * @connector_set: set of connectors for the new config
 * @fb: new framebuffer
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Setup a new configuration, provided by the user in @crtc_info, and enable
 * it.
 *
 * RETURNS:
 * Zero. (FIXME)
 */
int drm_crtc_helper_set_config(struct drm_mode_set *set)
{
	struct drm_device *dev;
	struct drm_crtc **save_crtcs, *new_crtc;
	struct drm_encoder **save_encoders, *new_encoder;
	bool save_enabled;
	bool changed = false;
	bool flip_or_move = false;
	struct drm_connector *connector;
	int count = 0, ro, fail = 0;
	struct drm_crtc_helper_funcs *crtc_funcs;
	int ret = 0;

	DRM_DEBUG("\n");

	if (!set)
		return -EINVAL;

	if (!set->crtc)
		return -EINVAL;

	if (!set->crtc->helper_private)
		return -EINVAL;
	
	crtc_funcs = set->crtc->helper_private;
       
	DRM_DEBUG("crtc: %p fb: %p connectors: %p num_connectors: %i (x, y) (%i, %i)\n", set->crtc, set->fb, set->connectors, set->num_connectors, set->x, set->y);
	dev = set->crtc->dev;

	/* save previous config */
	save_enabled = set->crtc->enabled;

	/* this is meant to be num_connector not num_crtc */
	save_crtcs = kzalloc(dev->mode_config.num_connector * sizeof(struct drm_crtc *), GFP_KERNEL);
	if (!save_crtcs)
		return -ENOMEM;

	save_encoders = kzalloc(dev->mode_config.num_connector * sizeof(struct drm_encoders *), GFP_KERNEL);
	if (!save_encoders) {
		kfree(save_crtcs);
		return -ENOMEM;
	}

	/* We should be able to check here if the fb has the same properties
	 * and then just flip_or_move it */
	if (set->crtc->fb != set->fb)
		flip_or_move = true;

	if (set->x != set->crtc->x || set->y != set->crtc->y)
		flip_or_move = true;

	if (set->mode && !drm_mode_equal(set->mode, &set->crtc->mode)) {
		DRM_DEBUG("modes are different\n");
		drm_mode_debug_printmodeline(&set->crtc->mode);
		drm_mode_debug_printmodeline(set->mode);
		changed = true;
	}

	/* a) traverse passed in connector list and get encoders for them */
	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		struct drm_connector_helper_funcs *connector_funcs = connector->helper_private;
		save_encoders[count++] = connector->encoder;
		new_encoder = NULL;
		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector) {
				new_encoder = connector_funcs->best_encoder(connector);
				/* if we can't get an encoder for a connector
				   we are setting now - then fail */
				if (new_encoder == NULL)
					/* don't break so fail path works correct */
					fail = 1;
				break;
			}
		}

		if (new_encoder != connector->encoder) {
			changed = true;
			connector->encoder = new_encoder;
		}
	}

	if (fail) {
		ret = -EINVAL;
		goto fail_no_encoder;
	}
	
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		if (!connector->encoder)
			continue;

		save_crtcs[count++] = connector->encoder->crtc;

		if (connector->encoder->crtc == set->crtc)
			new_crtc = NULL;
		else
			new_crtc = connector->encoder->crtc;

		for (ro = 0; ro < set->num_connectors; ro++) {
			if (set->connectors[ro] == connector)
				new_crtc = set->crtc;
		}
		if (new_crtc != connector->encoder->crtc) {
			changed = true;
			connector->encoder->crtc = new_crtc;
		}
	}

	/* mode_set_base is not a required function */
	if (flip_or_move && !crtc_funcs->mode_set_base)
		changed = true;

	if (changed) {
		set->crtc->fb = set->fb;
		set->crtc->enabled = (set->mode != NULL);
		if (set->mode != NULL) {
			DRM_DEBUG("attempting to set mode from userspace\n");
			drm_mode_debug_printmodeline(set->mode);
			if (!drm_crtc_helper_set_mode(set->crtc, set->mode, set->x,
						      set->y)) {
				ret = -EINVAL;
				goto fail_set_mode;
			}
			/* TODO are these needed? */
			set->crtc->desired_x = set->x;
			set->crtc->desired_y = set->y;
			set->crtc->desired_mode = set->mode;
		}
		drm_helper_disable_unused_functions(dev);
	} else if (flip_or_move) {
		if (set->crtc->fb != set->fb)
			set->crtc->fb = set->fb;
		crtc_funcs->mode_set_base(set->crtc, set->x, set->y);
	}

	kfree(save_encoders);
	kfree(save_crtcs);
	return 0;

fail_set_mode:
	set->crtc->enabled = save_enabled;
	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head)
		connector->encoder->crtc = save_crtcs[count++];
fail_no_encoder:
	kfree(save_crtcs);
	count = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		connector->encoder = save_encoders[count++];
	}
	kfree(save_encoders);
	return ret;
		
	
}
EXPORT_SYMBOL(drm_crtc_helper_set_config);

/**
 * drm_initial_config - setup a sane initial connector configuration
 * @dev: DRM device
 * @can_grow: this configuration is growable
 *
 * LOCKING:
 * Called at init time, must take mode config lock.
 *
 * Scan the CRTCs and connectors and try to put together an initial setup.
 * At the moment, this is a cloned configuration across all heads with
 * a new framebuffer object as the backing store.
 *
 * RETURNS:
 * Zero if everything went ok, nonzero otherwise.
 */
bool drm_helper_initial_config(struct drm_device *dev, bool can_grow)
{
	struct drm_connector *connector;
	int ret = false;

	mutex_lock(&dev->mode_config.mutex);

	drm_helper_probe_connector_modes(dev, 2048, 2048);

	drm_pick_crtcs(dev);

	/* This is a little screwy, as we've already walked the connectors 
	 * above, but it's a little bit of magic too. There's the potential
	 * for things not to get setup above if an existing device gets
	 * re-assigned thus confusing the hardware. By walking the connectors
	 * this fixes up their crtc's.
	 */
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {

		struct drm_encoder *encoder = connector->encoder;
		struct drm_crtc *crtc;

		if (!encoder)
			continue;

		crtc = connector->encoder->crtc;

		/* can't setup the connector if there's no assigned mode */
		if (!crtc || !crtc->desired_mode)
			continue;

		dev->driver->fb_probe(dev, crtc, connector);

		/* and needs an attached fb */
		if (crtc->fb)
			drm_crtc_helper_set_mode(crtc, crtc->desired_mode, 0, 0);
	}

	drm_helper_disable_unused_functions(dev);

	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}
EXPORT_SYMBOL(drm_helper_initial_config);

/**
 * drm_hotplug_stage_two
 * @dev DRM device
 * @connector hotpluged connector
 *
 * LOCKING.
 * Caller must hold mode config lock, function might grab struct lock.
 *
 * Stage two of a hotplug.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_helper_hotplug_stage_two(struct drm_device *dev, struct drm_connector *connector,
				 bool connected)
{
	int has_config = 0;

	dev->mode_config.hotplug_counter++;

	/* We might want to do something more here */
	if (!connected) {
		DRM_DEBUG("not connected\n");
		return 0;
	}

	if (connector->encoder->crtc && connector->encoder->crtc->desired_mode) {
		DRM_DEBUG("drm thinks that the connector already has a config\n");
		has_config = 1;
	}

	drm_helper_probe_connector_modes(dev, 2048, 2048);

	if (!has_config)
		drm_pick_crtcs(dev);

	if (!connector->encoder->crtc || !connector->encoder->crtc->desired_mode) {
		DRM_DEBUG("could not find a desired mode or crtc for connector\n");
		return 1;
	}

	/* We should really check if there is a fb using this crtc */
	if (!has_config)
		dev->driver->fb_probe(dev, connector->encoder->crtc, connector);
	else {
		dev->driver->fb_resize(dev, connector->encoder->crtc);

#if 0
		if (!drm_crtc_set_mode(connector->encoder->crtc, connector->encoder->crtc->desired_mode, 0, 0))
			DRM_ERROR("failed to set mode after hotplug\n");
#endif
	}

	drm_sysfs_hotplug_event(dev);

	drm_helper_disable_unused_functions(dev);

	return 0;
}
EXPORT_SYMBOL(drm_helper_hotplug_stage_two);


int drm_helper_mode_fill_fb_struct(struct drm_framebuffer *fb,
				   struct drm_mode_fb_cmd *mode_cmd)
{
	fb->width = mode_cmd->width;
	fb->height = mode_cmd->height;
	fb->pitch = mode_cmd->pitch;
	fb->bits_per_pixel = mode_cmd->bpp;
	fb->depth = mode_cmd->depth;
	fb->mm_handle = mode_cmd->handle;
	
	return 0;
}
EXPORT_SYMBOL(drm_helper_mode_fill_fb_struct);

/**
 * drm_get_buffer_object - find the buffer object for a given handle
 * @dev: DRM device
 * @bo: pointer to caller's buffer_object pointer
 * @handle: handle to lookup
 *
 * LOCKING:
 * Must take @dev's struct_mutex to protect buffer object lookup.
 *
 * Given @handle, lookup the buffer object in @dev and put it in the caller's
 * @bo pointer.
 *
 * RETURNS:
 * Zero on success, -EINVAL if the handle couldn't be found.
 */
int drm_get_buffer_object(struct drm_device *dev, struct drm_buffer_object **bo, unsigned long handle)
{
	struct drm_user_object *uo;
	struct drm_hash_item *hash;
	int ret;

	*bo = NULL;

	mutex_lock(&dev->struct_mutex);
	ret = drm_ht_find_item(&dev->object_hash, handle, &hash);
	if (ret) {
		DRM_ERROR("Couldn't find handle.\n");
		ret = -EINVAL;
		goto out_err;
	}

	uo = drm_hash_entry(hash, struct drm_user_object, hash);
	if (uo->type != drm_buffer_type) {
		ret = -EINVAL;
		goto out_err;
	}
	
	*bo = drm_user_object_entry(uo, struct drm_buffer_object, base);
	ret = 0;
out_err:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}
EXPORT_SYMBOL(drm_get_buffer_object);
