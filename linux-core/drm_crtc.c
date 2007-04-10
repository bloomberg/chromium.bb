#include <linux/list.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"

int drm_mode_idr_get(struct drm_device *dev, void *ptr)
{
	int new_id = 0;
	int ret;
again:
	if (idr_pre_get(&dev->mode_config.crtc_idr, GFP_KERNEL) == 0) {
		DRM_ERROR("Ran out memory getting a mode number\n");
		return 0;
	}

	spin_lock(&dev->mode_config.config_lock);

	ret = idr_get_new_above(&dev->mode_config.crtc_idr, ptr, 1, &new_id);
	if (ret == -EAGAIN) {
		spin_unlock(&dev->mode_config.config_lock);
		goto again;
	}	

	spin_unlock(&dev->mode_config.config_lock);
	return new_id;
}

void drm_mode_idr_put(struct drm_device *dev, int id)
{
	idr_remove(&dev->mode_config.crtc_idr, id);
}

struct drm_framebuffer *drm_framebuffer_create(drm_device_t *dev)
{
	struct drm_framebuffer *fb;

	spin_lock(&dev->mode_config.config_lock);
	/* Limit to single framebuffer for now */
	if (dev->mode_config.num_fb > 1) {
		DRM_ERROR("Attempt to add multiple framebuffers failed\n");
		return NULL;
	}
	spin_unlock(&dev->mode_config.config_lock);

	fb = kzalloc(sizeof(struct drm_framebuffer), GFP_KERNEL);
	if (!fb) {

		return NULL;
	}
	
	fb->id = drm_mode_idr_get(dev, fb);
	fb->dev = dev;
	spin_lock(&dev->mode_config.config_lock);
	dev->mode_config.num_fb++;
	list_add(&fb->head, &dev->mode_config.fb_list);
	spin_unlock(&dev->mode_config.config_lock);

	return fb;
}

void drm_framebuffer_destroy(struct drm_framebuffer *fb)
{
	drm_device_t *dev = fb->dev;

	spin_lock(&dev->mode_config.config_lock);
	drm_mode_idr_put(dev, fb->id);
	list_del(&fb->head);
	dev->mode_config.num_fb--;
	spin_unlock(&dev->mode_config.config_lock);

	kfree(fb);
}

struct drm_crtc *drm_crtc_create(drm_device_t *dev,
				 const struct drm_crtc_funcs *funcs)
{
	struct drm_crtc *crtc;

	crtc = kzalloc(sizeof(struct drm_crtc), GFP_KERNEL);
	if (!crtc)
		return NULL;

	crtc->dev = dev;
	crtc->funcs = funcs;

	crtc->id = drm_mode_idr_get(dev, crtc);
	DRM_DEBUG("crtc %p got id %d\n", crtc, crtc->id);

	spin_lock(&dev->mode_config.config_lock);
	list_add_tail(&crtc->head, &dev->mode_config.crtc_list);
	dev->mode_config.num_crtc++;
	spin_unlock(&dev->mode_config.config_lock);

	return crtc;
}
EXPORT_SYMBOL(drm_crtc_create);

void drm_crtc_destroy(struct drm_crtc *crtc)
{
	drm_device_t *dev = crtc->dev;

	if (crtc->funcs->cleanup)
		(*crtc->funcs->cleanup)(crtc);


	spin_lock(&dev->mode_config.config_lock);
	drm_mode_idr_put(dev, crtc->id);
	list_del(&crtc->head);
	dev->mode_config.num_crtc--;
	spin_unlock(&dev->mode_config.config_lock);
	kfree(crtc);
}
EXPORT_SYMBOL(drm_crtc_destroy);

