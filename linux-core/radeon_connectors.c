/*
 * Copyright 2007-8 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 */
#include "drmP.h"
#include "drm_edid.h"
#include "drm_crtc_helper.h"
#include "radeon_drm.h"
#include "radeon_drv.h"

static int radeon_lvds_get_modes(struct drm_connector *connector)
{
	struct radeon_connector *radeon_connector = to_radeon_connector(connector);
	struct drm_encoder *lvds_encoder;
	int ret = 0;
	struct edid *edid;

	radeon_i2c_do_lock(radeon_connector, 1);
	edid = drm_get_edid(&radeon_connector->base, &radeon_connector->ddc_bus->adapter);
	radeon_i2c_do_lock(radeon_connector, 0);
	if (edid) {
		drm_mode_connector_update_edid_property(&radeon_connector->base, edid);
		ret = drm_add_edid_modes(&radeon_connector->base, edid);
		kfree(edid);
		return ret;
	}

#if 0
	lvds_encoder = radeon_best_single_encoder(connector);

	if (!lvds_encoder)
		return ret;

	radeon_encoder_update_panel_size(lvds_encoder, connector);
#endif
	return ret;
}

static int radeon_lvds_mode_valid(struct drm_connector *connector,
				  struct drm_display_mode *mode)
{

	return MODE_OK;
}

static enum drm_connector_status radeon_lvds_detect(struct drm_connector *connector)
{
	return connector_status_connected;
}



struct drm_encoder *radeon_best_single_encoder(struct drm_connector *connector)
{
	int enc_id = connector->encoder_ids[0];
	struct drm_mode_object *obj;
	struct drm_encoder *encoder;

	/* pick the encoder ids */
	if (enc_id) {
		obj = drm_mode_object_find(connector->dev, enc_id, DRM_MODE_OBJECT_ENCODER);
		if (!obj)
			return NULL;
		encoder = obj_to_encoder(obj);
		return encoder;
	}
	return NULL;
}

static void radeon_connector_destroy(struct drm_connector *connector)
{
	struct radeon_connector *radeon_connector = to_radeon_connector(connector);

	if (radeon_connector->ddc_bus)
		radeon_i2c_destroy(radeon_connector->ddc_bus);
	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
	kfree(connector);
}

struct drm_connector_helper_funcs radeon_lvds_connector_helper_funcs = {
	.get_modes = radeon_lvds_get_modes,
	.mode_valid = radeon_lvds_mode_valid,
	.best_encoder = radeon_best_single_encoder,
};

struct drm_connector_funcs radeon_lvds_connector_funcs = {
	.detect = radeon_lvds_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = radeon_connector_destroy,
};

static int radeon_vga_get_modes(struct drm_connector *connector)
{
	struct radeon_connector *radeon_connector = to_radeon_connector(connector);
	int ret;

	ret = radeon_ddc_get_modes(radeon_connector);

	return ret;
}

static int radeon_vga_mode_valid(struct drm_connector *connector,
				  struct drm_display_mode *mode)
{

	return MODE_OK;
}

static enum drm_connector_status radeon_vga_detect(struct drm_connector *connector)
{
	struct edid *edid;
	struct radeon_connector *radeon_connector = to_radeon_connector(connector);
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *encoder_funcs;

	radeon_i2c_do_lock(radeon_connector, 1);
	edid = drm_get_edid(&radeon_connector->base, &radeon_connector->ddc_bus->adapter);
	radeon_i2c_do_lock(radeon_connector, 0);
	if (edid) {
		kfree(edid);
		return connector_status_connected;
	}

	/* if EDID fails to a load detect */
	encoder = radeon_best_single_encoder(connector);
	if (!encoder)
		return connector_status_disconnected;

	encoder_funcs = encoder->helper_private;
	return encoder_funcs->detect(encoder, connector);
}

struct drm_connector_helper_funcs radeon_vga_connector_helper_funcs = {
	.get_modes = radeon_vga_get_modes,
	.mode_valid = radeon_vga_mode_valid,
	.best_encoder = radeon_best_single_encoder,
};

struct drm_connector_funcs radeon_vga_connector_funcs = {
	.detect = radeon_vga_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = radeon_connector_destroy,
};


static enum drm_connector_status radeon_dvi_detect(struct drm_connector *connector)
{
	struct edid *edid;
	struct radeon_connector *radeon_connector = to_radeon_connector(connector);
	struct drm_encoder *encoder;
	struct drm_encoder_helper_funcs *encoder_funcs;
	struct drm_mode_object *obj;
	int i;
	enum drm_connector_status ret;

	radeon_i2c_do_lock(radeon_connector, 1);
	edid = drm_get_edid(&radeon_connector->base, &radeon_connector->ddc_bus->adapter);
	radeon_i2c_do_lock(radeon_connector, 0);
	if (edid) {
		/* if the monitor is digital - set the bits */
		if (edid->digital)
			radeon_connector->use_digital = 1;
		else
			radeon_connector->use_digital = 0;

		kfree(edid);
		return connector_status_connected;
	}

	for (i = 0; i < DRM_CONNECTOR_MAX_ENCODER; i++) {
		if (connector->encoder_ids[i] == 0)
			break;

		obj = drm_mode_object_find(connector->dev, connector->encoder_ids[i], DRM_MODE_OBJECT_ENCODER);
		if (!obj)
			continue;

		encoder = obj_to_encoder(obj);

		encoder_funcs = encoder->helper_private;
		if (encoder_funcs->detect) {
			ret = encoder_funcs->detect(encoder, connector);
			if (ret == connector_status_connected) {
				radeon_connector->use_digital = 0;
				return ret;
			}
		}
	}
	return connector_status_disconnected;
}

