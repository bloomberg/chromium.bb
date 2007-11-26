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
#include <linux/list.h>
#include "drm.h"
#include "drmP.h"
#include "drm_crtc.h"

/**
 * drm_idr_get - allocate a new identifier
 * @dev: DRM device
 * @ptr: object pointer, used to generate unique ID
 *
 * LOCKING:
 * Caller must hold DRM mode_config lock.
 *
 * Create a unique identifier based on @ptr in @dev's identifier space.  Used
 * for tracking modes, CRTCs and outputs.
 *
 * RETURNS:
 * New unique (relative to other objects in @dev) integer identifier for the
 * object.
 */
int drm_idr_get(struct drm_device *dev, void *ptr)
{
	int new_id = 0;
	int ret;
again:
	if (idr_pre_get(&dev->mode_config.crtc_idr, GFP_KERNEL) == 0) {
		DRM_ERROR("Ran out memory getting a mode number\n");
		return 0;
	}

	ret = idr_get_new_above(&dev->mode_config.crtc_idr, ptr, 1, &new_id);
	if (ret == -EAGAIN)
		goto again;	

	return new_id;
}

/**
 * drm_idr_put - free an identifer
 * @dev: DRM device
 * @id: ID to free
 *
 * LOCKING:
 * Caller must hold DRM mode_config lock.
 *
 * Free @id from @dev's unique identifier pool.
 */
void drm_idr_put(struct drm_device *dev, int id)
{
	idr_remove(&dev->mode_config.crtc_idr, id);
}

/**
 * drm_crtc_from_fb - find the CRTC structure associated with an fb
 * @dev: DRM device
 * @fb: framebuffer in question
 *
 * LOCKING:
 * Caller must hold mode_config lock.
 *
 * Find CRTC in the mode_config structure that matches @fb.
 *
 * RETURNS:
 * Pointer to the CRTC or NULL if it wasn't found.
 */
struct drm_crtc *drm_crtc_from_fb(struct drm_device *dev,
				  struct drm_framebuffer *fb)
{
	struct drm_crtc *crtc;

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc->fb == fb)
			return crtc;
	}
	return NULL;
}

/**
 * drm_framebuffer_create - create a new framebuffer object
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Creates a new framebuffer objects and adds it to @dev's DRM mode_config.
 *
 * RETURNS:
 * Pointer to new framebuffer or NULL on error.
 */
struct drm_framebuffer *drm_framebuffer_create(struct drm_device *dev)
{
	struct drm_framebuffer *fb;

	/* Limit to single framebuffer for now */
	if (dev->mode_config.num_fb > 1) {
		mutex_unlock(&dev->mode_config.mutex);
		DRM_ERROR("Attempt to add multiple framebuffers failed\n");
		return NULL;
	}

	fb = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
	if (!fb)
		return NULL;
	
	fb->id = drm_idr_get(dev, fb);
	fb->dev = dev;
	dev->mode_config.num_fb++;
	list_add(&fb->head, &dev->mode_config.fb_list);

	return fb;
}
EXPORT_SYMBOL(drm_framebuffer_create);

/**
 * drm_framebuffer_destroy - remove a framebuffer object
 * @fb: framebuffer to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Scans all the CRTCs in @dev's mode_config.  If they're using @fb, removes
 * it, setting it to NULL.
 */
void drm_framebuffer_destroy(struct drm_framebuffer *fb)
{
	struct drm_device *dev = fb->dev;
	struct drm_crtc *crtc;

	/* remove from any CRTC */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (crtc->fb == fb)
			crtc->fb = NULL;
	}

	drm_idr_put(dev, fb->id);
	list_del(&fb->head);
	dev->mode_config.num_fb--;

	kfree(fb);
}
EXPORT_SYMBOL(drm_framebuffer_destroy);

/**
 * drm_crtc_create - create a new CRTC object
 * @dev: DRM device
 * @funcs: callbacks for the new CRTC
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Creates a new CRTC object and adds it to @dev's mode_config structure.
 *
 * RETURNS:
 * Pointer to new CRTC object or NULL on error.
 */
struct drm_crtc *drm_crtc_create(struct drm_device *dev,
				 const struct drm_crtc_funcs *funcs)
{
	struct drm_crtc *crtc;

	crtc = kzalloc(sizeof(struct drm_crtc), GFP_KERNEL);
	if (!crtc)
		return NULL;

	crtc->dev = dev;
	crtc->funcs = funcs;

	crtc->id = drm_idr_get(dev, crtc);

	list_add_tail(&crtc->head, &dev->mode_config.crtc_list);
	dev->mode_config.num_crtc++;

	return crtc;
}
EXPORT_SYMBOL(drm_crtc_create);

/**
 * drm_crtc_destroy - remove a CRTC object
 * @crtc: CRTC to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Cleanup @crtc.  Calls @crtc's cleanup function, then removes @crtc from
 * its associated DRM device's mode_config.  Frees it afterwards.
 */
void drm_crtc_destroy(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;

	if (crtc->funcs->cleanup)
		(*crtc->funcs->cleanup)(crtc);

	drm_idr_put(dev, crtc->id);
	list_del(&crtc->head);
	dev->mode_config.num_crtc--;
	kfree(crtc);
}
EXPORT_SYMBOL(drm_crtc_destroy);

