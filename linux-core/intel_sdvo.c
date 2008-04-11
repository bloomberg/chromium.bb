/*
 * Copyright © 2006-2007 Intel Corporation
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *	Eric Anholt <eric@anholt.net>
 */
/*
 * Copyright 2006 Dave Airlie <airlied@linux.ie>
 *   Jesse Barnes <jesse.barnes@intel.com>
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include "drmP.h"
#include "drm.h"
#include "drm_crtc.h"
#include "intel_drv.h"
#include "i915_drm.h"
#include "i915_drv.h"
#include "intel_sdvo_regs.h"

struct intel_sdvo_priv {
	struct intel_i2c_chan *i2c_bus;
	int slaveaddr;
	int output_device;

	u16 active_outputs;

	struct intel_sdvo_caps caps;
	int pixel_clock_min, pixel_clock_max;

	int save_sdvo_mult;
	u16 save_active_outputs;
	struct intel_sdvo_dtd save_input_dtd_1, save_input_dtd_2;
	struct intel_sdvo_dtd save_output_dtd[16];
	u32 save_SDVOX;
};

/**
 * Writes the SDVOB or SDVOC with the given value, but always writes both
 * SDVOB and SDVOC to work around apparent hardware issues (according to
 * comments in the BIOS).
 */
void intel_sdvo_write_sdvox(struct drm_output *output, u32 val)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv   *sdvo_priv = intel_output->dev_priv;
	u32 bval = val, cval = val;
	int i;

	if (sdvo_priv->output_device == SDVOB) {
		cval = I915_READ(SDVOC);
	} else {
		bval = I915_READ(SDVOB);
	}
	/*
	 * Write the registers twice for luck. Sometimes,
	 * writing them only once doesn't appear to 'stick'.
	 * The BIOS does this too. Yay, magic
	 */
	for (i = 0; i < 2; i++)
	{
		I915_WRITE(SDVOB, bval);
		I915_READ(SDVOB);
		I915_WRITE(SDVOC, cval);
		I915_READ(SDVOC);
	}
}

static bool intel_sdvo_read_byte(struct drm_output *output, u8 addr,
				 u8 *ch)
{
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	u8 out_buf[2];
	u8 buf[2];
	int ret;

	struct i2c_msg msgs[] = {
		{ 
			.addr = sdvo_priv->i2c_bus->slave_addr,
			.flags = 0,
			.len = 1,
			.buf = out_buf,
		}, 
		{
			.addr = sdvo_priv->i2c_bus->slave_addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = buf,
		}
	};

	out_buf[0] = addr;
	out_buf[1] = 0;

	if ((ret = i2c_transfer(&sdvo_priv->i2c_bus->adapter, msgs, 2)) == 2)
	{
//		DRM_DEBUG("got back from addr %02X = %02x\n", out_buf[0], buf[0]); 
		*ch = buf[0];
		return true;
	}

	DRM_DEBUG("i2c transfer returned %d\n", ret);
	return false;
}

static bool intel_sdvo_write_byte(struct drm_output *output, int addr,
				  u8 ch)
{
	struct intel_output *intel_output = output->driver_private;
	u8 out_buf[2];
	struct i2c_msg msgs[] = {
		{ 
			.addr = intel_output->i2c_bus->slave_addr,
			.flags = 0,
			.len = 2,
			.buf = out_buf,
		}
	};

	out_buf[0] = addr;
	out_buf[1] = ch;

	if (i2c_transfer(&intel_output->i2c_bus->adapter, msgs, 1) == 1)
	{
		return true;
	}
	return false;
}

#define SDVO_CMD_NAME_ENTRY(cmd) {cmd, #cmd}
/** Mapping of command numbers to names, for debug output */
const static struct _sdvo_cmd_name {
    u8 cmd;
    char *name;
} sdvo_cmd_names[] = {
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_RESET),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_DEVICE_CAPS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_FIRMWARE_REV),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TRAINED_INPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ACTIVE_OUTPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ACTIVE_OUTPUTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_IN_OUT_MAP),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_IN_OUT_MAP),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ATTACHED_DISPLAYS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_HOT_PLUG_SUPPORT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_ACTIVE_HOT_PLUG),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_ACTIVE_HOT_PLUG),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INTERRUPT_EVENT_SOURCE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TARGET_INPUT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TARGET_OUTPUT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_INPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OUTPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_OUTPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_TIMINGS_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_TIMINGS_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_CREATE_PREFERRED_INPUT_TIMING),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_OUTPUT_PIXEL_CLOCK_RANGE),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_CLOCK_RATE_MULTS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_CLOCK_RATE_MULT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_CLOCK_RATE_MULT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_SUPPORTED_TV_FORMATS),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_GET_TV_FORMAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TV_FORMAT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_TV_RESOLUTION_SUPPORT),
    SDVO_CMD_NAME_ENTRY(SDVO_CMD_SET_CONTROL_BUS_SWITCH),
};