/* okay need to be smart in here about which encoder to pick */
struct drm_encoder *radeon_dvi_encoder(struct drm_connector *connector)
{
	int enc_id = connector->encoder_ids[0];
	struct radeon_connector *radeon_connector = to_radeon_connector(connector);
	struct drm_mode_object *obj;
	struct drm_encoder *encoder;
	int i;
	for (i = 0; i < DRM_CONNECTOR_MAX_ENCODER; i++) {
		if (connector->encoder_ids[i] == 0)
			break;

		obj = drm_mode_object_find(connector->dev, connector->encoder_ids[i], DRM_MODE_OBJECT_ENCODER);
		if (!obj)
			continue;

		encoder = obj_to_encoder(obj);

		if (radeon_connector->use_digital) {
			if (encoder->encoder_type == DRM_MODE_ENCODER_TMDS)
				return encoder;
		} else {
			if (encoder->encoder_type == DRM_MODE_ENCODER_DAC ||
			    encoder->encoder_type == DRM_MODE_ENCODER_TVDAC)
				return encoder;
		}
	}

	/* see if we have a default encoder  TODO */
	
	/* then check use digitial */
	/* pick the first one */
	if (enc_id) {
		obj = drm_mode_object_find(connector->dev, enc_id, DRM_MODE_OBJECT_ENCODER);
		if (!obj)
			return NULL;
		encoder = obj_to_encoder(obj);
		return encoder;
	}
	return NULL;
}

struct drm_connector_helper_funcs radeon_dvi_connector_helper_funcs = {
	.get_modes = radeon_vga_get_modes,
	.mode_valid = radeon_vga_mode_valid,
	.best_encoder = radeon_dvi_encoder,
};

struct drm_connector_funcs radeon_dvi_connector_funcs = {
	.detect = radeon_dvi_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = radeon_connector_destroy,
};


static struct connector_funcs {
	int conn_id;
	struct drm_connector_funcs *connector_funcs;
	struct drm_connector_helper_funcs *helper_funcs;
	int conn_type;
	char *i2c_id;
} connector_fns[] = {
	{ CONNECTOR_NONE, NULL, NULL, DRM_MODE_CONNECTOR_Unknown },
	{ CONNECTOR_VGA, &radeon_vga_connector_funcs, &radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA , "VGA"},
	{ CONNECTOR_LVDS, &radeon_lvds_connector_funcs, &radeon_lvds_connector_helper_funcs, DRM_MODE_CONNECTOR_LVDS, "LVDS" },
	{ CONNECTOR_DVI_A, &radeon_vga_connector_funcs, &radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_DVIA, "DVI" },
	{ CONNECTOR_DVI_I, &radeon_dvi_connector_funcs, &radeon_dvi_connector_helper_funcs, DRM_MODE_CONNECTOR_DVII, "DVI" },

#if 0
	{ CONNECTOR_DVI_D, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },

	{ CONNECTOR_STV, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_CTV, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_DIGITAL, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_SCART, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },

	{ CONNECTOR_HDMI_TYPE_A, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_HDMI_TYPE_B, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_HDMI_TYPE_B, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_HDMI_TYPE_B, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_DIN, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
	{ CONNECTOR_DISPLAY_PORT, radeon_vga_connector_funcs, radeon_vga_connector_helper_funcs, DRM_MODE_CONNECTOR_VGA },
#endif
};

struct drm_connector *radeon_connector_add(struct drm_device *dev, int bios_index)
{
	struct radeon_connector *radeon_connector;
	struct drm_radeon_private *dev_priv = dev->dev_private;
	struct radeon_mode_info *mode_info = &dev_priv->mode_info;
	struct drm_connector *connector;
	int table_idx;

	for (table_idx = 0; table_idx < ARRAY_SIZE(connector_fns); table_idx++) {
		if (connector_fns[table_idx].conn_id == mode_info->bios_connector[bios_index].connector_type)
			break;
	}

	if (table_idx == ARRAY_SIZE(connector_fns))
		return NULL;

	radeon_connector = kzalloc(sizeof(struct radeon_connector), GFP_KERNEL);
	if (!radeon_connector) {
		return NULL;
	}

	connector = &radeon_connector->base;

	drm_connector_init(dev, &radeon_connector->base, connector_fns[table_idx].connector_funcs,
			   connector_fns[table_idx].conn_type);

	drm_connector_helper_add(&radeon_connector->base, connector_fns[table_idx].helper_funcs);

	if (mode_info->bios_connector[bios_index].ddc_i2c.valid) {
		radeon_connector->ddc_bus = radeon_i2c_create(dev, &mode_info->bios_connector[bios_index].ddc_i2c,
							      connector_fns[table_idx].i2c_id);
		if (!radeon_connector->ddc_bus)
			goto failed;
	}

	drm_sysfs_connector_add(connector);
	return connector;


failed:
	if (radeon_connector->ddc_bus)
		radeon_i2c_destroy(radeon_connector->ddc_bus);
	drm_connector_cleanup(connector);
	kfree(connector);
	return NULL;
}
