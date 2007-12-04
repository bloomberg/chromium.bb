/*
 * Copyright © 2006 Keith Packard
 * Copyright © 2007 Intel Corporation
 *   Jesse Barnes <jesse.barnes@intel.com>
 */
#ifndef __DRM_CRTC_H__
#define __DRM_CRTC_H__

#include <linux/i2c.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/idr.h>

#include <linux/fb.h>

struct drm_device;

/*
 * Note on terminology:  here, for brevity and convenience, we refer to output
 * control chips as 'CRTCs'.  They can control any type of output, VGA, LVDS,
 * DVI, etc.  And 'screen' refers to the whole of the visible display, which
 * may span multiple monitors (and therefore multiple CRTC and output
 * structures).
 */

enum drm_mode_status {
    MODE_OK	= 0,	/* Mode OK */
    MODE_HSYNC,		/* hsync out of range */
    MODE_VSYNC,		/* vsync out of range */
    MODE_H_ILLEGAL,	/* mode has illegal horizontal timings */
    MODE_V_ILLEGAL,	/* mode has illegal horizontal timings */
    MODE_BAD_WIDTH,	/* requires an unsupported linepitch */
    MODE_NOMODE,	/* no mode with a maching name */
    MODE_NO_INTERLACE,	/* interlaced mode not supported */
    MODE_NO_DBLESCAN,	/* doublescan mode not supported */
    MODE_NO_VSCAN,	/* multiscan mode not supported */
    MODE_MEM,		/* insufficient video memory */
    MODE_VIRTUAL_X,	/* mode width too large for specified virtual size */
    MODE_VIRTUAL_Y,	/* mode height too large for specified virtual size */
    MODE_MEM_VIRT,	/* insufficient video memory given virtual size */
    MODE_NOCLOCK,	/* no fixed clock available */
    MODE_CLOCK_HIGH,	/* clock required is too high */
    MODE_CLOCK_LOW,	/* clock required is too low */
    MODE_CLOCK_RANGE,	/* clock/mode isn't in a ClockRange */
    MODE_BAD_HVALUE,	/* horizontal timing was out of range */
    MODE_BAD_VVALUE,	/* vertical timing was out of range */
    MODE_BAD_VSCAN,	/* VScan value out of range */
    MODE_HSYNC_NARROW,	/* horizontal sync too narrow */
    MODE_HSYNC_WIDE,	/* horizontal sync too wide */
    MODE_HBLANK_NARROW,	/* horizontal blanking too narrow */
    MODE_HBLANK_WIDE,	/* horizontal blanking too wide */
    MODE_VSYNC_NARROW,	/* vertical sync too narrow */
    MODE_VSYNC_WIDE,	/* vertical sync too wide */
    MODE_VBLANK_NARROW,	/* vertical blanking too narrow */
    MODE_VBLANK_WIDE,	/* vertical blanking too wide */
    MODE_PANEL,         /* exceeds panel dimensions */
    MODE_INTERLACE_WIDTH, /* width too large for interlaced mode */
    MODE_ONE_WIDTH,     /* only one width is supported */
    MODE_ONE_HEIGHT,    /* only one height is supported */
    MODE_ONE_SIZE,      /* only one resolution is supported */
    MODE_NO_REDUCED,    /* monitor doesn't accept reduced blanking */
    MODE_UNVERIFIED = -3, /* mode needs to reverified */
    MODE_BAD = -2,	/* unspecified reason */
    MODE_ERROR	= -1	/* error condition */
};

#define DRM_MODE_TYPE_CLOCK_CRTC_C (DRM_MODE_TYPE_CLOCK_C | \
				    DRM_MODE_TYPE_CRTC_C)

#define DRM_MODE(nm, t, c, hd, hss, hse, ht, hsk, vd, vss, vse, vt, vs, f) \
	.name = nm, .status = 0, .type = (t), .clock = (c), \
	.hdisplay = (hd), .hsync_start = (hss), .hsync_end = (hse), \
	.htotal = (ht), .hskew = (hsk), .vdisplay = (vd), \
	.vsync_start = (vss), .vsync_end = (vse), .vtotal = (vt), \
	.vscan = (vs), .flags = (f), .vrefresh = 0

