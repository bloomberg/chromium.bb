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

// Motion search supports 1/8 precision for sub-pixel.
#define GET_MV_SUBPEL(x) ((x) << 3)
#define GET_MV_RAWPEL(x) ((x) >> 3)

// Does motion search for blocks in temporal filtering. This is the first step
// for temporal filtering. More specifically, given a frame to be filtered and
// another frame as reference, this function searches the reference frame to
// find out the most alike block as that from the frame to be filtered. This
// found block will be further used for weighted averaging.
// NOTE: Besides doing motion search for the entire block, this function will
// also do motion search for each 1/4 sub-block to get more precise prediction.
// Inputs:
//   cpi: Pointer to the composed information of input video.
//   frame_to_filter: Pointer to the frame to be filtered.
//   ref_frame: Pointer to the reference frame.
//   block_size: Block size used for motion search.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   ref_mv: Reference motion vector, which is commonly inherited from the
//           motion search result of previous frame.
//   subblock_mvs: Pointer to the result motion vectors for 4 sub-blocks.
//   subblock_errors: Pointer to the search errors for 4 sub-blocks.
// Returns:
//   Search error of the entire block.
static int tf_motion_search(AV1_COMP *cpi,
                            const YV12_BUFFER_CONFIG *frame_to_filter,
                            const YV12_BUFFER_CONFIG *ref_frame,
                            const BLOCK_SIZE block_size, const int mb_row,
                            const int mb_col, MV *ref_mv, MV *subblock_mvs,
                            int *subblock_errors) {
  // Block information (ONLY Y-plane is used for motion search).
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_y = mb_height * mb_row;
  const int mb_x = mb_width * mb_col;
  const int y_stride = frame_to_filter->y_stride;
  assert(y_stride == ref_frame->y_stride);
  const int y_offset = mb_row * mb_height * y_stride + mb_col * mb_width;

  // Save input state.
  MACROBLOCK *const mb = &cpi->td.mb;
  MACROBLOCKD *const mbd = &mb->e_mbd;
  const struct buf_2d ori_src_buf = mb->plane[0].src;
  const struct buf_2d ori_pre_buf = mbd->plane[0].pre[0];
  const MvLimits ori_mv_limits = mb->mv_limits;

  // Parameters used for motion search.
  const int sadperbit16 = mb->sadperbit16;
  const search_site_config ss_cfg = cpi->ss_cfg[SS_CFG_LOOKAHEAD];
  const SEARCH_METHODS full_search_method = NSTEP;
  const int step_param = av1_init_search_range(
      AOMMIN(frame_to_filter->y_crop_width, frame_to_filter->y_crop_height));
  const SUBPEL_SEARCH_TYPE subpel_search_type = USE_8_TAPS;
  const int allow_high_precision_mv = cpi->common.allow_high_precision_mv;
  const int subpel_iters_per_step = cpi->sf.mv_sf.subpel_iters_per_step;
  const int errorperbit = mb->errorperbit;
  const int force_integer_mv = cpi->common.cur_frame_force_integer_mv;

  // Starting position for motion search.
  MV start_mv = { GET_MV_RAWPEL(ref_mv->row), GET_MV_RAWPEL(ref_mv->col) };
  // Baseline position for motion search (used for rate distortion comparison).
  const MV baseline_mv = kZeroMv;

  // Setup.
  mb->plane[0].src.buf = frame_to_filter->y_buffer + y_offset;
  mb->plane[0].src.stride = y_stride;
  mbd->plane[0].pre[0].buf = ref_frame->y_buffer + y_offset;
  mbd->plane[0].pre[0].stride = y_stride;
  av1_set_mv_search_range(&mb->mv_limits, &baseline_mv);
  // Unused intermediate results for motion search.
  unsigned int sse;
  int distortion;
  int cost_list[5];

  // Do motion search.
  // NOTE: In `av1_full_pixel_search()` and `find_fractional_mv_step()`, the
  // searched result will be stored in `mb->best_mv`.
  int block_error = INT_MAX;
  av1_full_pixel_search(cpi, mb, block_size, &start_mv, step_param, 1,
                        full_search_method, 1, sadperbit16,
                        cond_cost_list(cpi, cost_list), &baseline_mv, 0, 0,
                        mb_x, mb_y, 0, &ss_cfg, 0);
  if (force_integer_mv == 1) {  // Only do full search on the entire block.
    const int mv_row = mb->best_mv.as_mv.row;
    const int mv_col = mb->best_mv.as_mv.col;
    mb->best_mv.as_mv.row = GET_MV_SUBPEL(mv_row);
    mb->best_mv.as_mv.col = GET_MV_SUBPEL(mv_col);
    const int mv_offset = mv_row * y_stride + mv_col;
    block_error = cpi->fn_ptr[block_size].vf(
        ref_frame->y_buffer + y_offset + mv_offset, y_stride,
        frame_to_filter->y_buffer + y_offset, y_stride, &sse);
    mb->e_mbd.mi[0]->mv[0] = mb->best_mv;
  } else {  // Do fractional search on the entire block and all sub-blocks.
    block_error = cpi->find_fractional_mv_step(
        mb, &cpi->common, 0, 0, &baseline_mv, allow_high_precision_mv,
        errorperbit, &cpi->fn_ptr[block_size], 0, subpel_iters_per_step,
        cond_cost_list(cpi, cost_list), NULL, NULL, &distortion, &sse, NULL,
        NULL, 0, 0, mb_width, mb_height, subpel_search_type, 1);
    mb->e_mbd.mi[0]->mv[0] = mb->best_mv;
    *ref_mv = mb->best_mv.as_mv;
    // On 4 sub-blocks.
    const BLOCK_SIZE subblock_size = ss_size_lookup[BLOCK_32X32][1][1];
    const int subblock_height = block_size_high[subblock_size];
    const int subblock_width = block_size_wide[subblock_size];
    start_mv.row = GET_MV_RAWPEL(ref_mv->row);
    start_mv.col = GET_MV_RAWPEL(ref_mv->col);
    int subblock_idx = 0;
    for (int i = 0; i < mb_height; i += subblock_height) {
      for (int j = 0; j < mb_width; j += subblock_width) {
        const int offset = i * y_stride + j;
        mb->plane[0].src.buf = frame_to_filter->y_buffer + y_offset + offset;
        mbd->plane[0].pre[0].buf = ref_frame->y_buffer + y_offset + offset;
        av1_set_mv_search_range(&mb->mv_limits, &baseline_mv);
        av1_full_pixel_search(cpi, mb, subblock_size, &start_mv, step_param, 1,
                              full_search_method, 1, sadperbit16,
                              cond_cost_list(cpi, cost_list), &baseline_mv, 0,
                              0, mb_x, mb_y, 0, &ss_cfg, 0);
        subblock_errors[subblock_idx] = cpi->find_fractional_mv_step(
            mb, &cpi->common, 0, 0, &baseline_mv, allow_high_precision_mv,
            errorperbit, &cpi->fn_ptr[subblock_size], 0, subpel_iters_per_step,
            cond_cost_list(cpi, cost_list), NULL, NULL, &distortion, &sse, NULL,
            NULL, 0, 0, subblock_width, subblock_height, subpel_search_type, 1);
        subblock_mvs[subblock_idx] = mb->best_mv.as_mv;
        ++subblock_idx;
      }
    }
  }

  // Restore input state.
  mb->plane[0].src = ori_src_buf;
  mbd->plane[0].pre[0] = ori_pre_buf;
  mb->mv_limits = ori_mv_limits;

  return block_error;
}