#define SDVO_NAME(dev_priv) ((dev_priv)->output_device == SDVOB ? "SDVOB" : "SDVOC")
#define SDVO_PRIV(output)   ((struct intel_sdvo_priv *) (output)->dev_priv)

static void intel_sdvo_write_cmd(struct drm_output *output, u8 cmd,
				 void *args, int args_len)
{
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	int i;

        if (1) {
                DRM_DEBUG("%s: W: %02X ", SDVO_NAME(sdvo_priv), cmd);
                for (i = 0; i < args_len; i++)
                        printk("%02X ", ((u8 *)args)[i]);
                for (; i < 8; i++)
                        printk("   ");
                for (i = 0; i < sizeof(sdvo_cmd_names) / sizeof(sdvo_cmd_names[0]); i++) {
                        if (cmd == sdvo_cmd_names[i].cmd) {
                                printk("(%s)", sdvo_cmd_names[i].name);
                                break;
                        }
                }
                if (i == sizeof(sdvo_cmd_names)/ sizeof(sdvo_cmd_names[0]))
                        printk("(%02X)",cmd);
                printk("\n");
        }
                        
	for (i = 0; i < args_len; i++) {
		intel_sdvo_write_byte(output, SDVO_I2C_ARG_0 - i, ((u8*)args)[i]);
	}

	intel_sdvo_write_byte(output, SDVO_I2C_OPCODE, cmd);
}

static const char *cmd_status_names[] = {
	"Power on",
	"Success",
	"Not supported",
	"Invalid arg",
	"Pending",
	"Target not specified",
	"Scaling not supported"
};

static u8 intel_sdvo_read_response(struct drm_output *output, void *response,
				   int response_len)
{
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	int i;
	u8 status;
	u8 retry = 50;

	while (retry--) {
		/* Read the command response */
		for (i = 0; i < response_len; i++) {
			intel_sdvo_read_byte(output, SDVO_I2C_RETURN_0 + i,
				     &((u8 *)response)[i]);
		}

		/* read the return status */
		intel_sdvo_read_byte(output, SDVO_I2C_CMD_STATUS, &status);

	        if (1) {
			DRM_DEBUG("%s: R: ", SDVO_NAME(sdvo_priv));
       			for (i = 0; i < response_len; i++)
                        	printk("%02X ", ((u8 *)response)[i]);
                	for (; i < 8; i++)
                        	printk("   ");
                	if (status <= SDVO_CMD_STATUS_SCALING_NOT_SUPP)
                        	printk("(%s)", cmd_status_names[status]);
                	else
                        	printk("(??? %d)", status);
                	printk("\n");
        	}

		if (status != SDVO_CMD_STATUS_PENDING)
			return status;

		mdelay(50);
	}

	return status;
}

int intel_sdvo_get_pixel_multiplier(struct drm_display_mode *mode)
{
	if (mode->clock >= 100000)
		return 1;
	else if (mode->clock >= 50000)
		return 2;
	else
		return 4;
}

/**
 * Don't check status code from this as it switches the bus back to the
 * SDVO chips which defeats the purpose of doing a bus switch in the first
 * place.
 */
void intel_sdvo_set_control_bus_switch(struct drm_output *output, u8 target)
{
	intel_sdvo_write_cmd(output, SDVO_CMD_SET_CONTROL_BUS_SWITCH, &target, 1);
}

static bool intel_sdvo_set_target_input(struct drm_output *output, bool target_0, bool target_1)
{
	struct intel_sdvo_set_target_input_args targets = {0};
	u8 status;

	if (target_0 && target_1)
		return SDVO_CMD_STATUS_NOTSUPP;

	if (target_1)
		targets.target_1 = 1;

	intel_sdvo_write_cmd(output, SDVO_CMD_SET_TARGET_INPUT, &targets,
			     sizeof(targets));

	status = intel_sdvo_read_response(output, NULL, 0);

	return (status == SDVO_CMD_STATUS_SUCCESS);
}

/**
 * Return whether each input is trained.
 *
 * This function is making an assumption about the layout of the response,
 * which should be checked against the docs.
 */
