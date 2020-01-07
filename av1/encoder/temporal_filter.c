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

#include <math.h>
#include <limits.h>

#include "av1/common/blockd.h"
#include "config/aom_config.h"

#include "av1/common/alloccommon.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/odintrin.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/temporal_filter.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/system_state.h"
#include "aom_scale/aom_scale.h"

// NOTE: All `tf` in this file means `temporal filtering`.

// Builds predictors for blocks in temporal filtering.
// Inputs:
//   ref_frame: Pointer to the frame for filtering.
//   mbd: Pointer to the block for filtering.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   scale: Scaling factor.
//   num_planes: Number of planes in the frame.
//   use_subblock: Whether to use four sub-blocks to replace the original block.
//   subblock_mvs: The motion vectors for each sub-blocks (row-major order).
//   pred: Pointer to the predictors to build.
// Returns:
//   Nothing will be returned. But the content to which `pred` points will be
//   modified.
static void tf_build_predictors_mb_c(
    const YV12_BUFFER_CONFIG *ref_frame, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const struct scale_factors *scale, const int num_planes,
    const int use_subblock, const MV *subblock_mvs, uint8_t *pred) {
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  // Information of the entire block.
  const int mb_height = block_size_high[block_size];  // Height.
  const int mb_width = block_size_wide[block_size];   // Width.
  const int mb_pels = mb_height * mb_width;           // Number of pixels.
  const int mb_y = mb_height * mb_row;                // Y-coord (Top-left).
  const int mb_x = mb_width * mb_col;                 // X-coord (Top-left).
  const int bit_depth = mbd->bd;                      // Bit depth.
  const int is_high_bitdepth = is_cur_buf_hbd(mbd);   // Is high bit-depth?
  const int is_intrabc = 0;                           // Is intra-copied?
  const int mb_mv_row = mbd->mi[0]->mv[0].as_mv.row;  // Motion vector (y).
  const int mb_mv_col = mbd->mi[0]->mv[0].as_mv.col;  // Motion vector (x).
  const MV mb_mv = { (int16_t)mb_mv_row, (int16_t)mb_mv_col };

  // Information of each sub-block (actually in use).
  const int num_blocks = use_subblock ? 2 : 1;  // Num of blocks on each side.
  const int block_height = mb_height >> (num_blocks - 1);  // Height.
  const int block_width = mb_width >> (num_blocks - 1);    // Width.

  // Default interpolation filters.
  const int_interpfilters interp_filters =
      av1_broadcast_interp_filter(MULTITAP_SHARP);

  // Handle Y-plane, U-plane and V-plane (if needed) in sequence.
  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const int subsampling_y = mbd->plane[plane].subsampling_y;
    const int subsampling_x = mbd->plane[plane].subsampling_x;
    // Information of each sub-block in current plane.
    const int plane_h = mb_height >> subsampling_y;  // Plane height.
    const int plane_w = mb_width >> subsampling_x;   // Plane width.
    const int plane_y = mb_y >> subsampling_y;       // Y-coord (Top-left).
    const int plane_x = mb_x >> subsampling_x;       // X-coord (Top-left).
    const int h = block_height >> subsampling_y;     // Sub-block height.
    const int w = block_width >> subsampling_x;      // Sub-block width.
    const int is_y_plane = (plane == 0);             // Is Y-plane?

    const struct buf_2d ref_buf = { NULL, ref_frame->buffers[plane],
                                    ref_frame->widths[is_y_plane ? 0 : 1],
                                    ref_frame->heights[is_y_plane ? 0 : 1],
                                    ref_frame->strides[is_y_plane ? 0 : 1] };

    // Handle entire block or sub-blocks if needed.
    int subblock_idx = 0;
    for (int i = 0; i < plane_h; i += h) {
      for (int j = 0; j < plane_w; j += w) {
        // Choose proper motion vector.
        const MV mv = use_subblock ? subblock_mvs[subblock_idx] : mb_mv;
        assert(mv.row >= INT16_MIN && mv.row <= INT16_MAX &&
               mv.col >= INT16_MIN && mv.col <= INT16_MAX);

        const int y = plane_y + i;
        const int x = plane_x + j;

        // Build predictior for each sub-block on current plane.
        InterPredParams inter_pred_params;
        av1_init_inter_params(&inter_pred_params, w, h, y, x, subsampling_x,
                              subsampling_y, bit_depth, is_high_bitdepth,
                              is_intrabc, scale, &ref_buf, interp_filters);
        inter_pred_params.conv_params = get_conv_params(0, plane, bit_depth);
        av1_build_inter_predictor(&pred[plane_offset + i * plane_w + j],
                                  plane_w, &mv, &inter_pred_params);

        ++subblock_idx;
      }
    }
    plane_offset += mb_pels;
  }
}

static void apply_temporal_filter_self(const uint8_t *pred, int buf_stride,
                                       unsigned int block_width,
                                       unsigned int block_height,
                                       int filter_weight, uint32_t *accumulator,
                                       uint16_t *count,
                                       int use_new_temporal_mode) {
  const int modifier = use_new_temporal_mode ? SCALE : filter_weight * 16;
  unsigned int i, j, k = 0;

  for (i = 0; i < block_height; i++) {
    for (j = 0; j < block_width; j++) {
      const int pixel_value = pred[i * buf_stride + j];
      count[k] += modifier;
      accumulator[k] += modifier * pixel_value;
      ++k;
    }
  }
}

static void highbd_apply_temporal_filter_self(
    const uint8_t *pred8, int buf_stride, unsigned int block_width,
    unsigned int block_height, int filter_weight, uint32_t *accumulator,
    uint16_t *count, int use_new_temporal_mode) {
  const int modifier = use_new_temporal_mode ? SCALE : filter_weight * 16;
  const uint16_t *pred = CONVERT_TO_SHORTPTR(pred8);
  unsigned int i, j, k = 0;

  for (i = 0; i < block_height; i++) {
    for (j = 0; j < block_width; j++) {
      const int pixel_value = pred[i * buf_stride + j];
      count[k] += modifier;
      accumulator[k] += modifier * pixel_value;
      ++k;
    }
  }
}

// Function to compute pixel-wise squared difference between two buffers.
// Inputs:
//   ref: Pointer to reference buffer.
//   ref_offset: Start position of reference buffer for computation.
//   ref_stride: Stride for reference buffer.
//   tgt: Pointer to target buffer.
//   tgt_offset: Start position of target buffer for computation.
//   tgt_stride: Stride for target buffer.
//   height: Height of block for computation.
//   width: Width of block for computation.
//   is_high_bitdepth: Whether the two buffers point to high bit-depth frames.
//   square_diff: Pointer to save the squared differces.
// Returns:
//   Nothing will be returned. But the content to which `square_diff` points
//   will be modified.
static INLINE void compute_square_diff(const uint8_t *ref, const int ref_offset,
                                       const int ref_stride, const uint8_t *tgt,
                                       const int tgt_offset,
                                       const int tgt_stride, const int height,
                                       const int width,
                                       const int is_high_bitdepth,
                                       uint32_t *square_diff) {
  const uint16_t *ref16 = CONVERT_TO_SHORTPTR(ref);
  const uint16_t *tgt16 = CONVERT_TO_SHORTPTR(tgt);

  int ref_idx = 0;
  int tgt_idx = 0;
  int idx = 0;
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      const uint16_t ref_value = is_high_bitdepth ? ref16[ref_offset + ref_idx]
                                                  : ref[ref_offset + ref_idx];
      const uint16_t tgt_value = is_high_bitdepth ? tgt16[tgt_offset + tgt_idx]
                                                  : tgt[tgt_offset + tgt_idx];
      const uint32_t diff = (ref_value > tgt_value) ? (ref_value - tgt_value)
                                                    : (tgt_value - ref_value);
      square_diff[idx] = diff * diff;

      ++ref_idx;
      ++tgt_idx;
      ++idx;
    }
    ref_idx += (ref_stride - width);
    tgt_idx += (tgt_stride - width);
  }
}