// Helper function to get weight according to thresholds.
static INLINE int get_weight_by_thresh(const int value, const int low,
                                       const int high) {
  return value < low ? 2 : value < high ? 1 : 0;
}

// Gets filter weight for blocks in temporal filtering. The weights will be
// assigned based on the motion search errors.
// NOTE: Besides assigning filter weight for the block, this function will also
// determine whether to split the entire block into 4 sub-blocks for further
// filtering.
// TODO(any): Many magic numbers are used in this function. They may be tuned
// to improve the performance.
// Inputs:
//   block_error: Motion search error for the entire block.
//   subblock_errors: Pointer to the search errors for 4 sub-blocks.
//   use_planewise_strategy: Whether to use plane-wise filtering strategy. This
//                           field will affect the filter weight for the
//                           to-filter frame.
//   use_second_altref: Whether to perform temporal filtering on a second
//                         Alternate Reference Frame (ARF). This field will
//                         affect the filter weight for the to-filter frame.
//   subblock_filter_weights: Pointer to the assigned filter weight for each
//                            sub-block. If not using sub-blocks, the first
//                            element will be used for the entire block.
// Returns: Whether to use 4 sub-blocks to replace the original block.
static int tf_get_filter_weight(const int block_error,
                                const int *subblock_errors,
                                const int use_planewise_strategy,
                                const int use_second_altref,
                                int *subblock_filter_weights) {
  // `block_error` is initialized as INT_MAX and will be overwritten after
  // motion search with reference frame, therefore INT_MAX can ONLY be accessed
  // by to-filter frame.
  if (block_error == INT_MAX) {
    const int weight = use_planewise_strategy ? PLANEWISE_FILTER_WEIGHT_SCALE
                                              : use_second_altref ? 64 : 32;
    subblock_filter_weights[0] = subblock_filter_weights[1] =
        subblock_filter_weights[2] = subblock_filter_weights[3] = weight;
    return 0;
  }

  const int thresh_low = use_second_altref ? 5000 : 10000;
  const int thresh_high = use_second_altref ? 10000 : 20000;

  int min_subblock_error = INT_MAX;
  int max_subblock_error = INT_MIN;
  int sum_subblock_error = 0;
  for (int i = 0; i < 4; ++i) {
    sum_subblock_error += subblock_errors[i];
    min_subblock_error = AOMMIN(min_subblock_error, subblock_errors[i]);
    max_subblock_error = AOMMAX(max_subblock_error, subblock_errors[i]);
    subblock_filter_weights[i] =
        get_weight_by_thresh(subblock_errors[i], thresh_low, thresh_high);
  }

  if (((block_error * 15 < sum_subblock_error * 16) &&
       max_subblock_error - min_subblock_error < 12000) ||
      ((block_error * 14 < sum_subblock_error * 16) &&
       max_subblock_error - min_subblock_error < 6000)) {  // No split.
    const int weight =
        get_weight_by_thresh(block_error, thresh_low * 4, thresh_high * 4);
    subblock_filter_weights[0] = subblock_filter_weights[1] =
        subblock_filter_weights[2] = subblock_filter_weights[3] = weight;
    return 0;
  } else {  // Do split.
    return 1;
  }
}

