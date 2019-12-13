/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/tune_vmaf.h"

#include "aom_dsp/vmaf.h"
#include "aom_ports/system_state.h"
#include "av1/encoder/extend.h"

// TODO(sdeng): Add the SIMD implementation.
static AOM_INLINE void unsharp(const YV12_BUFFER_CONFIG *source,
                               const YV12_BUFFER_CONFIG *blurred,
                               const YV12_BUFFER_CONFIG *dst, double amount) {
  uint8_t *src = source->y_buffer;
  uint8_t *blur = blurred->y_buffer;
  uint8_t *dstbuf = dst->y_buffer;
  for (int i = 0; i < source->y_height; ++i) {
    for (int j = 0; j < source->y_width; ++j) {
      const double val =
          (double)src[j] + amount * ((double)src[j] - (double)blur[j]);
      dstbuf[j] = (uint8_t)clamp((int)(val + 0.5), 0, 255);
    }
    src += source->y_stride;
    blur += blurred->y_stride;
    dstbuf += dst->y_stride;
  }
}

// 8-tap Gaussian convolution filter with sigma = 1.0, sums to 128,
// all co-efficients must be even.
DECLARE_ALIGNED(16, static const int16_t, gauss_filter[8]) = { 0,  8, 30, 52,
                                                               30, 8, 0,  0 };
static AOM_INLINE void gaussian_blur(const AV1_COMP *const cpi,
                                     const YV12_BUFFER_CONFIG *source,
                                     const YV12_BUFFER_CONFIG *dst) {
  const AV1_COMMON *cm = &cpi->common;
  const ThreadData *td = &cpi->td;
  const MACROBLOCK *x = &td->mb;
  const MACROBLOCKD *xd = &x->e_mbd;

  const int block_size = BLOCK_128X128;

  const int num_mi_w = mi_size_wide[block_size];
  const int num_mi_h = mi_size_high[block_size];
  const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
  const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
  int row, col;
  const int use_hbd = source->flags & YV12_FLAG_HIGHBITDEPTH;

  ConvolveParams conv_params = get_conv_params(0, 0, xd->bd);
  InterpFilterParams filter = { .filter_ptr = gauss_filter,
                                .taps = 8,
                                .subpel_shifts = 0,
                                .interp_filter = EIGHTTAP_REGULAR };

  for (row = 0; row < num_rows; ++row) {
    for (col = 0; col < num_cols; ++col) {
      const int mi_row = row * num_mi_h;
      const int mi_col = col * num_mi_w;

      const int row_offset_y = mi_row << 2;
      const int col_offset_y = mi_col << 2;

      uint8_t *src_buf =
          source->y_buffer + row_offset_y * source->y_stride + col_offset_y;
      uint8_t *dst_buf =
          dst->y_buffer + row_offset_y * dst->y_stride + col_offset_y;

      if (use_hbd) {
        av1_highbd_convolve_2d_sr(
            CONVERT_TO_SHORTPTR(src_buf), source->y_stride,
            CONVERT_TO_SHORTPTR(dst_buf), dst->y_stride, num_mi_w << 2,
            num_mi_h << 2, &filter, &filter, 0, 0, &conv_params, xd->bd);
      } else {
        av1_convolve_2d_sr(src_buf, source->y_stride, dst_buf, dst->y_stride,
                           num_mi_w << 2, num_mi_h << 2, &filter, &filter, 0, 0,
                           &conv_params);
      }
    }
  }
}

void av1_vmaf_preprocessing(const AV1_COMP *cpi, YV12_BUFFER_CONFIG *source) {
  const int use_hbd = source->flags & YV12_FLAG_HIGHBITDEPTH;
  // TODO(sdeng): Add high bit depth support.
  if (use_hbd) {
    printf(
        "VMAF preprocessing for high bit depth videos is unsupported yet.\n");
    exit(0);
  }

  aom_clear_system_state();
  const AV1_COMMON *const cm = &cpi->common;
  const int width = source->y_width;
  const int height = source->y_height;

  YV12_BUFFER_CONFIG source_extended, blurred, sharpened;
  memset(&source_extended, 0, sizeof(source_extended));
  memset(&blurred, 0, sizeof(blurred));
  memset(&sharpened, 0, sizeof(sharpened));
  aom_alloc_frame_buffer(&source_extended, width, height, 1, 1,
                         cm->seq_params.use_highbitdepth,
                         cpi->oxcf.border_in_pixels, cm->byte_alignment);
  aom_alloc_frame_buffer(&blurred, width, height, 1, 1,
                         cm->seq_params.use_highbitdepth,
                         cpi->oxcf.border_in_pixels, cm->byte_alignment);
  aom_alloc_frame_buffer(&sharpened, width, height, 1, 1,
                         cm->seq_params.use_highbitdepth,
                         cpi->oxcf.border_in_pixels, cm->byte_alignment);

  av1_copy_and_extend_frame(source, &source_extended);
  gaussian_blur(cpi, &source_extended, &blurred);

  double unsharp_amount = 0.0;
  const double step_size = 0.05;
  const double max_vmaf_score = 100.0;

  double best_vmaf;
  aom_calc_vmaf(cpi->oxcf.vmaf_model_path, source, source, &best_vmaf);

  // We may get the same best VMAF scores for different unsharp_amount values.
  // We pick the average value in this case.
  double best_unsharp_amount_begin = best_vmaf == max_vmaf_score ? 0.0 : -1.0;
  bool exit_loop = false;

  int loop_count = 0;
  const int max_loop_count = 20;
  while (!exit_loop) {
    unsharp_amount += step_size;
    unsharp(source, &blurred, &sharpened, unsharp_amount);
    double new_vmaf;
    aom_calc_vmaf(cpi->oxcf.vmaf_model_path, source, &sharpened, &new_vmaf);
    if (new_vmaf < best_vmaf || loop_count == max_loop_count) {
      exit_loop = true;
    } else {
      if (new_vmaf == max_vmaf_score && best_unsharp_amount_begin < 0.0) {
        best_unsharp_amount_begin = unsharp_amount;
      }
      best_vmaf = new_vmaf;
    }
    loop_count++;
  }

  unsharp_amount -= step_size;
  if (best_unsharp_amount_begin >= 0.0) {
    unsharp_amount = (unsharp_amount + best_unsharp_amount_begin) / 2.0;
  }
  unsharp(source, &blurred, source, unsharp_amount);

  aom_free_frame_buffer(&source_extended);
  aom_free_frame_buffer(&sharpened);
  aom_free_frame_buffer(&blurred);
  aom_clear_system_state();
}
