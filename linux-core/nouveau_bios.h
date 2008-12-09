/*
 * Copyright (C) 2005-2006 Erik Waling
 * Copyright (C) 2006 Stephane Marchesin
 * Copyright (C) 2007-2008 Stuart Bennett
 * Copyright (C) 2008 Maarten Maathuis.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef __NOUVEAU_BIOS_H__
#define __NOUVEAU_BIOS_H__

#include "drmP.h"
#include "drm.h"

#define LOC_ON_CHIP 0

enum dcb_output_type {/* matches DCB types */
	DCB_OUTPUT_NONE = 4,
	DCB_OUTPUT_ANALOG = 0,
	DCB_OUTPUT_TMDS = 2,
	DCB_OUTPUT_LVDS = 3,
	DCB_OUTPUT_TV = 1,
};

struct bios {
	uint8_t *data;
	unsigned int length;
	bool execute;

	uint8_t major_version, chip_version;
	uint8_t feature_byte;

	uint32_t fmaxvco, fminvco;

	uint32_t dactestval;

	uint16_t init_script_tbls_ptr;
	uint16_t extra_init_script_tbl_ptr;
	uint16_t macro_index_tbl_ptr;
	uint16_t macro_tbl_ptr;
	uint16_t condition_tbl_ptr;
	uint16_t io_condition_tbl_ptr;
	uint16_t io_flag_condition_tbl_ptr;
	uint16_t init_function_tbl_ptr;

	uint16_t pll_limit_tbl_ptr;
	uint16_t ram_restrict_tbl_ptr;

	struct {
		struct nouveau_hw_mode *native_mode;
		uint8_t *edid;
		uint16_t lvdsmanufacturerpointer;
		uint16_t xlated_entry;
		bool power_off_for_reset;
		bool reset_after_pclk_change;
		bool dual_link;
		bool link_c_increment;
		bool if_is_24bit;
		bool BITbit1;
		int duallink_transition_clk;
		/* lower nibble stores PEXTDEV_BOOT_0 strap
		 * upper nibble stores xlated display strap */
		uint8_t strapping;
	} fp;

	struct {
		uint16_t output0_script_ptr;
		uint16_t output1_script_ptr;
	} tmds;

	struct {
		uint16_t mem_init_tbl_ptr;
		uint16_t sdr_seq_tbl_ptr;
		uint16_t ddr_seq_tbl_ptr;

		struct {
			uint8_t crt, tv, panel;
		} i2c_indices;
	} legacy;
};

struct dcb_entry {
	int index;
	uint8_t type;
	uint8_t i2c_index;
	uint8_t heads;
	uint8_t bus;
	uint8_t location;
	uint8_t or;
	bool duallink_possible;
	union {
		struct {
			bool use_straps_for_mode;
			bool use_power_scripts;
		} lvdsconf;
	};
};

/* changing these requires matching changes to reg tables in nv_get_clock */
#define MAX_PLL_TYPES	4
enum pll_types {
	NVPLL,
	MPLL,
	VPLL1,
	VPLL2
};

struct pll_lims {
	struct {
		int minfreq;
		int maxfreq;
		int min_inputfreq;
		int max_inputfreq;

		uint8_t min_m;
		uint8_t max_m;
		uint8_t min_n;
		uint8_t max_n;
	} vco1, vco2;

	uint8_t unk1c;
	uint8_t max_log2p_bias;
	uint8_t log2p_bias;
	int refclk;
};

bool get_pll_limits(struct drm_device *dev, uint32_t limit_match, struct pll_lims *pll_lim);
int nouveau_parse_bios(struct drm_device *dev);

#endif /* __NOUVEAU_BIOS_H__ */