// Helper function to determine whether a frame is encoded with high bit-depth.
static INLINE int is_frame_high_bitdepth(const YV12_BUFFER_CONFIG *frame) {
  return (frame->flags & YV12_FLAG_HIGHBITDEPTH) ? 1 : 0;
}

// Builds predictor for blocks in temporal filtering. This is the second step
// for temporal filtering, which is to construct predictions from all reference
// frames INCLUDING the frame to be filtered itself. These predictors are built
// based on the motion search results (motion vector is set as 0 for the frame
// to be filtered), and will be futher used for weighted averaging.
// Inputs:
//   ref_frame: Pointer to the reference frame (or the frame to be filtered).
//   mbd: Pointer to the block for filtering. Besides containing the subsampling
//        information of all planes, this field also gives the searched motion
//        vector for the entire block, i.e., `mbd->mi[0]->mv[0]`. This vector
//        should be 0 if the `ref_frame` itself is the frame to be filtered.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   num_planes: Number of planes in the frame.
//   scale: Scaling factor.
//   use_subblock: Whether to use 4 sub-blocks to replace the original block.
//   subblock_mvs: The motion vectors for each sub-block (row-major order).
//   pred: Pointer to the predictor to build.
// Returns:
//   Nothing will be returned. But the content to which `pred` points will be
//   modified.
static void tf_build_predictor(const YV12_BUFFER_CONFIG *ref_frame,
                               const MACROBLOCKD *mbd,
                               const BLOCK_SIZE block_size, const int mb_row,
                               const int mb_col, const int num_planes,
                               const struct scale_factors *scale,
                               const int use_subblock, const MV *subblock_mvs,
                               uint8_t *pred) {
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  // Information of the entire block.
  const int mb_height = block_size_high[block_size];  // Height.
  const int mb_width = block_size_wide[block_size];   // Width.
  const int mb_pels = mb_height * mb_width;           // Number of pixels.
  const int mb_y = mb_height * mb_row;                // Y-coord (Top-left).
  const int mb_x = mb_width * mb_col;                 // X-coord (Top-left).
  const int bit_depth = mbd->bd;                      // Bit depth.
  const int is_intrabc = 0;                           // Is intra-copied?
  const int mb_mv_row = mbd->mi[0]->mv[0].as_mv.row;  // Motion vector (y).
  const int mb_mv_col = mbd->mi[0]->mv[0].as_mv.col;  // Motion vector (x).
  const MV mb_mv = { (int16_t)mb_mv_row, (int16_t)mb_mv_col };
  const int is_high_bitdepth = is_frame_high_bitdepth(ref_frame);

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

// Computes temporal filter weights and accumulators for the frame to be
// filtered. More concretely, the filter weights for all pixels are the same.
// Inputs:
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of all planes as well as the bit-depth.
//   block_size: Size of the block.
//   num_planes: Number of planes in the frame.
//   filter_weight: Weight used for filtering.
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void av1_apply_temporal_filter_self(const MACROBLOCKD *mbd,
                                    const BLOCK_SIZE block_size,
                                    const int num_planes,
                                    const int filter_weight,
                                    const uint8_t *pred, uint32_t *accum,
                                    uint16_t *count) {
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  const int is_high_bitdepth = is_cur_buf_hbd(mbd);
  const uint16_t *pred16 = CONVERT_TO_SHORTPTR(pred);

  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const int subsampling_y = mbd->plane[plane].subsampling_y;
    const int subsampling_x = mbd->plane[plane].subsampling_x;
    const int h = mb_height >> subsampling_y;  // Plane height.
    const int w = mb_width >> subsampling_x;   // Plane width.

    int pred_idx = 0;
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        const int idx = plane_offset + pred_idx;  // Index with plane shift.
        const int pred_value = is_high_bitdepth ? pred16[idx] : pred[idx];
        accum[idx] += filter_weight * pred_value;
        count[idx] += filter_weight;
        ++pred_idx;
      }
    }
    plane_offset += mb_pels;
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
// TODO(any): Not sure what index 4 and index 9 are for.
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
//   frame_to_filter: Pointer to the frame to be filtered, which is used as
//                    reference to compute squared differece from the predictor.
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of all YUV planes.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   strength: Strength for filter weight adjustment.
//   use_subblock: Whether to use 4 sub-blocks to replace the original block.
//   subblock_filter_weights: The filter weights for each sub-block (row-major
//                            order). If `use_subblock` is set as 0, the first
//                            weight will be applied to the entire block.
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void av1_apply_temporal_filter_yuv_c(const YV12_BUFFER_CONFIG *frame_to_filter,
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
  const int is_high_bitdepth = is_frame_high_bitdepth(frame_to_filter);
  const uint16_t *pred16 = CONVERT_TO_SHORTPTR(pred);

  // Allocate memory for pixel-wise squared differences for Y, U, V planes. All
  // planes, regardless of the subsampling, are assigned with memory of size
  // `mb_pels`.
  uint32_t *square_diff = aom_memalign(16, 3 * mb_pels * sizeof(uint32_t));
  memset(square_diff, 0, 3 * mb_pels * sizeof(square_diff[0]));

  int plane_offset = 0;
  for (int plane = 0; plane < 3; ++plane) {
    // Locate pixel on reference frame.
    const int plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const int plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const int frame_stride = frame_to_filter->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;
    const uint8_t *ref = frame_to_filter->buffers[plane];
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
  for (int plane = 0; plane < 3; ++plane) {
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
          for (int p = 1; p < 3; ++p) {
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

        const int idx = plane_offset + pred_idx;  // Index with plane shift.
        const int pred_value = is_high_bitdepth ? pred16[idx] : pred[idx];
        const int adjusted_weight = adjust_filter_weight_yuv(
            filter_weight, sum_square_diff, num_ref_pixels, strength,
            is_high_bitdepth);
        accum[idx] += adjusted_weight * pred_value;
        count[idx] += adjusted_weight;

        ++pred_idx;
      }
    }
    plane_offset += mb_pels;
  }

  aom_free(square_diff);
}

