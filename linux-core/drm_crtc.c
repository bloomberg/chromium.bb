#include <linux/list.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"

struct drm_framebuffer *drm_framebuffer_create(drm_device_t *dev)
{
	struct drm_framebuffer *fb;

	spin_lock(&dev->crtc_config.config_lock);
	/* Limit to single framebuffer for now */
	if (dev->crtc_config.num_fb > 1) {
		DRM_ERROR("Attempt to add multiple framebuffers failed\n");
		return NULL;
	}
	spin_unlock(&dev->crtc_config.config_lock);

	fb = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
	if (!fb) {

		return NULL;
	}

	fb->dev = dev;
	spin_lock(&dev->crtc_config.config_lock);
	dev->crtc_config.num_fb++;
	list_add(&fb->head, &dev->crtc_config.fb_list);
	spin_unlock(&dev->crtc_config.config_lock);

	return fb;
}

void drm_framebuffer_destroy(struct drm_framebuffer *fb)
{
	drm_device_t *dev = fb->dev;

	spin_lock(&dev->crtc_config.config_lock);
	list_del(&fb->head);
	dev->crtc_config.num_fb--;
	spin_unlock(&dev->crtc_config.config_lock);

	kfree(fb);
}

struct drm_crtc *drm_crtc_create(drm_device_t *dev,
				 const struct drm_crtc_funcs *funcs)
{
	struct drm_crtc *crtc = NULL;
	crtc = kmalloc(sizeof(struct drm_crtc), GFP_KERNEL);
	if (!crtc)
		return NULL;

	crtc->dev = dev;
	crtc->funcs = funcs;

	spin_lock(&dev->crtc_config.config_lock);

	list_add_tail(&crtc->head, &dev->crtc_config.crtc_list);
	dev->crtc_config.num_crtc++;

	spin_unlock(&dev->crtc_config.config_lock);

	return crtc;
}
EXPORT_SYMBOL(drm_crtc_create);

void drm_crtc_destroy(struct drm_crtc *crtc)
{
	drm_device_t *dev = crtc->dev;

	if (crtc->funcs->cleanup)
		(*crtc->funcs->cleanup)(crtc);

	spin_lock(&dev->crtc_config.config_lock);
	list_del(&crtc->head);
	dev->crtc_config.num_crtc--;
	spin_unlock(&dev->crtc_config.config_lock);
	kfree(crtc);
}
EXPORT_SYMBOL(drm_crtc_destroy);

bool drm_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_output *output;
	drm_device_t *dev = crtc->dev;
	list_for_each_entry(output, &dev->crtc_config.output_list, head)
		if (output->crtc == crtc)
			return true;
	return false;
}
EXPORT_SYMBOL(drm_crtc_in_use);

void drm_crtc_probe_output_modes(struct drm_device *dev, int maxX, int maxY)
{
	struct drm_output *output;
	struct drm_display_mode *mode, *t;
	int ret;
	//if (maxX == 0 || maxY == 0) 
	// TODO

	list_for_each_entry(output, &dev->crtc_config.output_list, head) {
		
		list_for_each_entry_safe(mode, t, &output->modes, head)
			drm_mode_remove(output, mode);
		
		output->status = (*output->funcs->detect)(output);

		if (output->status == output_status_disconnected) {
			/* TODO set EDID to NULL */
			continue;
		}

		ret = (*output->funcs->get_modes)(output);

		if (ret) {
			/* move the modes over to the main mode list */
			drm_mode_list_concat(&output->probed_modes,
					     &output->modes);
		}

		if (maxX && maxY)
			drm_mode_validate_size(dev, &output->modes, maxX,
					       maxY, 0);
		list_for_each_entry_safe(mode, t, &output->modes, head) {
			if (mode->status == MODE_OK)
				mode->status = (*output->funcs->mode_valid)(output,mode);
		}
		

		drm_mode_prune_invalid(dev, &output->modes, TRUE);

		if (list_empty(&output->modes))
			continue;

		drm_mode_sort(&output->modes);

		DRM_DEBUG("Probed modes for %s\n", output->name);
		list_for_each_entry_safe(mode, t, &output->modes, head) {
			mode->vrefresh = drm_mode_vrefresh(mode);

			drm_mode_set_crtcinfo(mode, CRTC_INTERLACE_HALVE_V);
			drm_mode_debug_printmodeline(dev, mode);
		}
	}
}