// Magic numbers used to adjust the pixel-wise weight used in YUV filtering.
// For now, it only supports 3x3 window for filtering.
// The adjustment is performed with following steps:
//   (1) For a particular pixel, compute the sum of squared difference between
//       input frame and prediction in a small window (i.e., 3x3). There are
//       three possible outcomes:
//       (a) If the pixel locates in the middle of the plane, it has 9
//           neighbours (self-included).
//       (b) If the pixel locates on the edge of the plane, it has 6
//           neighbours (self-included).
//       (c) If the pixel locates on the corner of the plane, it has 4
//           neighbours (self-included).
//   (2) For Y-plane, it will also consider the squared difference from U-plane
//       and V-plane at the corresponding position as reference. This leads to
//       2 more neighbours.
//   (3) For U-plane and V-plane, it will consider the squared difference from
//       Y-plane at the corresponding position (after upsampling) as reference.
//       This leads to 1 more (subsampling = 0) or 4 more (subsampling = 1)
//       neighbours.
//   (4) Find the modifier for adjustment from the lookup table according to
//       number of reference pixels (neighbours) used. From above, the number
//       of neighbours can be 9+2 (11), 6+2 (8), 4+2 (6), 9+1 (10), 6+1 (7),
//       4+1 (5), 9+4 (13), 6+4 (10), 4+4 (8).
// TODO(yjshen): Not sure what index 4 and index 9 are for.
static const uint32_t filter_weight_adjustment_lookup_table_yuv[14] = {
  0, 0, 0, 0, 49152, 39322, 32768, 28087, 24576, 21846, 19661, 17874, 0, 15124
};
// Lookup table for high bit-depth.
static const uint64_t highbd_filter_weight_adjustment_lookup_table_yuv[14] = {
  0U,          0U,          0U,          0U,          3221225472U,
  2576980378U, 2147483648U, 1840700270U, 1610612736U, 1431655766U,
  1288490189U, 1171354718U, 0U,          991146300U
};

// Function to adjust the filter weight when applying YUV filter.
// Inputs:
//   filter_weight: Original filter weight.
//   sum_square_diff: Sum of squared difference between input frame and
//                    prediction. This field is computed pixel by pixel, and
//                    is used as a reference for the filter weight adjustment.
//   num_ref_pixels: Number of pixels used to compute the `sum_square_diff`.
//                   This field should align with the above lookup tables
//                   `filter_weight_adjustment_lookup_table_yuv` and
//                   `highbd_filter_weight_adjustment_lookup_table_yuv`.
//   strength: Strength for filter weight adjustment.
//   is_high_bitdepth: Whether apply temporal filter to high bie-depth video.
// Returns:
//   Adjusted filter weight which will finally be used for filtering..
static INLINE int adjust_filter_weight_yuv(const int filter_weight,
                                           const uint64_t sum_square_diff,
                                           const int num_ref_pixels,
                                           const int strength,
                                           const int is_high_bitdepth) {
  assert(YUV_FILTER_WINDOW_LENGTH == 3);
  assert(num_ref_pixels >= 0 && num_ref_pixels <= 13);

  const uint64_t multiplier =
      is_high_bitdepth
          ? highbd_filter_weight_adjustment_lookup_table_yuv[num_ref_pixels]
          : filter_weight_adjustment_lookup_table_yuv[num_ref_pixels];
  assert(multiplier != 0);

  const uint32_t max_value = is_high_bitdepth ? UINT32_MAX : UINT16_MAX;
  const int shift = is_high_bitdepth ? 32 : 16;
  int modifier =
      (int)((AOMMIN(sum_square_diff, max_value) * multiplier) >> shift);

  const int rounding = (1 << strength) >> 1;
  modifier = (modifier + rounding) >> strength;
  return (modifier >= 16) ? 0 : (16 - modifier) * filter_weight;
}

// Applies temporal filter to YUV planes.
// Inputs:
//   ref_frame: Pointer to the frame for filtering.
//   mbd: Pointer to the block for filtering.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   strength: Strength for filter weight adjustment.
//   use_subblock: Whether to use four sub-blocks to replace the original block.
//   subblock_filter_weights: The filter weights for each sub-block (row-major
//                            order). If `use_subblock` is set as 0, the first
//                            weight will be applied to the entire block.
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void av1_apply_temporal_filter_yuv_c(const YV12_BUFFER_CONFIG *ref_frame,
                                     const MACROBLOCKD *mbd,
                                     const BLOCK_SIZE block_size,
                                     const int mb_row, const int mb_col,
                                     const int strength, const int use_subblock,
                                     const int *subblock_filter_weights,
                                     const uint8_t *pred, uint32_t *accum,
                                     uint16_t *count) {
  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  // TODO(yjshen): Not sure if this is equivalent to is_cur_buf_hbd(mbd).
  const int is_high_bitdepth = (mbd->bd > 8);
  const uint16_t *pred16 = CONVERT_TO_SHORTPTR(pred);

  // Allocate memory for pixel-wise squared differences for Y, U, V planes. All
  // planes, regardless of the subsampling, are assigned with memory of size
  // `mb_pels`.
  uint32_t *square_diff =
      aom_memalign(16, MAX_MB_PLANE * mb_pels * sizeof(uint32_t));
  memset(square_diff, 0, MAX_MB_PLANE * mb_pels * sizeof(uint32_t));

  int plane_offset = 0;
  for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
    // Locate pixel on reference frame.
    const int plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const int plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const int frame_stride = ref_frame->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;
    const uint8_t *ref = ref_frame->buffers[plane];
    compute_square_diff(ref, frame_offset, frame_stride, pred, plane_offset,
                        plane_w, plane_h, plane_w, is_high_bitdepth,
                        square_diff + plane_offset);
    plane_offset += mb_pels;
  }

  // Get window size for pixel-wise filtering.
  assert(YUV_FILTER_WINDOW_LENGTH % 2 == 1);
  const int half_window = YUV_FILTER_WINDOW_LENGTH >> 1;

  // Handle Y-plane, U-plane, V-plane in sequence.
  plane_offset = 0;
  for (int plane = 0; plane < MAX_MB_PLANE; ++plane) {
    const int subsampling_y = mbd->plane[plane].subsampling_y;
    const int subsampling_x = mbd->plane[plane].subsampling_x;
    // Only 0 and 1 are supported for filter weight adjustment.
    assert(subsampling_y == 0 || subsampling_y == 1);
    assert(subsampling_x == 0 || subsampling_x == 1);
    const int h = mb_height >> subsampling_y;  // Plane height.
    const int w = mb_width >> subsampling_x;   // Plane width.

    // Perform filtering.
    int pred_idx = 0;
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        const int subblock_idx =
            use_subblock ? (i >= h / 2) * 2 + (j >= w / 2) : 0;
        const int filter_weight = subblock_filter_weights[subblock_idx];

        // non-local mean approach
        uint64_t sum_square_diff = 0;
        int num_ref_pixels = 0;

        for (int wi = -half_window; wi <= half_window; ++wi) {
          for (int wj = -half_window; wj <= half_window; ++wj) {
            const int y = i + wi;  // Y-coord on the current plane.
            const int x = j + wj;  // X-coord on the current plane.
            if (y >= 0 && y < h && x >= 0 && x < w) {
              sum_square_diff += square_diff[plane_offset + y * w + x];
              ++num_ref_pixels;
            }
          }
        }

        if (plane == 0) {  // Filter Y-plane using both U-plane and V-plane.
          for (int p = 1; p < MAX_MB_PLANE; ++p) {
            const int ss_y_shift = mbd->plane[p].subsampling_y - subsampling_y;
            const int ss_x_shift = mbd->plane[p].subsampling_x - subsampling_x;
            const int yy = i >> ss_y_shift;  // Y-coord on UV-plane.
            const int xx = j >> ss_x_shift;  // X-coord on UV-plane.
            const int ww = w >> ss_x_shift;  // Width of UV-plane.
            sum_square_diff += square_diff[p * mb_pels + yy * ww + xx];
            ++num_ref_pixels;
          }
        } else {  // Filter U-plane and V-plane using Y-plane.
          const int ss_y_shift = subsampling_y - mbd->plane[0].subsampling_y;
          const int ss_x_shift = subsampling_x - mbd->plane[0].subsampling_x;
          for (int ii = 0; ii < (1 << ss_y_shift); ++ii) {
            for (int jj = 0; jj < (1 << ss_x_shift); ++jj) {
              const int yy = (i << ss_y_shift) + ii;  // Y-coord on Y-plane.
              const int xx = (j << ss_x_shift) + jj;  // X-coord on Y-plane.
              const int ww = w << ss_x_shift;         // Width of Y-plane.
              sum_square_diff += square_diff[yy * ww + xx];
              ++num_ref_pixels;
            }
          }
        }

        const int adjusted_weight = adjust_filter_weight_yuv(
            filter_weight, sum_square_diff, num_ref_pixels, strength,
            is_high_bitdepth);

        const int pred_value = is_high_bitdepth
                                   ? pred16[plane_offset + pred_idx]
                                   : pred[plane_offset + pred_idx];
        accum[plane_offset + pred_idx] += adjusted_weight * pred_value;
        count[plane_offset + pred_idx] += adjusted_weight;

        ++pred_idx;
      }
    }
    plane_offset += mb_pels;
  }

  aom_free(square_diff);
}