// Function to adjust the filter weight when applying filter to Y-plane only.
// Inputs:
//   filter_weight: Original filter weight.
//   sum_square_diff: Sum of squared difference between input frame and
//                    prediction. This field is computed pixel by pixel, and
//                    is used as a reference for the filter weight adjustment.
//   num_ref_pixels: Number of pixels used to compute the `sum_square_diff`.
//   strength: Strength for filter weight adjustment.
// Returns:
//   Adjusted filter weight which will finally be used for filtering.
static INLINE int adjust_filter_weight_yonly(const int filter_weight,
                                             const uint64_t sum_square_diff,
                                             const int num_ref_pixels,
                                             const int strength) {
  assert(YONLY_FILTER_WINDOW_LENGTH == 3);

  int modifier = (int)(AOMMIN(sum_square_diff * 3, INT32_MAX));
  modifier /= num_ref_pixels;

  const int rounding = (1 << strength) >> 1;
  modifier = (modifier + rounding) >> strength;
  return (modifier >= 16) ? 0 : (16 - modifier) * filter_weight;
}

// Applies temporal filter to Y-plane ONLY.
// Different from the function `av1_apply_temporal_filter_yuv_c()`, this
// function only applies temporal filter to Y-plane. This should be used when
// the input video frame only has one plane.
// Inputs:
//   frame_to_filter: Pointer to the frame to be filtered, which is used as
//                    reference to compute squared differece from the predictor.
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of Y plane.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   strength: Strength for filter weight adjustment.
//   use_subblock: Whether to use 4 sub-blocks to replace the original block.
//   subblock_filter_weights: The filter weights for each sub-block (row-major
//                            order). If `use_subblock` is set as 0, the first
//                            weight will be applied to the entire block.
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void av1_apply_temporal_filter_yonly(const YV12_BUFFER_CONFIG *frame_to_filter,
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
  const int is_high_bitdepth = is_frame_high_bitdepth(frame_to_filter);
  const uint16_t *pred16 = CONVERT_TO_SHORTPTR(pred);

  // Y-plane information.
  const int subsampling_y = mbd->plane[0].subsampling_y;
  const int subsampling_x = mbd->plane[0].subsampling_x;
  const int h = mb_height >> subsampling_y;
  const int w = mb_width >> subsampling_x;

  // Pre-compute squared difference before filtering.
  const int frame_stride = frame_to_filter->y_stride;
  const int frame_offset = mb_row * h * frame_stride + mb_col * w;
  const uint8_t *ref = frame_to_filter->y_buffer;
  uint32_t *square_diff = aom_memalign(16, mb_pels * sizeof(uint32_t));
  memset(square_diff, 0, mb_pels * sizeof(square_diff[0]));
  compute_square_diff(ref, frame_offset, frame_stride, pred, 0, w, h, w,
                      is_high_bitdepth, square_diff);

  // Get window size for pixel-wise filtering.
  assert(YONLY_FILTER_WINDOW_LENGTH % 2 == 1);
  const int half_window = YONLY_FILTER_WINDOW_LENGTH >> 1;

  // Perform filtering.
  int idx = 0;
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
            sum_square_diff += square_diff[y * w + x];
            ++num_ref_pixels;
          }
        }
      }

      const int pred_value = is_high_bitdepth ? pred16[idx] : pred[idx];
      const int adjusted_weight = adjust_filter_weight_yonly(
          filter_weight, sum_square_diff, num_ref_pixels, strength);
      accum[idx] += adjusted_weight * pred_value;
      count[idx] += adjusted_weight;

      ++idx;
    }
  }

  aom_free(square_diff);
}

