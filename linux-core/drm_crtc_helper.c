
/*
 * Copyright (c) 2006-2007 Intel Corporation
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


/**
 * drm_pick_crtcs - pick crtcs for output devices
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 */
static void drm_pick_crtcs (struct drm_device *dev)
{
	int c, o, assigned;
	struct drm_output *output, *output_equal;
	struct drm_crtc   *crtc;
	struct drm_display_mode *des_mode = NULL, *modes, *modes_equal;
	int found;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
       		output->crtc = NULL;
    
    		/* Don't hook up outputs that are disconnected ??
		 *
		 * This is debateable. Do we want fixed /dev/fbX or
		 * dynamic on hotplug (need mode code for that though) ?
		 *
		 * If we don't hook up outputs now, then we only create
		 * /dev/fbX for the output that's enabled, that's good as
		 * the users console will be on that output.
		 *
		 * If we do hook up outputs that are disconnected now, then
		 * the user may end up having to muck about with the fbcon
		 * map flags to assign his console to the enabled output. Ugh.
		 */
    		if (output->status != output_status_connected)
			continue;

		if (list_empty(&output->modes))
			continue;

		des_mode = NULL;
		found = 0;
		list_for_each_entry(des_mode, &output->modes, head) {
			if (des_mode->type & DRM_MODE_TYPE_PREFERRED) {
				found = 1;
				break;
			}
		}

		/* No preferred mode, let's just select the first available */
		if (!found) {
			des_mode = NULL;
			list_for_each_entry(des_mode, &output->modes, head) {
				break;
			}
		}

		c = -1;
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			assigned = 0;

			c++;
			if ((output->possible_crtcs & (1 << c)) == 0)
		    		continue;
	
			list_for_each_entry(output_equal, &dev->mode_config.output_list, head) {
				if (output->id == output_equal->id)
					continue;

				/* Find out if crtc has been assigned before */
				if (output_equal->crtc == crtc)
					assigned = 1;
			}

#if 1 /* continue for now */
			if (assigned)
				continue;
#endif

			o = -1;
			list_for_each_entry(output_equal, &dev->mode_config.output_list, head) {
				o++;
				if (output->id == output_equal->id)
					continue;

				list_for_each_entry(modes, &output->modes, head) {
					list_for_each_entry(modes_equal, &output_equal->modes, head) {
						if (drm_mode_equal (modes, modes_equal)) {
							if ((output->possible_clones & output_equal->possible_clones) && (output_equal->crtc == crtc)) {
								printk("Cloning %s (0x%lx) to %s (0x%lx)\n",drm_get_output_name(output),output->possible_clones,drm_get_output_name(output_equal),output_equal->possible_clones);
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
			output->crtc = crtc;
			output->crtc->desired_mode = des_mode;
			output->initial_x = 0;
			output->initial_y = 0;
			DRM_DEBUG("Desired mode for CRTC %d is 0x%x:%s\n",c,des_mode->mode_id, des_mode->name);
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
 * Try to set @mode on @crtc.  Give @crtc and its associated outputs a chance
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
	struct drm_output_helper_funcs *output_funcs;
	int saved_x, saved_y;
	struct drm_output *output;
	bool ret = true;

	adjusted_mode = drm_mode_duplicate(dev, mode);

	crtc->enabled = drm_crtc_in_use(crtc);

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

	/* Pass our mode to the outputs and the CRTC to give them a chance to
	 * adjust it according to limitations or output properties, and also
	 * a chance to reject the mode entirely.
	 */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		
		if (output->crtc != crtc)
			continue;
		output_funcs = output->helper_private;
		if (!(ret = output_funcs->mode_fixup(output, mode, adjusted_mode))) {
			goto done;
		}
	}
	
	if (!(ret = crtc_funcs->mode_fixup(crtc, mode, adjusted_mode))) {
		goto done;
	}

	/* Prepare the outputs and CRTCs before setting the mode. */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		output_funcs = output->helper_private;
		/* Disable the output as the first thing we do. */
		output_funcs->prepare(output);
	}
	
	crtc_funcs->prepare(crtc);
	
	/* Set up the DPLL and any output state that needs to adjust or depend
	 * on the DPLL.
	 */
	crtc_funcs->mode_set(crtc, mode, adjusted_mode, x, y);

	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		
		DRM_INFO("%s: set mode %s %x\n", drm_get_output_name(output), mode->name, mode->mode_id);
		output_funcs = output->helper_private;
		output_funcs->mode_set(output, mode, adjusted_mode);
	}
	
	/* Now, enable the clocks, plane, pipe, and outputs that we set up. */
	crtc_funcs->commit(crtc);

	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;

		output_funcs = output->helper_private;		
		output_funcs->commit(output);

#if 0 // TODO def RANDR_12_INTERFACE
		if (output->randr_output)
			RRPostPendingProperties (output->randr_output);
#endif
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
 * @output_set: set of outputs for the new config
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
	bool save_enabled;
	bool changed = false;
	bool flip_or_move = false;
	struct drm_output *output;
	int count = 0, ro;
	struct drm_crtc_helper_funcs *crtc_funcs;

	DRM_DEBUG("\n");

	if (!set)
		return -EINVAL;

	if (!set->crtc)
		return -EINVAL;

	if (!set->crtc->helper_private)
		return -EINVAL;
	
	crtc_funcs = set->crtc->helper_private;
       
	DRM_DEBUG("crtc: %p fb: %p outputs: %p num_outputs: %i (x, y) (%i, %i)\n", set->crtc, set->fb, set->outputs, set->num_outputs, set->x, set->y);
	dev = set->crtc->dev;

	/* save previous config */
	save_enabled = set->crtc->enabled;

	/* this is meant to be num_output not num_crtc */
	save_crtcs = kzalloc(dev->mode_config.num_output * sizeof(struct drm_crtc *), GFP_KERNEL);
	if (!save_crtcs)
		return -ENOMEM;

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

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		save_crtcs[count++] = output->crtc;

		if (output->crtc == set->crtc)
			new_crtc = NULL;
		else
			new_crtc = output->crtc;

		for (ro = 0; ro < set->num_outputs; ro++) {
			if (set->outputs[ro] == output)
				new_crtc = set->crtc;
		}
		if (new_crtc != output->crtc) {
			changed = true;
			output->crtc = new_crtc;
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
				set->crtc->enabled = save_enabled;
				count = 0;
				list_for_each_entry(output, &dev->mode_config.output_list, head)
					output->crtc = save_crtcs[count++];
				kfree(save_crtcs);
				return -EINVAL;
			}
			/* TODO are these needed? */
			set->crtc->desired_x = set->x;
			set->crtc->desired_y = set->y;
			set->crtc->desired_mode = set->mode;
		}
		drm_disable_unused_functions(dev);
	} else if (flip_or_move) {
		if (set->crtc->fb != set->fb)
			set->crtc->fb = set->fb;
		crtc_funcs->mode_set_base(set->crtc, set->x, set->y);
	}

	kfree(save_crtcs);
	return 0;
}
EXPORT_SYMBOL(drm_crtc_helper_set_config);