// Applies temporal filter to YUV planes (high bit-depth video).
// NOTE: This function is now merged to `av1_apply_temporal_filter_yuv_c()`.
void av1_highbd_apply_temporal_filter_yuv_c(
    const YV12_BUFFER_CONFIG *ref_frame, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int strength, const int use_subblock,
    const int *subblock_filter_weights, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  av1_apply_temporal_filter_yuv_c(ref_frame, mbd, block_size, mb_row, mb_col,
                                  strength, use_subblock,
                                  subblock_filter_weights, pred, accum, count);
}

// Only used in single plane case
void av1_temporal_filter_apply_c(uint8_t *frame1, unsigned int stride,
                                 uint8_t *frame2, unsigned int block_width,
                                 unsigned int block_height, int strength,
                                 const int *blk_fw, int use_32x32,
                                 unsigned int *accumulator, uint16_t *count) {
  unsigned int i, j, k;
  int modifier;
  int byte = 0;
  const int rounding = strength > 0 ? 1 << (strength - 1) : 0;

  for (i = 0, k = 0; i < block_height; i++) {
    for (j = 0; j < block_width; j++, k++) {
      int pixel_value = *frame2;
      const int subblock_idx =
          use_32x32 ? 0 : (i >= block_height / 2) * 2 + (j >= block_width / 2);
      const int filter_weight = blk_fw[subblock_idx];

      // non-local mean approach
      int diff_sse[9] = { 0 };
      int idx, idy, index = 0;

      for (idy = -1; idy <= 1; ++idy) {
        for (idx = -1; idx <= 1; ++idx) {
          int row = (int)i + idy;
          int col = (int)j + idx;

          if (row >= 0 && row < (int)block_height && col >= 0 &&
              col < (int)block_width) {
            int diff = frame1[byte + idy * (int)stride + idx] -
                       frame2[idy * (int)block_width + idx];
            diff_sse[index] = diff * diff;
            ++index;
          }
        }
      }

      assert(index > 0);

      modifier = 0;
      for (idx = 0; idx < 9; ++idx) modifier += diff_sse[idx];

      modifier *= 3;
      modifier /= index;

      ++frame2;

      modifier += rounding;
      modifier >>= strength;

      if (modifier > 16) modifier = 16;

      modifier = 16 - modifier;
      modifier *= filter_weight;

      count[k] += modifier;
      accumulator[k] += modifier * pixel_value;

      byte++;
    }

    byte += stride - block_width;
  }
}

// Only used in single plane case
void av1_highbd_temporal_filter_apply_c(
    uint8_t *frame1_8, unsigned int stride, uint8_t *frame2_8,
    unsigned int block_width, unsigned int block_height, int strength,
    int *blk_fw, int use_32x32, unsigned int *accumulator, uint16_t *count) {
  uint16_t *frame1 = CONVERT_TO_SHORTPTR(frame1_8);
  uint16_t *frame2 = CONVERT_TO_SHORTPTR(frame2_8);
  unsigned int i, j, k;
  int modifier;
  int byte = 0;
  const int rounding = strength > 0 ? 1 << (strength - 1) : 0;

  for (i = 0, k = 0; i < block_height; i++) {
    for (j = 0; j < block_width; j++, k++) {
      int pixel_value = *frame2;
      const int subblock_idx =
          use_32x32 ? 0 : (i >= block_height / 2) * 2 + (j >= block_width / 2);
      const int filter_weight = blk_fw[subblock_idx];

      // non-local mean approach
      int diff_sse[9] = { 0 };
      int idx, idy, index = 0;

      for (idy = -1; idy <= 1; ++idy) {
        for (idx = -1; idx <= 1; ++idx) {
          int row = (int)i + idy;
          int col = (int)j + idx;

          if (row >= 0 && row < (int)block_height && col >= 0 &&
              col < (int)block_width) {
            int diff = frame1[byte + idy * (int)stride + idx] -
                       frame2[idy * (int)block_width + idx];
            diff_sse[index] = diff * diff;
            ++index;
          }
        }
      }

      assert(index > 0);

      modifier = 0;
      for (idx = 0; idx < 9; ++idx) modifier += diff_sse[idx];

      modifier *= 3;
      modifier /= index;

      ++frame2;

      modifier += rounding;
      modifier >>= strength;

      if (modifier > 16) modifier = 16;

      modifier = 16 - modifier;
      modifier *= filter_weight;

      count[k] += modifier;
      accumulator[k] += modifier * pixel_value;

      byte++;
    }

    byte += stride - block_width;
  }
}

#if EXPERIMENT_TEMPORAL_FILTER
void av1_temporal_filter_plane_c(uint8_t *frame1, unsigned int stride,
                                 uint8_t *frame2, unsigned int stride2,
                                 int block_width, int block_height,
                                 int strength, double sigma, int decay_control,
                                 const int *blk_fw, int use_32x32,
                                 unsigned int *accumulator, uint16_t *count) {
  (void)strength;
  (void)blk_fw;
  (void)use_32x32;
  const double h = decay_control * (0.7 + log(sigma + 0.5));
  const double beta = 1.0;
  for (int i = 0, k = 0; i < block_height; i++) {
    for (int j = 0; j < block_width; j++, k++) {
      const int pixel_value = frame2[i * stride2 + j];

      int diff_sse = 0;
      for (int idy = -WINDOW_LENGTH; idy <= WINDOW_LENGTH; ++idy) {
        for (int idx = -WINDOW_LENGTH; idx <= WINDOW_LENGTH; ++idx) {
          int row = i + idy;
          int col = j + idx;
          if (row < 0) row = 0;
          if (row >= block_height) row = block_height - 1;
          if (col < 0) col = 0;
          if (col >= block_width) col = block_width - 1;

          int diff = frame1[row * (int)stride + col] -
                     frame2[row * (int)stride2 + col];
          diff_sse += diff * diff;
        }
      }
      diff_sse /= WINDOW_SIZE;

      double scaled_diff = -diff_sse / (2 * beta * h * h);
      // clamp the value to avoid underflow in exp()
      if (scaled_diff < -15) scaled_diff = -15;
      double w = exp(scaled_diff);
      const int weight = (int)(w * SCALE);

      count[k] += weight;
      accumulator[k] += weight * pixel_value;
    }
  }
}