static bool intel_sdvo_get_trained_inputs(struct drm_output *output, bool *input_1, bool *input_2)
{
	struct intel_sdvo_get_trained_inputs_response response;
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_TRAINED_INPUTS, NULL, 0);
	status = intel_sdvo_read_response(output, &response, sizeof(response));
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	*input_1 = response.input0_trained;
	*input_2 = response.input1_trained;
	return true;
}

static bool intel_sdvo_get_active_outputs(struct drm_output *output,
					  u16 *outputs)
{
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_ACTIVE_OUTPUTS, NULL, 0);
	status = intel_sdvo_read_response(output, outputs, sizeof(*outputs));

	return (status == SDVO_CMD_STATUS_SUCCESS);
}

static bool intel_sdvo_set_active_outputs(struct drm_output *output,
					  u16 outputs)
{
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_SET_ACTIVE_OUTPUTS, &outputs,
			     sizeof(outputs));
	status = intel_sdvo_read_response(output, NULL, 0);
	return (status == SDVO_CMD_STATUS_SUCCESS);
}

static bool intel_sdvo_set_encoder_power_state(struct drm_output *output,
					       int mode)
{
	u8 status, state = SDVO_ENCODER_STATE_ON;

	switch (mode) {
	case DPMSModeOn:
		state = SDVO_ENCODER_STATE_ON;
		break;
	case DPMSModeStandby:
		state = SDVO_ENCODER_STATE_STANDBY;
		break;
	case DPMSModeSuspend:
		state = SDVO_ENCODER_STATE_SUSPEND;
		break;
	case DPMSModeOff:
		state = SDVO_ENCODER_STATE_OFF;
		break;
	}
	
	intel_sdvo_write_cmd(output, SDVO_CMD_SET_ENCODER_POWER_STATE, &state,
			     sizeof(state));
	status = intel_sdvo_read_response(output, NULL, 0);

	return (status == SDVO_CMD_STATUS_SUCCESS);
}

static bool intel_sdvo_get_input_pixel_clock_range(struct drm_output *output,
						   int *clock_min,
						   int *clock_max)
{
	struct intel_sdvo_pixel_clock_range clocks;
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_INPUT_PIXEL_CLOCK_RANGE,
			     NULL, 0);

	status = intel_sdvo_read_response(output, &clocks, sizeof(clocks));

	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	/* Convert the values from units of 10 kHz to kHz. */
	*clock_min = clocks.min * 10;
	*clock_max = clocks.max * 10;

	return true;
}

static bool intel_sdvo_set_target_output(struct drm_output *output,
					 u16 outputs)
{
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_SET_TARGET_OUTPUT, &outputs,
			     sizeof(outputs));

	status = intel_sdvo_read_response(output, NULL, 0);
	return (status == SDVO_CMD_STATUS_SUCCESS);
}

static bool intel_sdvo_get_timing(struct drm_output *output, u8 cmd,
				  struct intel_sdvo_dtd *dtd)
{
	u8 status;

	intel_sdvo_write_cmd(output, cmd, NULL, 0);
	status = intel_sdvo_read_response(output, &dtd->part1,
					  sizeof(dtd->part1));
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	intel_sdvo_write_cmd(output, cmd + 1, NULL, 0);
	status = intel_sdvo_read_response(output, &dtd->part2,
					  sizeof(dtd->part2));
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	return true;
}

static bool intel_sdvo_get_input_timing(struct drm_output *output,
					 struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_get_timing(output,
				     SDVO_CMD_GET_INPUT_TIMINGS_PART1, dtd);
}

static bool intel_sdvo_get_output_timing(struct drm_output *output,
					 struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_get_timing(output,
				     SDVO_CMD_GET_OUTPUT_TIMINGS_PART1, dtd);
}

static bool intel_sdvo_set_timing(struct drm_output *output, u8 cmd,
				  struct intel_sdvo_dtd *dtd)
{
	u8 status;

	intel_sdvo_write_cmd(output, cmd, &dtd->part1, sizeof(dtd->part1));
	status = intel_sdvo_read_response(output, NULL, 0);
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	intel_sdvo_write_cmd(output, cmd + 1, &dtd->part2, sizeof(dtd->part2));
	status = intel_sdvo_read_response(output, NULL, 0);
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	return true;
}

static bool intel_sdvo_set_input_timing(struct drm_output *output,
					 struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_set_timing(output,
				     SDVO_CMD_SET_INPUT_TIMINGS_PART1, dtd);
}

static bool intel_sdvo_set_output_timing(struct drm_output *output,
					 struct intel_sdvo_dtd *dtd)
{
	return intel_sdvo_set_timing(output,
				     SDVO_CMD_SET_OUTPUT_TIMINGS_PART1, dtd);
}