// Applies temporal filter plane by plane.
// Different from the function `av1_apply_temporal_filter_yuv_c()` and the
// function `av1_apply_temporal_filter_yonly()`, this function applies temporal
// filter to each plane independently. Besides, the strategy of filter weight
// adjustment is different from the other two functions.
// Inputs:
//   frame_to_filter: Pointer to the frame to be filtered, which is used as
//                    reference to compute squared differece from the predictor.
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of all planes.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   num_planes: Number of planes in the frame.
//   noise_level: Noise level of the to-filter frame, estimated with Y plane.
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void av1_apply_temporal_filter_planewise_c(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const double noise_level, const uint8_t *pred,
    uint32_t *accum, uint16_t *count) {
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  // Hyper-parameter for filter weight adjustment.
  const int frame_height = frame_to_filter->heights[0]
                           << mbd->plane[0].subsampling_y;
  const int decay_control = frame_height >= 480 ? 4 : 3;
  // Control factor for non-local mean approach.
  const double r = (double)decay_control * (0.7 + log(noise_level + 0.5));

  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  const int is_high_bitdepth = is_frame_high_bitdepth(frame_to_filter);
  const uint16_t *pred16 = CONVERT_TO_SHORTPTR(pred);

  // Allocate memory for pixel-wise squared differences for all planes. They,
  // regardless of the subsampling, are assigned with memory of size `mb_pels`.
  uint32_t *square_diff =
      aom_memalign(16, num_planes * mb_pels * sizeof(uint32_t));
  memset(square_diff, 0, num_planes * mb_pels * sizeof(square_diff[0]));

  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    // Locate pixel on reference frame.
    const int plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const int plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const int frame_stride = frame_to_filter->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;
    const uint8_t *ref = frame_to_filter->buffers[plane];
    compute_square_diff(ref, frame_offset, frame_stride, pred, plane_offset,
                        plane_w, plane_h, plane_w, is_high_bitdepth,
                        square_diff + plane_offset);
    plane_offset += mb_pels;
  }

  // Get window size for pixel-wise filtering.
  assert(PLANEWISE_FILTER_WINDOW_LENGTH % 2 == 1);
  const int half_window = PLANEWISE_FILTER_WINDOW_LENGTH >> 1;

  // Handle planes in sequence.
  plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const int subsampling_y = mbd->plane[plane].subsampling_y;
    const int subsampling_x = mbd->plane[plane].subsampling_x;
    const int h = mb_height >> subsampling_y;  // Plane height.
    const int w = mb_width >> subsampling_x;   // Plane width.

    // Perform filtering.
    int pred_idx = 0;
    for (int i = 0; i < h; ++i) {
      for (int j = 0; j < w; ++j) {
        // non-local mean approach
        uint64_t sum_square_diff = 0;
        int num_ref_pixels = 0;

        for (int wi = -half_window; wi <= half_window; ++wi) {
          for (int wj = -half_window; wj <= half_window; ++wj) {
            const int y = CLIP(i + wi, 0, h - 1);  // Y-coord on current plane.
            const int x = CLIP(j + wj, 0, w - 1);  // X-coord on current plane.
            sum_square_diff += square_diff[plane_offset + y * w + x];
            ++num_ref_pixels;
          }
        }

        const int idx = plane_offset + pred_idx;  // Index with plane shift.
        const int pred_value = is_high_bitdepth ? pred16[idx] : pred[idx];
        const double scaled_diff = AOMMAX(
            -(double)(sum_square_diff / num_ref_pixels) / (2 * r * r), -15.0);
        const int adjusted_weight =
            (int)(exp(scaled_diff) * PLANEWISE_FILTER_WEIGHT_SCALE);
        accum[idx] += adjusted_weight * pred_value;
        count[idx] += adjusted_weight;

        ++pred_idx;
      }
    }
    plane_offset += mb_pels;
  }

  aom_free(square_diff);
}

