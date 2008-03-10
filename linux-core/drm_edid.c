/*
 * Copyright (c) 2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 *
 * DDC probing routines (drm_ddc_read & drm_do_probe_ddc_edid) originally from
 * FB layer.
 *   Copyright (C) 2006 Dennis Munsie <dmunsie@cecropia.com>
 */
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include "drmP.h"
#include "drm_edid.h"

/* Valid EDID header has these bytes */
static u8 edid_header[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

/**
 * edid_valid - sanity check EDID data
 * @edid: EDID data
 *
 * Sanity check the EDID block by looking at the header, the version number
 * and the checksum.  Return 0 if the EDID doesn't check out, or 1 if it's
 * valid.
 */
static bool edid_valid(struct edid *edid)
{
	int i;
	u8 csum = 0;
	u8 *raw_edid = (u8 *)edid;

	if (memcmp(edid->header, edid_header, sizeof(edid_header)))
		goto bad;
	if (edid->version != 1)
		goto bad;
	if (edid->revision <= 0 || edid->revision > 3)
		goto bad;

	for (i = 0; i < EDID_LENGTH; i++)
		csum += raw_edid[i];
	if (csum)
		goto bad;

	return 1;

bad:
	return 0;
}

/**
 * drm_mode_std - convert standard mode info (width, height, refresh) into mode
 * @t: standard timing params
 *
 * Take the standard timing params (in this case width, aspect, and refresh)
 * and convert them into a real mode using CVT.
 *
 * Punts for now, but should eventually use the FB layer's CVT based mode
 * generation code.
 */
struct drm_display_mode *drm_mode_std(struct drm_device *dev,
				      struct std_timing *t)
{
//	struct fb_videomode mode;

//	fb_find_mode_cvt(&mode, 0, 0);
	/* JJJ:  convert to drm_display_mode */
	struct drm_display_mode *mode;
	int hsize = t->hsize * 8 + 248, vsize;

	mode = drm_mode_create(dev);
	if (!mode)
		return NULL;

	if (t->aspect_ratio == 0)
		vsize = (hsize * 10) / 16;
	else if (t->aspect_ratio == 1)
		vsize = (hsize * 3) / 4;
	else if (t->aspect_ratio == 2)
		vsize = (hsize * 4) / 5;
	else
		vsize = (hsize * 9) / 16;

	drm_mode_set_name(mode);

	return mode;
}

/**
 * drm_mode_detailed - create a new mode from an EDID detailed timing section
 * @timing: EDID detailed timing info
 * @preferred: is this a preferred mode?
 *
 * An EDID detailed timing block contains enough info for us to create and
 * return a new struct drm_display_mode.  The @preferred flag will be set
 * if this is the display's preferred timing, and we'll use it to indicate
 * to the other layers that this mode is desired.
 */
struct drm_display_mode *drm_mode_detailed(struct drm_device *dev,
					   struct detailed_timing *timing)
{
	struct drm_display_mode *mode;
	struct detailed_pixel_timing *pt = &timing->data.pixel_data;

	if (pt->stereo) {
		printk(KERN_WARNING "stereo mode not supported\n");
		return NULL;
	}
	if (!pt->separate_sync) {
		printk(KERN_WARNING "integrated sync not supported\n");
		return NULL;
	}

	mode = drm_mode_create(dev);
	if (!mode)
		return NULL;

	mode->type = DRM_MODE_TYPE_DRIVER;
	mode->clock = timing->pixel_clock * 10;

	mode->hdisplay = (pt->hactive_hi << 8) | pt->hactive_lo;
	mode->hsync_start = mode->hdisplay + ((pt->hsync_offset_hi << 8) |
					      pt->hsync_offset_lo);
	mode->hsync_end = mode->hsync_start +
		((pt->hsync_pulse_width_hi << 8) |
		 pt->hsync_pulse_width_lo);
	mode->htotal = mode->hdisplay + ((pt->hblank_hi << 8) | pt->hblank_lo);

	mode->vdisplay = (pt->vactive_hi << 8) | pt->vactive_lo;
	mode->vsync_start = mode->vdisplay + ((pt->vsync_offset_hi << 8) |
					      pt->vsync_offset_lo);
	mode->vsync_end = mode->vsync_start +
		((pt->vsync_pulse_width_hi << 8) |
		 pt->vsync_pulse_width_lo);
	mode->vtotal = mode->vdisplay + ((pt->vblank_hi << 8) | pt->vblank_lo);

	drm_mode_set_name(mode);

	if (pt->interlaced)
		mode->flags |= V_INTERLACE;

	mode->flags |= pt->hsync_positive ? V_PHSYNC : V_NHSYNC;
	mode->flags |= pt->vsync_positive ? V_PVSYNC : V_NVSYNC;

	return mode;
}

/*
 * Detailed mode info for the EDID "established modes" data to use.
 */
static struct drm_display_mode edid_est_modes[] = {
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 40000, 800, 840,
		   968, 1056, 0, 600, 601, 605, 628, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 800x600@60Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 36000, 800, 824,
		   896, 1024, 0, 600, 601, 603,  625, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 800x600@56Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 31500, 640, 656,
		   720, 840, 0, 480, 481, 484, 500, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 640x480@75Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 31500, 640, 664,
		   704,  832, 0, 480, 489, 491, 520, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 640x480@72Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 30240, 640, 704,
		   768,  864, 0, 480, 483, 486, 525, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 640x480@67Hz */
	{ DRM_MODE("640x480", DRM_MODE_TYPE_DRIVER, 25200, 640, 656,
		   752, 800, 0, 480, 490, 492, 525, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 640x480@60Hz */
	{ DRM_MODE("720x400", DRM_MODE_TYPE_DRIVER, 35500, 720, 738,
		   846, 900, 0, 400, 421, 423,  449, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 720x400@88Hz */
	{ DRM_MODE("720x400", DRM_MODE_TYPE_DRIVER, 28320, 720, 738,
		   846,  900, 0, 400, 412, 414, 449, 0,
		   V_NHSYNC | V_PVSYNC) }, /* 720x400@70Hz */
	{ DRM_MODE("1280x1024", DRM_MODE_TYPE_DRIVER, 135000, 1280, 1296,
		   1440, 1688, 0, 1024, 1025, 1028, 1066, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 1280x1024@75Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 78800, 1024, 1040,
		   1136, 1312, 0,  768, 769, 772, 800, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 1024x768@75Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 75000, 1024, 1048,
		   1184, 1328, 0,  768, 771, 777, 806, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 1024x768@70Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER, 65000, 1024, 1048,
		   1184, 1344, 0,  768, 771, 777, 806, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 1024x768@60Hz */
	{ DRM_MODE("1024x768", DRM_MODE_TYPE_DRIVER,44900, 1024, 1032,
		   1208, 1264, 0, 768, 768, 776, 817, 0,
		   V_PHSYNC | V_PVSYNC | V_INTERLACE) }, /* 1024x768@43Hz */
	{ DRM_MODE("832x624", DRM_MODE_TYPE_DRIVER, 57284, 832, 864,
		   928, 1152, 0, 624, 625, 628, 667, 0,
		   V_NHSYNC | V_NVSYNC) }, /* 832x624@75Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 49500, 800, 816,
		   896, 1056, 0, 600, 601, 604,  625, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 800x600@75Hz */
	{ DRM_MODE("800x600", DRM_MODE_TYPE_DRIVER, 50000, 800, 856,
		   976, 1040, 0, 600, 637, 643, 666, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 800x600@72Hz */
	{ DRM_MODE("1152x864", DRM_MODE_TYPE_DRIVER, 108000, 1152, 1216,
		   1344, 1600, 0,  864, 865, 868, 900, 0,
		   V_PHSYNC | V_PVSYNC) }, /* 1152x864@75Hz */
};

#define EDID_EST_TIMINGS 16
#define EDID_STD_TIMINGS 8
#define EDID_DETAILED_TIMINGS 4

/**
 * add_established_modes - get est. modes from EDID and add them
 * @edid: EDID block to scan
 *
 * Each EDID block contains a bitmap of the supported "established modes" list
 * (defined above).  Tease them out and add them to the global modes list.
 */
static int add_established_modes(struct drm_output *output, struct edid *edid)
{
	struct drm_device *dev = output->dev;
	unsigned long est_bits = edid->established_timings.t1 |
		(edid->established_timings.t2 << 8) |
		((edid->established_timings.mfg_rsvd & 0x80) << 9);
	int i, modes = 0;

	for (i = 0; i <= EDID_EST_TIMINGS; i++)
		if (est_bits & (1<<i)) {
			struct drm_display_mode *newmode;
			newmode = drm_mode_duplicate(dev, &edid_est_modes[i]);
			if (newmode) {
				drm_mode_probed_add(output, newmode);
				modes++;
			}
		}

	return modes;
}

/**
 * add_standard_modes - get std. modes from EDID and add them
 * @edid: EDID block to scan
 *
 * Standard modes can be calculated using the CVT standard.  Grab them from
 * @edid, calculate them, and add them to the list.
 */
static int add_standard_modes(struct drm_output *output, struct edid *edid)
{
	struct drm_device *dev = output->dev;
	int i, modes = 0;

	for (i = 0; i < EDID_STD_TIMINGS; i++) {
		struct std_timing *t = &edid->standard_timings[i];
		struct drm_display_mode *newmode;

		/* If std timings bytes are 1, 1 it's empty */
		if (t->hsize == 1 && (t->aspect_ratio | t->vfreq) == 1)
			continue;

		newmode = drm_mode_std(dev, &edid->standard_timings[i]);
		if (newmode) {
			drm_mode_probed_add(output, newmode);
			modes++;
		}
	}

	return modes;
}

/**
 * add_detailed_modes - get detailed mode info from EDID data
 * @edid: EDID block to scan
 *
 * Some of the detailed timing sections may contain mode information.  Grab
 * it and add it to the list.
 */
static int add_detailed_info(struct drm_output *output, struct edid *edid)
{
	struct drm_device *dev = output->dev;
	int i, j, modes = 0;

	for (i = 0; i < EDID_DETAILED_TIMINGS; i++) {
		struct detailed_timing *timing = &edid->detailed_timings[i];
		struct detailed_non_pixel *data = &timing->data.other_data;
		struct drm_display_mode *newmode;

		/* EDID up to and including 1.2 may put monitor info here */
		if (edid->version == 1 && edid->revision < 3)
			continue;

		/* Detailed mode timing */
		if (timing->pixel_clock) {
			newmode = drm_mode_detailed(dev, timing);
			/* First detailed mode is preferred */
			if (newmode) {
				if (i == 0 && edid->preferred_timing)
					newmode->type |= DRM_MODE_TYPE_PREFERRED;
				drm_mode_probed_add(output, newmode);
				     
				modes++;
			}
			continue;
		}

		/* Other timing or info */
		switch (data->type) {
		case EDID_DETAIL_MONITOR_SERIAL:
			break;
		case EDID_DETAIL_MONITOR_STRING:
			break;
		case EDID_DETAIL_MONITOR_RANGE:
			/* Get monitor range data */
			break;
		case EDID_DETAIL_MONITOR_NAME:
			break;
		case EDID_DETAIL_MONITOR_CPDATA:
			break;
		case EDID_DETAIL_STD_MODES:
			/* Five modes per detailed section */
			for (j = 0; j < 5; i++) {
				struct std_timing *std;
				struct drm_display_mode *newmode;

				std = &data->data.timings[j];
				newmode = drm_mode_std(dev, std);
				if (newmode) {
					drm_mode_probed_add(output, newmode);
					modes++;
				}
			}
			break;
		default:
			break;
		}
	}

	return modes;
}

#define DDC_ADDR 0x50

static unsigned char *drm_do_probe_ddc_edid(struct i2c_adapter *adapter)
{
	unsigned char start = 0x0;
	unsigned char *buf = kmalloc(EDID_LENGTH, GFP_KERNEL);
	struct i2c_msg msgs[] = {
		{
			.addr	= DDC_ADDR,
			.flags	= 0,
			.len	= 1,
			.buf	= &start,
		}, {
			.addr	= DDC_ADDR,
			.flags	= I2C_M_RD,
			.len	= EDID_LENGTH,
			.buf	= buf,
		}
	};

	if (!buf) {
		dev_warn(&adapter->dev, "unable to allocate memory for EDID "
			 "block.\n");
		return NULL;
	}

	if (i2c_transfer(adapter, msgs, 2) == 2)
		return buf;

	dev_info(&adapter->dev, "unable to read EDID block.\n");
	kfree(buf);
	return NULL;
}

static unsigned char *drm_ddc_read(struct i2c_adapter *adapter)
{
	struct i2c_algo_bit_data *algo_data = adapter->algo_data;
	unsigned char *edid = NULL;
	int i, j;

	/*
	 * Startup the bus:
	 *   Set clock line high (but give it time to come up)
	 *   Then set clock & data low
	 */
	algo_data->setscl(algo_data->data, 1);
	udelay(550); /* startup delay */
	algo_data->setscl(algo_data->data, 0);
	algo_data->setsda(algo_data->data, 0);

	for (i = 0; i < 3; i++) {
		/* For some old monitors we need the
		 * following process to initialize/stop DDC
		 */
		algo_data->setsda(algo_data->data, 0);
		msleep(13);

		algo_data->setscl(algo_data->data, 1);
		for (j = 0; j < 5; j++) {
			msleep(10);
			if (algo_data->getscl(algo_data->data))
				break;
		}
		if (j == 5)
			continue;

		algo_data->setsda(algo_data->data, 0);
		msleep(15);
		algo_data->setscl(algo_data->data, 0);
		msleep(15);
		algo_data->setsda(algo_data->data, 1);
		msleep(15);

		/* Do the real work */
		edid = drm_do_probe_ddc_edid(adapter);
		algo_data->setsda(algo_data->data, 0);
		algo_data->setscl(algo_data->data, 0);
		msleep(15);

		algo_data->setscl(algo_data->data, 1);
		for (j = 0; j < 10; j++) {
			msleep(10);
			if (algo_data->getscl(algo_data->data))
				break;
		}

		algo_data->setsda(algo_data->data, 1);
		msleep(15);
		algo_data->setscl(algo_data->data, 0);
		if (edid)
			break;
	}
	/* Release the DDC lines when done or the Apple Cinema HD display
	 * will switch off
	 */
	algo_data->setsda(algo_data->data, 0);
	algo_data->setscl(algo_data->data, 0);
	algo_data->setscl(algo_data->data, 1);

	return edid;
}

/**
 * drm_get_edid - get EDID data, if available
 * @output: output we're probing
 * @adapter: i2c adapter to use for DDC
 *
 * Poke the given output's i2c channel to grab EDID data if possible.
 * 
 * Return edid data or NULL if we couldn't find any.
 */
struct edid *drm_get_edid(struct drm_output *output,
			  struct i2c_adapter *adapter)
{
	struct edid *edid;

	edid = (struct edid *)drm_ddc_read(adapter);
	if (!edid) {
		dev_warn(&output->dev->pdev->dev, "%s: no EDID data\n",
			 drm_get_output_name(output));
		return NULL;
	}
	if (!edid_valid(edid)) {
		dev_warn(&output->dev->pdev->dev, "%s: EDID invalid.\n",
			 drm_get_output_name(output));
		kfree(edid);
		return NULL;
	}
	return edid;
}
EXPORT_SYMBOL(drm_get_edid);

/**
 * drm_add_edid_modes - add modes from EDID data, if available
 * @output: output we're probing
 * @edid: edid data
 *
 * Add the specified modes to the output's mode list.
 *
 * Return number of modes added or 0 if we couldn't find any.
 */
int drm_add_edid_modes(struct drm_output *output, struct edid *edid)
{
	int num_modes = 0;

	if (edid == NULL) {
		return 0;
	}
	if (!edid_valid(edid)) {
		dev_warn(&output->dev->pdev->dev, "%s: EDID invalid.\n",
			 drm_get_output_name(output));
		return 0;
	}
	num_modes += add_established_modes(output, edid);
	num_modes += add_standard_modes(output, edid);
	num_modes += add_detailed_info(output, edid);
	return num_modes;
}
EXPORT_SYMBOL(drm_add_edid_modes);