void av1_highbd_temporal_filter_plane_c(
    uint8_t *frame1_8bit, unsigned int stride, uint8_t *frame2_8bit,
    unsigned int stride2, int block_width, int block_height, int strength,
    double sigma, int decay_control, const int *blk_fw, int use_32x32,
    unsigned int *accumulator, uint16_t *count) {
  (void)strength;
  (void)blk_fw;
  (void)use_32x32;
  uint16_t *frame1 = CONVERT_TO_SHORTPTR(frame1_8bit);
  uint16_t *frame2 = CONVERT_TO_SHORTPTR(frame2_8bit);
  const double h = decay_control * (0.7 + log(sigma + 0.5));
  const double beta = 1.0;
  for (int i = 0, k = 0; i < block_height; i++) {
    for (int j = 0; j < block_width; j++, k++) {
      const int pixel_value = frame2[i * stride2 + j];

      int diff_sse = 0;
      for (int idy = -WINDOW_LENGTH; idy <= WINDOW_LENGTH; ++idy) {
        for (int idx = -WINDOW_LENGTH; idx <= WINDOW_LENGTH; ++idx) {
          int row = i + idy;
          int col = j + idx;
          if (row < 0) row = 0;
          if (row >= block_height) row = block_height - 1;
          if (col < 0) col = 0;
          if (col >= block_width) col = block_width - 1;

          int diff = frame1[row * (int)stride + col] -
                     frame2[row * (int)stride2 + col];
          diff_sse += diff * diff;
        }
      }
      diff_sse /= WINDOW_SIZE;

      double scaled_diff = -diff_sse / (2 * beta * h * h);
      // clamp the value to avoid underflow in exp()
      if (scaled_diff < -20) scaled_diff = -20;
      double w = exp(scaled_diff);
      const int weight = (int)(w * SCALE);

      count[k] += weight;
      accumulator[k] += weight * pixel_value;
    }
  }
}

void apply_temporal_filter_block(
    YV12_BUFFER_CONFIG *frame, MACROBLOCKD *mbd, int mb_y_src_offset,
    int mb_uv_src_offset, int mb_uv_width, int mb_uv_height, int num_planes,
    uint8_t *predictor, int frame_height, int strength, double sigma,
    int *blk_fw, int use_32x32, unsigned int *accumulator, uint16_t *count,
    int use_new_temporal_mode, int mb_row, int mb_col) {
  const int is_hbd = is_cur_buf_hbd(mbd);
  // High bitdepth
  if (is_hbd) {
    if (use_new_temporal_mode) {
      // Apply frame size dependent non-local means filtering.
      int decay_control;
      // The decay is obtained empirically, subject to better tuning.
      if (frame_height >= 720) {
        decay_control = 4;
      } else if (frame_height >= 480) {
        decay_control = 4;
      } else {
        decay_control = 3;
      }
      av1_highbd_temporal_filter_plane_c(frame->y_buffer + mb_y_src_offset,
                                         frame->y_stride, predictor, BW, BW, BH,
                                         strength, sigma, decay_control, blk_fw,
                                         use_32x32, accumulator, count);
      if (num_planes > 1) {
        av1_highbd_temporal_filter_plane_c(
            frame->u_buffer + mb_uv_src_offset, frame->uv_stride,
            predictor + BLK_PELS, mb_uv_width, mb_uv_width, mb_uv_height,
            strength, sigma, decay_control, blk_fw, use_32x32,
            accumulator + BLK_PELS, count + BLK_PELS);
        av1_highbd_temporal_filter_plane_c(
            frame->v_buffer + mb_uv_src_offset, frame->uv_stride,
            predictor + (BLK_PELS << 1), mb_uv_width, mb_uv_width, mb_uv_height,
            strength, sigma, decay_control, blk_fw, use_32x32,
            accumulator + (BLK_PELS << 1), count + (BLK_PELS << 1));
      }
    } else {
      // Apply original non-local means filtering for small resolution
      const int adj_strength = strength + 2 * (mbd->bd - 8);
      if (num_planes <= 1) {
        // Single plane case
        av1_highbd_temporal_filter_apply_c(
            frame->y_buffer + mb_y_src_offset, frame->y_stride, predictor, BW,
            BH, adj_strength, blk_fw, use_32x32, accumulator, count);
      } else {
        // Process 3 planes together.
        av1_highbd_apply_temporal_filter_yuv(
            frame, mbd, TF_BLOCK, mb_row, mb_col, adj_strength, !(use_32x32),
            blk_fw, predictor, accumulator, count);
      }
    }
    return;
  }

  // Low bitdepth
  if (use_new_temporal_mode) {
    // Apply frame size dependent non-local means filtering.
    int decay_control;
    // The decay is obtained empirically, subject to better tuning.
    if (frame_height >= 720) {
      decay_control = 4;
    } else if (frame_height >= 480) {
      decay_control = 4;
    } else {
      decay_control = 3;
    }
    av1_temporal_filter_plane(frame->y_buffer + mb_y_src_offset,
                              frame->y_stride, predictor, BW, BW, BH, strength,
                              sigma, decay_control, blk_fw, use_32x32,
                              accumulator, count);
    if (num_planes > 1) {
      av1_temporal_filter_plane(
          frame->u_buffer + mb_uv_src_offset, frame->uv_stride,
          predictor + BLK_PELS, mb_uv_width, mb_uv_width, mb_uv_height,
          strength, sigma, decay_control, blk_fw, use_32x32,
          accumulator + BLK_PELS, count + BLK_PELS);
      av1_temporal_filter_plane(
          frame->v_buffer + mb_uv_src_offset, frame->uv_stride,
          predictor + (BLK_PELS << 1), mb_uv_width, mb_uv_width, mb_uv_height,
          strength, sigma, decay_control, blk_fw, use_32x32,
          accumulator + (BLK_PELS << 1), count + (BLK_PELS << 1));
    }
  } else {
    // Apply original non-local means filtering for small resolution
    if (num_planes <= 1) {
      // Single plane case
      av1_temporal_filter_apply_c(frame->y_buffer + mb_y_src_offset,
                                  frame->y_stride, predictor, BW, BH, strength,
                                  blk_fw, use_32x32, accumulator, count);
    } else {
      // Process 3 planes together.
      av1_apply_temporal_filter_yuv(frame, mbd, TF_BLOCK, mb_row, mb_col,
                                    strength, !(use_32x32), blk_fw, predictor,
                                    accumulator, count);
    }
  }
}
#endif  // EXPERIMENT_TEMPORAL_FILTER