// Computes temporal filter weights and accumulators from all reference frames
// excluding the current frame to be filtered.
// Inputs:
//   frame_to_filter: Pointer to the frame to be filtered, which is used as
//                    reference to compute squared differece from the predictor.
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of all planes and the bit-depth.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   num_planes: Number of planes in the frame.
//   use_planewise_strategy: Whether to use plane-wise temporal filtering
//                           strategy. If set as 0, YUV or YONLY filtering will
//                           be used (depending on number ofplanes).
//   strength: Strength for filter weight adjustment. (Used in YUV filtering and
//             YONLY filtering.)
//   use_subblock: Whether to use 4 sub-blocks to replace the original block.
//                 (Used in YUV filtering and YONLY filtering.)
//   subblock_filter_weights: The filter weights for each sub-block (row-major
//                            order). If `use_subblock` is set as 0, the first
//                            weight will be applied to the entire block. (Used
//                            in YUV filtering and YONLY filtering.)
//   noise_level: Noise level of the to-filter frame, estimated with Y plane.
//                (Used in plane-wise filtering.)
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void av1_apply_temporal_filter_others(
    const YV12_BUFFER_CONFIG *frame_to_filter, const MACROBLOCKD *mbd,
    const BLOCK_SIZE block_size, const int mb_row, const int mb_col,
    const int num_planes, const int use_planewise_strategy, const int strength,
    const int use_subblock, const int *subblock_filter_weights,
    const double noise_level, const uint8_t *pred, uint32_t *accum,
    uint16_t *count) {
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  if (use_planewise_strategy) {  // Commonly used for high-resolution video.
    // TODO(any): avx2 and sse version should also support high bit-depth.
    if (is_frame_high_bitdepth(frame_to_filter)) {
      av1_apply_temporal_filter_planewise_c(frame_to_filter, mbd, block_size,
                                            mb_row, mb_col, num_planes,
                                            noise_level, pred, accum, count);
    } else {
      av1_apply_temporal_filter_planewise(frame_to_filter, mbd, block_size,
                                          mb_row, mb_col, num_planes,
                                          noise_level, pred, accum, count);
    }
  } else {  // Commonly used for low-resolution video.
    const int adj_strength = strength + 2 * (mbd->bd - 8);
    if (num_planes == 1) {
      av1_apply_temporal_filter_yonly(
          frame_to_filter, mbd, block_size, mb_row, mb_col, adj_strength,
          use_subblock, subblock_filter_weights, pred, accum, count);
    } else if (num_planes == 3) {
      av1_apply_temporal_filter_yuv(
          frame_to_filter, mbd, block_size, mb_row, mb_col, adj_strength,
          use_subblock, subblock_filter_weights, pred, accum, count);
    } else {
      assert(0 && "Only support Y-plane and YUV-plane modes.");
    }
  }
}

// Normalizes the accumulated filtering result to produce the filtered frame.
// Inputs:
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of all planes.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   num_planes: Number of planes in the frame.
//   accum: Pointer to the pre-computed accumulator.
//   conut: Pointer to the pre-computed count.
//   result_buffer: Pointer to result buffer.
// Return:
//   Nothing will be returned. But the content to which `result_buffer` point
//   will be modified.
static void tf_normalize_filtered_frame(
    const MACROBLOCKD *mbd, const BLOCK_SIZE block_size, const int mb_row,
    const int mb_col, const int num_planes, const uint32_t *accum,
    const uint16_t *count, YV12_BUFFER_CONFIG *result_buffer) {
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  // Block information.
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  const int is_high_bitdepth = is_frame_high_bitdepth(result_buffer);

  int plane_offset = 0;
  for (int plane = 0; plane < num_planes; ++plane) {
    const int plane_h = mb_height >> mbd->plane[plane].subsampling_y;
    const int plane_w = mb_width >> mbd->plane[plane].subsampling_x;
    const int frame_stride = result_buffer->strides[plane == 0 ? 0 : 1];
    const int frame_offset = mb_row * plane_h * frame_stride + mb_col * plane_w;
    uint8_t *const buf = result_buffer->buffers[plane];
    uint16_t *const buf16 = CONVERT_TO_SHORTPTR(buf);

    int plane_idx = 0;             // Pixel index on current plane (block-base).
    int frame_idx = frame_offset;  // Pixel index on the entire frame.
    for (int i = 0; i < plane_h; ++i) {
      for (int j = 0; j < plane_w; ++j) {
        const int idx = plane_idx + plane_offset;
        const uint16_t rounding = count[idx] >> 1;
        if (is_high_bitdepth) {
          buf16[frame_idx] =
              (uint16_t)OD_DIVU(accum[idx] + rounding, count[idx]);
        } else {
          buf[frame_idx] = (uint8_t)OD_DIVU(accum[idx] + rounding, count[idx]);
        }
        ++plane_idx;
        ++frame_idx;
      }
      frame_idx += (frame_stride - plane_w);
    }
    plane_offset += mb_pels;
  }
}

// Helper function to compute number of blocks on either side of the frame.
static INLINE int get_num_blocks(const int frame_length, const int mb_length) {
  return (frame_length + mb_length - 1) / mb_length;
}