bool drm_crtc_in_use(struct drm_crtc *crtc)
{
	struct drm_output *output;
	drm_device_t *dev = crtc->dev;
	list_for_each_entry(output, &dev->mode_config.output_list, head)
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

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		
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
		if (output->crtc == crtc)
			output->funcs->mode_set(output, mode, adjusted_mode);
	}
	
	/* Now, enable the clocks, plane, pipe, and outputs that we set up. */
	crtc->funcs->commit(crtc);
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
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
	drm_crtc_mode_destroy(dev, adjusted_mode);
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

	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		output = NULL;

		DRM_DEBUG("crtc is %d\n", crtc->id);
		list_for_each_entry(list_output, &dev->mode_config.output_list, head) {
			if (list_output->crtc == crtc) {
				output = list_output;
				break;
			}
		}
		/* Skip disabled crtcs */
		if (!output)
			continue;

		memset(&crtc->mode, 0, sizeof(crtc->mode));
		if (!crtc->desired_mode->crtc_hdisplay) {
			
		}
		if (!drm_crtc_set_mode(crtc, crtc->desired_mode,
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
 * Add @mode to @output's mode list for later use.
 */
void drm_mode_probed_add(struct drm_output *output, struct drm_display_mode *mode)
{
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

	output = kzalloc(sizeof(struct drm_output), GFP_KERNEL);
	if (!output)
		return NULL;
		
	output->dev = dev;
	output->funcs = funcs;
	output->id = drm_mode_idr_get(dev, output);
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

	spin_lock(&dev->mode_config.config_lock);
	list_add_tail(&output->head, &dev->mode_config.output_list);
	dev->mode_config.num_output++;

	spin_unlock(&dev->mode_config.config_lock);

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

	spin_lock(&dev->mode_config.config_lock);
	drm_mode_idr_put(dev, output->id);
	list_del(&output->head);
	spin_unlock(&dev->mode_config.config_lock);
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

struct drm_display_mode *drm_crtc_mode_create(struct drm_device *dev)
{
	struct drm_display_mode *nmode;

	nmode = kzalloc(sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmode)
		return NULL;

	nmode->mode_id = drm_mode_idr_get(dev, nmode);
	return nmode;
}

void drm_crtc_mode_destroy(struct drm_device *dev, struct drm_display_mode *mode)
{
	drm_mode_idr_put(dev, mode->mode_id);

	kfree(mode);
}

void drm_mode_config_init(drm_device_t *dev)
{
	spin_lock_init(&dev->mode_config.config_lock);
	INIT_LIST_HEAD(&dev->mode_config.fb_list);
	INIT_LIST_HEAD(&dev->mode_config.crtc_list);
	INIT_LIST_HEAD(&dev->mode_config.output_list);
	idr_init(&dev->mode_config.crtc_idr);
}
EXPORT_SYMBOL(drm_mode_config_init);

static int drm_get_buffer_object(drm_device_t *dev, struct drm_buffer_object **bo, unsigned long handle)
{
	drm_user_object_t *uo;
	drm_hash_item_t *hash;
	int ret;

	*bo = NULL;

	mutex_lock(&dev->struct_mutex);
	ret = drm_ht_find_item(&dev->object_hash, handle, &hash);
	if (ret) {
		DRM_ERROR("Couldn't find handle.\n");
		ret = -EINVAL;
		goto out_err;
	}

	uo = drm_hash_entry(hash, drm_user_object_t, hash);
	if (uo->type != drm_buffer_type) {
		ret = -EINVAL;
		goto out_err;
	}
	
	*bo = drm_user_object_entry(uo, drm_buffer_object_t, base);
	ret = 0;
out_err:
	mutex_unlock(&dev->struct_mutex);
	return ret;
}

bool drm_initial_config(drm_device_t *dev, bool can_grow)
{
	/* do a hardcoded initial configuration here */
	struct drm_crtc *crtc, *vga_crtc = NULL, *dvi_crtc = NULL,
		*lvds_crtc = NULL;;
	struct drm_framebuffer *fb;
	struct drm_output *output, *use_output = NULL;

#if 0
	fb = drm_framebuffer_create(dev);
	if (!fb)
		return false;

	fb->pitch = 1024;
	fb->width = 1024;
	fb->height = 768;
	fb->depth = 24;
	fb->bits_per_pixel = 32;
	
#endif
	/* bind both CRTCs to this fb */
	/* only initialise one crtc to enabled state */
	list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
		//		crtc->fb = fb;
		if (!vga_crtc) {
			vga_crtc = crtc;
			crtc->enabled = 1;
			crtc->desired_x = 0;
			crtc->desired_y = 0;
		} else if (!lvds_crtc) {
			lvds_crtc = crtc;
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
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		struct drm_display_mode *des_mode;

		/* Get the first preferred moded */
		list_for_each_entry(des_mode, &output->modes, head) {
			if (des_mode->flags & DRM_MODE_TYPE_PREFERRED)
				break;
		}
		if (!strncmp(output->name, "VGA", 3)) {
			output->crtc = vga_crtc;
			drm_mode_debug_printmodeline(dev, des_mode);
			output->crtc->desired_mode = des_mode;
			output->initial_x = 0;
			output->initial_y = 0;
			use_output = output;
		} else if (!strncmp(output->name, "TMDS", 4)) {
			output->crtc = vga_crtc;
#if 0
			drm_mode_debug_printmodeline(dev, des_mode);
			output->crtc->desired_mode = des_mode;
#endif
			output->initial_x = 0;
			output->initial_y = 0;
		} else 	if (!strncmp(output->name, "LVDS", 3)) {
			output->crtc = lvds_crtc;
			drm_mode_debug_printmodeline(dev, des_mode);
			output->crtc->desired_mode = des_mode;
			output->initial_x = 0;
			output->initial_y = 0;
		} else
			output->crtc = NULL;
	       
	}

	return false;
}
EXPORT_SYMBOL(drm_initial_config);

void drm_mode_config_cleanup(drm_device_t *dev)
{
	struct drm_output *output, *ot;
	struct drm_crtc *crtc, *ct;
	
	list_for_each_entry_safe(output, ot, &dev->mode_config.output_list, head) {
		drm_output_destroy(output);
	}

	list_for_each_entry_safe(crtc, ct, &dev->mode_config.crtc_list, head) {
		drm_crtc_destroy(crtc);
	}
}
EXPORT_SYMBOL(drm_mode_config_cleanup);

int drm_crtc_set_config(struct drm_crtc *crtc, struct drm_mode_crtc *crtc_info, struct drm_display_mode *new_mode, struct drm_output **output_set)
{
	drm_device_t *dev = crtc->dev;
	struct drm_crtc **save_crtcs, *new_crtc;
	bool save_enabled = crtc->enabled;
	bool changed;
	struct drm_output *output;
	int count = 0, ro;

	save_crtcs = kzalloc(dev->mode_config.num_crtc * sizeof(struct drm_crtc *), GFP_KERNEL);
	if (!save_crtcs)
		return -ENOMEM;

	if (crtc_info->x != crtc->x || crtc_info->y != crtc->y)
		changed = true;

	if (crtc->mode.mode_id != new_mode->mode_id)
		changed = true;

	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		save_crtcs[count++] = output->crtc;

		if (output->crtc == crtc)
			new_crtc = NULL;
		else
			new_crtc = output->crtc;

		for (ro = 0; ro < crtc_info->count_outputs; ro++)
		{
			if (output_set[ro] == output)
				new_crtc = crtc;
		}
		if (new_crtc != output->crtc) {
			changed = true;
			output->crtc = new_crtc;
		}
	}

	if (changed) {
		crtc->enabled = new_mode != NULL;
		if (new_mode) {
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
			crtc->desired_mode =  new_mode;
		}
		drm_disable_unused_functions(dev);
	}
	kfree(save_crtcs);
	return 0;
}

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
	
	out->flags = in->flags;
	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;
}

	
/* IOCTL code from userspace */
int drm_mode_getresources(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	struct drm_mode_card_res __user *argp = (void __user *)arg;
	struct drm_mode_card_res card_res;
	struct list_head *lh;
	struct drm_output *output;
	struct drm_crtc *crtc;
	struct drm_mode_modeinfo u_mode;
	struct drm_display_mode *mode;
	int retcode = 0;
	int mode_count= 0;
	int output_count = 0;
	int crtc_count = 0;
	int copied = 0;
	
	memset(&u_mode, 0, sizeof(struct drm_mode_modeinfo));

	list_for_each(lh, &dev->mode_config.crtc_list)
		crtc_count++;

	list_for_each_entry(output, &dev->mode_config.output_list,
			    head) {
		output_count++;
		list_for_each(lh, &output->modes)
			mode_count++;
	}

	if (copy_from_user(&card_res, argp, sizeof(card_res)))
		return -EFAULT;

	if (card_res.count_modes == 0) {
		DRM_DEBUG("probing modes %dx%d\n", dev->mode_config.max_width, dev->mode_config.max_height);
		drm_crtc_probe_output_modes(dev, dev->mode_config.max_width, dev->mode_config.max_height);
		mode_count = 0;
		list_for_each_entry(output, &dev->mode_config.output_list, head) {
			list_for_each(lh, &output->modes)
				mode_count++;
		}
	}

	/* handle this in 3 parts */
	/* CRTCs */
	if (card_res.count_crtcs >= crtc_count) {
		copied = 0;
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head){
			DRM_DEBUG("CRTC ID is %d\n", crtc->id);
			if (put_user(crtc->id, &card_res.crtc_id[copied++])) {
				retcode = -EFAULT;
				goto done;
			}
		}
	}
	card_res.count_crtcs = crtc_count;


	/* Outputs */
	if (card_res.count_outputs >= output_count) {
		copied = 0;
		list_for_each_entry(output, &dev->mode_config.output_list,
				    head) {
 			DRM_DEBUG("OUTPUT ID is %d\n", output->id);
			if (put_user(output->id, &card_res.output_id[copied++])) {
				retcode = -EFAULT;
				goto done;
			}
		}
	}
	card_res.count_outputs = output_count;
	
	/* Modes */
	if (card_res.count_modes >= mode_count) {
		copied = 0;
		list_for_each_entry(output, &dev->mode_config.output_list,
				    head) {
			list_for_each_entry(mode, &output->modes, head) {
				drm_crtc_convert_to_umode(&u_mode, mode);
				if (copy_to_user(&card_res.modes[copied++], &u_mode, sizeof(struct drm_mode_modeinfo))) {
					retcode = -EFAULT;
					goto done;
				}
			}
		}
	}
	card_res.count_modes = mode_count;

done:
	DRM_DEBUG("Counted %d %d %d\n", card_res.count_crtcs,
		  card_res.count_outputs,
		  card_res.count_modes);
	
	if (copy_to_user(argp, &card_res, sizeof(card_res)))
		return -EFAULT;

	return retcode;
}