#if 0
static bool intel_sdvo_get_preferred_input_timing(struct drm_output *output,
						  struct intel_sdvo_dtd *dtd)
{
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART1,
			     NULL, 0);

	status = intel_sdvo_read_response(output, &dtd->part1,
					  sizeof(dtd->part1));
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_PREFERRED_INPUT_TIMING_PART2,
			     NULL, 0);
	status = intel_sdvo_read_response(output, &dtd->part2,
					  sizeof(dtd->part2));
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	return true;
}
#endif

static int intel_sdvo_get_clock_rate_mult(struct drm_output *output)
{
	u8 response, status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_CLOCK_RATE_MULT, NULL, 0);
	status = intel_sdvo_read_response(output, &response, 1);

	if (status != SDVO_CMD_STATUS_SUCCESS) {
		DRM_DEBUG("Couldn't get SDVO clock rate multiplier\n");
		return SDVO_CLOCK_RATE_MULT_1X;
	} else {
		DRM_DEBUG("Current clock rate multiplier: %d\n", response);
	}

	return response;
}

static bool intel_sdvo_set_clock_rate_mult(struct drm_output *output, u8 val)
{
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_SET_CLOCK_RATE_MULT, &val, 1);
	status = intel_sdvo_read_response(output, NULL, 0);
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	return true;
}

static bool intel_sdvo_mode_fixup(struct drm_output *output,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	/* Make the CRTC code factor in the SDVO pixel multiplier.  The SDVO
	 * device will be told of the multiplier during mode_set.
	 */
	adjusted_mode->clock *= intel_sdvo_get_pixel_multiplier(mode);
	return true;
}

static void intel_sdvo_mode_set(struct drm_output *output,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct drm_crtc *crtc = output->crtc;
	struct intel_crtc *intel_crtc = crtc->driver_private;
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	u16 width, height;
	u16 h_blank_len, h_sync_len, v_blank_len, v_sync_len;
	u16 h_sync_offset, v_sync_offset;
	u32 sdvox;
	struct intel_sdvo_dtd output_dtd;
	int sdvo_pixel_multiply;

	if (!mode)
		return;

	width = mode->crtc_hdisplay;
	height = mode->crtc_vdisplay;

	/* do some mode translations */
	h_blank_len = mode->crtc_hblank_end - mode->crtc_hblank_start;
	h_sync_len = mode->crtc_hsync_end - mode->crtc_hsync_start;

	v_blank_len = mode->crtc_vblank_end - mode->crtc_vblank_start;
	v_sync_len = mode->crtc_vsync_end - mode->crtc_vsync_start;

	h_sync_offset = mode->crtc_hsync_start - mode->crtc_hblank_start;
	v_sync_offset = mode->crtc_vsync_start - mode->crtc_vblank_start;

	output_dtd.part1.clock = mode->clock / 10;
	output_dtd.part1.h_active = width & 0xff;
	output_dtd.part1.h_blank = h_blank_len & 0xff;
	output_dtd.part1.h_high = (((width >> 8) & 0xf) << 4) |
		((h_blank_len >> 8) & 0xf);
	output_dtd.part1.v_active = height & 0xff;
	output_dtd.part1.v_blank = v_blank_len & 0xff;
	output_dtd.part1.v_high = (((height >> 8) & 0xf) << 4) |
		((v_blank_len >> 8) & 0xf);
	
	output_dtd.part2.h_sync_off = h_sync_offset;
	output_dtd.part2.h_sync_width = h_sync_len & 0xff;
	output_dtd.part2.v_sync_off_width = (v_sync_offset & 0xf) << 4 |
		(v_sync_len & 0xf);
	output_dtd.part2.sync_off_width_high = ((h_sync_offset & 0x300) >> 2) |
		((h_sync_len & 0x300) >> 4) | ((v_sync_offset & 0x30) >> 2) |
		((v_sync_len & 0x30) >> 4);
	
	output_dtd.part2.dtd_flags = 0x18;
	if (mode->flags & V_PHSYNC)
		output_dtd.part2.dtd_flags |= 0x2;
	if (mode->flags & V_PVSYNC)
		output_dtd.part2.dtd_flags |= 0x4;

	output_dtd.part2.sdvo_flags = 0;
	output_dtd.part2.v_sync_off_high = v_sync_offset & 0xc0;
	output_dtd.part2.reserved = 0;

	/* Set the output timing to the screen */
	intel_sdvo_set_target_output(output, sdvo_priv->active_outputs);
	intel_sdvo_set_output_timing(output, &output_dtd);

	/* Set the input timing to the screen. Assume always input 0. */
	intel_sdvo_set_target_input(output, true, false);

	/* We would like to use i830_sdvo_create_preferred_input_timing() to
	 * provide the device with a timing it can support, if it supports that
	 * feature.  However, presumably we would need to adjust the CRTC to
	 * output the preferred timing, and we don't support that currently.
	 */
#if 0
	success = intel_sdvo_create_preferred_input_timing(output, clock,
							   width, height);
	if (success) {
		struct intel_sdvo_dtd *input_dtd;
		
		intel_sdvo_get_preferred_input_timing(output, &input_dtd);
		intel_sdvo_set_input_timing(output, &input_dtd);
	}
#else
	intel_sdvo_set_input_timing(output, &output_dtd);
#endif	

	switch (intel_sdvo_get_pixel_multiplier(mode)) {
	case 1:
		intel_sdvo_set_clock_rate_mult(output,
					       SDVO_CLOCK_RATE_MULT_1X);
		break;
	case 2:
		intel_sdvo_set_clock_rate_mult(output,
					       SDVO_CLOCK_RATE_MULT_2X);
		break;
	case 4:
		intel_sdvo_set_clock_rate_mult(output,
					       SDVO_CLOCK_RATE_MULT_4X);
		break;
	}	

	/* Set the SDVO control regs. */
        if (0/*IS_I965GM(dev)*/) {
                sdvox = SDVO_BORDER_ENABLE;
        } else {
                sdvox = I915_READ(sdvo_priv->output_device);
                switch (sdvo_priv->output_device) {
                case SDVOB:
                        sdvox &= SDVOB_PRESERVE_MASK;
                        break;
                case SDVOC:
                        sdvox &= SDVOC_PRESERVE_MASK;
                        break;
                }
                sdvox |= (9 << 19) | SDVO_BORDER_ENABLE;
        }
	if (intel_crtc->pipe == 1)
		sdvox |= SDVO_PIPE_B_SELECT;

	sdvo_pixel_multiply = intel_sdvo_get_pixel_multiplier(mode);
	if (IS_I965G(dev)) {
		/* done in crtc_mode_set as the dpll_md reg must be written 
		   early */
	} else if (IS_I945G(dev) || IS_I945GM(dev)) {
		/* done in crtc_mode_set as it lives inside the 
		   dpll register */
	} else {
		sdvox |= (sdvo_pixel_multiply - 1) << SDVO_PORT_MULTIPLY_SHIFT;
	}

	intel_sdvo_write_sdvox(output, sdvox);
}