struct drm_display_mode {
	/* Header */
	struct list_head head;
	char name[DRM_DISPLAY_MODE_LEN];
	int mode_id;
	int output_count;
	enum drm_mode_status status;
	int type;

	/* Proposed mode values */
	int clock;
	int hdisplay;
	int hsync_start;
	int hsync_end;
	int htotal;
	int hskew;
	int vdisplay;
	int vsync_start;
	int vsync_end;
	int vtotal;
	int vscan;
	unsigned int flags;

	/* Actual mode we give to hw */
	int clock_index;
	int synth_clock;
	int crtc_hdisplay;
	int crtc_hblank_start;
	int crtc_hblank_end;
	int crtc_hsync_start;
	int crtc_hsync_end;
	int crtc_htotal;
	int crtc_hskew;
	int crtc_vdisplay;
	int crtc_vblank_start;
	int crtc_vblank_end;
	int crtc_vsync_start;
	int crtc_vsync_end;
	int crtc_vtotal;
	int crtc_hadjusted;
	int crtc_vadjusted;

	/* Driver private mode info */
	int private_size;
	int *private;
	int private_flags;

	int vrefresh;
	float hsync;
};

/* Video mode flags */
#define V_PHSYNC	(1<<0)
#define V_NHSYNC	(1<<1)
#define V_PVSYNC	(1<<2)
#define V_NVSYNC	(1<<3)
#define V_INTERLACE	(1<<4)
#define V_DBLSCAN	(1<<5)
#define V_CSYNC		(1<<6)
#define V_PCSYNC	(1<<7)
#define V_NCSYNC	(1<<8)
#define V_HSKEW		(1<<9) /* hskew provided */
#define V_BCAST		(1<<10)
#define V_PIXMUX	(1<<11)
#define V_DBLCLK	(1<<12)
#define V_CLKDIV2	(1<<13)

#define CRTC_INTERLACE_HALVE_V 0x1 /* halve V values for interlacing */

#define DPMSModeOn 0
#define DPMSModeStandby 1
#define DPMSModeSuspend 2
#define DPMSModeOff 3

#define ConnectorUnknown 0
#define ConnectorVGA 1
#define ConnectorDVII 2
#define ConnectorDVID 3
#define ConnectorDVIA 4
#define ConnectorComposite 5
#define ConnectorSVIDEO 6
#define ConnectorLVDS 7
#define ConnectorComponent 8
#define Connector9PinDIN 9
#define ConnectorDisplayPort 10
#define ConnectorHDMIA 11
#define ConnectorHDMIB 12

enum drm_output_status {
	output_status_connected = 1,
	output_status_disconnected = 2,
	output_status_unknown = 3,
};

enum subpixel_order {
	SubPixelUnknown = 0,
	SubPixelHorizontalRGB,
	SubPixelHorizontalBGR,
	SubPixelVerticalRGB,
	SubPixelVerticalBGR,
	SubPixelNone,
};

/*
 * Describes a given display (e.g. CRT or flat panel) and its limitations.
 */
struct drm_display_info {
	char name[DRM_DISPLAY_INFO_LEN];
	/* Input info */
	bool serration_vsync;
	bool sync_on_green;
	bool composite_sync;
	bool separate_syncs;
	bool blank_to_black;
	unsigned char video_level;
	bool digital;
	/* Physical size */
        unsigned int width_mm;
	unsigned int height_mm;

	/* Display parameters */
	unsigned char gamma; /* FIXME: storage format */
	bool gtf_supported;
	bool standard_color;
	enum {
		monochrome,
		rgb,
		other,
		unknown,
	} display_type;
	bool active_off_supported;
	bool suspend_supported;
	bool standby_supported;