int drm_mode_getcrtc(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	struct drm_mode_crtc __user *argp = (void __user *)arg;
	struct drm_mode_crtc crtc_resp;
	struct drm_crtc *crtc;
	struct drm_output *output;
	int ocount;
	int retcode = 0;

	if (copy_from_user(&crtc_resp, argp, sizeof(crtc_resp)))
		return -EFAULT;

	crtc = idr_find(&dev->mode_config.crtc_idr, crtc_resp.crtc_id);
	if (!crtc || (crtc->id != crtc_resp.crtc_id))
		return -EINVAL;
	crtc_resp.x = crtc->x;
	crtc_resp.y = crtc->y;
	crtc_resp.fb_id = 1;

	crtc_resp.outputs = 0;
	if (crtc->enabled) {

		crtc_resp.mode = crtc->mode.mode_id;
		ocount = 0;
		list_for_each_entry(output, &dev->mode_config.output_list, head) {
			if (output->crtc == crtc)
				crtc_resp.outputs |= 1 << (ocount++);
		}
	} else {
		crtc_resp.mode = 0;
	}

	if (copy_to_user(argp, &crtc_resp, sizeof(crtc_resp)))
		return -EFAULT;

	return retcode;
}

int drm_mode_getoutput(struct inode *inode, struct file *filp,
		       unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	struct drm_mode_get_output __user *argp = (void __user *)arg;
	struct drm_mode_get_output out_resp;
	struct drm_output *output;
	struct drm_display_mode *mode;
	int mode_count = 0;
	int retcode = 0;
	int copied = 0;

	if (copy_from_user(&out_resp, argp, sizeof(out_resp)))
		return -EFAULT;	

	DRM_DEBUG("output id %d:\n", out_resp.output);
	output= idr_find(&dev->mode_config.crtc_idr, out_resp.output);
	if (!output || (output->id != out_resp.output))
		return -EINVAL;

	DRM_DEBUG("about to count modes: %s\n", output->name);
	list_for_each_entry(mode, &output->modes, head)
		mode_count++;

	DRM_DEBUG("about to count modes %d %d %p\n", mode_count, out_resp.count_modes, output->crtc);
	strncpy(out_resp.name, output->name, DRM_OUTPUT_NAME_LEN);
	out_resp.name[DRM_OUTPUT_NAME_LEN-1] = 0;

	out_resp.mm_width = output->mm_width;
	out_resp.mm_height = output->mm_height;
	out_resp.subpixel = output->subpixel_order;
	out_resp.connection = output->status;
	if (output->crtc)
		out_resp.crtc = output->crtc->id;
	else
		out_resp.crtc = 0;

	if ((out_resp.count_modes >= mode_count) && mode_count) {
		copied = 0;
		list_for_each_entry(mode, &output->modes, head) {
			if (put_user(mode->mode_id, &out_resp.modes[copied++])) {
				retcode = -EFAULT;
				goto done;
			}
		}
	}
	out_resp.count_modes = mode_count;

done:
	if (copy_to_user(argp, &out_resp, sizeof(out_resp)))
		return -EFAULT;

	return retcode;
}