static void intel_sdvo_dpms(struct drm_output *output, int mode)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	u32 temp;

	if (mode != DPMSModeOn) {
		intel_sdvo_set_active_outputs(output, 0);
		if (0)
			intel_sdvo_set_encoder_power_state(output, mode);

		if (mode == DPMSModeOff) {
			temp = I915_READ(sdvo_priv->output_device);
			if ((temp & SDVO_ENABLE) != 0) {
				intel_sdvo_write_sdvox(output, temp & ~SDVO_ENABLE);
			}
		}
	} else {
		bool input1, input2;
		int i;
		u8 status;
		
		temp = I915_READ(sdvo_priv->output_device);
		if ((temp & SDVO_ENABLE) == 0)
			intel_sdvo_write_sdvox(output, temp | SDVO_ENABLE);
		for (i = 0; i < 2; i++)
		  intel_wait_for_vblank(dev);
		
		status = intel_sdvo_get_trained_inputs(output, &input1,
						       &input2);

		
		/* Warn if the device reported failure to sync. 
		 * A lot of SDVO devices fail to notify of sync, but it's
		 * a given it the status is a success, we succeeded.
		 */
		if (status == SDVO_CMD_STATUS_SUCCESS && !input1) {
			DRM_DEBUG("First %s output reported failure to sync\n",
				   SDVO_NAME(sdvo_priv));
		}
		
		if (0)
			intel_sdvo_set_encoder_power_state(output, mode);
		intel_sdvo_set_active_outputs(output, sdvo_priv->active_outputs);
	}	
	return;
}