	/* Color info FIXME: storage format */
	unsigned short redx, redy;
	unsigned short greenx, greeny;
	unsigned short bluex, bluey;
	unsigned short whitex, whitey;

	/* Clock limits FIXME: storage format */
	unsigned int min_vfreq, max_vfreq;
	unsigned int min_hfreq, max_hfreq;
	unsigned int pixel_clock;

	/* White point indices FIXME: storage format */
	unsigned int wpx1, wpy1;
	unsigned int wpgamma1;
	unsigned int wpx2, wpy2;
	unsigned int wpgamma2;

	/* Preferred mode (if any) */
	struct drm_display_mode *preferred_mode;
	char *raw_edid; /* if any */
};

struct drm_framebuffer {
	struct drm_device *dev;
	struct list_head head;
	int id; /* idr assigned */
	unsigned int pitch;
	unsigned long offset;
	unsigned int width;
	unsigned int height;
	/* depth can be 15 or 16 */
	unsigned int depth;
	int bits_per_pixel;
	int flags;
	struct drm_buffer_object *bo;
	void *fbdev;
	u32 pseudo_palette[17];
	void *virtual_base;
	struct list_head filp_head;
};

struct drm_property_blob {
	struct list_head head;
	unsigned int length;
	unsigned int id;
	void *data;
};

struct drm_property_enum {
	uint64_t value;
	struct list_head head;
	unsigned char name[DRM_PROP_NAME_LEN];
};

struct drm_property {
	struct list_head head;
	int id; /* idr assigned */
	uint32_t flags;
	char name[DRM_PROP_NAME_LEN];
	uint32_t num_values;
	uint64_t *values;

	struct list_head enum_blob_list;
};

struct drm_crtc;
struct drm_output;

/**
 * drm_crtc_funcs - control CRTCs for a given device
 * @dpms: control display power levels
 * @save: save CRTC state
 * @resore: restore CRTC state
 * @lock: lock the CRTC
 * @unlock: unlock the CRTC
 * @shadow_allocate: allocate shadow pixmap
 * @shadow_create: create shadow pixmap for rotation support
 * @shadow_destroy: free shadow pixmap
 * @mode_fixup: fixup proposed mode
 * @mode_set: set the desired mode on the CRTC
 * @gamma_set: specify color ramp for CRTC
 * @cleanup: cleanup driver private state prior to close
 *
 * The drm_crtc_funcs structure is the central CRTC management structure
 * in the DRM.  Each CRTC controls one or more outputs (note that the name
 * CRTC is simply historical, a CRTC may control LVDS, VGA, DVI, TV out, etc.
 * outputs, not just CRTs).
 *
 * Each driver is responsible for filling out this structure at startup time,
 * in addition to providing other modesetting features, like i2c and DDC
 * bus accessors.
 */
struct drm_crtc_funcs {
	/*
	 * Control power levels on the CRTC.  If the mode passed in is
	 * unsupported, the provider must use the next lowest power level.
	 */
	void (*dpms)(struct drm_crtc *crtc, int mode);

	/* JJJ:  Are these needed? */
	/* Save CRTC state */
	void (*save)(struct drm_crtc *crtc); /* suspend? */
	/* Restore CRTC state */
	void (*restore)(struct drm_crtc *crtc); /* resume? */
	bool (*lock)(struct drm_crtc *crtc);
	void (*unlock)(struct drm_crtc *crtc);

	void (*prepare)(struct drm_crtc *crtc);
	void (*commit)(struct drm_crtc *crtc);