static int temporal_filter_find_matching_mb_c(
    AV1_COMP *cpi, uint8_t *arf_frame_buf, uint8_t *frame_ptr_buf, int stride,
    int x_pos, int y_pos, MV *blk_mvs, int *blk_bestsme, MV *best_ref_mv1,
    int step_param) {
  MACROBLOCK *const x = &cpi->td.mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MV_SPEED_FEATURES *const mv_sf = &cpi->sf.mv_sf;
  int sadpb = x->sadperbit16;
  int bestsme = INT_MAX;
  int distortion;
  unsigned int sse;
  int cost_list[5];
  MvLimits tmp_mv_limits = x->mv_limits;
  MV best_ref_mv1_full; /* full-pixel value of best_ref_mv1 */
  MV ref_mv = kZeroMv;
  // Save input state
  struct buf_2d src = x->plane[0].src;
  struct buf_2d pre = xd->plane[0].pre[0];
  best_ref_mv1_full.col = best_ref_mv1->col >> 3;
  best_ref_mv1_full.row = best_ref_mv1->row >> 3;

  // Setup frame pointers
  x->plane[0].src.buf = arf_frame_buf;
  x->plane[0].src.stride = stride;
  xd->plane[0].pre[0].buf = frame_ptr_buf;
  xd->plane[0].pre[0].stride = stride;

  av1_set_mv_search_range(&x->mv_limits, &ref_mv);

  // av1_full_pixel_search() parameters: best_ref_mv1_full is the start mv, and
  // ref_mv is for mv rate calculation. The search result is stored in
  // x->best_mv.
  av1_full_pixel_search(cpi, x, TF_BLOCK, &best_ref_mv1_full, step_param, 1,
                        NSTEP, 1, sadpb, cond_cost_list(cpi, cost_list),
                        &ref_mv, 0, 0, x_pos, y_pos, 0,
                        &cpi->ss_cfg[SS_CFG_LOOKAHEAD], 0);
  x->mv_limits = tmp_mv_limits;

  // Ignore mv costing by sending NULL pointer instead of cost array
  if (cpi->common.cur_frame_force_integer_mv == 1) {
    const uint8_t *const src_address = x->plane[0].src.buf;
    const int src_stride = x->plane[0].src.stride;
    const uint8_t *const y = xd->plane[0].pre[0].buf;
    const int y_stride = xd->plane[0].pre[0].stride;
    const int offset = x->best_mv.as_mv.row * y_stride + x->best_mv.as_mv.col;

    x->best_mv.as_mv.row *= 8;
    x->best_mv.as_mv.col *= 8;

    bestsme = cpi->fn_ptr[TF_BLOCK].vf(y + offset, y_stride, src_address,
                                       src_stride, &sse);

    x->e_mbd.mi[0]->mv[0] = x->best_mv;

    // Restore input state
    x->plane[0].src = src;
    xd->plane[0].pre[0] = pre;

    return bestsme;
  }

  // find_fractional_mv_step parameters: ref_mv is for mv rate cost
  // calculation. The start full mv and the search result are stored in
  // x->best_mv. mi_row and mi_col are only needed for "av1_is_scaled(sf)=1"
  // case.
  bestsme = cpi->find_fractional_mv_step(
      x, &cpi->common, 0, 0, &ref_mv, cpi->common.allow_high_precision_mv,
      x->errorperbit, &cpi->fn_ptr[TF_BLOCK], 0, mv_sf->subpel_iters_per_step,
      cond_cost_list(cpi, cost_list), NULL, NULL, &distortion, &sse, NULL, NULL,
      0, 0, BW, BH, USE_8_TAPS, 1);

  x->e_mbd.mi[0]->mv[0] = x->best_mv;

  // DO motion search on 4 16x16 sub_blocks.
  int i, j, k = 0;
  best_ref_mv1->row = x->e_mbd.mi[0]->mv[0].as_mv.row;
  best_ref_mv1->col = x->e_mbd.mi[0]->mv[0].as_mv.col;
  best_ref_mv1_full.col = best_ref_mv1->col >> 3;
  best_ref_mv1_full.row = best_ref_mv1->row >> 3;

  for (i = 0; i < BH; i += SUB_BH) {
    for (j = 0; j < BW; j += SUB_BW) {
      // Setup frame pointers
      x->plane[0].src.buf = arf_frame_buf + i * stride + j;
      x->plane[0].src.stride = stride;
      xd->plane[0].pre[0].buf = frame_ptr_buf + i * stride + j;
      xd->plane[0].pre[0].stride = stride;

      av1_set_mv_search_range(&x->mv_limits, &ref_mv);
      av1_full_pixel_search(cpi, x, TF_SUB_BLOCK, &best_ref_mv1_full,
                            step_param, 1, NSTEP, 1, sadpb,
                            cond_cost_list(cpi, cost_list), &ref_mv, 0, 0,
                            x_pos, y_pos, 0, &cpi->ss_cfg[SS_CFG_LOOKAHEAD], 0);
      x->mv_limits = tmp_mv_limits;

      blk_bestsme[k] = cpi->find_fractional_mv_step(
          x, &cpi->common, 0, 0, &ref_mv, cpi->common.allow_high_precision_mv,
          x->errorperbit, &cpi->fn_ptr[TF_SUB_BLOCK], 0,
          mv_sf->subpel_iters_per_step, cond_cost_list(cpi, cost_list), NULL,
          NULL, &distortion, &sse, NULL, NULL, 0, 0, SUB_BW, SUB_BH, USE_8_TAPS,
          1);

      blk_mvs[k] = x->best_mv.as_mv;
      k++;
    }
  }

  // Restore input state
  x->plane[0].src = src;
  xd->plane[0].pre[0] = pre;

  return bestsme;
}

static int get_rows(int h) { return (h + BH - 1) >> BH_LOG2; }
static int get_cols(int w) { return (w + BW - 1) >> BW_LOG2; }

typedef struct {
  int64_t sum;
  int64_t sse;
} FRAME_DIFF;