int drm_mode_setcrtc(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	struct drm_mode_crtc __user *argp = (void __user *)arg;
	struct drm_mode_crtc crtc_req;
	struct drm_crtc *crtc;
	struct drm_output **output_set = NULL, *output;
	struct drm_display_mode *mode;
	int retcode = 0;
	int i;

	if (copy_from_user(&crtc_req, argp, sizeof(crtc_req)))
		return -EFAULT;

	crtc = idr_find(&dev->mode_config.crtc_idr, crtc_req.crtc_id);
	if (!crtc || (crtc->id != crtc_req.crtc_id))
	{
		DRM_DEBUG("Unknown CRTC ID %d\n", crtc_req.crtc_id);
		return -EINVAL;
	}

	if (crtc_req.mode) {
		mode = idr_find(&dev->mode_config.crtc_idr, crtc_req.mode);
		if (!mode || (mode->mode_id != crtc_req.mode))
		{
			{
				struct drm_output *output;

				list_for_each_entry(output, &dev->mode_config.output_list, head) {
					list_for_each_entry(mode, &output->modes, head) {
						drm_mode_debug_printmodeline(dev, mode);
					}
				}
			}

			DRM_DEBUG("Unknown mode id %d, %p\n", crtc_req.mode, mode);
			return -EINVAL;
		}
	} else
		mode = NULL;

	if (crtc_req.count_outputs == 0 && mode) {
		DRM_DEBUG("Count outputs is 0 but mode set\n");
		return -EINVAL;
	}

	if (crtc_req.count_outputs > 0 && !mode) {
		DRM_DEBUG("Count outputs is %d but no mode set\n", crtc_req.count_outputs);
		return -EINVAL;
	}

	if (crtc_req.count_outputs > 0) {
		u32 out_id;
		output_set = kmalloc(crtc_req.count_outputs * sizeof(struct drm_output *), GFP_KERNEL);
		if (!output_set)
			return -ENOMEM;

		for (i = 0; i < crtc_req.count_outputs; i++) {
			if (get_user(out_id, &crtc_req.set_outputs[i]))
				return -EFAULT;

			output = idr_find(&dev->mode_config.crtc_idr, out_id);
			if (!output || (out_id != output->id)) {
				DRM_DEBUG("Output id %d unknown\n", out_id);
				return -EINVAL;
			}

			output_set[i] = output;
		}
	}
		
	retcode = drm_crtc_set_config(crtc, &crtc_req, mode, output_set);
	return retcode;
}

