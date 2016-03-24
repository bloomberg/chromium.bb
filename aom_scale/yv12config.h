/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef VPX_SCALE_YV12CONFIG_H_
#define VPX_SCALE_YV12CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "./vpx_config.h"
#include "aom/vpx_codec.h"
#include "aom/vpx_frame_buffer.h"
#include "aom/vpx_integer.h"

#define VP8BORDERINPIXELS 32
#define VPXINNERBORDERINPIXELS 96
#define VPX_INTERP_EXTEND 4
#define VPX_ENC_BORDER_IN_PIXELS 160
#define VPX_DEC_BORDER_IN_PIXELS 32

typedef struct yv12_buffer_config {
  int y_width;
  int y_height;
  int y_crop_width;
  int y_crop_height;
  int y_stride;

  int uv_width;
  int uv_height;
  int uv_crop_width;
  int uv_crop_height;
  int uv_stride;

  int alpha_width;
  int alpha_height;
  int alpha_stride;

  uint8_t *y_buffer;
  uint8_t *u_buffer;
  uint8_t *v_buffer;
  uint8_t *alpha_buffer;

  uint8_t *buffer_alloc;
  int buffer_alloc_sz;
  int border;
  int frame_size;
  int subsampling_x;
  int subsampling_y;
  unsigned int bit_depth;
  vpx_color_space_t color_space;
  vpx_color_range_t color_range;
  int render_width;
  int render_height;

  int corrupted;
  int flags;
} YV12_BUFFER_CONFIG;

#define YV12_FLAG_HIGHBITDEPTH 8

int vpx_yv12_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                                int border);
int vpx_yv12_realloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width,
                                  int height, int border);
int vpx_yv12_de_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf);

int vpx_alloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                           int ss_x, int ss_y,
#if CONFIG_VPX_HIGHBITDEPTH
                           int use_highbitdepth,
#endif
                           int border, int byte_alignment);

// Updates the yv12 buffer config with the frame buffer. |byte_alignment| must
// be a power of 2, from 32 to 1024. 0 sets legacy alignment. If cb is not
// NULL, then libaom is using the frame buffer callbacks to handle memory.
// If cb is not NULL, libaom will call cb with minimum size in bytes needed
// to decode the current frame. If cb is NULL, libaom will allocate memory
// internally to decode the current frame. Returns 0 on success. Returns < 0
// on failure.
int vpx_realloc_frame_buffer(YV12_BUFFER_CONFIG *ybf, int width, int height,
                             int ss_x, int ss_y,
#if CONFIG_VPX_HIGHBITDEPTH
                             int use_highbitdepth,
#endif
                             int border, int byte_alignment,
                             vpx_codec_frame_buffer_t *fb,
                             vpx_get_frame_buffer_cb_fn_t cb, void *cb_priv);
int vpx_free_frame_buffer(YV12_BUFFER_CONFIG *ybf);

#ifdef __cplusplus
}
#endif

#endif  // VPX_SCALE_YV12CONFIG_H_