/**
 * drm_initial_config - setup a sane initial output configuration
 * @dev: DRM device
 * @can_grow: this configuration is growable
 *
 * LOCKING:
 * Called at init time, must take mode config lock.
 *
 * Scan the CRTCs and outputs and try to put together an initial setup.
 * At the moment, this is a cloned configuration across all heads with
 * a new framebuffer object as the backing store.
 *
 * RETURNS:
 * Zero if everything went ok, nonzero otherwise.
 */
bool drm_helper_initial_config(struct drm_device *dev, bool can_grow)
{
	struct drm_output *output;
	int ret = false;

	mutex_lock(&dev->mode_config.mutex);

	drm_crtc_probe_output_modes(dev, 2048, 2048);

	drm_pick_crtcs(dev);

	/* This is a little screwy, as we've already walked the outputs 
	 * above, but it's a little bit of magic too. There's the potential
	 * for things not to get setup above if an existing device gets
	 * re-assigned thus confusing the hardware. By walking the outputs
	 * this fixes up their crtc's.
	 */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		/* can't setup the output if there's no assigned mode */
		if (!output->crtc || !output->crtc->desired_mode)
			continue;

		dev->driver->fb_probe(dev, output->crtc, output);

		/* and needs an attached fb */
		if (output->crtc->fb)
			drm_crtc_helper_set_mode(output->crtc, output->crtc->desired_mode, 0, 0);
	}

	drm_disable_unused_functions(dev);

	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}
EXPORT_SYMBOL(drm_helper_initial_config);

/**
 * drm_hotplug_stage_two
 * @dev DRM device
 * @output hotpluged output
 *
 * LOCKING.
 * Caller must hold mode config lock, function might grab struct lock.
 *
 * Stage two of a hotplug.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_helper_hotplug_stage_two(struct drm_device *dev, struct drm_output *output,
				 bool connected)
{
	int has_config = 0;

	dev->mode_config.hotplug_counter++;

	/* We might want to do something more here */
	if (!connected) {
		DRM_DEBUG("not connected\n");
		return 0;
	}

	if (output->crtc && output->crtc->desired_mode) {
		DRM_DEBUG("drm thinks that the output already has a config\n");
		has_config = 1;
	}

	drm_crtc_probe_output_modes(dev, 2048, 2048);

	if (!has_config)
		drm_pick_crtcs(dev);

	if (!output->crtc || !output->crtc->desired_mode) {
		DRM_DEBUG("could not find a desired mode or crtc for output\n");
		return 1;
	}

	/* We should really check if there is a fb using this crtc */
	if (!has_config)
		dev->driver->fb_probe(dev, output->crtc, output);
	else {
		dev->driver->fb_resize(dev, output->crtc);

#if 0
		if (!drm_crtc_set_mode(output->crtc, output->crtc->desired_mode, 0, 0))
			DRM_ERROR("failed to set mode after hotplug\n");
#endif
	}

	drm_sysfs_hotplug_event(dev);

	drm_disable_unused_functions(dev);

	return 0;
}
EXPORT_SYMBOL(drm_helper_hotplug_stage_two);