/* Add framebuffer ioctl */
int drm_mode_addfb(struct inode *inode, struct file *filp,
		   unsigned int cmd, unsigned long arg)
{
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->head->dev;
	struct drm_mode_fb_cmd __user *argp = (void __user *)arg;
	struct drm_mode_fb_cmd r;
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_framebuffer *fb;
	struct drm_buffer_object *bo;
	int ret;

	if (copy_from_user(&r, argp, sizeof(r)))
		return -EFAULT;

	if ((config->min_width > r.width) || (r.width > config->max_width)) {
		DRM_ERROR("mode new framebuffer width not within limits\n");
		return -EINVAL;
	}
	if ((config->min_height > r.height) || (r.height > config->max_height)) {
		DRM_ERROR("mode new framebuffer height not within limits\n");
		return -EINVAL;
	}

	/* TODO check limits are okay */
	ret = drm_get_buffer_object(dev, &bo, r.handle);
	if (ret || !bo)
		return -EINVAL;

	/* TODO check buffer is sufficently large */
	/* TODO setup destructor callback */

	fb = drm_framebuffer_create(dev);
	if(!fb)
		return -EINVAL;;

	fb->width	  = r.width;
	fb->height	 = r.height;
	fb->pitch	  = r.pitch;
	fb->bits_per_pixel = r.bpp;
	fb->offset	 = bo->offset;
	fb->bo	     = bo;

	r.buffer_id = fb->id;

	/* bind the fb to the crtc for now */
	{
		struct drm_crtc *crtc;
		list_for_each_entry(crtc, &dev->mode_config.crtc_list, head) {
			crtc->fb = fb;
		}
	}
	if (copy_to_user(argp, &r, sizeof(r)))
		return -EFAULT;

	return 0;
}

int drm_mode_rmfb(struct inode *inode, struct file *filp,
		  unsigned int cmd, unsigned long arg)
{
	drm_file_t *priv = filp->private_data;
	drm_device_t *dev = priv->head->dev;
	struct drm_framebuffer *fb = 0;
	uint32_t id = arg;

	fb = idr_find(&dev->mode_config.crtc_idr, id);
	/* TODO check that we realy get a framebuffer back. */
	if (!fb || (id != fb->id)) {
		DRM_ERROR("mode invalid framebuffer id\n");
		return -EINVAL;
	}

	/* TODO check if we own the buffer */
	/* TODO release all crtc connected to the framebuffer */
	/* TODO unhock the destructor from the buffer object */

	drm_framebuffer_destroy(fb);

	return 0;
}