bool drm_crtc_set_mode(struct drm_crtc *crtc, struct drm_display_mode *mode,
		       int x, int y)
{
	drm_device_t *dev = crtc->dev;
	struct drm_display_mode *adjusted_mode, saved_mode;
	int saved_x, saved_y;
	bool didLock = false;
	bool ret = false;
	struct drm_output *output;

	adjusted_mode = drm_mode_duplicate(mode);

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
	list_for_each_entry(output, &dev->crtc_config.output_list, head) {
		
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
	list_for_each_entry(output, &dev->crtc_config.output_list, head) {

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
	list_for_each_entry(output, &dev->crtc_config.output_list, head) {
		if (output->crtc == crtc)
			output->funcs->mode_set(output, mode, adjusted_mode);
	}
	
	/* Now, enable the clocks, plane, pipe, and outputs that we set up. */
	crtc->funcs->commit(crtc);
	list_for_each_entry(output, &dev->crtc_config.output_list, head) {
		if (output->crtc == crtc)
		{
			output->funcs->commit(output);
#if 0 // TODO def RANDR_12_INTERFACE
			if (output->randr_output)
				RRPostPendingProperties (output->randr_output);
#endif
		}
	}
	
	/* XXX free adjustedmode */
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

bool drm_set_desired_modes(struct drm_device *dev)
{
	struct drm_crtc *crtc;
	struct drm_output *output, *list_output;

	list_for_each_entry(crtc, &dev->crtc_config.crtc_list, head) {
		output = NULL;

		list_for_each_entry(list_output, &dev->crtc_config.output_list, head) {
			if (list_output->crtc == crtc) {
				output = list_output;
				break;
			}
		}
		/* Skip disabled crtcs */
		if (!output)
			continue;

		memset(&crtc->mode, 0, sizeof(crtc->mode));
		if (!crtc->desired_mode.crtc_hdisplay) {
			
		}
		if (!drm_crtc_set_mode(crtc, &crtc->desired_mode,
				       crtc->desired_x, crtc->desired_y))
			return false;
	}

	drm_disable_unused_functions(dev);
	return true;
}
EXPORT_SYMBOL(drm_set_desired_modes);

void drm_disable_unused_functions(struct drm_device *dev)
{
	struct drm_output *output;
	struct drm_crtc *crtc;

	list_for_each_entry(output, &dev->crtc_config.output_list, head) {
		if (!output->crtc)
			(*output->funcs->dpms)(output, DPMSModeOff);
	}

	list_for_each_entry(crtc, &dev->crtc_config.crtc_list, head) {
		if (!crtc->enabled)
			crtc->funcs->dpms(crtc, DPMSModeOff);
	}
}
	
/**
 * drm_mode_probed_add - add a mode to the specified output's probed mode list
 * @output: output the new mode
 * @mode: mode data
 *
 * Add @mode to @output's mode list for later use.
 */
void drm_mode_probed_add(struct drm_output *output, struct drm_display_mode *mode)
{
	printk(KERN_ERR "adding DDC mode %s to output %s\n", mode->name,
	       output->name);
	spin_lock(&output->modes_lock);
	list_add(&mode->head, &output->probed_modes);
	spin_unlock(&output->modes_lock);
}
EXPORT_SYMBOL(drm_mode_probed_add);

/**
 * drm_mode_remove - remove and free a mode
 * @output: output list to modify
 * @mode: mode to remove
 *
 * Remove @mode from @output's mode list, then free it.
 */
void drm_mode_remove(struct drm_output *output, struct drm_display_mode *mode)
{
	spin_lock(&output->modes_lock);
	list_del(&mode->head);
	spin_unlock(&output->modes_lock);
	kfree(mode);
}
EXPORT_SYMBOL(drm_mode_remove);

/*
 * Probably belongs in the DRM device structure
 */
struct drm_output *drm_output_create(drm_device_t *dev,
				     const struct drm_output_funcs *funcs,
				     const char *name)
{
	struct drm_output *output = NULL;

	output = kmalloc(sizeof(struct drm_output), GFP_KERNEL);
	if (!output)
		return NULL;
		
	output->dev = dev;
	output->funcs = funcs;
	if (name)
		strncpy(output->name, name, DRM_OUTPUT_LEN);
	output->name[DRM_OUTPUT_LEN - 1] = 0;
	output->subpixel_order = SubPixelUnknown;
	INIT_LIST_HEAD(&output->probed_modes);
	INIT_LIST_HEAD(&output->modes);
	spin_lock_init(&output->modes_lock);
	/* randr_output? */
	/* output_set_monitor(output)? */
	/* check for output_ignored(output)? */

	spin_lock(&dev->crtc_config.config_lock);
	list_add_tail(&output->head, &dev->crtc_config.output_list);
	dev->crtc_config.num_output++;

	spin_unlock(&dev->crtc_config.config_lock);

	return output;

}
EXPORT_SYMBOL(drm_output_create);

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

	spin_lock(&dev->crtc_config.config_lock);
	list_del(&output->head);
	spin_unlock(&dev->crtc_config.config_lock);
	kfree(output);
}
EXPORT_SYMBOL(drm_output_destroy);

bool drm_output_rename(struct drm_output *output, const char *name)
{
	if (!name)
		return false;

	strncpy(output->name, name, DRM_OUTPUT_LEN);
	output->name[DRM_OUTPUT_LEN - 1] = 0;
//	drm_output_set_monitor(output);
//	if (drm_output_ignored(output))
//		return FALSE;
	return TRUE;
}
EXPORT_SYMBOL(drm_output_rename);

void drm_crtc_config_init(drm_device_t *dev)
{
	spin_lock_init(&dev->crtc_config.config_lock);
	INIT_LIST_HEAD(&dev->crtc_config.fb_list);
	INIT_LIST_HEAD(&dev->crtc_config.crtc_list);
	INIT_LIST_HEAD(&dev->crtc_config.output_list);
}
EXPORT_SYMBOL(drm_crtc_config_init);

void drm_framebuffer_set_object(drm_device_t *dev, unsigned long handle)
{
	struct drm_framebuffer *fb;
	drm_user_object_t *uo;
	drm_hash_item_t *hash;
	drm_buffer_object_t *bo;
	int ret;

	mutex_lock(&dev->struct_mutex);
	ret = drm_ht_find_item(&dev->object_hash, handle, &hash);
	if (ret) {
		DRM_ERROR("Couldn't find handle.\n");
		goto out_err;
	}

	uo = drm_hash_entry(hash, drm_user_object_t, hash);
	if (uo->type != drm_buffer_type) {
		ret = -EINVAL;
		goto out_err;
	}

	bo = drm_user_object_entry(uo, drm_buffer_object_t, base);
	
	/* get the first fb */
	list_for_each_entry(fb, &dev->crtc_config.fb_list, head) {
		fb->offset = bo->offset;
		break;
	}
	ret = 0;
out_err:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}
EXPORT_SYMBOL(drm_framebuffer_set_object);

bool drm_initial_config(drm_device_t *dev, bool can_grow)
{
	/* do a hardcoded initial configuration here */
	struct drm_crtc *crtc, *vga_crtc = NULL, *dvi_crtc = NULL;
	struct drm_framebuffer *fb;
	struct drm_output *output, *use_output = NULL;

	fb = drm_framebuffer_create(dev);
	if (!fb)
		return false;

	fb->pitch = 1024;
	fb->width = 1024;
	fb->height = 768;
	fb->depth = 24;
	fb->bits_per_pixel = 32;
	
	/* bind both CRTCs to this fb */
	/* only initialise one crtc to enabled state */
	list_for_each_entry(crtc, &dev->crtc_config.crtc_list, head) {
		crtc->fb = fb;
		if (!vga_crtc) {
			vga_crtc = crtc;
			crtc->enabled = 1;
			crtc->desired_x = 0;
			crtc->desired_y = 0;
		} 
#if 0
		else if (!dvi_crtc) {
			dvi_crtc = crtc;
			crtc->enabled = 1;
			crtc->desired_x = 0;
			crtc->desired_y = 0;
		}
#endif
	}

	drm_crtc_probe_output_modes(dev, 1024, 768);

	/* hard bind the CRTCS */

	/* bind analog output to one crtc */
	list_for_each_entry(output, &dev->crtc_config.output_list, head) {
		struct drm_display_mode *des_mode;

		if (strncmp(output->name, "VGA", 3)) {
			output->crtc = vga_crtc;
			/* just pull the first mode out of that hat */
			list_for_each_entry(des_mode, &output->modes, head)
				break;
			DRM_DEBUG("Setting desired mode for output %s\n", output->name);
			drm_mode_debug_printmodeline(dev, des_mode);
			output->crtc->desired_mode = *des_mode;
			output->initial_x = 0;
			output->initial_y = 0;
			use_output = output;
		} else if (strncmp(output->name, "TMDS", 4)) {
			output->crtc = vga_crtc;
#if 0
			/* just pull the first mode out of that hat */
			list_for_each_entry(des_mode, &output->modes, head)
				break;
			DRM_DEBUG("Setting desired mode for output %s\n", output->name);
			drm_mode_debug_printmodeline(dev, des_mode);
			output->crtc->desired_mode = *des_mode;
#endif
			output->initial_x = 0;
			output->initial_y = 0;
		} else
			output->crtc = NULL;
	       
	}

	return false;
}
EXPORT_SYMBOL(drm_initial_config);

void drm_crtc_config_cleanup(drm_device_t *dev)
{
	struct drm_output *output, *ot;
	struct drm_crtc *crtc, *ct;
	
	list_for_each_entry_safe(output, ot, &dev->crtc_config.output_list, head) {
		drm_output_destroy(output);
	}

	
	list_for_each_entry_safe(crtc, ct, &dev->crtc_config.crtc_list, head) {
		drm_crtc_destroy(crtc);
	}
}
EXPORT_SYMBOL(drm_crtc_config_cleanup);

