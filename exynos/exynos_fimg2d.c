/*
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/mman.h>
#include <linux/stddef.h>

#include <xf86drm.h>

#include "libdrm_macros.h"
#include "exynos_drm.h"
#include "fimg2d_reg.h"
#include "exynos_fimg2d.h"

#define		SET_BF(val, sc, si, scsa, scda, dc, di, dcsa, dcda) \
			val.data.src_coeff = sc;		\
			val.data.inv_src_color_coeff = si;	\
			val.data.src_coeff_src_a = scsa;	\
			val.data.src_coeff_dst_a = scda;	\
			val.data.dst_coeff = dc;		\
			val.data.inv_dst_color_coeff = di;	\
			val.data.dst_coeff_src_a = dcsa;	\
			val.data.dst_coeff_dst_a = dcda;

#define MIN(a, b)	((a) < (b) ? (a) : (b))

enum g2d_base_addr_reg {
	g2d_dst = 0,
	g2d_src
};

static unsigned int g2d_get_scaling(unsigned int src, unsigned int dst)
{
	/*
	 * The G2D hw scaling factor is a normalized inverse of the scaling factor.
	 * For example: When source width is 100 and destination width is 200
	 * (scaling of 2x), then the hw factor is NC * 100 / 200.
	 * The normalization factor (NC) is 2^16 = 0x10000.
	 */

	return ((src << 16) / dst);
}

static unsigned int g2d_get_blend_op(enum e_g2d_op op)
{
	union g2d_blend_func_val val;

	val.val = 0;

	switch (op) {
	case G2D_OP_CLEAR:
	case G2D_OP_DISJOINT_CLEAR:
	case G2D_OP_CONJOINT_CLEAR:
		SET_BF(val, G2D_COEFF_MODE_ZERO, 0, 0, 0, G2D_COEFF_MODE_ZERO,
				0, 0, 0);
		break;
	case G2D_OP_SRC:
	case G2D_OP_DISJOINT_SRC:
	case G2D_OP_CONJOINT_SRC:
		SET_BF(val, G2D_COEFF_MODE_ONE, 0, 0, 0, G2D_COEFF_MODE_ZERO,
				0, 0, 0);
		break;
	case G2D_OP_DST:
	case G2D_OP_DISJOINT_DST:
	case G2D_OP_CONJOINT_DST:
		SET_BF(val, G2D_COEFF_MODE_ZERO, 0, 0, 0, G2D_COEFF_MODE_ONE,
				0, 0, 0);
		break;
	case G2D_OP_OVER:
		SET_BF(val, G2D_COEFF_MODE_ONE, 0, 0, 0,
				G2D_COEFF_MODE_SRC_ALPHA, 1, 0, 0);
		break;
	case G2D_OP_INTERPOLATE:
		SET_BF(val, G2D_COEFF_MODE_SRC_ALPHA, 0, 0, 0,
				G2D_COEFF_MODE_SRC_ALPHA, 1, 0, 0);
		break;
	default:
		fprintf(stderr, "Not support operation(%d).\n", op);
		SET_BF(val, G2D_COEFF_MODE_ONE, 0, 0, 0, G2D_COEFF_MODE_ZERO,
				0, 0, 0);
		break;
	}

	return val.val;
}

/*
 * g2d_add_cmd - set given command and value to user side command buffer.
 *
 * @ctx: a pointer to g2d_context structure.
 * @cmd: command data.
 * @value: value data.
 */