	/* Provider can fixup or change mode timings before modeset occurs */
	bool (*mode_fixup)(struct drm_crtc *crtc,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	/* Actually set the mode */
	void (*mode_set)(struct drm_crtc *crtc, struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode, int x, int y);
	/* Set gamma on the CRTC */
	void (*gamma_set)(struct drm_crtc *crtc, u16 r, u16 g, u16 b,
			  int regno);
	/* Driver cleanup routine */
	void (*cleanup)(struct drm_crtc *crtc);
};

/**
 * drm_crtc - central CRTC control structure
 * @enabled: is this CRTC enabled?
 * @x: x position on screen
 * @y: y position on screen
 * @desired_mode: new desired mode
 * @desired_x: desired x for desired_mode
 * @desired_y: desired y for desired_mode
 * @funcs: CRTC control functions
 * @driver_private: arbitrary driver data
 *
 * Each CRTC may have one or more outputs associated with it.  This structure
 * allows the CRTC to be controlled.
 */
struct drm_crtc {
	struct drm_device *dev;
	struct list_head head;

	int id; /* idr assigned */

	/* framebuffer the output is currently bound to */
	struct drm_framebuffer *fb;

	bool enabled;

	/* JJJ: are these needed? */
	bool cursor_in_range;
	bool cursor_shown;

	struct drm_display_mode mode;

	int x, y;
	struct drm_display_mode *desired_mode;
	int desired_x, desired_y;
	const struct drm_crtc_funcs *funcs;
	void *driver_private;

	/* RRCrtcPtr randr_crtc? */
};

extern struct drm_crtc *drm_crtc_create(struct drm_device *dev,
					const struct drm_crtc_funcs *funcs);

/**
 * drm_output_funcs - control outputs on a given device
 * @init: setup this output
 * @dpms: set power state (see drm_crtc_funcs above)
 * @save: save output state
 * @restore: restore output state
 * @mode_valid: is this mode valid on the given output?
 * @mode_fixup: try to fixup proposed mode for this output
 * @mode_set: set this mode
 * @detect: is this output active?
 * @get_modes: get mode list for this output
 * @set_property: property for this output may need update
 * @cleanup: output is going away, cleanup
 *
 * Each CRTC may have one or more outputs attached to it.  The functions
 * below allow the core DRM code to control outputs, enumerate available modes,
 * etc.
 */
struct drm_output_funcs {
	void (*init)(struct drm_output *output);
	void (*dpms)(struct drm_output *output, int mode);
	void (*save)(struct drm_output *output);
	void (*restore)(struct drm_output *output);
	int (*mode_valid)(struct drm_output *output,
			  struct drm_display_mode *mode);
	bool (*mode_fixup)(struct drm_output *output,
			   struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	void (*prepare)(struct drm_output *output);
	void (*commit)(struct drm_output *output);
	void (*mode_set)(struct drm_output *output,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode);
	enum drm_output_status (*detect)(struct drm_output *output);
	int (*get_modes)(struct drm_output *output);
	/* JJJ: type checking for properties via property value type */
	bool (*set_property)(struct drm_output *output, struct drm_property *property,
			     uint64_t val);
	void (*cleanup)(struct drm_output *output);
};

#define DRM_OUTPUT_MAX_UMODES 16
#define DRM_OUTPUT_MAX_PROPERTY 16
#define DRM_OUTPUT_LEN 32
/**
 * drm_output - central DRM output control structure
 * @crtc: CRTC this output is currently connected to, NULL if none
 * @possible_crtcs: bitmap of CRTCS this output could be attached to
 * @possible_clones: bitmap of possible outputs this output could clone
 * @interlace_allowed: can this output handle interlaced modes?
 * @doublescan_allowed: can this output handle doublescan?
 * @available_modes: modes available on this output (from get_modes() + user)
 * @initial_x: initial x position for this output
 * @initial_y: initial y position for this output
 * @status: output connected?
 * @subpixel_order: for this output
 * @mm_width: displayable width of output in mm
 * @mm_height: displayable height of output in mm
 * @name: name of output (should be one of a few standard names)
 * @funcs: output control functions
 * @driver_private: private driver data
 *
 * Each output may be connected to one or more CRTCs, or may be clonable by
 * another output if they can share a CRTC.  Each output also has a specific
 * position in the broader display (referred to as a 'screen' though it could
 * span multiple monitors).
 */
struct drm_output {
	struct drm_device *dev;
	struct list_head head;
	struct drm_crtc *crtc;
	int id; /* idr assigned */
	unsigned long possible_crtcs;
	unsigned long possible_clones;
	bool interlace_allowed;
	bool doublescan_allowed;
	struct list_head modes; /* list of modes on this output */

