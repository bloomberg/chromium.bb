/*
 * Copyright (c) 2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 */

#include <linux/i2c.h>
#include <linux/fb.h>
#include "drmP.h"
#include "intel_drv.h"

/*
 * DDC/EDID probing rippped off from FB layer
 */

#include "edid.h"
#define DDC_ADDR 0x50

#ifdef BIG_ENDIAN
#error "EDID structure is little endian, need big endian versions"
#endif

struct est_timings {
	u8 t1;
	u8 t2;
	u8 mfg_rsvd;
} __attribute__((packed));

struct std_timing {
	u8 hsize; /* need to multiply by 8 then add 248 */
	u8 vfreq:6; /* need to add 60 */
	u8 aspect_ratio:2; /* 00=16:10, 01=4:3, 10=5:4, 11=16:9 */
} __attribute__((packed));

/* If detailed data is pixel timing */
struct detailed_pixel_timing {
	u8 hactive_lo;
	u8 hblank_lo;
	u8 hblank_hi:4;
	u8 hactive_hi:4;
	u8 vactive_lo;
	u8 vblank_lo;
	u8 vblank_hi:4;
	u8 vactive_hi:4;
	u8 hsync_offset_lo;
	u8 hsync_pulse_width_lo;
	u8 vsync_pulse_width_lo:4;
	u8 vsync_offset_lo:4;
	u8 hsync_pulse_width_hi:2;
	u8 hsync_offset_hi:2;
	u8 vsync_pulse_width_hi:2;
	u8 vsync_offset_hi:2;
	u8 width_mm_lo;
	u8 height_mm_lo;
	u8 height_mm_hi:4;
	u8 width_mm_hi:4;
	u8 hborder;
	u8 vborder;
	u8 unknown0:1;
	u8 vsync_positive:1;
	u8 hsync_positive:1;
	u8 separate_sync:2;
	u8 stereo:1;
	u8 unknown6:1;
	u8 interlaced:1;
} __attribute__((packed));

/* If it's not pixel timing, it'll be one of the below */
struct detailed_data_string {
	u8 str[13];
} __attribute__((packed));

struct detailed_data_monitor_range {
	u8 min_vfreq;
	u8 max_vfreq;
	u8 min_hfreq_khz;
	u8 max_hfreq_khz;
	u8 pixel_clock_mhz; /* need to multiply by 10 */
	u16 sec_gtf_toggle; /* A000=use above, 20=use below */
	u8 hfreq_start_khz; /* need to multiply by 2 */
	u8 c; /* need to divide by 2 */
	u16 m; /* FIXME: byte order */
	u8 k;
	u8 j; /* need to divide by 2 */
} __attribute__((packed));

struct detailed_data_wpindex {
	u8 white_y_lo:2;
	u8 white_x_lo:2;
	u8 pad:4;
	u8 white_x_hi;
	u8 white_y_hi;
	u8 gamma; /* need to divide by 100 then add 1 */
} __attribute__((packed));

struct detailed_data_color_point {
	u8 windex1;
	u8 wpindex1[3];
	u8 windex2;
	u8 wpindex2[3];
} __attribute__((packed));

struct detailed_non_pixel {
	u8 pad1;
	u8 type; /* ff=serial, fe=string, fd=monitor range, fc=monitor name
		    fb=color point data, fa=standard timing data,
		    f9=undefined, f8=mfg. reserved */
	u8 pad2;
	union {
		struct detailed_data_string str;
		struct detailed_data_monitor_range range;
		struct detailed_data_wpindex color;
		struct std_timing timings[5];
	} data;
} __attribute__((packed));

#define EDID_DETAIL_STD_MODES 0xfa
#define EDID_DETAIL_CPDATA 0xfb
#define EDID_DETAIL_NAME 0xfc
#define EDID_DETAIL_RANGE 0xfd
#define EDID_DETAIL_STRING 0xfe
#define EDID_DETAIL_SERIAL 0xff