static int g2d_add_cmd(struct g2d_context *ctx, unsigned long cmd,
			unsigned long value)
{
	switch (cmd & ~(G2D_BUF_USERPTR)) {
	case SRC_BASE_ADDR_REG:
	case SRC_PLANE2_BASE_ADDR_REG:
	case DST_BASE_ADDR_REG:
	case DST_PLANE2_BASE_ADDR_REG:
	case PAT_BASE_ADDR_REG:
	case MASK_BASE_ADDR_REG:
		if (ctx->cmd_buf_nr >= G2D_MAX_GEM_CMD_NR) {
			fprintf(stderr, "Overflow cmd_gem size.\n");
			return -EINVAL;
		}

		ctx->cmd_buf[ctx->cmd_buf_nr].offset = cmd;
		ctx->cmd_buf[ctx->cmd_buf_nr].data = value;
		ctx->cmd_buf_nr++;
		break;
	default:
		if (ctx->cmd_nr >= G2D_MAX_CMD_NR) {
			fprintf(stderr, "Overflow cmd size.\n");
			return -EINVAL;
		}

		ctx->cmd[ctx->cmd_nr].offset = cmd;
		ctx->cmd[ctx->cmd_nr].data = value;
		ctx->cmd_nr++;
		break;
	}

	return 0;
}

/*
 * g2d_add_base_addr - helper function to set dst/src base address register.
 *
 * @ctx: a pointer to g2d_context structure.
 * @img: a pointer to the dst/src g2d_image structure.
 * @reg: the register that should be set.
 */
static void g2d_add_base_addr(struct g2d_context *ctx, struct g2d_image *img,
			enum g2d_base_addr_reg reg)
{
	const unsigned long cmd = (reg == g2d_dst) ?
		DST_BASE_ADDR_REG : SRC_BASE_ADDR_REG;

	if (img->buf_type == G2D_IMGBUF_USERPTR)
		g2d_add_cmd(ctx, cmd | G2D_BUF_USERPTR,
				(unsigned long)&img->user_ptr[0]);
	else
		g2d_add_cmd(ctx, cmd, img->bo[0]);
}

/*
 * g2d_reset - reset fimg2d hardware.
 *
 * @ctx: a pointer to g2d_context structure.
 *
 */
static void g2d_reset(struct g2d_context *ctx)
{
	ctx->cmd_nr = 0;
	ctx->cmd_buf_nr = 0;

	g2d_add_cmd(ctx, SOFT_RESET_REG, 0x01);
}

/*
 * g2d_flush - submit all commands and values in user side command buffer
 *		to command queue aware of fimg2d dma.
 *
 * @ctx: a pointer to g2d_context structure.
 *
 * This function should be called after all commands and values to user
 * side command buffer are set. It submits that buffer to the kernel side driver.
 */
static int g2d_flush(struct g2d_context *ctx)
{
	int ret;
	struct drm_exynos_g2d_set_cmdlist cmdlist = {0};

	if (ctx->cmd_nr == 0 && ctx->cmd_buf_nr == 0)
		return -1;

	if (ctx->cmdlist_nr >= G2D_MAX_CMD_LIST_NR) {
		fprintf(stderr, "Overflow cmdlist.\n");
		return -EINVAL;
	}

	cmdlist.cmd = (uint64_t)(uintptr_t)&ctx->cmd[0];
	cmdlist.cmd_buf = (uint64_t)(uintptr_t)&ctx->cmd_buf[0];
	cmdlist.cmd_nr = ctx->cmd_nr;
	cmdlist.cmd_buf_nr = ctx->cmd_buf_nr;
	cmdlist.event_type = G2D_EVENT_NOT;
	cmdlist.user_data = 0;

	ctx->cmd_nr = 0;
	ctx->cmd_buf_nr = 0;

	ret = drmIoctl(ctx->fd, DRM_IOCTL_EXYNOS_G2D_SET_CMDLIST, &cmdlist);
	if (ret < 0) {
		fprintf(stderr, "failed to set cmdlist.\n");
		return ret;
	}

	ctx->cmdlist_nr++;

	return ret;
}

/**
 * g2d_init - create a new g2d context and get hardware version.
 *
 * fd: a file descriptor to an opened drm device.
 */
struct g2d_context *g2d_init(int fd)
{
	struct drm_exynos_g2d_get_ver ver;
	struct g2d_context *ctx;
	int ret;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) {
		fprintf(stderr, "failed to allocate context.\n");
		return NULL;
	}

	ctx->fd = fd;

	ret = drmIoctl(fd, DRM_IOCTL_EXYNOS_G2D_GET_VER, &ver);
	if (ret < 0) {
		fprintf(stderr, "failed to get version.\n");
		free(ctx);
		return NULL;
	}

	ctx->major = ver.major;
	ctx->minor = ver.minor;

	printf("g2d version(%d.%d).\n", ctx->major, ctx->minor);
	return ctx;
}