	/*
	  OptionInfoPtr options;
	  XF86ConfMonitorPtr conf_monitor;
	 */
	int initial_x, initial_y;
	enum drm_output_status status;

	/* these are modes added by probing with DDC or the BIOS */
	struct list_head probed_modes;
	
	/* xf86MonPtr MonInfo; */
	enum subpixel_order subpixel_order;
	int mm_width, mm_height;
	struct drm_display_info *monitor_info; /* if any */
	char name[DRM_OUTPUT_LEN];
	const struct drm_output_funcs *funcs;
	void *driver_private;

	struct list_head user_modes;
	struct drm_property_blob *edid_blob_ptr;
	u32 property_ids[DRM_OUTPUT_MAX_PROPERTY];
	uint64_t property_values[DRM_OUTPUT_MAX_PROPERTY];
};

/**
 * struct drm_mode_config_funcs - configure CRTCs for a given screen layout
 * @resize: adjust CRTCs as necessary for the proposed layout
 *
 * Currently only a resize hook is available.  DRM will call back into the
 * driver with a new screen width and height.  If the driver can't support
 * the proposed size, it can return false.  Otherwise it should adjust
 * the CRTC<->output mappings as needed and update its view of the screen.
 */
struct drm_mode_config_funcs {
	bool (*resize)(struct drm_device *dev, int width, int height);
};

/**
 * drm_mode_config - Mode configuration control structure
 *
 */
struct drm_mode_config {
	struct mutex mutex; /* protects configuration and IDR */
	struct idr crtc_idr; /* use this idr for all IDs, fb, crtc, output, modes - just makes life easier */
	/* this is limited to one for now */
	int num_fb;
	struct list_head fb_list;
	int num_output;
	struct list_head output_list;

	/* int compat_output? */
	int num_crtc;
	struct list_head crtc_list;

	struct list_head property_list;

	int min_width, min_height;
	int max_width, max_height;
	/* DamagePtr rotationDamage? */
	/* DGA stuff? */
	struct drm_mode_config_funcs *funcs;
	unsigned long fb_base;