static void intel_sdvo_save(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	int o;

	sdvo_priv->save_sdvo_mult = intel_sdvo_get_clock_rate_mult(output);
	intel_sdvo_get_active_outputs(output, &sdvo_priv->save_active_outputs);

	if (sdvo_priv->caps.sdvo_inputs_mask & 0x1) {
		intel_sdvo_set_target_input(output, true, false);
		intel_sdvo_get_input_timing(output,
					    &sdvo_priv->save_input_dtd_1);
	}

	if (sdvo_priv->caps.sdvo_inputs_mask & 0x2) {
		intel_sdvo_set_target_input(output, false, true);
		intel_sdvo_get_input_timing(output,
					    &sdvo_priv->save_input_dtd_2);
	}

	for (o = SDVO_OUTPUT_FIRST; o <= SDVO_OUTPUT_LAST; o++)
	{
	        u16  this_output = (1 << o);
		if (sdvo_priv->caps.output_flags & this_output)
		{
			intel_sdvo_set_target_output(output, this_output);
			intel_sdvo_get_output_timing(output,
						     &sdvo_priv->save_output_dtd[o]);
		}
	}

	sdvo_priv->save_SDVOX = I915_READ(sdvo_priv->output_device);
}

static void intel_sdvo_restore(struct drm_output *output)
{
	struct drm_device *dev = output->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;
	int o;
	int i;
	bool input1, input2;
	u8 status;

	intel_sdvo_set_active_outputs(output, 0);

	for (o = SDVO_OUTPUT_FIRST; o <= SDVO_OUTPUT_LAST; o++)
	{
		u16  this_output = (1 << o);
		if (sdvo_priv->caps.output_flags & this_output) {
			intel_sdvo_set_target_output(output, this_output);
			intel_sdvo_set_output_timing(output, &sdvo_priv->save_output_dtd[o]);
		}
	}

	if (sdvo_priv->caps.sdvo_inputs_mask & 0x1) {
		intel_sdvo_set_target_input(output, true, false);
		intel_sdvo_set_input_timing(output, &sdvo_priv->save_input_dtd_1);
	}

	if (sdvo_priv->caps.sdvo_inputs_mask & 0x2) {
		intel_sdvo_set_target_input(output, false, true);
		intel_sdvo_set_input_timing(output, &sdvo_priv->save_input_dtd_2);
	}
	
	intel_sdvo_set_clock_rate_mult(output, sdvo_priv->save_sdvo_mult);
	
	I915_WRITE(sdvo_priv->output_device, sdvo_priv->save_SDVOX);
	
	if (sdvo_priv->save_SDVOX & SDVO_ENABLE)
	{
		for (i = 0; i < 2; i++)
			intel_wait_for_vblank(dev);
		status = intel_sdvo_get_trained_inputs(output, &input1, &input2);
		if (status == SDVO_CMD_STATUS_SUCCESS && !input1)
			DRM_DEBUG("First %s output reported failure to sync\n",
				   SDVO_NAME(sdvo_priv));
	}
	
	intel_sdvo_set_active_outputs(output, sdvo_priv->save_active_outputs);
}

static int intel_sdvo_mode_valid(struct drm_output *output,
				 struct drm_display_mode *mode)
{
	struct intel_output *intel_output = output->driver_private;
	struct intel_sdvo_priv *sdvo_priv = intel_output->dev_priv;

	if (mode->flags & V_DBLSCAN)
		return MODE_NO_DBLESCAN;

	if (sdvo_priv->pixel_clock_min > mode->clock)
		return MODE_CLOCK_LOW;

	if (sdvo_priv->pixel_clock_max < mode->clock)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

static bool intel_sdvo_get_capabilities(struct drm_output *output, struct intel_sdvo_caps *caps)
{
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_DEVICE_CAPS, NULL, 0);
	status = intel_sdvo_read_response(output, caps, sizeof(*caps));
	if (status != SDVO_CMD_STATUS_SUCCESS)
		return false;

	return true;
}

struct drm_output* intel_sdvo_find(struct drm_device *dev, int sdvoB)
{
	struct drm_output *output = 0;
	struct intel_output *iout = 0;
	struct intel_sdvo_priv *sdvo;

	/* find the sdvo output */
	list_for_each_entry(output, &dev->mode_config.output_list, head) {
		iout = output->driver_private;

		if (iout->type != INTEL_OUTPUT_SDVO)
			continue;

		sdvo = iout->dev_priv;

		if (sdvo->output_device == SDVOB && sdvoB)
			return output;

		if (sdvo->output_device == SDVOC && !sdvoB)
			return output;

    }

	return 0;
}

int intel_sdvo_supports_hotplug(struct drm_output *output)
{
	u8 response[2];
	u8 status;
	DRM_DEBUG("\n");

	if (!output)
		return 0;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_HOT_PLUG_SUPPORT, NULL, 0);
	status = intel_sdvo_read_response(output, &response, 2);

	if (response[0] !=0)
		return 1;

	return 0;
}