void g2d_fini(struct g2d_context *ctx)
{
	free(ctx);
}

/**
 * g2d_exec - start the dma to process all commands summited by g2d_flush().
 *
 * @ctx: a pointer to g2d_context structure.
 */
int g2d_exec(struct g2d_context *ctx)
{
	struct drm_exynos_g2d_exec exec;
	int ret;

	if (ctx->cmdlist_nr == 0)
		return -EINVAL;

	exec.async = 0;

	ret = drmIoctl(ctx->fd, DRM_IOCTL_EXYNOS_G2D_EXEC, &exec);
	if (ret < 0) {
		fprintf(stderr, "failed to execute.\n");
		return ret;
	}

	ctx->cmdlist_nr = 0;

	return ret;
}

/**
 * g2d_solid_fill - fill given buffer with given color data.
 *
 * @ctx: a pointer to g2d_context structure.
 * @img: a pointer to g2d_image structure including image and buffer
 *	information.
 * @x: x start position to buffer filled with given color data.
 * @y: y start position to buffer filled with given color data.
 * @w: width value to buffer filled with given color data.
 * @h: height value to buffer filled with given color data.
 */
int
g2d_solid_fill(struct g2d_context *ctx, struct g2d_image *img,
			unsigned int x, unsigned int y, unsigned int w,
			unsigned int h)
{
	union g2d_bitblt_cmd_val bitblt;
	union g2d_point_val pt;

	g2d_add_cmd(ctx, DST_SELECT_REG, G2D_SELECT_MODE_NORMAL);
	g2d_add_cmd(ctx, DST_COLOR_MODE_REG, img->color_mode);
	g2d_add_base_addr(ctx, img, g2d_dst);
	g2d_add_cmd(ctx, DST_STRIDE_REG, img->stride);

	if (x + w > img->width)
		w = img->width - x;
	if (y + h > img->height)
		h = img->height - y;

	pt.val = 0;
	pt.data.x = x;
	pt.data.y = y;
	g2d_add_cmd(ctx, DST_LEFT_TOP_REG, pt.val);

	pt.val = 0;
	pt.data.x = x + w;
	pt.data.y = y + h;

	g2d_add_cmd(ctx, DST_RIGHT_BOTTOM_REG, pt.val);

	g2d_add_cmd(ctx, SF_COLOR_REG, img->color);

	bitblt.val = 0;
	bitblt.data.fast_solid_color_fill_en = 1;
	g2d_add_cmd(ctx, BITBLT_COMMAND_REG, bitblt.val);

	return g2d_flush(ctx);
}

/**
 * g2d_copy - copy contents in source buffer to destination buffer.
 *
 * @ctx: a pointer to g2d_context structure.
 * @src: a pointer to g2d_image structure including image and buffer
 *	information to source.
 * @dst: a pointer to g2d_image structure including image and buffer
 *	information to destination.
 * @src_x: x start position to source buffer.
 * @src_y: y start position to source buffer.
 * @dst_x: x start position to destination buffer.
 * @dst_y: y start position to destination buffer.
 * @w: width value to source and destination buffers.
 * @h: height value to source and destination buffers.
 */