/**
 * drm_crtc_in_use - check if a given CRTC is in a mode_config
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
bool drm_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_output *output;
	struct drm_device *dev = crtc->dev;
	/* FIXME: Locking around list access? */
	list_for_each_entry(output, &dev->mode_config.output_list, head)
		if (output->crtc == crtc)
			return true;
	return false;
}
EXPORT_SYMBOL(drm_crtc_in_use);

/*
 * Detailed mode info for a standard 640x480@60Hz monitor
 */
static struct drm_display_mode std_mode[] = {
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DEFAULT, 25200, 640, 656,
		   752, 800, 0, 480, 490, 492, 525, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 640x480@60Hz */
};

/**
 * drm_crtc_probe_output_modes - get complete set of display modes
 * @dev: DRM device
 * @maxX: max width for modes
 * @maxY: max height for modes
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Based on @dev's mode_config layout, scan all the outputs and try to detect
 * modes on them.  Modes will first be added to the output's probed_modes
 * list, then culled (based on validity and the @maxX, @maxY parameters) and
 * put into the normal modes list.
 *
 * Intended to be used either at bootup time or when major configuration
 * changes have occurred.
 *
 * FIXME: take into account monitor limits
 */
void drm_crtc_probe_output_modes(struct drm_device *dev, int maxX, int maxY)
{
	struct drm_output *output;
	struct drm_display_mode *mode, *t;
	int ret;
	//if (maxX == 0 || maxY == 0) 
	// TODO

	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		/* set all modes to the unverified state */
		list_for_each_entry_safe(mode, t, &output->modes, head)
			mode->status = MODE_UNVERIFIED;
		
		output->status = (*output->funcs->detect)(output);

		if (output->status == output_status_disconnected) {
			DRM_DEBUG("%s is disconnected\n", output->name);
			/* TODO set EDID to NULL */
			continue;
		}

		ret = (*output->funcs->get_modes)(output);

		if (ret) {
			drm_mode_output_list_update(output);
		}

		if (maxX && maxY)
			drm_mode_validate_size(dev, &output->modes, maxX,
					       maxY, 0);
		list_for_each_entry_safe(mode, t, &output->modes, head) {
			if (mode->status == MODE_OK)
				mode->status = (*output->funcs->mode_valid)(output,mode);
		}
		

		drm_mode_prune_invalid(dev, &output->modes, TRUE);

		if (list_empty(&output->modes)) {
			struct drm_display_mode *stdmode;

			DRM_DEBUG("No valid modes on %s\n", output->name);

			/* Should we do this here ???
			 * When no valid EDID modes are available we end up
			 * here and bailed in the past, now we add a standard
			 * 640x480@60Hz mode and carry on.
			 */
			stdmode = drm_mode_duplicate(dev, &std_mode[0]);
			drm_mode_probed_add(output, stdmode);
			drm_mode_list_concat(&output->probed_modes,
					     &output->modes);

			DRM_DEBUG("Adding standard 640x480 @ 60Hz to %s\n",
								output->name);
		}

		drm_mode_sort(&output->modes);

		DRM_DEBUG("Probed modes for %s\n", output->name);
		list_for_each_entry_safe(mode, t, &output->modes, head) {
			mode->vrefresh = drm_mode_vrefresh(mode);

			drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
			drm_mode_debug_printmodeline(dev, mode);
		}
	}
}

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
bool drm_crtc_set_mode(struct drm_crtc *crtc, struct drm_display_mode *mode,
		       int x, int y)
{
	struct drm_device *dev = crtc->dev;
	struct drm_display_mode *adjusted_mode, saved_mode;
	int saved_x, saved_y;
	bool didLock = false;
	bool ret = false;
	struct drm_output *output;

	adjusted_mode = drm_mode_duplicate(dev, mode);

	crtc->enabled = drm_crtc_in_use(crtc);

	if (!crtc->enabled) {
		return true;
	}

	didLock = crtc->funcs->lock(crtc);

	saved_mode = crtc->mode;
	saved_x = crtc->x;
	saved_y = crtc->y;
	
	/* Update crtc values up front so the driver can rely on them for mode
	 * setting.
	 */
	crtc->mode = *mode;
	crtc->x = x;
	crtc->y = y;

	/* XXX short-circuit changes to base location only */
	
	/* Pass our mode to the outputs and the CRTC to give them a chance to
	 * adjust it according to limitations or output properties, and also
	 * a chance to reject the mode entirely.
	 */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		
		if (output->crtc != crtc)
			continue;
		
		if (!output->funcs->mode_fixup(output, mode, adjusted_mode)) {
			goto done;
		}
	}
	
	if (!crtc->funcs->mode_fixup(crtc, mode, adjusted_mode)) {
		goto done;
	}

	/* Prepare the outputs and CRTCs before setting the mode. */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		
		/* Disable the output as the first thing we do. */
		output->funcs->prepare(output);
	}
	
	crtc->funcs->prepare(crtc);
	
	/* Set up the DPLL and any output state that needs to adjust or depend
	 * on the DPLL.
	 */
	crtc->funcs->mode_set(crtc, mode, adjusted_mode, x, y);

	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		
		DRM_INFO("%s: set mode %s %x\n", output->name, mode->name, mode->mode_id);

		output->funcs->mode_set(output, mode, adjusted_mode);
	}
	
	/* Now, enable the clocks, plane, pipe, and outputs that we set up. */
	crtc->funcs->commit(crtc);

	list_for_each_entry(output, &dev->mode_config.output_list, head) {

		if (output->crtc != crtc)
			continue;
		
		output->funcs->commit(output);

#if 0 // TODO def RANDR_12_INTERFACE
		if (output->randr_output)
			RRPostPendingProperties (output->randr_output);
#endif
	}
	
	/* XXX free adjustedmode */
	drm_mode_destroy(dev, adjusted_mode);
	ret = TRUE;
	/* TODO */