void intel_sdvo_set_hotplug(struct drm_output *output, int on)
{
	u8 response[2];
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_ACTIVE_HOT_PLUG, NULL, 0);
	intel_sdvo_read_response(output, &response, 2);

	if (on) {
		intel_sdvo_write_cmd(output, SDVO_CMD_GET_HOT_PLUG_SUPPORT, NULL, 0);
		status = intel_sdvo_read_response(output, &response, 2);

		intel_sdvo_write_cmd(output, SDVO_CMD_SET_ACTIVE_HOT_PLUG, &response, 2);
	} else {
		response[0] = 0;
		response[1] = 0;
		intel_sdvo_write_cmd(output, SDVO_CMD_SET_ACTIVE_HOT_PLUG, &response, 2);
	}

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_ACTIVE_HOT_PLUG, NULL, 0);
	intel_sdvo_read_response(output, &response, 2);
}

static enum drm_output_status intel_sdvo_detect(struct drm_output *output)
{
	u8 response[2];
	u8 status;

	intel_sdvo_write_cmd(output, SDVO_CMD_GET_ATTACHED_DISPLAYS, NULL, 0);
	status = intel_sdvo_read_response(output, &response, 2);

	DRM_DEBUG("SDVO response %d %d\n", response[0], response[1]);
	if ((response[0] != 0) || (response[1] != 0))
		return output_status_connected;
	else
		return output_status_disconnected;
}

static int intel_sdvo_get_modes(struct drm_output *output)
{
	/* set the bus switch and get the modes */
	intel_sdvo_set_control_bus_switch(output, SDVO_CONTROL_BUS_DDC2);
	intel_ddc_get_modes(output);

	if (list_empty(&output->probed_modes))
		return 0;
	return 1;
#if 0
	/* Mac mini hack.  On this device, I get DDC through the analog, which
	 * load-detects as disconnected.  I fail to DDC through the SDVO DDC,
	 * but it does load-detect as connected.  So, just steal the DDC bits 
	 * from analog when we fail at finding it the right way.
	 */
	/* TODO */
	return NULL;

	return NULL;
#endif
}

static void intel_sdvo_destroy(struct drm_output *output)
{
	struct intel_output *intel_output = output->driver_private;

	if (intel_output->i2c_bus)
		intel_i2c_destroy(intel_output->i2c_bus);

	if (intel_output) {
		kfree(intel_output);
		output->driver_private = NULL;
	}
}

static const struct drm_output_funcs intel_sdvo_output_funcs = {
	.dpms = intel_sdvo_dpms,
	.save = intel_sdvo_save,
	.restore = intel_sdvo_restore,
	.mode_valid = intel_sdvo_mode_valid,
	.mode_fixup = intel_sdvo_mode_fixup,
	.prepare = intel_output_prepare,
	.mode_set = intel_sdvo_mode_set,
	.commit = intel_output_commit,
	.detect = intel_sdvo_detect,
	.get_modes = intel_sdvo_get_modes,
	.cleanup = intel_sdvo_destroy
};