// Helper function to get motion search range, or say limit of motion vector, on
// either side of the frame.
// TODO(chengchen): Figure out how this function works.
// Previous comments: """
//     Source frames are extended to 16 pixels. This is different than
//      L/A/G reference frames that have a border of 32 (AV1ENCBORDERINPIXELS)
//     A 6/8 tap filter is used for motion search.  This requires 2 pixels
//      before and 3 pixels after.  So the largest Y mv on a border would
//      then be 16 - AOM_INTERP_EXTEND. The UV blocks are half the size of the
//      Y and therefore only extended by 8.  The largest mv that a UV block
//      can support is 8 - AOM_INTERP_EXTEND.  A UV mv is half of a Y mv.
//      (16 - AOM_INTERP_EXTEND) >> 1 which is greater than
//      8 - AOM_INTERP_EXTEND.
//     To keep the mv in play for both Y and UV planes the max that it
//      can be on a border is therefore 16 - (2*AOM_INTERP_EXTEND+1).
// """
static INLINE int get_min_mv(const int block_idx, const int mb_length) {
  return -((block_idx * mb_length) + (17 - 2 * AOM_INTERP_EXTEND));
}
static INLINE int get_max_mv(const int block_reverse_idx, const int mb_length) {
  return (block_reverse_idx * mb_length) + (17 - 2 * AOM_INTERP_EXTEND);
}

typedef struct {
  int64_t sum;
  int64_t sse;
} FRAME_DIFF;