//	if (scrn->pScreen)
//		drm_crtc_set_screen_sub_pixel_order(dev);

done:
	if (!ret) {
		crtc->x = saved_x;
		crtc->y = saved_y;
		crtc->mode = saved_mode;
	}
	
	if (didLock)
		crtc->funcs->unlock (crtc);
	
	return ret;
}
EXPORT_SYMBOL(drm_crtc_set_mode);

/**
 * drm_disable_unused_functions - disable unused objects
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * If an output or CRTC isn't part of @dev's mode_config, it can be disabled
 * by calling its dpms function, which should power it off.
 */
void drm_disable_unused_functions(struct drm_device *dev)
{
	struct drm_output *output;
	struct drm_crtc *crtc;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		if (!output->crtc)
			(*output->funcs->dpms)(output, DPMSModeOff);
	}

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		if (!crtc->enabled)
			crtc->funcs->dpms(crtc, DPMSModeOff);
	}
}
	
/**
 * drm_mode_probed_add - add a mode to the specified output's probed mode list
 * @output: output the new mode
 * @mode: mode data
 *
 * LOCKING:
 * Caller must hold mode config lock.
 * 
 * Add @mode to @output's mode list for later use.
 */
void drm_mode_probed_add(struct drm_output *output,
			 struct drm_display_mode *mode)
{
	list_add(&mode->head, &output->probed_modes);
}
EXPORT_SYMBOL(drm_mode_probed_add);

/**
 * drm_mode_remove - remove and free a mode
 * @output: output list to modify
 * @mode: mode to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 * 
 * Remove @mode from @output's mode list, then free it.
 */
void drm_mode_remove(struct drm_output *output, struct drm_display_mode *mode)
{
	list_del(&mode->head);
	kfree(mode);
}
EXPORT_SYMBOL(drm_mode_remove);

/**
 * drm_output_create - create a new output
 * @dev: DRM device
 * @funcs: callbacks for this output
 * @name: user visible name of the output
 *
 * LOCKING:
 * Caller must hold @dev's mode_config lock.
 *
 * Creates a new drm_output structure and adds it to @dev's mode_config
 * structure.
 *
 * RETURNS:
 * Pointer to the new output or NULL on error.
 */
struct drm_output *drm_output_create(struct drm_device *dev,
				     const struct drm_output_funcs *funcs,
				     const char *name)
{
	struct drm_output *output = NULL;

	output = kzalloc(sizeof(struct drm_output), GFP_KERNEL);
	if (!output)
		return NULL;
		
	output->dev = dev;
	output->funcs = funcs;
	output->id = drm_idr_get(dev, output);
	if (name)
		strncpy(output->name, name, DRM_OUTPUT_LEN);
	output->name[DRM_OUTPUT_LEN - 1] = 0;
	output->subpixel_order = SubPixelUnknown;
	INIT_LIST_HEAD(&output->probed_modes);
	INIT_LIST_HEAD(&output->modes);
	/* randr_output? */
	/* output_set_monitor(output)? */
	/* check for output_ignored(output)? */

	mutex_lock(&dev->mode_config.mutex);
	list_add_tail(&output->head, &dev->mode_config.output_list);
	dev->mode_config.num_output++;

	mutex_unlock(&dev->mode_config.mutex);

	return output;

}
EXPORT_SYMBOL(drm_output_create);

/**
 * drm_output_destroy - remove an output
 * @output: output to remove
 *
 * LOCKING:
 * Caller must hold @dev's mode_config lock.
 *
 * Call @output's cleanup function, then remove the output from the DRM
 * mode_config after freeing @output's modes.
 */
void drm_output_destroy(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_display_mode *mode, *t;

	if (*output->funcs->cleanup)
		(*output->funcs->cleanup)(output);

	list_for_each_entry_safe(mode, t, &output->probed_modes, head)
		drm_mode_remove(output, mode);

	list_for_each_entry_safe(mode, t, &output->modes, head)
		drm_mode_remove(output, mode);

	mutex_lock(&dev->mode_config.mutex);
	drm_idr_put(dev, output->id);
	list_del(&output->head);
	mutex_unlock(&dev->mode_config.mutex);
	kfree(output);
}
EXPORT_SYMBOL(drm_output_destroy);

/**
 * drm_output_rename - rename an output
 * @output: output to rename
 * @name: new user visible name
 *
 * LOCKING:
 * None.
 *
 * Simply stuff a new name into @output's name field, based on @name.
 *
 * RETURNS:
 * True if the name was changed, false otherwise.
 */
bool drm_output_rename(struct drm_output *output, const char *name)
{
	if (!name)
		return false;

	strncpy(output->name, name, DRM_OUTPUT_LEN);
	output->name[DRM_OUTPUT_LEN - 1] = 0;

	DRM_DEBUG("Changed name to %s\n", output->name);
//	drm_output_set_monitor(output);
//	if (drm_output_ignored(output))
//		return FALSE;

	return TRUE;
}
EXPORT_SYMBOL(drm_output_rename);

/**
 * drm_mode_create - create a new display mode
 * @dev: DRM device
 *
 * LOCKING:
 * None.
 *
 * Create a new drm_display_mode, give it an ID, and return it.
 *
 * RETURNS:
 * Pointer to new mode on success, NULL on error.
 */