static FRAME_DIFF temporal_filter_iterate_c(
    AV1_COMP *cpi, YV12_BUFFER_CONFIG **frames, int frame_count,
    int alt_ref_index, int strength, double sigma, int is_key_frame,
    struct scale_factors *ref_scale_factors, int second_alt_ref) {
  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int mb_cols = get_cols(frames[alt_ref_index]->y_crop_width);
  const int mb_rows = get_rows(frames[alt_ref_index]->y_crop_height);
  // TODO(any): the thresholds in this function need to adjusted based on bit_
  // depth, so that they work better in HBD encoding.
  const int bd_shift = cm->seq_params.bit_depth - 8;
  int byte;
  int frame;
  int mb_col, mb_row;
  int mb_y_offset = 0;
  int mb_y_src_offset = 0;
  int mb_uv_offset = 0;
  int mb_uv_src_offset = 0;
  DECLARE_ALIGNED(16, unsigned int, accumulator[BLK_PELS * 3]);
  DECLARE_ALIGNED(16, uint16_t, count[BLK_PELS * 3]);
  MACROBLOCKD *mbd = &cpi->td.mb.e_mbd;
  YV12_BUFFER_CONFIG *f = frames[alt_ref_index];
  uint8_t *dst1, *dst2;
  DECLARE_ALIGNED(32, uint16_t, predictor16[BLK_PELS * 3]);
  DECLARE_ALIGNED(32, uint8_t, predictor8[BLK_PELS * 3]);
  memset(predictor16, 0, BLK_PELS * 3 * sizeof(predictor16[0]));
  memset(predictor8, 0, BLK_PELS * 3 * sizeof(predictor8[0]));
  uint8_t *predictor;
  const int mb_uv_height = BH >> mbd->plane[1].subsampling_y;
  const int mb_uv_width = BW >> mbd->plane[1].subsampling_x;
#if EXPERIMENT_TEMPORAL_FILTER
  const int is_screen_content_type = cm->allow_screen_content_tools != 0;
  const int use_new_temporal_mode =
      !is_screen_content_type && AOMMIN(cm->width, cm->height) >= 480;
#else
  (void)sigma;
  const int use_new_temporal_mode = 0;
#endif

  // Save input state
  uint8_t *input_buffer[MAX_MB_PLANE];
  int i;
  const int is_hbd = is_cur_buf_hbd(mbd);
  if (is_hbd) {
    predictor = CONVERT_TO_BYTEPTR(predictor16);
  } else {
    predictor = predictor8;
  }

  const unsigned int dim = AOMMIN(frames[alt_ref_index]->y_crop_width,
                                  frames[alt_ref_index]->y_crop_height);
  // Decide search param based on image resolution.
  const int step_param = av1_init_search_range(dim);

  mbd->block_ref_scale_factors[0] = ref_scale_factors;
  mbd->block_ref_scale_factors[1] = ref_scale_factors;

  for (i = 0; i < num_planes; i++) input_buffer[i] = mbd->plane[i].pre[0].buf;

  // Make a temporary mbmi for temporal filtering
  MB_MODE_INFO **backup_mi_grid = mbd->mi;
  MB_MODE_INFO mbmi;
  memset(&mbmi, 0, sizeof(mbmi));
  MB_MODE_INFO *mbmi_ptr = &mbmi;
  mbd->mi = &mbmi_ptr;

  FRAME_DIFF diff = { 0, 0 };

  for (mb_row = 0; mb_row < mb_rows; mb_row++) {
    // Source frames are extended to 16 pixels. This is different than
    //  L/A/G reference frames that have a border of 32 (AV1ENCBORDERINPIXELS)
    // A 6/8 tap filter is used for motion search.  This requires 2 pixels
    //  before and 3 pixels after.  So the largest Y mv on a border would
    //  then be 16 - AOM_INTERP_EXTEND. The UV blocks are half the size of the
    //  Y and therefore only extended by 8.  The largest mv that a UV block
    //  can support is 8 - AOM_INTERP_EXTEND.  A UV mv is half of a Y mv.
    //  (16 - AOM_INTERP_EXTEND) >> 1 which is greater than
    //  8 - AOM_INTERP_EXTEND.
    // To keep the mv in play for both Y and UV planes the max that it
    //  can be on a border is therefore 16 - (2*AOM_INTERP_EXTEND+1).
    cpi->td.mb.mv_limits.row_min =
        -((mb_row * BH) + (17 - 2 * AOM_INTERP_EXTEND));
    cpi->td.mb.mv_limits.row_max =
        ((mb_rows - 1 - mb_row) * BH) + (17 - 2 * AOM_INTERP_EXTEND);

    for (mb_col = 0; mb_col < mb_cols; mb_col++) {
      int j, k;
      int stride;
      MV best_ref_mv1 = kZeroMv;

      memset(accumulator, 0, BLK_PELS * 3 * sizeof(accumulator[0]));
      memset(count, 0, BLK_PELS * 3 * sizeof(count[0]));

      cpi->td.mb.mv_limits.col_min =
          -((mb_col * BW) + (17 - 2 * AOM_INTERP_EXTEND));
      cpi->td.mb.mv_limits.col_max =
          ((mb_cols - 1 - mb_col) * BW) + (17 - 2 * AOM_INTERP_EXTEND);

      for (frame = 0; frame < frame_count; frame++) {
        // MVs for 4 16x16 sub blocks.
        MV blk_mvs[4];
        // Filter weights for 4 16x16 sub blocks.
        int blk_fw[4] = { 0, 0, 0, 0 };
        int use_32x32 = 0;

        if (frames[frame] == NULL) continue;

        mbd->mi[0]->mv[0].as_mv.row = 0;
        mbd->mi[0]->mv[0].as_mv.col = 0;
        mbd->mi[0]->motion_mode = SIMPLE_TRANSLATION;
        blk_mvs[0] = kZeroMv;
        blk_mvs[1] = kZeroMv;
        blk_mvs[2] = kZeroMv;
        blk_mvs[3] = kZeroMv;

        if (frame == alt_ref_index) {
          const int weight = second_alt_ref ? 4 : 2;
          blk_fw[0] = blk_fw[1] = blk_fw[2] = blk_fw[3] = weight;
          use_32x32 = 1;
          // Change ref_mv sign for following frames.
          best_ref_mv1.row *= -1;
          best_ref_mv1.col *= -1;
        } else {
          int thresh_low = 10000 >> second_alt_ref;
          int thresh_high = 20000 >> second_alt_ref;
          int blk_bestsme[4] = { INT_MAX, INT_MAX, INT_MAX, INT_MAX };

          // Find best match in this frame by MC
          int err = temporal_filter_find_matching_mb_c(
              cpi, frames[alt_ref_index]->y_buffer + mb_y_src_offset,
              frames[frame]->y_buffer + mb_y_src_offset,
              frames[frame]->y_stride, mb_col * BW, mb_row * BH, blk_mvs,
              blk_bestsme, &best_ref_mv1, step_param);

          int err16 =
              blk_bestsme[0] + blk_bestsme[1] + blk_bestsme[2] + blk_bestsme[3];
          int max_err = INT_MIN, min_err = INT_MAX;
          for (k = 0; k < 4; k++) {
            if (min_err > blk_bestsme[k]) min_err = blk_bestsme[k];
            if (max_err < blk_bestsme[k]) max_err = blk_bestsme[k];
          }

          if (((err * 15 < (err16 << 4)) && max_err - min_err < 12000) ||
              ((err * 14 < (err16 << 4)) && max_err - min_err < 6000)) {
            use_32x32 = 1;
            // Assign higher weight to matching MB if it's error
            // score is lower. If not applying MC default behavior
            // is to weight all MBs equal.
            blk_fw[0] = err < (thresh_low << THR_SHIFT)
                            ? 2
                            : err < (thresh_high << THR_SHIFT) ? 1 : 0;
            blk_fw[1] = blk_fw[2] = blk_fw[3] = blk_fw[0];
          } else {
            use_32x32 = 0;
            for (k = 0; k < 4; k++)
              blk_fw[k] = blk_bestsme[k] < thresh_low
                              ? 2
                              : blk_bestsme[k] < thresh_high ? 1 : 0;
          }

          // Don't use previous frame's mv result if error is large.
          if (err > (3000 << bd_shift)) best_ref_mv1 = kZeroMv;
        }

        if (blk_fw[0] || blk_fw[1] || blk_fw[2] || blk_fw[3]) {
          // Construct the predictors
          tf_build_predictors_mb_c(frames[frame], mbd, TF_BLOCK, mb_row, mb_col,
                                   ref_scale_factors, num_planes, !(use_32x32),
                                   blk_mvs, predictor);

          // Apply the filter (YUV)
          if (frame == alt_ref_index) {
            uint8_t *pred = predictor;
            uint32_t *accum = accumulator;
            uint16_t *cnt = count;
            int plane;

            // All 4 blk_fws are equal to 2.
            for (plane = 0; plane < num_planes; ++plane) {
              const int pred_stride = plane ? mb_uv_width : BW;
              const unsigned int w = plane ? mb_uv_width : BW;
              const unsigned int h = plane ? mb_uv_height : BH;

              if (is_hbd) {
                highbd_apply_temporal_filter_self(pred, pred_stride, w, h,
                                                  blk_fw[0], accum, cnt,
                                                  use_new_temporal_mode);
              } else {
                apply_temporal_filter_self(pred, pred_stride, w, h, blk_fw[0],
                                           accum, cnt, use_new_temporal_mode);
              }

              pred += BLK_PELS;
              accum += BLK_PELS;
              cnt += BLK_PELS;
            }
          } else {
            if (is_hbd) {
#if EXPERIMENT_TEMPORAL_FILTER
              apply_temporal_filter_block(
                  f, mbd, mb_y_src_offset, mb_uv_src_offset, mb_uv_width,
                  mb_uv_height, num_planes, predictor, cm->height, strength,
                  sigma, blk_fw, use_32x32, accumulator, count,
                  use_new_temporal_mode, mb_row, mb_col);
#else
              const int adj_strength = strength + 2 * (mbd->bd - 8);
              if (num_planes <= 1) {
                // Single plane case
                av1_highbd_temporal_filter_apply_c(
                    f->y_buffer + mb_y_src_offset, f->y_stride, predictor, BW,
                    BH, adj_strength, blk_fw, use_32x32, accumulator, count);
              } else {
                // Process 3 planes together.
                av1_highbd_apply_temporal_filter_yuv(
                    frame, mbd, TF_BLOCK, mb_row, mb_col, adj_strength,
                    !(use_32x32), blk_fw, predictor, accumulator, count);
              }
#endif  // EXPERIMENT_TEMPORAL_FILTER
            } else {
#if EXPERIMENT_TEMPORAL_FILTER
              apply_temporal_filter_block(
                  f, mbd, mb_y_src_offset, mb_uv_src_offset, mb_uv_width,
                  mb_uv_height, num_planes, predictor, cm->height, strength,
                  sigma, blk_fw, use_32x32, accumulator, count,
                  use_new_temporal_mode, mb_row, mb_col);
#else
              if (num_planes <= 1) {
                // Single plane case
                av1_temporal_filter_apply_c(
                    f->y_buffer + mb_y_src_offset, f->y_stride, predictor, BW,
                    BH, strength, blk_fw, use_32x32, accumulator, count);
              } else {
                // Process 3 planes together.
                av1_apply_temporal_filter_yuv(
                    frame, mbd, TF_BLOCK, mb_row, mb_col, strength,
                    !(use_32x32), blk_fw, predictor, accumulator, count);
              }
#endif  // EXPERIMENT_TEMPORAL_FILTER
            }
          }
        }
      }

      // Normalize filter output to produce AltRef frame
      if (is_hbd) {
        uint16_t *dst1_16;
        uint16_t *dst2_16;
        dst1 = cpi->alt_ref_buffer.y_buffer;
        dst1_16 = CONVERT_TO_SHORTPTR(dst1);
        stride = cpi->alt_ref_buffer.y_stride;
        byte = mb_y_offset;
        for (i = 0, k = 0; i < BH; i++) {
          for (j = 0; j < BW; j++, k++) {
            dst1_16[byte] =
                (uint16_t)OD_DIVU(accumulator[k] + (count[k] >> 1), count[k]);

            // move to next pixel
            byte++;
          }

          byte += stride - BW;
        }
        if (num_planes > 1) {
          dst1 = cpi->alt_ref_buffer.u_buffer;
          dst2 = cpi->alt_ref_buffer.v_buffer;
          dst1_16 = CONVERT_TO_SHORTPTR(dst1);
          dst2_16 = CONVERT_TO_SHORTPTR(dst2);
          stride = cpi->alt_ref_buffer.uv_stride;
          byte = mb_uv_offset;
          for (i = 0, k = BLK_PELS; i < mb_uv_height; i++) {
            for (j = 0; j < mb_uv_width; j++, k++) {
              int m = k + BLK_PELS;
              // U
              dst1_16[byte] =
                  (uint16_t)OD_DIVU(accumulator[k] + (count[k] >> 1), count[k]);
              // V
              dst2_16[byte] =
                  (uint16_t)OD_DIVU(accumulator[m] + (count[m] >> 1), count[m]);
              // move to next pixel
              byte++;
            }
            byte += stride - mb_uv_width;
          }
        }
      } else {
        dst1 = cpi->alt_ref_buffer.y_buffer;
        stride = cpi->alt_ref_buffer.y_stride;
        byte = mb_y_offset;
        for (i = 0, k = 0; i < BH; i++) {
          for (j = 0; j < BW; j++, k++) {
            dst1[byte] =
                (uint8_t)OD_DIVU(accumulator[k] + (count[k] >> 1), count[k]);

            // move to next pixel
            byte++;
          }
          byte += stride - BW;
        }
        if (num_planes > 1) {
          dst1 = cpi->alt_ref_buffer.u_buffer;
          dst2 = cpi->alt_ref_buffer.v_buffer;
          stride = cpi->alt_ref_buffer.uv_stride;
          byte = mb_uv_offset;
          for (i = 0, k = BLK_PELS; i < mb_uv_height; i++) {
            for (j = 0; j < mb_uv_width; j++, k++) {
              int m = k + BLK_PELS;
              // U
              dst1[byte] =
                  (uint8_t)OD_DIVU(accumulator[k] + (count[k] >> 1), count[k]);
              // V
              dst2[byte] =
                  (uint8_t)OD_DIVU(accumulator[m] + (count[m] >> 1), count[m]);
              // move to next pixel
              byte++;
            }
            byte += stride - mb_uv_width;
          }
        }
      }

      if (!is_key_frame && cpi->sf.hl_sf.adaptive_overlay_encoding) {
        // Calculate the difference(dist) between source and filtered source.
        dst1 = cpi->alt_ref_buffer.y_buffer + mb_y_offset;
        stride = cpi->alt_ref_buffer.y_stride;
        const uint8_t *src = f->y_buffer + mb_y_src_offset;
        const int src_stride = f->y_stride;
        const BLOCK_SIZE bsize = dims_to_size(BW, BH);
        unsigned int sse = 0;
        cpi->fn_ptr[bsize].vf(src, src_stride, dst1, stride, &sse);

        diff.sum += sse;
        diff.sse += sse * sse;
      }

      mb_y_offset += BW;
      mb_y_src_offset += BW;
      mb_uv_offset += mb_uv_width;
      mb_uv_src_offset += mb_uv_width;
    }
    mb_y_offset += BH * cpi->alt_ref_buffer.y_stride - BW * mb_cols;
    mb_y_src_offset += BH * f->y_stride - BW * mb_cols;
    mb_uv_src_offset += mb_uv_height * f->uv_stride - mb_uv_width * mb_cols;
    mb_uv_offset +=
        mb_uv_height * cpi->alt_ref_buffer.uv_stride - mb_uv_width * mb_cols;
  }

  // Restore input state
  for (i = 0; i < num_planes; i++) mbd->plane[i].pre[0].buf = input_buffer[i];

  mbd->mi = backup_mi_grid;
  return diff;
}