void intel_sdvo_init(struct drm_device *dev, int output_device)
{
	struct drm_output *output;
	struct intel_output *intel_output;
	struct intel_sdvo_priv *sdvo_priv;
	struct intel_i2c_chan *i2cbus = NULL;
	int connector_type;
	u8 ch[0x40];
	int i;
	int output_type, output_id;

	output = drm_output_create(dev, &intel_sdvo_output_funcs,
				   DRM_MODE_OUTPUT_NONE);
	if (!output)
		return;

	intel_output = kcalloc(sizeof(struct intel_output)+sizeof(struct intel_sdvo_priv), 1, GFP_KERNEL);
	if (!intel_output) {
		drm_output_destroy(output);
		return;
	}

	sdvo_priv = (struct intel_sdvo_priv *)(intel_output + 1);
	intel_output->type = INTEL_OUTPUT_SDVO;
	output->driver_private = intel_output;
	output->interlace_allowed = 0;
	output->doublescan_allowed = 0;

	/* setup the DDC bus. */
	if (output_device == SDVOB)
		i2cbus = intel_i2c_create(dev, GPIOE, "SDVOCTRL_E for SDVOB");
	else
		i2cbus = intel_i2c_create(dev, GPIOE, "SDVOCTRL_E for SDVOC");

	if (i2cbus == NULL) {
		drm_output_destroy(output);
		return;
	}

	sdvo_priv->i2c_bus = i2cbus;

	if (output_device == SDVOB) {
		output_id = 1;
		sdvo_priv->i2c_bus->slave_addr = 0x38;
	} else {
		output_id = 2;
		sdvo_priv->i2c_bus->slave_addr = 0x39;
	}

	sdvo_priv->output_device = output_device;
	intel_output->i2c_bus = i2cbus;
	intel_output->dev_priv = sdvo_priv;


	/* Read the regs to test if we can talk to the device */
	for (i = 0; i < 0x40; i++) {
		if (!intel_sdvo_read_byte(output, i, &ch[i])) {
			DRM_DEBUG("No SDVO device found on SDVO%c\n",
				  output_device == SDVOB ? 'B' : 'C');
			drm_output_destroy(output);
			return;
		}
	}

	intel_sdvo_get_capabilities(output, &sdvo_priv->caps);

	memset(&sdvo_priv->active_outputs, 0, sizeof(sdvo_priv->active_outputs));

	/* TODO, CVBS, SVID, YPRPB & SCART outputs. */
	if (sdvo_priv->caps.output_flags & SDVO_OUTPUT_RGB0)
	{
		sdvo_priv->active_outputs = SDVO_OUTPUT_RGB0;
		output->display_info.subpixel_order = SubPixelHorizontalRGB;
		output_type = DRM_MODE_OUTPUT_DAC;
		connector_type = ConnectorVGA;
	}
	else if (sdvo_priv->caps.output_flags & SDVO_OUTPUT_RGB1)
	{
		sdvo_priv->active_outputs = SDVO_OUTPUT_RGB1;
		output->display_info.subpixel_order = SubPixelHorizontalRGB;
		output_type = DRM_MODE_OUTPUT_DAC;
		connector_type = ConnectorVGA;
	}
	else if (sdvo_priv->caps.output_flags & SDVO_OUTPUT_TMDS0)
	{
		sdvo_priv->active_outputs = SDVO_OUTPUT_TMDS0;
		output->display_info.subpixel_order = SubPixelHorizontalRGB;
		output_type = DRM_MODE_OUTPUT_TMDS;
		connector_type = ConnectorDVID;
	}
	else if (sdvo_priv->caps.output_flags & SDVO_OUTPUT_TMDS1)
	{
		sdvo_priv->active_outputs = SDVO_OUTPUT_TMDS1;
		output->display_info.subpixel_order = SubPixelHorizontalRGB;
		output_type = DRM_MODE_OUTPUT_TMDS;
		connector_type = ConnectorDVID;
	}
	else
	{
		unsigned char bytes[2];
		
		memcpy (bytes, &sdvo_priv->caps.output_flags, 2);
		DRM_DEBUG("%s: No active RGB or TMDS outputs (0x%02x%02x)\n",
			  SDVO_NAME(sdvo_priv),
			  bytes[0], bytes[1]);
		drm_output_destroy(output);
		return;
	}
	
	output->output_type = output_type;
	output->output_type_id = output_id;

	drm_sysfs_output_add(output);

	/* Set the input timing to the screen. Assume always input 0. */
	intel_sdvo_set_target_input(output, true, false);
	
	intel_sdvo_get_input_pixel_clock_range(output,
					       &sdvo_priv->pixel_clock_min,
					       &sdvo_priv->pixel_clock_max);


	DRM_DEBUG("%s device VID/DID: %02X:%02X.%02X, "
		  "clock range %dMHz - %dMHz, "
		  "input 1: %c, input 2: %c, "
		  "output 1: %c, output 2: %c\n",
		  SDVO_NAME(sdvo_priv),
		  sdvo_priv->caps.vendor_id, sdvo_priv->caps.device_id,
		  sdvo_priv->caps.device_rev_id,
		  sdvo_priv->pixel_clock_min / 1000,
		  sdvo_priv->pixel_clock_max / 1000,
		  (sdvo_priv->caps.sdvo_inputs_mask & 0x1) ? 'Y' : 'N',
		  (sdvo_priv->caps.sdvo_inputs_mask & 0x2) ? 'Y' : 'N',
		  /* check currently supported outputs */
		  sdvo_priv->caps.output_flags & 
		  	(SDVO_OUTPUT_TMDS0 | SDVO_OUTPUT_RGB0) ? 'Y' : 'N',
		  sdvo_priv->caps.output_flags & 
		  	(SDVO_OUTPUT_TMDS1 | SDVO_OUTPUT_RGB1) ? 'Y' : 'N');

	intel_output->ddc_bus = i2cbus;	

	drm_output_attach_property(output, dev->mode_config.connector_type_property, connector_type);
}