struct drm_display_mode *drm_mode_create(struct drm_device *dev)
{
	struct drm_display_mode *nmode;

	nmode = kzalloc(sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmode)
		return NULL;

	nmode->mode_id = drm_idr_get(dev, nmode);
	return nmode;
}
EXPORT_SYMBOL(drm_mode_create);

/**
 * drm_mode_destroy - remove a mode
 * @dev: DRM device
 * @mode: mode to remove
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Free @mode's unique identifier, then free it.
 */
void drm_mode_destroy(struct drm_device *dev, struct drm_display_mode *mode)
{
	drm_idr_put(dev, mode->mode_id);

	kfree(mode);
}
EXPORT_SYMBOL(drm_mode_destroy);

/**
 * drm_mode_config_init - initialize DRM mode_configuration structure
 * @dev: DRM device
 *
 * LOCKING:
 * None, should happen single threaded at init time.
 *
 * Initialize @dev's mode_config structure, used for tracking the graphics
 * configuration of @dev.
 */
void drm_mode_config_init(struct drm_device *dev)
{
	mutex_init(&dev->mode_config.mutex);
	INIT_LIST_HEAD(&dev->mode_config.fb_list);
	INIT_LIST_HEAD(&dev->mode_config.crtc_list);
	INIT_LIST_HEAD(&dev->mode_config.output_list);
	INIT_LIST_HEAD(&dev->mode_config.usermode_list);
	idr_init(&dev->mode_config.crtc_idr);
}
EXPORT_SYMBOL(drm_mode_config_init);

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
static int drm_get_buffer_object(struct drm_device *dev, struct drm_buffer_object **bo, unsigned long handle)
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

		des_mode = NULL;
		list_for_each_entry(des_mode, &output->modes, head) {
			if (des_mode->type & DRM_MODE_TYPE_PREFERRED)
				break;
		}

		/* No preferred mode, let's just select the first available */
		if (!des_mode || !(des_mode->type & DRM_MODE_TYPE_PREFERRED)) {
			list_for_each_entry(des_mode, &output->modes, head) {
				if (des_mode)
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
								printk("Cloning %s (0x%lx) to %s (0x%lx)\n",output->name,output->possible_clones,output_equal->name,output_equal->possible_clones);
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
bool drm_initial_config(struct drm_device *dev, bool can_grow)
{
	struct drm_output *output;
	struct drm_crtc *crtc;
	int ret = false;

	mutex_lock(&dev->mode_config.mutex);

	drm_crtc_probe_output_modes(dev, 2048, 2048);

	drm_pick_crtcs(dev);

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {

		/* can't setup the crtc if there's no assigned mode */
		if (!crtc->desired_mode)
			continue;

		/* Now setup the fbdev for attached crtcs */
		dev->driver->fb_probe(dev, crtc);
	}

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

		/* and needs an attached fb */
		if (output->crtc->fb)
			drm_crtc_set_mode(output->crtc, output->crtc->desired_mode, 0, 0);
	}

	drm_disable_unused_functions(dev);

	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}
EXPORT_SYMBOL(drm_initial_config);

/**
 * drm_mode_config_cleanup - free up DRM mode_config info
 * @dev: DRM device
 *
 * LOCKING:
 * Caller must hold mode config lock.
 *
 * Free up all the outputs and CRTCs associated with this DRM device, then
 * free up the framebuffers and associated buffer objects.
 *
 * FIXME: cleanup any dangling user buffer objects too
 */
void drm_mode_config_cleanup(struct drm_device *dev)
{
	struct drm_output *output, *ot;
	struct drm_crtc *crtc, *ct;
	struct drm_framebuffer *fb, *fbt;
	struct drm_display_mode *mode, *mt;
	list_for_each_entry_safe(output, ot, &dev->mode_config.output_list, head) {
		drm_output_destroy(output);
	}

	list_for_each_entry_safe(mode, mt, &dev->mode_config.usermode_list, head) {
		drm_mode_destroy(dev, mode);
	}
		
	list_for_each_entry_safe(fb, fbt, &dev->mode_config.fb_list, head) {
		if (fb->bo->type != drm_bo_type_kernel)
			drm_framebuffer_destroy(fb);
		else
			dev->driver->fb_remove(dev, drm_crtc_from_fb(dev, fb));
	}

	list_for_each_entry_safe(crtc, ct, &dev->mode_config.crtc_list, head) {
		drm_crtc_destroy(crtc);
	}

}
EXPORT_SYMBOL(drm_mode_config_cleanup);

/**
 * drm_crtc_set_config - set a new config from userspace
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
int drm_crtc_set_config(struct drm_crtc *crtc, struct drm_mode_crtc *crtc_info, struct drm_display_mode *new_mode, struct drm_output **output_set, struct drm_framebuffer *fb)
{
	struct drm_device *dev = crtc->dev;
	struct drm_crtc **save_crtcs, *new_crtc;
	bool save_enabled = crtc->enabled;
	bool changed;
	struct drm_output *output;
	int count = 0, ro;

	save_crtcs = kzalloc(dev->mode_config.num_crtc * sizeof(struct drm_crtc *), GFP_KERNEL);
	if (!save_crtcs)
		return -ENOMEM;

	if (crtc->fb != fb)
		changed = true;

	if (crtc_info->x != crtc->x || crtc_info->y != crtc->y)
		changed = true;

	if (new_mode && (crtc->mode.mode_id != new_mode->mode_id))
		changed = true;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		save_crtcs[count++] = output->crtc;

		if (output->crtc == crtc)
			new_crtc = NULL;
		else
			new_crtc = output->crtc;

		for (ro = 0; ro < crtc_info->count_outputs; ro++) {
			if (output_set[ro] == output)
				new_crtc = crtc;
		}
		if (new_crtc != output->crtc) {
			changed = true;
			output->crtc = new_crtc;
		}
	}

	if (changed) {
		crtc->fb = fb;
		crtc->enabled = (new_mode != NULL);
		if (new_mode != NULL) {
			DRM_DEBUG("attempting to set mode from userspace\n");
			drm_mode_debug_printmodeline(dev, new_mode);
			if (!drm_crtc_set_mode(crtc, new_mode, crtc_info->x,
					       crtc_info->y)) {
				crtc->enabled = save_enabled;
				count = 0;
				list_for_each_entry(output, &dev->mode_config.output_list, head)
					output->crtc = save_crtcs[count++];
				kfree(save_crtcs);
				return -EINVAL;
			}
			crtc->desired_x = crtc_info->x;
			crtc->desired_y = crtc_info->y;
			crtc->desired_mode = new_mode;
		}
		drm_disable_unused_functions(dev);
	}
	kfree(save_crtcs);
	return 0;
}

/**
 * drm_crtc_convert_to_umode - convert a drm_display_mode into a modeinfo
 * @out: drm_mode_modeinfo struct to return to the user
 * @in: drm_display_mode to use
 *
 * LOCKING:
 * None.
 *
 * Convert a drm_display_mode into a drm_mode_modeinfo structure to return to
 * the user.
 */
void drm_crtc_convert_to_umode(struct drm_mode_modeinfo *out, struct drm_display_mode *in)
{

	out->id = in->mode_id;
	out->clock = in->clock;
	out->hdisplay = in->hdisplay;
	out->hsync_start = in->hsync_start;
	out->hsync_end = in->hsync_end;
	out->htotal = in->htotal;
	out->hskew = in->hskew;
	out->vdisplay = in->vdisplay;
	out->vsync_start = in->vsync_start;
	out->vsync_end = in->vsync_end;
	out->vtotal = in->vtotal;
	out->vscan = in->vscan;
	out->vrefresh = in->vrefresh;
	out->flags = in->flags;
	out->type = in->type;
	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;
}

/**
 * drm_crtc_convert_to_umode - convert a modeinfo into a drm_display_mode
 * @out: drm_display_mode to return to the user
 * @in: drm_mode_modeinfo to use
 *
 * LOCKING:
 * None.
 *
 * Convert a drmo_mode_modeinfo into a drm_display_mode structure to return to
 * the caller.
 */
void drm_crtc_convert_umode(struct drm_display_mode *out, struct drm_mode_modeinfo *in)
{
	out->clock = in->clock;
	out->hdisplay = in->hdisplay;
	out->hsync_start = in->hsync_start;
	out->hsync_end = in->hsync_end;
	out->htotal = in->htotal;
	out->hskew = in->hskew;
	out->vdisplay = in->vdisplay;
	out->vsync_start = in->vsync_start;
	out->vsync_end = in->vsync_end;
	out->vtotal = in->vtotal;
	out->vscan = in->vscan;
	out->vrefresh = in->vrefresh;
	out->flags = in->flags;
	out->type = in->type;
	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;
}
	
/**
 * drm_mode_getresources - get graphics configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Construct a set of configuration description structures and return
 * them to the user, including CRTC, output and framebuffer configuration.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getresources(struct drm_device *dev,
			  void *data, struct drm_file *file_priv)
{
	struct drm_mode_card_res *card_res = data;
	struct list_head *lh;
	struct drm_framebuffer *fb;
	struct drm_output *output;
	struct drm_crtc *crtc;
	struct drm_mode_modeinfo u_mode;
	struct drm_display_mode *mode;
	int ret = 0;
	int mode_count= 0;
	int output_count = 0;
	int crtc_count = 0;
	int fb_count = 0;
	int copied = 0;

	memset(&u_mode, 0, sizeof(struct drm_mode_modeinfo));

	mutex_lock(&dev->mode_config.mutex);

	list_for_each(lh, &dev->mode_config.fb_list)
		fb_count++;

	list_for_each(lh, &dev->mode_config.crtc_list)
		crtc_count++;

	list_for_each_entry(output, &dev->mode_config.output_list,
			    head) {
		output_count++;
		list_for_each(lh, &output->modes)
			mode_count++;
	}
	list_for_each(lh, &dev->mode_config.usermode_list)
		mode_count++;

	if (card_res->count_modes == 0) {
		DRM_DEBUG("probing modes %dx%d\n", dev->mode_config.max_width, dev->mode_config.max_height);
		drm_crtc_probe_output_modes(dev, dev->mode_config.max_width, dev->mode_config.max_height);
		mode_count = 0;
		list_for_each_entry(output, &dev->mode_config.output_list, head) {
			list_for_each(lh, &output->modes)
				mode_count++;
		}
		list_for_each(lh, &dev->mode_config.usermode_list)
			mode_count++;
	}

	/* handle this in 4 parts */
	/* FBs */
	if (card_res->count_fbs >= fb_count) {
		copied = 0;
		list_for_each_entry(fb, &dev->mode_config.fb_list, head) {
			if (put_user(fb->id, card_res->fb_id + copied))
				return -EFAULT;
			copied++;
		}
	}
	card_res->count_fbs = fb_count;

	/* CRTCs */
	if (card_res->count_crtcs >= crtc_count) {
		copied = 0;
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head){
			DRM_DEBUG("CRTC ID is %d\n", crtc->id);
			if (put_user(crtc->id, card_res->crtc_id + copied))
				return -EFAULT;
			copied++;
		}
	}
	card_res->count_crtcs = crtc_count;


	/* Outputs */
	if (card_res->count_outputs >= output_count) {
		copied = 0;
		list_for_each_entry(output, &dev->mode_config.output_list,
				    head) {
 			DRM_DEBUG("OUTPUT ID is %d\n", output->id);
			if (put_user(output->id, card_res->output_id + copied))
				return -EFAULT;
			copied++;
		}
	}
	card_res->count_outputs = output_count;
	
	/* Modes */
	if (card_res->count_modes >= mode_count) {
		copied = 0;
		list_for_each_entry(output, &dev->mode_config.output_list,
				    head) {
			list_for_each_entry(mode, &output->modes, head) {
				drm_crtc_convert_to_umode(&u_mode, mode);
				if (copy_to_user(card_res->modes + copied,
						 &u_mode, sizeof(u_mode)))
					return -EFAULT;
				copied++;
			}
		}
		/* add in user modes */
		list_for_each_entry(mode, &dev->mode_config.usermode_list, head) {
			drm_crtc_convert_to_umode(&u_mode, mode);
			if (copy_to_user(card_res->modes + copied, &u_mode,
					 sizeof(u_mode)))
				return -EFAULT;
			copied++;
		}
	}
	card_res->count_modes = mode_count;

	DRM_DEBUG("Counted %d %d %d\n", card_res->count_crtcs,
		  card_res->count_outputs,
		  card_res->count_modes);
	
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_getcrtc - get CRTC configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Construct a CRTC configuration structure to return to the user.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getcrtc(struct drm_device *dev,
		     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc *crtc_resp = data;
	struct drm_crtc *crtc;
	struct drm_output *output;
	int ocount;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);
	crtc = idr_find(&dev->mode_config.crtc_idr, crtc_resp->crtc_id);
	if (!crtc || (crtc->id != crtc_resp->crtc_id)) {
		ret = -EINVAL;
		goto out;
	}

	crtc_resp->x = crtc->x;
	crtc_resp->y = crtc->y;

	if (crtc->fb)
		crtc_resp->fb_id = crtc->fb->id;
	else
		crtc_resp->fb_id = 0;

	crtc_resp->outputs = 0;
	if (crtc->enabled) {

		crtc_resp->mode = crtc->mode.mode_id;
		ocount = 0;
		list_for_each_entry(output, &dev->mode_config.output_list, head) {
			if (output->crtc == crtc)
				crtc_resp->outputs |= 1 << (ocount++);
		}
	} else {
		crtc_resp->mode = 0;
	}

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_getoutput - get output configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Construct a output configuration structure to return to the user.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getoutput(struct drm_device *dev,
		       void *data, struct drm_file *file_priv)
{
	struct drm_mode_get_output *out_resp = data;
	struct drm_output *output;
	struct drm_display_mode *mode;
	int mode_count = 0;
	int ret = 0;
	int copied = 0;
	int i;

	DRM_DEBUG("output id %d:\n", out_resp->output);

	mutex_lock(&dev->mode_config.mutex);
	output= idr_find(&dev->mode_config.crtc_idr, out_resp->output);
	if (!output || (output->id != out_resp->output)) {
		ret = -EINVAL;
		goto done;
	}

	list_for_each_entry(mode, &output->modes, head)
		mode_count++;
	
	for (i = 0; i < DRM_OUTPUT_MAX_UMODES; i++)
		if (output->user_mode_ids[i] != 0)
			mode_count++;

	strncpy(out_resp->name, output->name, DRM_OUTPUT_NAME_LEN);
	out_resp->name[DRM_OUTPUT_NAME_LEN-1] = 0;

	out_resp->mm_width = output->mm_width;
	out_resp->mm_height = output->mm_height;
	out_resp->subpixel = output->subpixel_order;
	out_resp->connection = output->status;
	if (output->crtc)
		out_resp->crtc = output->crtc->id;
	else
		out_resp->crtc = 0;

	out_resp->crtcs = output->possible_crtcs;
	out_resp->clones = output->possible_clones;
	if ((out_resp->count_modes >= mode_count) && mode_count) {
		copied = 0;
		list_for_each_entry(mode, &output->modes, head) {
			out_resp->modes[copied++] = mode->mode_id;
		}
		for (i = 0; i < DRM_OUTPUT_MAX_UMODES; i++) {
			if (output->user_mode_ids[i] != 0)
				out_resp->modes[copied++] = output->user_mode_ids[i];
		}
			
	}
	out_resp->count_modes = mode_count;

done:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_setcrtc - set CRTC configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Build a new CRTC configuration based on user request.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_setcrtc(struct drm_device *dev,
		     void *data, struct drm_file *file_priv)
{
	struct drm_mode_crtc *crtc_req = data;
	struct drm_crtc *crtc;
	struct drm_output **output_set = NULL, *output;
	struct drm_display_mode *mode;
	struct drm_framebuffer *fb = NULL;
	int ret = 0;
	int i;

	mutex_lock(&dev->mode_config.mutex);
	crtc = idr_find(&dev->mode_config.crtc_idr, crtc_req->crtc_id);
	if (!crtc || (crtc->id != crtc_req->crtc_id)) {
		DRM_DEBUG("Unknown CRTC ID %d\n", crtc_req->crtc_id);
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->mode) {
		/* if we have a mode we need a framebuffer */
		if (crtc_req->fb_id) {
			fb = idr_find(&dev->mode_config.crtc_idr, crtc_req->fb_id);
			if (!fb || (fb->id != crtc_req->fb_id)) {
				DRM_DEBUG("Unknown FB ID%d\n", crtc_req->fb_id);
				ret = -EINVAL;
				goto out;
			}
		}
		mode = idr_find(&dev->mode_config.crtc_idr, crtc_req->mode);
		if (!mode || (mode->mode_id != crtc_req->mode)) {
			struct drm_output *output;
			
			list_for_each_entry(output, 
					    &dev->mode_config.output_list,
					    head) {
				list_for_each_entry(mode, &output->modes,
						    head) {
					drm_mode_debug_printmodeline(dev, 
								     mode);
				}
			}

			DRM_DEBUG("Unknown mode id %d, %p\n", crtc_req->mode, mode);
			ret = -EINVAL;
			goto out;
		}
	} else
		mode = NULL;

	if (crtc_req->count_outputs == 0 && mode) {
		DRM_DEBUG("Count outputs is 0 but mode set\n");
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->count_outputs > 0 && !mode && !fb) {
		DRM_DEBUG("Count outputs is %d but no mode or fb set\n", crtc_req->count_outputs);
		ret = -EINVAL;
		goto out;
	}

	if (crtc_req->count_outputs > 0) {
		u32 out_id;
		output_set = kmalloc(crtc_req->count_outputs *
				     sizeof(struct drm_output *), GFP_KERNEL);
		if (!output_set) {
			ret = -ENOMEM;
			goto out;
		}

		for (i = 0; i < crtc_req->count_outputs; i++) {
			if (get_user(out_id, &crtc_req->set_outputs[i])) {
				ret = -EFAULT;
				goto out;
			}

			output = idr_find(&dev->mode_config.crtc_idr, out_id);
			if (!output || (out_id != output->id)) {
				DRM_DEBUG("Output id %d unknown\n", out_id);
				ret = -EINVAL;
				goto out;
			}

			output_set[i] = output;
		}
	}
		
	ret = drm_crtc_set_config(crtc, crtc_req, mode, output_set, fb);

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_addfb - add an FB to the graphics configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Add a new FB to the specified CRTC, given a user request.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_addfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd *r = data;
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_framebuffer *fb;
	struct drm_buffer_object *bo;
	struct drm_crtc *crtc;
	int ret = 0;

	if ((config->min_width > r->width) || (r->width > config->max_width)) {
		DRM_ERROR("mode new framebuffer width not within limits\n");
		return -EINVAL;
	}
	if ((config->min_height > r->height) || (r->height > config->max_height)) {
		DRM_ERROR("mode new framebuffer height not within limits\n");
		return -EINVAL;
	}

	mutex_lock(&dev->mode_config.mutex);
	/* TODO check limits are okay */
	ret = drm_get_buffer_object(dev, &bo, r->handle);
	if (ret || !bo) {
		ret = -EINVAL;
		goto out;
	}

	/* TODO check buffer is sufficently large */
	/* TODO setup destructor callback */

	fb = drm_framebuffer_create(dev);
	if (!fb) {
		ret = -EINVAL;
		goto out;
	}

	fb->width = r->width;
	fb->height = r->height;
	fb->pitch = r->pitch;
	fb->bits_per_pixel = r->bpp;
	fb->depth = r->depth;
	fb->offset = bo->offset;
	fb->bo = bo;

	r->buffer_id = fb->id;

	list_add(&fb->filp_head, &file_priv->fbs);

	/* FIXME: bind the fb to the right crtc */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		crtc->fb = fb;
		dev->driver->fb_probe(dev, crtc);
	}

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_rmfb - remove an FB from the configuration
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Remove the FB specified by the user.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_rmfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_framebuffer *fb = 0;
	uint32_t *id = data;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);
	fb = idr_find(&dev->mode_config.crtc_idr, *id);
	/* TODO check that we realy get a framebuffer back. */
	if (!fb || (*id != fb->id)) {
		DRM_ERROR("mode invalid framebuffer id\n");
		ret = -EINVAL;
		goto out;
	}

	/* TODO check if we own the buffer */
	/* TODO release all crtc connected to the framebuffer */
	/* bind the fb to the crtc for now */
	/* TODO unhock the destructor from the buffer object */

	if (fb->bo->type != drm_bo_type_kernel)
		drm_framebuffer_destroy(fb);
	else
		dev->driver->fb_remove(dev, drm_crtc_from_fb(dev, fb));

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_mode_getfb - get FB info
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * LOCKING:
 * Caller? (FIXME)
 *
 * Lookup the FB given its ID and return info about it.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_getfb(struct drm_device *dev,
		   void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd *r = data;
	struct drm_framebuffer *fb;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);
	fb = idr_find(&dev->mode_config.crtc_idr, r->buffer_id);
	if (!fb || (r->buffer_id != fb->id)) {
		DRM_ERROR("invalid framebuffer id\n");
		ret = -EINVAL;
		goto out;
	}

	r->height = fb->height;
	r->width = fb->width;
	r->depth = fb->depth;
	r->bpp = fb->bits_per_pixel;
	r->handle = fb->bo->base.hash.key;
	r->pitch = fb->pitch;

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_fb_release - remove and free the FBs on this file
 * @filp: file * from the ioctl
 *
 * LOCKING:
 * Takes mode config lock.
 *
 * Destroy all the FBs associated with @filp.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