// This is an adaptation of the mehtod in the following paper:
// Shen-Chuan Tai, Shih-Ming Yang, "A fast method for image noise
// estimation using Laplacian operator and adaptive edge detection,"
// Proc. 3rd International Symposium on Communications, Control and
// Signal Processing, 2008, St Julians, Malta.
//
// Return noise estimate, or -1.0 if there was a failure
double estimate_noise(const uint8_t *src, int width, int height, int stride,
                      int edge_thresh) {
  int64_t sum = 0;
  int64_t num = 0;
  for (int i = 1; i < height - 1; ++i) {
    for (int j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      // Sobel gradients
      const int Gx = (src[k - stride - 1] - src[k - stride + 1]) +
                     (src[k + stride - 1] - src[k + stride + 1]) +
                     2 * (src[k - 1] - src[k + 1]);
      const int Gy = (src[k - stride - 1] - src[k + stride - 1]) +
                     (src[k - stride + 1] - src[k + stride + 1]) +
                     2 * (src[k - stride] - src[k + stride]);
      const int Ga = abs(Gx) + abs(Gy);
      if (Ga < edge_thresh) {  // Smooth pixels
        // Find Laplacian
        const int v =
            4 * src[k] -
            2 * (src[k - 1] + src[k + 1] + src[k - stride] + src[k + stride]) +
            (src[k - stride - 1] + src[k - stride + 1] + src[k + stride - 1] +
             src[k + stride + 1]);
        sum += abs(v);
        ++num;
      }
    }
  }
  // If very few smooth pels, return -1 since the estimate is unreliable
  if (num < 16) return -1.0;

  const double sigma = (double)sum / (6 * num) * SQRT_PI_BY_2;
  return sigma;
}

// Return noise estimate, or -1.0 if there was a failure
double highbd_estimate_noise(const uint8_t *src8, int width, int height,
                             int stride, int bd, int edge_thresh) {
  uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  int64_t sum = 0;
  int64_t num = 0;
  for (int i = 1; i < height - 1; ++i) {
    for (int j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      // Sobel gradients
      const int Gx = (src[k - stride - 1] - src[k - stride + 1]) +
                     (src[k + stride - 1] - src[k + stride + 1]) +
                     2 * (src[k - 1] - src[k + 1]);
      const int Gy = (src[k - stride - 1] - src[k + stride - 1]) +
                     (src[k - stride + 1] - src[k + stride + 1]) +
                     2 * (src[k - stride] - src[k + stride]);
      const int Ga = ROUND_POWER_OF_TWO(abs(Gx) + abs(Gy), bd - 8);
      if (Ga < edge_thresh) {  // Smooth pixels
        // Find Laplacian
        const int v =
            4 * src[k] -
            2 * (src[k - 1] + src[k + 1] + src[k - stride] + src[k + stride]) +
            (src[k - stride - 1] + src[k - stride + 1] + src[k + stride - 1] +
             src[k + stride + 1]);
        sum += ROUND_POWER_OF_TWO(abs(v), bd - 8);
        ++num;
      }
    }
  }
  // If very few smooth pels, return -1 since the estimate is unreliable
  if (num < 16) return -1.0;

  const double sigma = (double)sum / (6 * num) * SQRT_PI_BY_2;
  return sigma;
}

static int estimate_strength(AV1_COMP *cpi, int distance, int group_boost,
                             double *sigma) {
  // Adjust the strength based on active max q.
  int q;
  if (cpi->common.current_frame.frame_number > 1)
    q = ((int)av1_convert_qindex_to_q(cpi->rc.avg_frame_qindex[INTER_FRAME],
                                      cpi->common.seq_params.bit_depth));
  else
    q = ((int)av1_convert_qindex_to_q(cpi->rc.avg_frame_qindex[KEY_FRAME],
                                      cpi->common.seq_params.bit_depth));
  MACROBLOCKD *mbd = &cpi->td.mb.e_mbd;
  struct lookahead_entry *buf =
      av1_lookahead_peek(cpi->lookahead, distance, cpi->compressor_stage);
  int strength;
  double noiselevel;
  if (is_cur_buf_hbd(mbd)) {
    noiselevel = highbd_estimate_noise(
        buf->img.y_buffer, buf->img.y_crop_width, buf->img.y_crop_height,
        buf->img.y_stride, mbd->bd, EDGE_THRESHOLD);
    *sigma = noiselevel;
  } else {
    noiselevel = estimate_noise(buf->img.y_buffer, buf->img.y_crop_width,
                                buf->img.y_crop_height, buf->img.y_stride,
                                EDGE_THRESHOLD);
    *sigma = noiselevel;
  }
  int adj_strength = cpi->oxcf.arnr_strength;
  if (noiselevel > 0) {
    // Get 4 integer adjustment levels in [-2, 1]
    int noiselevel_adj;
    if (noiselevel < 0.75)
      noiselevel_adj = -2;
    else if (noiselevel < 1.75)
      noiselevel_adj = -1;
    else if (noiselevel < 4.0)
      noiselevel_adj = 0;
    else
      noiselevel_adj = 1;
    adj_strength += noiselevel_adj;
  }
  // printf("[noise level: %g, strength = %d]\n", noiselevel, adj_strength);

  if (q > 16) {
    strength = adj_strength;
  } else {
    strength = adj_strength - ((16 - q) / 2);
    if (strength < 0) strength = 0;
  }

  if (strength > group_boost / 300) {
    strength = group_boost / 300;
  }

  return strength;
}