// Does temporal filter for a particular frame.
// Inputs:
//   cpi: Pointer to the composed information of input video.
//   frames: Frame buffers used for temporal filtering.
//   num_frames: Number of frames in the frame buffer.
//   filter_frame_idx: Index of the frame to be filtered.
//   is_key_frame: Whether the to-filter is a key frame.
//   use_second_altref: Whether to filter a second Alternate Reference Frame
//                      (ARF) in the entire temporal filtering pipeline. This
//                      field is ONLY used for assigning filter weight in this
//                      function.
//   block_size: Block size used for temporal filtering.
//   scale: Scaling factor.
//   strength: Pre-estimated strength for filter weight adjustment.
//   noise_level: Noise level of the to-filter frame, estimated with Y plane.
static FRAME_DIFF tf_do_filtering(
    AV1_COMP *cpi, YV12_BUFFER_CONFIG **frames, const int num_frames,
    const int filter_frame_idx, const int is_key_frame,
    const int use_second_altref, const BLOCK_SIZE block_size,
    const struct scale_factors *scale, const int strength,
    const double noise_level) {
  // Basic information.
  const YV12_BUFFER_CONFIG *const frame_to_filter = frames[filter_frame_idx];
  const int frame_height = frame_to_filter->y_crop_height;
  const int frame_width = frame_to_filter->y_crop_width;
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  const int mb_rows = get_num_blocks(frame_height, mb_height);
  const int mb_cols = get_num_blocks(frame_width, mb_width);
  const int num_planes = av1_num_planes(&cpi->common);
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  const int is_high_bitdepth = is_frame_high_bitdepth(frame_to_filter);

  // Save input state.
  MACROBLOCK *const mb = &cpi->td.mb;
  MACROBLOCKD *const mbd = &mb->e_mbd;
  uint8_t *input_buffer[MAX_MB_PLANE];
  for (int i = 0; i < num_planes; i++) {
    input_buffer[i] = mbd->plane[i].pre[0].buf;
  }
  MB_MODE_INFO **input_mb_mode_info = mbd->mi;

  // Setup.
  mbd->block_ref_scale_factors[0] = scale;
  mbd->block_ref_scale_factors[1] = scale;
  // A temporary block info used to store state in temporal filtering process.
  MB_MODE_INFO *tmp_mb_mode_info = (MB_MODE_INFO *)malloc(sizeof(MB_MODE_INFO));
  memset(tmp_mb_mode_info, 0, sizeof(MB_MODE_INFO));
  mbd->mi = &tmp_mb_mode_info;
  mbd->mi[0]->motion_mode = SIMPLE_TRANSLATION;
  // Allocate memory for predictor, accumulator and count.
  uint8_t *pred8 = aom_memalign(32, num_planes * mb_pels * sizeof(uint8_t));
  uint16_t *pred16 = aom_memalign(32, num_planes * mb_pels * sizeof(uint16_t));
  uint32_t *accum = aom_memalign(16, num_planes * mb_pels * sizeof(uint32_t));
  uint16_t *count = aom_memalign(16, num_planes * mb_pels * sizeof(uint16_t));
  memset(pred8, 0, num_planes * mb_pels * sizeof(pred8[0]));
  memset(pred16, 0, num_planes * mb_pels * sizeof(pred16[0]));
  uint8_t *const pred = is_high_bitdepth ? CONVERT_TO_BYTEPTR(pred16) : pred8;

  // Do filtering.
  FRAME_DIFF diff = { 0, 0 };
  const int use_planewise_strategy =
      ENABLE_PLANEWISE_STRATEGY &&
      (cpi->common.allow_screen_content_tools == 0) &&
      AOMMIN(frame_height, frame_width) >= 480;
  // Perform temporal filtering block by block.
  for (int mb_row = 0; mb_row < mb_rows; mb_row++) {
    mb->mv_limits.row_min = get_min_mv(mb_row, mb_height);
    mb->mv_limits.row_max = get_max_mv(mb_rows - 1 - mb_row, mb_height);
    for (int mb_col = 0; mb_col < mb_cols; mb_col++) {
      mb->mv_limits.col_min = get_min_mv(mb_col, mb_width);
      mb->mv_limits.col_max = get_max_mv(mb_cols - 1 - mb_col, mb_width);
      memset(accum, 0, num_planes * mb_pels * sizeof(accum[0]));
      memset(count, 0, num_planes * mb_pels * sizeof(count[0]));
      MV ref_mv = kZeroMv;  // Reference motion vector passed down along frames.
      // Perform temporal filtering frame by frame.
      for (int frame = 0; frame < num_frames; frame++) {
        if (frames[frame] == NULL) continue;

        MV subblock_mvs[4] = { kZeroMv, kZeroMv, kZeroMv, kZeroMv };
        int subblock_filter_weights[4] = { 0, 0, 0, 0 };
        int block_error = INT_MAX;
        int subblock_errors[4] = { INT_MAX, INT_MAX, INT_MAX, INT_MAX };

        if (frame == filter_frame_idx) {  // Frame to be filtered.
          // Set motion vector as 0 for the frame to be filtered.
          mbd->mi[0]->mv[0].as_mv = kZeroMv;
          // Change ref_mv sign for following frames.
          ref_mv.row *= -1;
          ref_mv.col *= -1;
        } else {  // Other reference frames.
          block_error = tf_motion_search(cpi, frame_to_filter, frames[frame],
                                         block_size, mb_row, mb_col, &ref_mv,
                                         subblock_mvs, subblock_errors);
          // Do not pass down the reference motion vector if error is too large.
          if (block_error > (3000 << (mbd->bd - 8))) {
            ref_mv = kZeroMv;
          }
        }
        int use_subblock = tf_get_filter_weight(
            block_error, subblock_errors, use_planewise_strategy,
            use_second_altref, subblock_filter_weights);
        if (subblock_filter_weights[0] || subblock_filter_weights[1] ||
            subblock_filter_weights[2] || subblock_filter_weights[3]) {
          tf_build_predictor(frames[frame], mbd, block_size, mb_row, mb_col,
                             num_planes, scale, use_subblock, subblock_mvs,
                             pred);
          if (frame == filter_frame_idx) {  // Frame to be filtered.
            av1_apply_temporal_filter_self(mbd, block_size, num_planes,
                                           subblock_filter_weights[0], pred,
                                           accum, count);
          } else {
            av1_apply_temporal_filter_others(  // Other reference frames.
                frame_to_filter, mbd, block_size, mb_row, mb_col, num_planes,
                use_planewise_strategy, strength, use_subblock,
                subblock_filter_weights, noise_level, pred, accum, count);
          }
        }
      }

      tf_normalize_filtered_frame(mbd, block_size, mb_row, mb_col, num_planes,
                                  accum, count, &cpi->alt_ref_buffer);

      if (!is_key_frame && cpi->sf.hl_sf.adaptive_overlay_encoding) {
        const int y_height = mb_height >> mbd->plane[0].subsampling_y;
        const int y_width = mb_width >> mbd->plane[0].subsampling_x;
        const int source_y_stride = frame_to_filter->y_stride;
        const int filter_y_stride = cpi->alt_ref_buffer.y_stride;
        const int source_offset =
            mb_row * y_height * source_y_stride + mb_col * y_width;
        const int filter_offset =
            mb_row * y_height * filter_y_stride + mb_col * y_width;
        unsigned int sse = 0;
        cpi->fn_ptr[block_size].vf(frame_to_filter->y_buffer + source_offset,
                                   source_y_stride,
                                   cpi->alt_ref_buffer.y_buffer + filter_offset,
                                   filter_y_stride, &sse);
        diff.sum += sse;
        diff.sse += sse * sse;
      }
    }
  }

  // Restore input state
  for (int i = 0; i < num_planes; i++) {
    mbd->plane[i].pre[0].buf = input_buffer[i];
  }
  mbd->mi = input_mb_mode_info;

  free(tmp_mb_mode_info);
  aom_free(pred8);
  aom_free(pred16);
  aom_free(accum);
  aom_free(count);

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

  FRAME_DIFF diff = tf_do_filtering(
      cpi, frames, frames_to_blur, frames_to_blur_backward, distance < 0,
      second_alt_ref, TF_BLOCK, &sf, strength, sigma);

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
    const int mb_cols =
        get_num_blocks(frames[frames_to_blur_backward]->y_crop_width, BW);
    const int mb_rows =
        get_num_blocks(frames[frames_to_blur_backward]->y_crop_height, BH);
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