struct detailed_timing {
	u16 pixel_clock; /* need to multiply by 10 KHz */
	union {
		struct detailed_pixel_timing pixel_data;
		struct detailed_non_pixel other_data;
	} data;
} __attribute__((packed));

struct edid {
	u8 header[8];
	/* Vendor & product info */
	u16 mfg_id; /* FIXME: byte order */
	u16 prod_code; /* FIXME: byte order */
	u32 serial; /* FIXME: byte order */
	u8 mfg_week;
	u8 mfg_year;
	/* EDID version */
	u8 version;
	u8 revision;
	/* Display info: */
	/*   input definition */
	u8 serration_vsync:1;
	u8 sync_on_green:1;
	u8 composite_sync:1;
	u8 separate_syncs:1;
	u8 blank_to_black:1;
	u8 video_level:2;
	u8 digital:1; /* bits below must be zero if set */
	u8 width_cm;
	u8 height_cm;
	u8 gamma;
	/*   feature support */
	u8 default_gtf:1;
	u8 preferred_timing:1;
	u8 standard_color:1;
	u8 display_type:2; /* 00=mono, 01=rgb, 10=non-rgb, 11=unknown */
	u8 pm_active_off:1;
	u8 pm_suspend:1;
	u8 pm_standby:1;
	/* Color characteristics */
	u8 red_green_lo;
	u8 black_white_lo;
	u8 red_x;
	u8 red_y;
	u8 green_x;
	u8 green_y;
	u8 blue_x;
	u8 blue_y;
	u8 white_x;
	u8 white_y;
	/* Est. timings and mfg rsvd timings*/
	struct est_timings established_timings;
	/* Standard timings 1-8*/
	struct std_timing standard_timings[8];
	/* Detailing timings 1-4 */
	struct detailed_timing detailed_timings[4];
	/* Number of 128 byte ext. blocks */
	u8 extensions;
	/* Checksum */
	u8 checksum;
} __attribute__((packed));

static u8 edid_header[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

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
 * Punts for now.
 */
struct drm_display_mode *drm_mode_std(struct drm_device *dev,
				      struct std_timing *t)
{
//	struct fb_videomode mode;

//	fb_find_mode_cvt(&mode, 0, 0);
	/* JJJ:  convert to drm_display_mode */
	struct drm_display_mode *mode;
	int hsize = t->hsize * 8 + 248, vsize;

	mode = drm_crtc_mode_create(dev);
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

	snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "%dx%d", hsize, vsize);

	return mode;
}

struct drm_display_mode *drm_mode_detailed(drm_device_t *dev,
					   struct detailed_timing *timing,
					   bool preferred)
{
	struct drm_display_mode *mode;
	struct detailed_pixel_timing *pt = &timing->data.pixel_data;

	if (pt->stereo) {
		printk(KERN_ERR "stereo mode not supported\n");
		return NULL;
	}
	if (!pt->separate_sync) {
		printk(KERN_ERR "integrated sync not supported\n");
		return NULL;
	}

	mode = drm_crtc_mode_create(dev);
	if (!mode)
		return NULL;

	mode->type = DRM_MODE_TYPE_DRIVER;
	mode->type |= preferred ? DRM_MODE_TYPE_PREFERRED : 0;
	mode->clock = timing->pixel_clock / 100;

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

	snprintf(mode->name, DRM_DISPLAY_MODE_LEN, "%dx%d", mode->hdisplay,
		 mode->vdisplay);

	if (pt->interlaced)
		mode->flags |= V_INTERLACE;

	mode->flags |= pt->hsync_positive ? V_PHSYNC : V_NHSYNC;
	mode->flags |= pt->vsync_positive ? V_PVSYNC : V_NVSYNC;

	return mode;
}

static struct drm_display_mode established_modes[] = {
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
			drm_mode_probed_add(output,
					    drm_mode_duplicate(dev, &established_modes[i]));
			modes++;
		}

	return modes;
}

/**
 * add_established_modes - get std. modes from EDID and add them
 * @edid: EDID block to scan
 *
 * Standard modes can be calculated using the CVT standard.  Grab them from
 * @edid, calculate them, and add them to the list.
 */