int
g2d_copy(struct g2d_context *ctx, struct g2d_image *src,
		struct g2d_image *dst, unsigned int src_x, unsigned int src_y,
		unsigned int dst_x, unsigned dst_y, unsigned int w,
		unsigned int h)
{
	union g2d_rop4_val rop4;
	union g2d_point_val pt;
	unsigned int src_w = 0, src_h = 0, dst_w = 0, dst_h = 0;

	g2d_add_cmd(ctx, DST_SELECT_REG, G2D_SELECT_MODE_BGCOLOR);
	g2d_add_cmd(ctx, DST_COLOR_MODE_REG, dst->color_mode);
	g2d_add_base_addr(ctx, dst, g2d_dst);
	g2d_add_cmd(ctx, DST_STRIDE_REG, dst->stride);

	g2d_add_cmd(ctx, SRC_SELECT_REG, G2D_SELECT_MODE_NORMAL);
	g2d_add_cmd(ctx, SRC_COLOR_MODE_REG, src->color_mode);
	g2d_add_base_addr(ctx, src, g2d_src);
	g2d_add_cmd(ctx, SRC_STRIDE_REG, src->stride);

	src_w = w;
	src_h = h;
	if (src_x + src->width > w)
		src_w = src->width - src_x;
	if (src_y + src->height > h)
		src_h = src->height - src_y;

	dst_w = w;
	dst_h = w;
	if (dst_x + dst->width > w)
		dst_w = dst->width - dst_x;
	if (dst_y + dst->height > h)
		dst_h = dst->height - dst_y;

	w = MIN(src_w, dst_w);
	h = MIN(src_h, dst_h);

	if (w <= 0 || h <= 0) {
		fprintf(stderr, "invalid width or height.\n");
		g2d_reset(ctx);
		return -EINVAL;
	}

	pt.val = 0;
	pt.data.x = src_x;
	pt.data.y = src_y;
	g2d_add_cmd(ctx, SRC_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = src_x + w;
	pt.data.y = src_y + h;
	g2d_add_cmd(ctx, SRC_RIGHT_BOTTOM_REG, pt.val);

	pt.val = 0;
	pt.data.x = dst_x;
	pt.data.y = dst_y;
	g2d_add_cmd(ctx, DST_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = dst_x + w;
	pt.data.y = dst_y + h;
	g2d_add_cmd(ctx, DST_RIGHT_BOTTOM_REG, pt.val);

	rop4.val = 0;
	rop4.data.unmasked_rop3 = G2D_ROP3_SRC;
	g2d_add_cmd(ctx, ROP4_REG, rop4.val);

	return g2d_flush(ctx);
}

/**
 * g2d_copy_with_scale - copy contents in source buffer to destination buffer
 *	scaling up or down properly.
 *
 * @ctx: a pointer to g2d_context structure.
 * @src: a pointer to g2d_image structure including image and buffer
 *	information to source.
 * @dst: a pointer to g2d_image structure including image and buffer
 *	information to destination.
 * @src_x: x start position to source buffer.
 * @src_y: y start position to source buffer.
 * @src_w: width value to source buffer.
 * @src_h: height value to source buffer.
 * @dst_x: x start position to destination buffer.
 * @dst_y: y start position to destination buffer.
 * @dst_w: width value to destination buffer.
 * @dst_h: height value to destination buffer.
 * @negative: indicate that it uses color negative to source and
 *	destination buffers.
 */
int
g2d_copy_with_scale(struct g2d_context *ctx, struct g2d_image *src,
				struct g2d_image *dst, unsigned int src_x,
				unsigned int src_y, unsigned int src_w,
				unsigned int src_h, unsigned int dst_x,
				unsigned int dst_y, unsigned int dst_w,
				unsigned int dst_h, unsigned int negative)
{
	union g2d_rop4_val rop4;
	union g2d_point_val pt;
	unsigned int scale;
	unsigned int scale_x, scale_y;

	g2d_add_cmd(ctx, DST_SELECT_REG, G2D_SELECT_MODE_BGCOLOR);
	g2d_add_cmd(ctx, DST_COLOR_MODE_REG, dst->color_mode);
	g2d_add_base_addr(ctx, dst, g2d_dst);
	g2d_add_cmd(ctx, DST_STRIDE_REG, dst->stride);

	g2d_add_cmd(ctx, SRC_SELECT_REG, G2D_SELECT_MODE_NORMAL);
	g2d_add_cmd(ctx, SRC_COLOR_MODE_REG, src->color_mode);

	g2d_add_cmd(ctx, SRC_REPEAT_MODE_REG, src->repeat_mode);
	if (src->repeat_mode == G2D_REPEAT_MODE_PAD)
		g2d_add_cmd(ctx, SRC_PAD_VALUE_REG, dst->color);

	g2d_add_base_addr(ctx, src, g2d_src);
	g2d_add_cmd(ctx, SRC_STRIDE_REG, src->stride);

	if (src_w == dst_w && src_h == dst_h)
		scale = 0;
	else {
		scale = 1;
		scale_x = g2d_get_scaling(src_w, dst_w);
		scale_y = g2d_get_scaling(src_h, dst_h);
	}

	if (src_x + src_w > src->width)
		src_w = src->width - src_x;
	if (src_y + src_h > src->height)
		src_h = src->height - src_y;

	if (dst_x + dst_w > dst->width)
		dst_w = dst->width - dst_x;
	if (dst_y + dst_h > dst->height)
		dst_h = dst->height - dst_y;

	if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
		fprintf(stderr, "invalid width or height.\n");
		g2d_reset(ctx);
		return -EINVAL;
	}

	if (negative) {
		g2d_add_cmd(ctx, BG_COLOR_REG, 0x00FFFFFF);
		rop4.val = 0;
		rop4.data.unmasked_rop3 = G2D_ROP3_SRC^G2D_ROP3_DST;
		g2d_add_cmd(ctx, ROP4_REG, rop4.val);
	} else {
		rop4.val = 0;
		rop4.data.unmasked_rop3 = G2D_ROP3_SRC;
		g2d_add_cmd(ctx, ROP4_REG, rop4.val);
	}

	if (scale) {
		g2d_add_cmd(ctx, SRC_SCALE_CTRL_REG, G2D_SCALE_MODE_BILINEAR);
		g2d_add_cmd(ctx, SRC_XSCALE_REG, scale_x);
		g2d_add_cmd(ctx, SRC_YSCALE_REG, scale_y);
	}

	pt.val = 0;
	pt.data.x = src_x;
	pt.data.y = src_y;
	g2d_add_cmd(ctx, SRC_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = src_x + src_w;
	pt.data.y = src_y + src_h;
	g2d_add_cmd(ctx, SRC_RIGHT_BOTTOM_REG, pt.val);

	pt.val = 0;
	pt.data.x = dst_x;
	pt.data.y = dst_y;
	g2d_add_cmd(ctx, DST_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = dst_x + dst_w;
	pt.data.y = dst_y + dst_h;
	g2d_add_cmd(ctx, DST_RIGHT_BOTTOM_REG, pt.val);

	return g2d_flush(ctx);
}

/**
 * g2d_blend - blend image data in source and destination buffers.
 *
 * @ctx: a pointer to g2d_context structure.
 * @src: a pointer to g2d_image structure including image and buffer
 *	information to source.
 * @dst: a pointer to g2d_image structure including image and buffer
 *	information to destination.
 * @src_x: x start position to source buffer.
 * @src_y: y start position to source buffer.
 * @dst_x: x start position to destination buffer.
 * @dst_y: y start position to destination buffer.
 * @w: width value to source and destination buffer.
 * @h: height value to source and destination buffer.
 * @op: blend operation type.
 */
int
g2d_blend(struct g2d_context *ctx, struct g2d_image *src,
		struct g2d_image *dst, unsigned int src_x,
		unsigned int src_y, unsigned int dst_x, unsigned int dst_y,
		unsigned int w, unsigned int h, enum e_g2d_op op)
{
	union g2d_point_val pt;
	union g2d_bitblt_cmd_val bitblt;
	union g2d_blend_func_val blend;
	unsigned int src_w = 0, src_h = 0, dst_w = 0, dst_h = 0;

	bitblt.val = 0;
	blend.val = 0;

	if (op == G2D_OP_SRC || op == G2D_OP_CLEAR)
		g2d_add_cmd(ctx, DST_SELECT_REG, G2D_SELECT_MODE_BGCOLOR);
	else
		g2d_add_cmd(ctx, DST_SELECT_REG, G2D_SELECT_MODE_NORMAL);

	g2d_add_cmd(ctx, DST_COLOR_MODE_REG, dst->color_mode);
	g2d_add_base_addr(ctx, dst, g2d_dst);
	g2d_add_cmd(ctx, DST_STRIDE_REG, dst->stride);

	g2d_add_cmd(ctx, SRC_SELECT_REG, src->select_mode);
	g2d_add_cmd(ctx, SRC_COLOR_MODE_REG, src->color_mode);

	switch (src->select_mode) {
	case G2D_SELECT_MODE_NORMAL:
		g2d_add_base_addr(ctx, src, g2d_src);
		g2d_add_cmd(ctx, SRC_STRIDE_REG, src->stride);
		break;
	case G2D_SELECT_MODE_FGCOLOR:
		g2d_add_cmd(ctx, FG_COLOR_REG, src->color);
		break;
	case G2D_SELECT_MODE_BGCOLOR:
		g2d_add_cmd(ctx, BG_COLOR_REG, src->color);
		break;
	default:
		fprintf(stderr , "failed to set src.\n");
		return -EINVAL;
	}

	src_w = w;
	src_h = h;
	if (src_x + w > src->width)
		src_w = src->width - src_x;
	if (src_y + h > src->height)
		src_h = src->height - src_y;

	dst_w = w;
	dst_h = h;
	if (dst_x + w > dst->width)
		dst_w = dst->width - dst_x;
	if (dst_y + h > dst->height)
		dst_h = dst->height - dst_y;

	w = MIN(src_w, dst_w);
	h = MIN(src_h, dst_h);

	if (w <= 0 || h <= 0) {
		fprintf(stderr, "invalid width or height.\n");
		g2d_reset(ctx);
		return -EINVAL;
	}

	bitblt.data.alpha_blend_mode = G2D_ALPHA_BLEND_MODE_ENABLE;
	blend.val = g2d_get_blend_op(op);
	g2d_add_cmd(ctx, BITBLT_COMMAND_REG, bitblt.val);
	g2d_add_cmd(ctx, BLEND_FUNCTION_REG, blend.val);

	pt.val = 0;
	pt.data.x = src_x;
	pt.data.y = src_y;
	g2d_add_cmd(ctx, SRC_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = src_x + w;
	pt.data.y = src_y + h;
	g2d_add_cmd(ctx, SRC_RIGHT_BOTTOM_REG, pt.val);

	pt.val = 0;
	pt.data.x = dst_x;
	pt.data.y = dst_y;
	g2d_add_cmd(ctx, DST_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = dst_x + w;
	pt.data.y = dst_y + h;
	g2d_add_cmd(ctx, DST_RIGHT_BOTTOM_REG, pt.val);

	return g2d_flush(ctx);
}

/**
 * g2d_scale_and_blend - apply scaling to source buffer and then blend to destination buffer
 *
 * @ctx: a pointer to g2d_context structure.
 * @src: a pointer to g2d_image structure including image and buffer
 *	information to source.
 * @dst: a pointer to g2d_image structure including image and buffer
 *	information to destination.
 * @src_x: x start position to source buffer.
 * @src_y: y start position to source buffer.
 * @src_w: width value to source buffer.
 * @src_h: height value to source buffer.
 * @dst_x: x start position to destination buffer.
 * @dst_y: y start position to destination buffer.
 * @dst_w: width value to destination buffer.
 * @dst_h: height value to destination buffer.
 * @op: blend operation type.
 */
int
g2d_scale_and_blend(struct g2d_context *ctx, struct g2d_image *src,
		struct g2d_image *dst, unsigned int src_x, unsigned int src_y,
		unsigned int src_w, unsigned int src_h, unsigned int dst_x,
		unsigned int dst_y, unsigned int dst_w, unsigned int dst_h,
		enum e_g2d_op op)
{
	union g2d_point_val pt;
	union g2d_bitblt_cmd_val bitblt;
	union g2d_blend_func_val blend;
	unsigned int scale;
	unsigned int scale_x, scale_y;

	bitblt.val = 0;
	blend.val = 0;

	if (op == G2D_OP_SRC || op == G2D_OP_CLEAR)
		g2d_add_cmd(ctx, DST_SELECT_REG, G2D_SELECT_MODE_BGCOLOR);
	else
		g2d_add_cmd(ctx, DST_SELECT_REG, G2D_SELECT_MODE_NORMAL);

	g2d_add_cmd(ctx, DST_COLOR_MODE_REG, dst->color_mode);
	if (dst->buf_type == G2D_IMGBUF_USERPTR)
		g2d_add_cmd(ctx, DST_BASE_ADDR_REG | G2D_BUF_USERPTR,
				(unsigned long)&dst->user_ptr[0]);
	else
		g2d_add_cmd(ctx, DST_BASE_ADDR_REG, dst->bo[0]);

	g2d_add_cmd(ctx, DST_STRIDE_REG, dst->stride);

	g2d_add_cmd(ctx, SRC_SELECT_REG, src->select_mode);
	g2d_add_cmd(ctx, SRC_COLOR_MODE_REG, src->color_mode);

	switch (src->select_mode) {
	case G2D_SELECT_MODE_NORMAL:
		if (src->buf_type == G2D_IMGBUF_USERPTR)
			g2d_add_cmd(ctx, SRC_BASE_ADDR_REG | G2D_BUF_USERPTR,
					(unsigned long)&src->user_ptr[0]);
		else
			g2d_add_cmd(ctx, SRC_BASE_ADDR_REG, src->bo[0]);

		g2d_add_cmd(ctx, SRC_STRIDE_REG, src->stride);
		break;
	case G2D_SELECT_MODE_FGCOLOR:
		g2d_add_cmd(ctx, FG_COLOR_REG, src->color);
		break;
	case G2D_SELECT_MODE_BGCOLOR:
		g2d_add_cmd(ctx, BG_COLOR_REG, src->color);
		break;
	default:
		fprintf(stderr , "failed to set src.\n");
		return -EINVAL;
	}

	if (src_w == dst_w && src_h == dst_h)
		scale = 0;
	else {
		scale = 1;
		scale_x = g2d_get_scaling(src_w, dst_w);
		scale_y = g2d_get_scaling(src_h, dst_h);
	}

	if (src_x + src_w > src->width)
		src_w = src->width - src_x;
	if (src_y + src_h > src->height)
		src_h = src->height - src_y;

	if (dst_x + dst_w > dst->width)
		dst_w = dst->width - dst_x;
	if (dst_y + dst_h > dst->height)
		dst_h = dst->height - dst_y;

	if (src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) {
		fprintf(stderr, "invalid width or height.\n");
		g2d_reset(ctx);
		return -EINVAL;
	}

	if (scale) {
		g2d_add_cmd(ctx, SRC_SCALE_CTRL_REG, G2D_SCALE_MODE_BILINEAR);
		g2d_add_cmd(ctx, SRC_XSCALE_REG, scale_x);
		g2d_add_cmd(ctx, SRC_YSCALE_REG, scale_y);
	}

	bitblt.data.alpha_blend_mode = G2D_ALPHA_BLEND_MODE_ENABLE;
	blend.val = g2d_get_blend_op(op);
	g2d_add_cmd(ctx, BITBLT_COMMAND_REG, bitblt.val);
	g2d_add_cmd(ctx, BLEND_FUNCTION_REG, blend.val);

	pt.val = 0;
	pt.data.x = src_x;
	pt.data.y = src_y;
	g2d_add_cmd(ctx, SRC_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = src_x + src_w;
	pt.data.y = src_y + src_h;
	g2d_add_cmd(ctx, SRC_RIGHT_BOTTOM_REG, pt.val);

	pt.val = 0;
	pt.data.x = dst_x;
	pt.data.y = dst_y;
	g2d_add_cmd(ctx, DST_LEFT_TOP_REG, pt.val);
	pt.val = 0;
	pt.data.x = dst_x + dst_w;
	pt.data.y = dst_y + dst_h;
	g2d_add_cmd(ctx, DST_RIGHT_BOTTOM_REG, pt.val);

	return g2d_flush(ctx);
}