void drm_fb_release(struct file *filp)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->head->dev;
	struct drm_framebuffer *fb, *tfb;

	mutex_lock(&dev->mode_config.mutex);
	list_for_each_entry_safe(fb, tfb, &priv->fbs, filp_head) {
		list_del(&fb->filp_head);
		if (fb->bo->type != drm_bo_type_kernel)
			drm_framebuffer_destroy(fb);
		else
			dev->driver->fb_remove(dev, drm_crtc_from_fb(dev, fb));
	}
	mutex_unlock(&dev->mode_config.mutex);
}

/*
 *
 */
void drm_mode_addmode(struct drm_device *dev, struct drm_display_mode *user_mode)
{
	user_mode->type |= DRM_MODE_TYPE_USERDEF;

	user_mode->output_count = 0;
	list_add(&user_mode->head, &dev->mode_config.usermode_list);
}
EXPORT_SYMBOL(drm_mode_addmode);

int drm_mode_rmmode(struct drm_device *dev, struct drm_display_mode *mode)
{
	struct drm_display_mode *t;
	int ret = -EINVAL;
	list_for_each_entry(t, &dev->mode_config.usermode_list, head) {
		if (t == mode) {
			list_del(&mode->head);
			drm_mode_destroy(dev, mode);
			ret = 0;
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(drm_mode_rmmode);

/**
 * drm_fb_addmode - adds a user defined mode
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * Adds a user specified mode to the kernel.
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * writes new mode id into arg.
 * Zero on success, errno on failure.
 */
int drm_mode_addmode_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	struct drm_mode_modeinfo *new_mode = data;
	struct drm_display_mode *user_mode;
	int ret = 0;

	mutex_lock(&dev->mode_config.mutex);
	user_mode = drm_mode_create(dev);
	if (!user_mode) {
		ret = -ENOMEM;
		goto out;
	}

	drm_crtc_convert_umode(user_mode, new_mode);

	drm_mode_addmode(dev, user_mode);
	new_mode->id = user_mode->mode_id;

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_fb_rmmode - removes a user defined mode
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * Remove the user defined mode specified by the user.
 *
 * Called by the user via ioctl
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_rmmode_ioctl(struct drm_device *dev,
			  void *data, struct drm_file *file_priv)
{
	uint32_t *id = data;
	struct drm_display_mode *mode;
	int ret = -EINVAL;

	mutex_lock(&dev->mode_config.mutex);	
	mode = idr_find(&dev->mode_config.crtc_idr, *id);
	if (!mode || (*id != mode->mode_id)) {
		goto out;
	}

	if (!(mode->type & DRM_MODE_TYPE_USERDEF)) {
		goto out;
	}

	if (mode->output_count) {
		goto out;
	}

	ret = drm_mode_rmmode(dev, mode);

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}

/**
 * drm_fb_attachmode - Attach a user mode to an output
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * This attaches a user specified mode to an output.
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_attachmode(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	struct drm_mode_mode_cmd *mode_cmd = data;
	struct drm_output *output;
	struct drm_display_mode *mode;
	int i, ret = 0;

	mutex_lock(&dev->mode_config.mutex);

	mode = idr_find(&dev->mode_config.crtc_idr, mode_cmd->mode_id);
	if (!mode || (mode->mode_id != mode_cmd->mode_id)) {
		ret = -EINVAL;
		goto out;
	}

	output = idr_find(&dev->mode_config.crtc_idr, mode_cmd->output_id);
	if (!output || (output->id != mode_cmd->output_id)) {
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < DRM_OUTPUT_MAX_UMODES; i++) {
		if (output->user_mode_ids[i] == 0) {
			output->user_mode_ids[i] = mode->mode_id;
			mode->output_count++;
			break;
		}
	}

	if (i == DRM_OUTPUT_MAX_UMODES)
		ret = -ENOSPC;

out:
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}


/**
 * drm_fb_detachmode - Detach a user specified mode from an output
 * @inode: inode from the ioctl
 * @filp: file * from the ioctl
 * @cmd: cmd from ioctl
 * @arg: arg from ioctl
 *
 * Called by the user via ioctl.
 *
 * RETURNS:
 * Zero on success, errno on failure.
 */
int drm_mode_detachmode(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	struct drm_mode_mode_cmd *mode_cmd = data;
	struct drm_output *output;
	struct drm_display_mode *mode;
	int i, found = 0, ret = 0;

	mutex_lock(&dev->mode_config.mutex);

	mode = idr_find(&dev->mode_config.crtc_idr, mode_cmd->mode_id);
	if (!mode || (mode->mode_id != mode_cmd->mode_id)) {
		ret = -EINVAL;
		goto out;
	}

	output = idr_find(&dev->mode_config.crtc_idr, mode_cmd->output_id);
	if (!output || (output->id != mode_cmd->output_id)) {
		ret = -EINVAL;
		goto out;
	}


	for (i = 0; i < DRM_OUTPUT_MAX_UMODES; i++) {
		if (output->user_mode_ids[i] == mode->mode_id) {
			output->user_mode_ids[i] = 0;
			mode->output_count--;
			found = 1;
		}
	}

	if (!found)
		ret = -EINVAL;

out:	       
	mutex_unlock(&dev->mode_config.mutex);
	return ret;
}