static int add_standard_modes(struct drm_output *output, struct edid *edid)
{
	int i, modes = 0;
	struct drm_device *dev = output->dev;

	for (i = 0; i < EDID_STD_TIMINGS; i++) {
		struct std_timing *t = &edid->standard_timings[i];

		/* If std timings bytes are 1, 1 it's empty */
		if (t->hsize == 1 && (t->aspect_ratio | t->vfreq) == 1)
			continue;

		drm_mode_probed_add(output,
				    drm_mode_std(dev, &edid->standard_timings[i]));
		modes++;
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
	int i, j, modes = 0;
	bool preferred = 0;
	struct drm_device *dev = output->dev;

	for (i = 0; i < EDID_DETAILED_TIMINGS; i++) {
		struct detailed_timing *timing = &edid->detailed_timings[i];
		struct detailed_non_pixel *data = &timing->data.other_data;

		/* EDID up to and including 1.2 may put monitor info here */
		if (edid->version == 1 && edid->revision < 3)
			continue;

		/* Detailed mode timing */
		if (timing->pixel_clock) {
			if (i == 0 && edid->preferred_timing)
				preferred = 1;
			drm_mode_probed_add(output,
					    drm_mode_detailed(dev, timing, preferred));
			modes++;
			continue;
		}

		/* Other timing or info */
		switch (data->type) {
		case EDID_DETAIL_SERIAL:
			break;
		case EDID_DETAIL_STRING:
			break;
		case EDID_DETAIL_RANGE:
			break;
		case EDID_DETAIL_NAME:
			break;
		case EDID_DETAIL_CPDATA:
			break;
		case EDID_DETAIL_STD_MODES:
			/* Five modes per detailed section */
			for (j = 0; j < 5; i++) {
				struct std_timing *std;

				std = &data->data.timings[j];
				drm_mode_probed_add(output, drm_mode_std(dev, std));
				modes++;
			}
			break;
		default:
			break;
		}
	}

	return modes;
}

/**
 * drm_add_edid_modes - add modes from EDID data, if available
 * @output: output we're probing
 * @adapter: i2c adapter to use for DDC
 *
 * Poke the given output's i2c channel to grab EDID data if possible.  If we
 * get any, add the specified modes to the output's mode list.
 *
 * Return number of modes added or 0 if we couldn't find any.
 */
int drm_add_edid_modes(struct drm_output *output, struct i2c_adapter *adapter)
{
	struct edid *edid;
	u8 *raw_edid;
	int i, est_modes, std_modes, det_modes;

	edid = (struct edid *)fb_ddc_read(adapter);

	if (!edid) {
		dev_warn(&output->dev->pdev->dev, "no EDID data\n");
		goto out_err;
	}

	if (!edid_valid(edid)) {
		dev_warn(&output->dev->pdev->dev, "EDID invalid, ignoring.\n");
		goto out_err;
	}

	est_modes = add_established_modes(output, edid);
	std_modes = add_standard_modes(output, edid);
	det_modes = add_detailed_info(output, edid);
	printk(KERN_ERR "est modes: %d, std_modes: %d, det_modes: %d\n",
	       est_modes, std_modes, det_modes);

	raw_edid = (u8 *)edid;
	printk(KERN_ERR "EDID:\n" KERN_ERR);
	for (i = 0; i < EDID_LENGTH; i++) {
		if (i != 0 && ((i % 16) == 0))
		    printk("\n" KERN_ERR);
		printk("%02x", raw_edid[i] & 0xff);
	}
	printk("\n");

	printk(KERN_ERR "EDID info:\n");
	printk(KERN_ERR "  mfg_id: 0x%04x\n", edid->mfg_id);
	printk(KERN_ERR "  digital?  %s\n", edid->digital ? "Yes" : "No");
	printk(KERN_ERR "  extensions: %d\n", edid->extensions);

	return est_modes + std_modes + det_modes;

out_err:
	kfree(edid);
	return 0;
}
EXPORT_SYMBOL(drm_add_edid_modes);
