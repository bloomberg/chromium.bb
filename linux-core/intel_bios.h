/*
 * Copyright © 2006 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *    Eric Anholt <eric@anholt.net>
 *
 */

#ifndef _I830_BIOS_H_
#define _I830_BIOS_H_

#include "drmP.h"

struct vbt_header {
	u8 signature[20];		/**< Always starts with 'VBT$' */
	u16 version;			/**< decimal */
	u16 header_size;		/**< in bytes */
	u16 vbt_size;			/**< in bytes */
	u8 vbt_checksum;
	u8 reserved0;
	u32 bdb_offset;			/**< from beginning of VBT */
	u32 aim_offset[4];		/**< from beginning of VBT */
} __attribute__((packed));

struct bdb_header {
	u8 signature[16];		/**< Always 'BIOS_DATA_BLOCK' */
	u16 version;			/**< decimal */
	u16 header_size;		/**< in bytes */
	u16 bdb_size;			/**< in bytes */
	u8 type; /* 0 == desktop, 1 == mobile */
	u8 relstage;
	u8 chipset;
	u8 lvds_present:1;
	u8 tv_present:1;
	u8 rsvd2:6; /* finish byte */
	u8 rsvd3[4];
	u8 signon[155];
	u8 copyright[61];
	u16 code_segment;
	u8 dos_boot_mode;
	u8 bandwidth_percent;
	u8 rsvd4;
	u8 resize_pci_bios;
	u8 rsvd5;
	u8 rsvd6[3];
	u8 panel_fitting:2;
	u8 flexaim:1;
	u8 msg_enable:1;
	u8 clear_screen:1;
	u8 color_flip:1;
	u8 rsvd7:2; /* finish byte */
	u8 download_ext_vbt:1;
	u8 enable_ssc:1;
	u8 ssc_freq:1;
	u8 enable_lfp_on_override:1;
	u8 disable_ssc_ddt:1;
	u8 rsvd8:3; /* finish byte */
	u8 disable_smooth_vision:1;
	u8 single_dvi:1;
	u8 rsvd9:6; /* finish byte */
	u8 legacy_monitor_detect:1;
	u8 rsvd10:7; /* finish byte */
	u8 int_crt_support:1;
	u8 int_tv_support:1;
	u8 rsvd11:6; /* finish byte */
} __attribute__((packed));

#define LVDS_CAP_EDID			(1 << 6)
#define LVDS_CAP_DITHER			(1 << 5)
#define LVDS_CAP_PFIT_AUTO_RATIO	(1 << 4)
#define LVDS_CAP_PFIT_GRAPHICS_MODE	(1 << 3)
#define LVDS_CAP_PFIT_TEXT_MODE		(1 << 2)
#define LVDS_CAP_PFIT_GRAPHICS		(1 << 1)
#define LVDS_CAP_PFIT_TEXT		(1 << 0)
struct lvds_bdb_1 {
	u8 id;				/**< 40 */
	u16 size;
	u8 panel_type;
	u8 reserved0;
	u16 caps;
} __attribute__((packed));

struct lvds_bdb_2_fp_params {
	u16 x_res;
	u16 y_res;
	u32 lvds_reg;
	u32 lvds_reg_val;
	u32 pp_on_reg;
	u32 pp_on_reg_val;
	u32 pp_off_reg;
	u32 pp_off_reg_val;
	u32 pp_cycle_reg;
	u32 pp_cycle_reg_val;
	u32 pfit_reg;
	u32 pfit_reg_val;
	u16 terminator;
} __attribute__((packed));

struct lvds_bdb_2_fp_edid_dtd {
	u16 dclk;		/**< In 10khz */
	u8 hactive;
	u8 hblank;
	u8 high_h;		/**< 7:4 = hactive 11:8, 3:0 = hblank 11:8 */
	u8 vactive;
	u8 vblank;
	u8 high_v;		/**< 7:4 = vactive 11:8, 3:0 = vblank 11:8 */
	u8 hsync_off;
	u8 hsync_pulse_width;
	u8 vsync_off;
	u8 high_hsync_off;	/**< 7:6 = hsync off 9:8 */
	u8 h_image;
	u8 v_image;
	u8 max_hv;
	u8 h_border;
	u8 v_border;
	u8 flags;
#define FP_EDID_FLAG_VSYNC_POSITIVE	(1 << 2)
#define FP_EDID_FLAG_HSYNC_POSITIVE	(1 << 1)
} __attribute__((packed));

struct lvds_bdb_2_entry {
	u16 fp_params_offset;		/**< From beginning of BDB */
	u8 fp_params_size;
	u16 fp_edid_dtd_offset;
	u8 fp_edid_dtd_size;
	u16 fp_edid_pid_offset;
	u8 fp_edid_pid_size;
} __attribute__((packed));

struct lvds_bdb_2 {
	u8 id;			/**< 41 */
	u16 size;
	u8 table_size;	/* not sure on this one */
	struct lvds_bdb_2_entry panels[16];
} __attribute__((packed));

struct aimdb_header {
	char signature[16];
	char oem_device[20];
	u16 aimdb_version;
	u16 aimdb_header_size;
	u16 aimdb_size;
} __attribute__((packed));

struct aimdb_block {
	u8 aimdb_id;
	u16 aimdb_size;
} __attribute__((packed));

struct vch_panel_data {
	u16 fp_timing_offset;
	u8 fp_timing_size;
	u16 dvo_timing_offset;
	u8 dvo_timing_size;
	u16 text_fitting_offset;
	u8 text_fitting_size;
	u16 graphics_fitting_offset;
	u8 graphics_fitting_size;
} __attribute__((packed));

struct vch_bdb_22 {
	struct aimdb_block aimdb_block;
	struct vch_panel_data panels[16];
} __attribute__((packed));

bool intel_find_bios(struct drm_device *dev);

#endif /* _I830_BIOS_H_ */