	/* pointers to standard properties */
	struct list_head property_blob_list;
	struct drm_property *edid_property;
	struct drm_property *dpms_property;
	struct drm_property *connector_type_property;
	struct drm_property *connector_num_property;
};

struct drm_output *drm_output_create(struct drm_device *dev,
				     const struct drm_output_funcs *funcs,
				     const char *name);
extern void drm_output_destroy(struct drm_output *output);
extern bool drm_output_rename(struct drm_output *output, const char *name);
extern void drm_fb_release(struct file *filp);

extern struct edid *drm_get_edid(struct drm_output *output,
				 struct i2c_adapter *adapter);
extern int drm_add_edid_modes(struct drm_output *output, struct edid *edid);
extern void drm_mode_probed_add(struct drm_output *output, struct drm_display_mode *mode);
extern void drm_mode_remove(struct drm_output *output, struct drm_display_mode *mode);
extern struct drm_display_mode *drm_mode_duplicate(struct drm_device *dev,
						   struct drm_display_mode *mode);
extern void drm_mode_debug_printmodeline(struct drm_device *dev,
					 struct drm_display_mode *mode);
extern void drm_mode_config_init(struct drm_device *dev);
extern void drm_mode_config_cleanup(struct drm_device *dev);
extern void drm_mode_set_name(struct drm_display_mode *mode);
extern bool drm_mode_equal(struct drm_display_mode *mode1, struct drm_display_mode *mode2);
extern void drm_disable_unused_functions(struct drm_device *dev);

/* for us by fb module */
extern int drm_mode_attachmode_crtc(struct drm_device *dev,
				    struct drm_crtc *crtc,
				    struct drm_display_mode *mode);
extern int drm_mode_detachmode_crtc(struct drm_device *dev, struct drm_display_mode *mode);

extern struct drm_display_mode *drm_mode_create(struct drm_device *dev);
extern void drm_mode_destroy(struct drm_device *dev, struct drm_display_mode *mode);
extern void drm_mode_list_concat(struct list_head *head,
				 struct list_head *new);
extern void drm_mode_validate_size(struct drm_device *dev,
				   struct list_head *mode_list,
				   int maxX, int maxY, int maxPitch);
extern void drm_mode_prune_invalid(struct drm_device *dev,
				   struct list_head *mode_list, bool verbose);
extern void drm_mode_sort(struct list_head *mode_list);
extern int drm_mode_vrefresh(struct drm_display_mode *mode);
extern void drm_mode_set_crtcinfo(struct drm_display_mode *p,
				  int adjust_flags);
extern void drm_mode_output_list_update(struct drm_output *output);
extern int drm_mode_output_update_edid_property(struct drm_output *output, unsigned char *edid);
extern struct drm_display_mode *drm_crtc_mode_create(struct drm_device *dev);
extern bool drm_initial_config(struct drm_device *dev, bool cangrow);
extern void drm_framebuffer_set_object(struct drm_device *dev,
				       unsigned long handle);
extern struct drm_framebuffer *drm_framebuffer_create(struct drm_device *dev);
extern void drm_framebuffer_destroy(struct drm_framebuffer *fb);
extern int drmfb_probe(struct drm_device *dev, struct drm_crtc *crtc);
extern int drmfb_remove(struct drm_device *dev, struct drm_framebuffer *fb);
extern bool drm_crtc_set_mode(struct drm_crtc *crtc, struct drm_display_mode *mode,
		       int x, int y);
extern int drm_hotplug_stage_two(struct drm_device *dev, struct drm_output *output);

extern int drm_output_attach_property(struct drm_output *output,
				      struct drm_property *property, uint64_t init_val);
extern struct drm_property *drm_property_create(struct drm_device *dev, int flags,
						const char *name, int num_values);
extern void drm_property_destroy(struct drm_device *dev, struct drm_property *property);
extern int drm_property_add_enum(struct drm_property *property, int index, 
				 uint64_t value, const char *name);

/* IOCTLs */
extern int drm_mode_getresources(struct drm_device *dev,
				 void *data, struct drm_file *file_priv);

extern int drm_mode_getcrtc(struct drm_device *dev,
			    void *data, struct drm_file *file_priv);
extern int drm_mode_getoutput(struct drm_device *dev,
			      void *data, struct drm_file *file_priv);
extern int drm_mode_setcrtc(struct drm_device *dev,
			    void *data, struct drm_file *file_priv);
extern int drm_mode_addfb(struct drm_device *dev,
			  void *data, struct drm_file *file_priv);
extern int drm_mode_rmfb(struct drm_device *dev,
			 void *data, struct drm_file *file_priv);
extern int drm_mode_getfb(struct drm_device *dev,
			  void *data, struct drm_file *file_priv);
extern int drm_mode_addmode_ioctl(struct drm_device *dev,
				  void *data, struct drm_file *file_priv);
extern int drm_mode_rmmode_ioctl(struct drm_device *dev,
				 void *data, struct drm_file *file_priv);
extern int drm_mode_attachmode_ioctl(struct drm_device *dev,
				     void *data, struct drm_file *file_priv);
extern int drm_mode_detachmode_ioctl(struct drm_device *dev,
				     void *data, struct drm_file *file_priv);

extern int drm_mode_getproperty_ioctl(struct drm_device *dev,
				      void *data, struct drm_file *file_priv);
extern int drm_mode_getblob_ioctl(struct drm_device *dev,
				  void *data, struct drm_file *file_priv);
extern int drm_mode_output_property_set_ioctl(struct drm_device *dev,
					      void *data, struct drm_file *file_priv);
#endif /* __DRM_CRTC_H__ */