// Apply buffer limits and context specific adjustments to arnr filter.
static void adjust_arnr_filter(AV1_COMP *cpi, int distance, int group_boost,
                               int *arnr_frames, int *arnr_strength,
                               double *sigma, int *frm_bwd, int *frm_fwd,
                               int second_alt_ref) {
  int frames = cpi->oxcf.arnr_max_frames;

  // Only use the 2 nearest frames in second alt ref's temporal filtering.
  if (second_alt_ref) frames = AOMMIN(frames, 3);

  // Adjust number of frames in filter and strength based on gf boost level.
  if (frames > group_boost / 150) {
    frames = group_boost / 150;
    frames += !(frames & 1);
  }

  const int frames_after_arf =
      av1_lookahead_depth(cpi->lookahead, cpi->compressor_stage) - distance - 1;
  int frames_fwd = (frames - 1) >> 1;
  int frames_bwd = frames >> 1;

  // Define the forward and backwards filter limits for this arnr group.
  if (frames_fwd > frames_after_arf) frames_fwd = frames_after_arf;
  if (frames_bwd > distance) frames_bwd = distance;

  // Set the baseline active filter size.
  frames = frames_bwd + 1 + frames_fwd;

  *arnr_frames = frames;
  *arnr_strength = estimate_strength(cpi, distance, group_boost, sigma);
  *frm_bwd = frames_bwd;
  *frm_fwd = frames_fwd;
}

int av1_temporal_filter(AV1_COMP *cpi, int distance,
                        int *show_existing_alt_ref) {
  RATE_CONTROL *const rc = &cpi->rc;
  int frame;
  int frames_to_blur;
  int start_frame;
  int strength;
  int frames_to_blur_backward;
  int frames_to_blur_forward;
  struct scale_factors sf;

  YV12_BUFFER_CONFIG *frames[MAX_LAG_BUFFERS] = { NULL };
  const GF_GROUP *const gf_group = &cpi->gf_group;
  int rdmult = 0;
  double sigma = 0;

  // Temporal filter 1 more alt ref if its distance >= 7. This frame is always
  // a show existing frame.
  const int second_alt_ref =
      (gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE) &&
      (distance >= 7) && cpi->sf.hl_sf.second_alt_ref_filtering;

  // TODO(yunqing): For INTNL_ARF_UPDATE type, the following me initialization
  // is used somewhere unexpectedly. Should be resolved later.
  // Initialize errorperbit, sadperbit16 and sadperbit4.
  rdmult = av1_compute_rd_mult_based_on_qindex(cpi, ARNR_FILT_QINDEX);
  set_error_per_bit(&cpi->td.mb, rdmult);
  av1_initialize_me_consts(cpi, &cpi->td.mb, ARNR_FILT_QINDEX);
  av1_fill_mv_costs(cpi->common.fc, cpi->common.cur_frame_force_integer_mv,
                    cpi->common.allow_high_precision_mv, &cpi->td.mb);

  // Apply context specific adjustments to the arnr filter parameters.
  if (gf_group->update_type[gf_group->index] == INTNL_ARF_UPDATE &&
      !second_alt_ref) {
    // TODO(weitinglin): Currently, we enforce the filtering strength on
    // internal ARFs to be zeros. We should investigate in which case it is more
    // beneficial to use non-zero strength filtering.
    strength = 0;
    frames_to_blur = 1;
    return 0;
  }

  if (distance < 0) {
    frames_to_blur = NUM_KEY_FRAME_DENOISING;
    if (distance == -1) {
      // Apply temporal filtering on key frame.
      strength = estimate_strength(cpi, distance, rc->gfu_boost, &sigma);
      // Number of frames for temporal filtering, could be tuned.
      frames_to_blur_backward = 0;
      frames_to_blur_forward = frames_to_blur - 1;
      start_frame = distance + frames_to_blur_forward;
    } else {
      // Apply temporal filtering on forward key frame. This requires filtering
      // backwards rather than forwards.
      strength = estimate_strength(cpi, -1 * distance, rc->gfu_boost, &sigma);
      // Number of frames for temporal filtering, could be tuned.
      frames_to_blur_backward = frames_to_blur - 1;
      frames_to_blur_forward = 0;
      start_frame = -1 * distance;
    }
  } else {
    adjust_arnr_filter(cpi, distance, rc->gfu_boost, &frames_to_blur, &strength,
                       &sigma, &frames_to_blur_backward,
                       &frames_to_blur_forward, second_alt_ref);
    start_frame = distance + frames_to_blur_forward;

    cpi->common.showable_frame =
        (strength == 0 && frames_to_blur == 1) || second_alt_ref ||
        (cpi->oxcf.enable_overlay == 0 || cpi->sf.hl_sf.disable_overlay_frames);
  }

  // Setup frame pointers, NULL indicates frame not included in filter.
  for (frame = 0; frame < frames_to_blur; ++frame) {
    const int which_buffer = start_frame - frame;
    struct lookahead_entry *buf =
        av1_lookahead_peek(cpi->lookahead, which_buffer, cpi->compressor_stage);
    if (buf == NULL) {
      frames[frames_to_blur - 1 - frame] = NULL;
    } else {
      frames[frames_to_blur - 1 - frame] = &buf->img;
    }
  }

  if (frames_to_blur > 0 && frames[0] != NULL) {
    // Setup scaling factors. Scaling on each of the arnr frames is not
    // supported.
    // ARF is produced at the native frame size and resized when coded.
    av1_setup_scale_factors_for_frame(
        &sf, frames[0]->y_crop_width, frames[0]->y_crop_height,
        frames[0]->y_crop_width, frames[0]->y_crop_height);
  }

  FRAME_DIFF diff = temporal_filter_iterate_c(
      cpi, frames, frames_to_blur, frames_to_blur_backward, strength, sigma,
      distance < 0, &sf, second_alt_ref);

  if (distance < 0) return 1;

  if ((show_existing_alt_ref != NULL &&
       cpi->sf.hl_sf.adaptive_overlay_encoding) ||
      second_alt_ref) {
    AV1_COMMON *const cm = &cpi->common;
    int top_index = 0, bottom_index = 0;

    aom_clear_system_state();
    // TODO(yunqing): This can be combined with TPL q calculation later.
    cpi->rc.base_frame_target = gf_group->bit_allocation[gf_group->index];
    av1_set_target_rate(cpi, cm->width, cm->height);
    const int q = av1_rc_pick_q_and_bounds(cpi, &cpi->rc, cpi->oxcf.width,
                                           cpi->oxcf.height, gf_group->index,
                                           &bottom_index, &top_index);
    const int ac_q = av1_ac_quant_QTX(q, 0, cm->seq_params.bit_depth);
    const int ac_q_2 = ac_q * ac_q;
    const int mb_cols = get_cols(frames[frames_to_blur_backward]->y_crop_width);
    const int mb_rows =
        get_rows(frames[frames_to_blur_backward]->y_crop_height);
    const int mbs = AOMMAX(1, mb_rows * mb_cols);
    const float mean = (float)diff.sum / mbs;
    const float std = (float)sqrt((float)diff.sse / mbs - mean * mean);
    const float threshold = 0.7f;

    if (!second_alt_ref) {
      *show_existing_alt_ref = 0;
      if (mean / ac_q_2 < threshold && std < mean * 1.2)
        *show_existing_alt_ref = 1;
      cpi->common.showable_frame |= *show_existing_alt_ref;
    } else {
      // Use source frame if the filtered frame becomes very different.
      if (!(mean / ac_q_2 < threshold && std < mean * 1.2)) return 0;
    }
  }

  return 1;
}
