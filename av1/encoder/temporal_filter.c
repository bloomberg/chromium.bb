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

#include "config/aom_config.h"

#include "av1/common/alloccommon.h"
#include "av1/common/av1_common_int.h"
#include "av1/common/odintrin.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconinter.h"
#include "av1/encoder/av1_quantize.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/mcomp.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/temporal_filter.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/mem.h"
#include "aom_ports/system_state.h"
#include "aom_scale/aom_scale.h"

// NOTE: All `tf` in this file means `temporal filtering`.

// Forward Declaration.
static void tf_determine_block_partition(const MV block_mv, const int block_mse,
                                         MV *subblock_mvs, int *subblock_mses);

// Does motion search for blocks in temporal filtering. This is the first step
// for temporal filtering. More specifically, given a frame to be filtered and
// another frame as reference, this function searches the reference frame to
// find out the most similar block as that from the frame to be filtered. This
// found block will be further used for weighted averaging.
// NOTE: Besides doing motion search for the entire block, this function will
//       also do motion search for each 1/4 sub-block to get more precise
//       predictions. Then, this function will determines whether to use 4
//       sub-blocks to replace the entire block. If we do need to split the
//       entire block, 4 elements in `subblock_mvs` and `subblock_mses` refer to
//       the searched motion vector and search error (MSE) w.r.t. each sub-block
//       respectively. Otherwise, the 4 elements will be the same, all of which
//       are assigned as the searched motion vector and search error (MSE) for
//       the entire block.
// Inputs:
//   cpi: Pointer to the composed information of input video.
//   frame_to_filter: Pointer to the frame to be filtered.
//   ref_frame: Pointer to the reference frame.
//   block_size: Block size used for motion search.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   ref_mv: Reference motion vector, which is commonly inherited from the
//           motion search result of previous frame.
//   subblock_mvs: Pointer to the motion vectors for 4 sub-blocks.
//   subblock_mses: Pointer to the search errors (MSE) for 4 sub-blocks.
// Returns:
//   Nothing will be returned. Results are saved in `subblock_mvs` and
//   `subblock_mses`.
static void tf_motion_search(AV1_COMP *cpi,
                             const YV12_BUFFER_CONFIG *frame_to_filter,
                             const YV12_BUFFER_CONFIG *ref_frame,
                             const BLOCK_SIZE block_size, const int mb_row,
                             const int mb_col, MV *ref_mv, MV *subblock_mvs,
                             int *subblock_mses) {
  // Frame information
  const int min_frame_size = AOMMIN(cpi->common.width, cpi->common.height);

  // Block information (ONLY Y-plane is used for motion search).
  const int mb_height = block_size_high[block_size];
  const int mb_width = block_size_wide[block_size];
  const int mb_pels = mb_height * mb_width;
  const int y_stride = frame_to_filter->y_stride;
  assert(y_stride == ref_frame->y_stride);
  const int y_offset = mb_row * mb_height * y_stride + mb_col * mb_width;

  // Save input state.
  MACROBLOCK *const mb = &cpi->td.mb;
  MACROBLOCKD *const mbd = &mb->e_mbd;
  const struct buf_2d ori_src_buf = mb->plane[0].src;
  const struct buf_2d ori_pre_buf = mbd->plane[0].pre[0];

  // Parameters used for motion search.
  FULLPEL_MOTION_SEARCH_PARAMS full_ms_params;
  SUBPEL_MOTION_SEARCH_PARAMS ms_params;
  const search_site_config search_site_cfg =
      cpi->mv_search_params.search_site_cfg[SS_CFG_LOOKAHEAD];
  const SEARCH_METHODS full_search_method = NSTEP;
  const int step_param = av1_init_search_range(
      AOMMAX(frame_to_filter->y_crop_width, frame_to_filter->y_crop_height));
  const SUBPEL_SEARCH_TYPE subpel_search_type = USE_8_TAPS;
  const int force_integer_mv = cpi->common.features.cur_frame_force_integer_mv;
  const MV_COST_TYPE mv_cost_type =
      min_frame_size >= 720
          ? MV_COST_L1_HDRES
          : (min_frame_size >= 480 ? MV_COST_L1_MIDRES : MV_COST_L1_LOWRES);

  // Starting position for motion search.
  FULLPEL_MV start_mv = get_fullmv_from_mv(ref_mv);
  // Baseline position for motion search (used for rate distortion comparison).
  const MV baseline_mv = kZeroMv;

  // Setup.
  mb->plane[0].src.buf = frame_to_filter->y_buffer + y_offset;
  mb->plane[0].src.stride = y_stride;
  mbd->plane[0].pre[0].buf = ref_frame->y_buffer + y_offset;
  mbd->plane[0].pre[0].stride = y_stride;
  // Unused intermediate results for motion search.
  unsigned int sse, error;
  int distortion;
  int cost_list[5];

  // Do motion search.
  int_mv best_mv;  // Searched motion vector.
  int block_mse = INT_MAX;
  MV block_mv = kZeroMv;

  av1_make_default_fullpel_ms_params(&full_ms_params, cpi, mb, block_size,
                                     &baseline_mv, &search_site_cfg,
                                     /*fine_search_interval=*/0);
  full_ms_params.run_mesh_search = 1;
  full_ms_params.search_method = full_search_method;
  full_ms_params.mv_cost_params.mv_cost_type = mv_cost_type;

  av1_full_pixel_search(start_mv, &full_ms_params, step_param,
                        cond_cost_list(cpi, cost_list), &best_mv.as_fullmv,
                        NULL);

  if (force_integer_mv == 1) {  // Only do full search on the entire block.
    const int mv_row = best_mv.as_mv.row;
    const int mv_col = best_mv.as_mv.col;
    best_mv.as_mv.row = GET_MV_SUBPEL(mv_row);
    best_mv.as_mv.col = GET_MV_SUBPEL(mv_col);
    const int mv_offset = mv_row * y_stride + mv_col;
    error = cpi->fn_ptr[block_size].vf(
        ref_frame->y_buffer + y_offset + mv_offset, y_stride,
        frame_to_filter->y_buffer + y_offset, y_stride, &sse);
    block_mse = DIVIDE_AND_ROUND(error, mb_pels);
    block_mv = best_mv.as_mv;
  } else {  // Do fractional search on the entire block and all sub-blocks.
    av1_make_default_subpel_ms_params(&ms_params, cpi, mb, block_size,
                                      &baseline_mv, cost_list);
    ms_params.forced_stop = EIGHTH_PEL;
    ms_params.var_params.subpel_search_type = subpel_search_type;
    // Since we are merely refining the result from full pixel search, we don't
    // need regularization for subpel search
    ms_params.mv_cost_params.mv_cost_type = MV_COST_NONE;

    MV subpel_start_mv = get_mv_from_fullmv(&best_mv.as_fullmv);
    error = cpi->mv_search_params.find_fractional_mv_step(
        &mb->e_mbd, &cpi->common, &ms_params, subpel_start_mv, &best_mv.as_mv,
        &distortion, &sse, NULL);
    block_mse = DIVIDE_AND_ROUND(error, mb_pels);
    block_mv = best_mv.as_mv;
    *ref_mv = best_mv.as_mv;
    // On 4 sub-blocks.
    const BLOCK_SIZE subblock_size = ss_size_lookup[block_size][1][1];
    const int subblock_height = block_size_high[subblock_size];
    const int subblock_width = block_size_wide[subblock_size];
    const int subblock_pels = subblock_height * subblock_width;
    start_mv = get_fullmv_from_mv(ref_mv);

    int subblock_idx = 0;
    for (int i = 0; i < mb_height; i += subblock_height) {
      for (int j = 0; j < mb_width; j += subblock_width) {
        const int offset = i * y_stride + j;
        mb->plane[0].src.buf = frame_to_filter->y_buffer + y_offset + offset;
        mbd->plane[0].pre[0].buf = ref_frame->y_buffer + y_offset + offset;

        av1_make_default_fullpel_ms_params(&full_ms_params, cpi, mb,
                                           subblock_size, &baseline_mv,
                                           &search_site_cfg,
                                           /*fine_search_interval=*/0);
        full_ms_params.run_mesh_search = 1;
        full_ms_params.search_method = full_search_method;
        full_ms_params.mv_cost_params.mv_cost_type = mv_cost_type;

        av1_full_pixel_search(start_mv, &full_ms_params, step_param,
                              cond_cost_list(cpi, cost_list),
                              &best_mv.as_fullmv, NULL);

        av1_make_default_subpel_ms_params(&ms_params, cpi, mb, subblock_size,
                                          &baseline_mv, cost_list);
        ms_params.forced_stop = EIGHTH_PEL;
        ms_params.var_params.subpel_search_type = subpel_search_type;
        // Since we are merely refining the result from full pixel search, we
        // don't need regularization for subpel search
        ms_params.mv_cost_params.mv_cost_type = MV_COST_NONE;

        subpel_start_mv = get_mv_from_fullmv(&best_mv.as_fullmv);
        error = cpi->mv_search_params.find_fractional_mv_step(
            &mb->e_mbd, &cpi->common, &ms_params, subpel_start_mv,
            &best_mv.as_mv, &distortion, &sse, NULL);
        subblock_mses[subblock_idx] = DIVIDE_AND_ROUND(error, subblock_pels);
        subblock_mvs[subblock_idx] = best_mv.as_mv;
        ++subblock_idx;
      }
    }
  }

  // Restore input state.
  mb->plane[0].src = ori_src_buf;
  mbd->plane[0].pre[0] = ori_pre_buf;

  // Make partition decision.
  tf_determine_block_partition(block_mv, block_mse, subblock_mvs,
                               subblock_mses);

  // Do not pass down the reference motion vector if error is too large.
  const int thresh = (min_frame_size >= 720) ? 12 : 3;
  if (block_mse > (thresh << (mbd->bd - 8))) {
    *ref_mv = kZeroMv;
  }
}

// Determines whether to split the entire block to 4 sub-blocks for filtering.
// In particular, this decision is made based on the comparison between the
// motion search error of the entire block and the errors of all sub-blocks.
// Inputs:
//   block_mv: Motion vector for the entire block (ONLY as reference).
//   block_mse: Motion search error (MSE) for the entire block (ONLY as
//              reference).
//   subblock_mvs: Pointer to the motion vectors for 4 sub-blocks (will be
//                 modified based on the partition decision).
//   subblock_mses: Pointer to the search errors (MSE) for 4 sub-blocks (will
//                  be modified based on the partition decision).
// Returns:
//   Nothing will be returned. Results are saved in `subblock_mvs` and
//   `subblock_mses`.
static void tf_determine_block_partition(const MV block_mv, const int block_mse,
                                         MV *subblock_mvs, int *subblock_mses) {
  int min_subblock_mse = INT_MAX;
  int max_subblock_mse = INT_MIN;
  int sum_subblock_mse = 0;
  for (int i = 0; i < 4; ++i) {
    sum_subblock_mse += subblock_mses[i];
    min_subblock_mse = AOMMIN(min_subblock_mse, subblock_mses[i]);
    max_subblock_mse = AOMMAX(max_subblock_mse, subblock_mses[i]);
  }

  // TODO(any): The following magic numbers may be tuned to improve the
  // performance OR find a way to get rid of these magic numbers.
  if (((block_mse * 15 < sum_subblock_mse * 4) &&
       max_subblock_mse - min_subblock_mse < 48) ||
      ((block_mse * 14 < sum_subblock_mse * 4) &&
       max_subblock_mse - min_subblock_mse < 24)) {  // No split.
    for (int i = 0; i < 4; ++i) {
      subblock_mvs[i] = block_mv;
      subblock_mses[i] = block_mse;
    }
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
                               const MV *subblock_mvs, uint8_t *pred) {
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);

  // Information of the entire block.
  const int mb_height = block_size_high[block_size];  // Height.
  const int mb_width = block_size_wide[block_size];   // Width.
  const int mb_pels = mb_height * mb_width;           // Number of pixels.
  const int mb_y = mb_height * mb_row;                // Y-coord (Top-left).
  const int mb_x = mb_width * mb_col;                 // X-coord (Top-left).
  const int bit_depth = mbd->bd;                      // Bit depth.
  const int is_intrabc = 0;                           // Is intra-copied?
  const int is_high_bitdepth = is_frame_high_bitdepth(ref_frame);

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
    const int h = plane_h >> 1;                      // Sub-block height.
    const int w = plane_w >> 1;                      // Sub-block width.
    const int is_y_plane = (plane == 0);             // Is Y-plane?

    const struct buf_2d ref_buf = { NULL, ref_frame->buffers[plane],
                                    ref_frame->widths[is_y_plane ? 0 : 1],
                                    ref_frame->heights[is_y_plane ? 0 : 1],
                                    ref_frame->strides[is_y_plane ? 0 : 1] };

    // Handle each subblock.
    int subblock_idx = 0;
    for (int i = 0; i < plane_h; i += h) {
      for (int j = 0; j < plane_w; j += w) {
        // Choose proper motion vector.
        const MV mv = subblock_mvs[subblock_idx++];
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
        av1_enc_build_one_inter_predictor(&pred[plane_offset + i * plane_w + j],
                                          plane_w, &mv, &inter_pred_params);
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
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void tf_apply_temporal_filter_self(const MACROBLOCKD *mbd,
                                   const BLOCK_SIZE block_size,
                                   const int num_planes, const uint8_t *pred,
                                   uint32_t *accum, uint16_t *count) {
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
        accum[idx] += TF_WEIGHT_SCALE * pred_value;
        count[idx] += TF_WEIGHT_SCALE;
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

// Applies temporal filtering.
// Inputs:
//   frame_to_filter: Pointer to the frame to be filtered, which is used as
//                    reference to compute squared differece from the predictor.
//   mbd: Pointer to the block for filtering, which is ONLY used to get
//        subsampling information of all planes.
//   block_size: Size of the block.
//   mb_row: Row index of the block in the entire frame.
//   mb_col: Column index of the block in the entire frame.
//   num_planes: Number of planes in the frame.
//   noise_levels: Pointer to the noise levels of the to-filter frame, estimated
//                 with each plane (in Y, U, V order).
//   subblock_mses: Pointer to the search errors (MSE) for 4 sub-blocks.
//   q_factor: Quantization factor. This is actually the `q` defined in libaom,
//             which is converted from `qindex`.
//   filter_strength: Filtering strength. This value lies in range [0, 6], where
//                    6 is the maximum filtering strength.
//   pred: Pointer to the well-built predictors.
//   accum: Pointer to the pixel-wise accumulator for filtering.
//   count: Pointer to the pixel-wise counter fot filtering.
// Returns:
//   Nothing will be returned. But the content to which `accum` and `pred`
//   point will be modified.
void av1_apply_temporal_filter_c(const YV12_BUFFER_CONFIG *frame_to_filter,
                                 const MACROBLOCKD *mbd,
                                 const BLOCK_SIZE block_size, const int mb_row,
                                 const int mb_col, const int num_planes,
                                 const double *noise_levels,
                                 const int *subblock_mses, const int q_factor,
                                 const int filter_strength, const uint8_t *pred,
                                 uint32_t *accum, uint16_t *count) {
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
  assert(TF_WINDOW_LENGTH % 2 == 1);
  const int half_window = TF_WINDOW_LENGTH >> 1;

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

        // Filter U-plane and V-plane using Y-plane. This is because motion
        // search is only done on Y-plane, so the information from Y-plane will
        // be more accurate.
        if (plane != 0) {
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

        // Scale down the difference for high bit depth input.
        if (mbd->bd > 8) sum_square_diff >>= (mbd->bd - 8) * (mbd->bd - 8);
        const double window_error = (double)sum_square_diff / num_ref_pixels;
        const int subblock_idx = (i >= h / 2) * 2 + (j >= w / 2);
        const double block_error = (double)subblock_mses[subblock_idx];
        // Combine window error and block error, and normalize it.
        const double combined_error =
            (TF_WINDOW_BLOCK_BALANCE_WEIGHT * window_error + block_error) /
            (TF_WINDOW_BLOCK_BALANCE_WEIGHT + 1) / TF_SEARCH_ERROR_NORM_WEIGHT;

        // Decay factor for non-local mean approach.
        // Larger noise -> larger filtering weight.
        const double n_decay = 0.5 + log(2 * noise_levels[plane] + 5.0);
        // Smaller q -> smaller filtering weight.
        const double q_decay =
            CLIP(pow((double)q_factor / TF_Q_DECAY_THRESHOLD, 2), 1e-5, 1);
        // Smaller strength -> smaller filtering weight.
        const double s_decay = CLIP(
            pow((double)filter_strength / TF_STRENGTH_THRESHOLD, 2), 1e-5, 1);

        // Compute filter weight.
        const double scaled_error =
            AOMMIN(combined_error / n_decay / q_decay / s_decay, 7);
        const int weight = (int)(exp(-scaled_error) * TF_WEIGHT_SCALE);

        const int idx = plane_offset + pred_idx;  // Index with plane shift.
        const int pred_value = is_high_bitdepth ? pred16[idx] : pred[idx];
        accum[idx] += weight * pred_value;
        count[idx] += weight;

        ++pred_idx;
      }
    }
    plane_offset += mb_pels;
  }

  aom_free(square_diff);
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
//   count: Pointer to the pre-computed count.
//   result_buffer: Pointer to result buffer.
// Returns:
//   Nothing will be returned. But the content to which `result_buffer` point
//   will be modified.
static void tf_normalize_filtered_frame(
    const MACROBLOCKD *mbd, const BLOCK_SIZE block_size, const int mb_row,
    const int mb_col, const int num_planes, const uint32_t *accum,
    const uint16_t *count, YV12_BUFFER_CONFIG *result_buffer) {
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
//   block_size: Block size used for temporal filtering.
//   scale: Scaling factor.
//   noise_levels: Pointer to the noise levels of the to-filter frame, estimated
//                 with each plane (in Y, U, V order).
// Returns:
//   Difference between filtered frame and the original frame.
static FRAME_DIFF tf_do_filtering(AV1_COMP *cpi, YV12_BUFFER_CONFIG **frames,
                                  const int num_frames,
                                  const int filter_frame_idx,
                                  const int is_key_frame,
                                  const BLOCK_SIZE block_size,
                                  const struct scale_factors *scale,
                                  const double *noise_levels) {
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
  const int mi_h = mi_size_high_log2[block_size];
  const int mi_w = mi_size_wide_log2[block_size];
  assert(num_planes >= 1 && num_planes <= MAX_MB_PLANE);
  const int is_high_bitdepth = is_frame_high_bitdepth(frame_to_filter);

  // Quantization factor used in temporal filtering.
  const FRAME_TYPE frame_type =
      (cpi->common.current_frame.frame_number > 1) ? INTER_FRAME : KEY_FRAME;
  const int q_factor = (int)av1_convert_qindex_to_q(
      cpi->rc.avg_frame_qindex[frame_type], cpi->common.seq_params.bit_depth);
  // Factor to control the filering strength.
  const int filter_strength = cpi->oxcf.arnr_strength;

  // Save input state.
  MACROBLOCK *const mb = &cpi->td.mb;
  MACROBLOCKD *const mbd = &mb->e_mbd;
  uint8_t *input_buffer[MAX_MB_PLANE];
  for (int i = 0; i < num_planes; i++) {
    input_buffer[i] = mbd->plane[i].pre[0].buf;
  }
  MB_MODE_INFO **input_mb_mode_info = mbd->mi;

  // Determine whether the video is with `YUV 4:2:2` format, since the avx2/sse2
  // function only supports square block size. We will use C function instead
  // for videos with `YUV 4:2:2` format.
  int is_yuv422_format = 0;
  for (int plane = 1; plane < num_planes; ++plane) {
    if (mbd->plane[plane].subsampling_x != mbd->plane[plane].subsampling_y) {
      is_yuv422_format = 1;
      break;
    }
  }

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
  // Perform temporal filtering block by block.
  for (int mb_row = 0; mb_row < mb_rows; mb_row++) {
    av1_set_mv_row_limits(&cpi->common.mi_params, &mb->mv_limits,
                          (mb_row << mi_h), (mb_height >> MI_SIZE_LOG2),
                          cpi->oxcf.border_in_pixels);
    for (int mb_col = 0; mb_col < mb_cols; mb_col++) {
      av1_set_mv_col_limits(&cpi->common.mi_params, &mb->mv_limits,
                            (mb_col << mi_w), (mb_width >> MI_SIZE_LOG2),
                            cpi->oxcf.border_in_pixels);
      memset(accum, 0, num_planes * mb_pels * sizeof(accum[0]));
      memset(count, 0, num_planes * mb_pels * sizeof(count[0]));
      MV ref_mv = kZeroMv;  // Reference motion vector passed down along frames.
      // Perform temporal filtering frame by frame.
      for (int frame = 0; frame < num_frames; frame++) {
        if (frames[frame] == NULL) continue;

        // Motion search.
        MV subblock_mvs[4] = { kZeroMv, kZeroMv, kZeroMv, kZeroMv };
        int subblock_mses[4] = { INT_MAX, INT_MAX, INT_MAX, INT_MAX };
        if (frame == filter_frame_idx) {  // Frame to be filtered.
          // Change ref_mv sign for following frames.
          ref_mv.row *= -1;
          ref_mv.col *= -1;
        } else {  // Other reference frames.
          tf_motion_search(cpi, frame_to_filter, frames[frame], block_size,
                           mb_row, mb_col, &ref_mv, subblock_mvs,
                           subblock_mses);
        }
        tf_build_predictor(frames[frame], mbd, block_size, mb_row, mb_col,
                           num_planes, scale, subblock_mvs, pred);

        // Perform weighted averaging.
        if (frame == filter_frame_idx) {  // Frame to be filtered.
          tf_apply_temporal_filter_self(mbd, block_size, num_planes, pred,
                                        accum, count);
        } else {  // Other reference frames.
          // TODO(any): avx2/sse2 version should be changed to align with C
          // function before using. In particular, current avx2/sse2 function
          // only supports 32x32 block size, 5x5 filtering window, 8-bit
          // encoding, and the case when the video is not with `YUV 4:2:2`
          // format.
          if (TF_BLOCK_SIZE == BLOCK_32X32 && TF_WINDOW_LENGTH == 5 &&
              !is_frame_high_bitdepth(frame_to_filter) && !is_yuv422_format) {
            av1_apply_temporal_filter(frame_to_filter, mbd, block_size, mb_row,
                                      mb_col, num_planes, noise_levels,
                                      subblock_mses, q_factor, filter_strength,
                                      pred, accum, count);
          } else {
            av1_apply_temporal_filter_c(frame_to_filter, mbd, block_size,
                                        mb_row, mb_col, num_planes,
                                        noise_levels, subblock_mses, q_factor,
                                        filter_strength, pred, accum, count);
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

// Setups the frame buffer for temporal filtering. Basically, this fuction
// determines how many frames will be used for temporal filtering and then
// groups them into a buffer.
// Inputs:
//   cpi: Pointer to the composed information of input video.
//   filter_frame_lookahead_idx: The index of the to-filter frame in the
//                               lookahead buffer `cpi->lookahead`.
//   is_second_arf: Whether the to-filter frame is the second ARF. This field
//                  will affect the number of frames used for filtering.
//   frames: Pointer to the frame buffer to setup.
//   num_frames_for_filtering: Number of frames used for filtering.
//   filter_frame_idx: Index of the to-filter frame in the setup frame buffer.
// Returns:
//   Nothing will be returned. But the frame buffer `frames`, number of frames
//   in the buffer `num_frames_for_filtering`, and the index of the to-filter
//   frame in the buffer `filter_frame_idx` will be updated in this function.
static void tf_setup_filtering_buffer(const AV1_COMP *cpi,
                                      const int filter_frame_lookahead_idx,
                                      const int is_second_arf,
                                      YV12_BUFFER_CONFIG **frames,
                                      int *num_frames_for_filtering,
                                      int *filter_frame_idx) {
  int num_frames = 0;          // Number of frames used for filtering.
  int num_frames_before = -1;  // Number of frames before the to-filter frame.
  int filter_frame_offset;

  if (filter_frame_lookahead_idx == -1) {  // Key frame.
    num_frames = TF_NUM_FILTERING_FRAMES_FOR_KEY_FRAME;
    num_frames_before = 0;
    filter_frame_offset = filter_frame_lookahead_idx;
  } else if (filter_frame_lookahead_idx < -1) {  // Key frame in one-pass mode.
    num_frames = TF_NUM_FILTERING_FRAMES_FOR_KEY_FRAME;
    num_frames_before = num_frames - 1;
    filter_frame_offset = -filter_frame_lookahead_idx;
  } else {
    // Set `arnr_max_frames` as 1 to disable temporal filtering.
    num_frames = cpi->oxcf.arnr_max_frames;
    if (is_second_arf) {  // Only use 2 neighbours for the second ARF.
      num_frames = AOMMIN(num_frames, 3);
    }
    if (num_frames > cpi->rc.gfu_boost / 150) {
      num_frames = cpi->rc.gfu_boost / 150;
      num_frames += !(num_frames & 1);
    }
    num_frames_before = AOMMIN(num_frames >> 1, filter_frame_lookahead_idx + 1);
    const int lookahead_depth =
        av1_lookahead_depth(cpi->lookahead, cpi->compressor_stage);
    const int num_frames_after =
        AOMMIN((num_frames - 1) >> 1,
               lookahead_depth - filter_frame_lookahead_idx - 1);
    num_frames = num_frames_before + 1 + num_frames_after;
    filter_frame_offset = filter_frame_lookahead_idx;
  }
  *num_frames_for_filtering = num_frames;
  *filter_frame_idx = num_frames_before;

  // Setup the frame buffer.
  for (int frame = 0; frame < num_frames; ++frame) {
    const int lookahead_idx = frame - num_frames_before + filter_frame_offset;
    struct lookahead_entry *buf = av1_lookahead_peek(
        cpi->lookahead, lookahead_idx, cpi->compressor_stage);
    frames[frame] = (buf == NULL) ? NULL : &buf->img;
  }
}

// A constant number, sqrt(pi / 2),  used for noise estimation.
static const double SQRT_PI_BY_2 = 1.25331413732;

double av1_estimate_noise_from_single_plane(const YV12_BUFFER_CONFIG *frame,
                                            const int plane,
                                            const int bit_depth) {
  const int is_y_plane = (plane == 0);
  const int height = frame->crop_heights[is_y_plane ? 0 : 1];
  const int width = frame->crop_widths[is_y_plane ? 0 : 1];
  const int stride = frame->strides[is_y_plane ? 0 : 1];
  const uint8_t *src = frame->buffers[plane];
  const uint16_t *src16 = CONVERT_TO_SHORTPTR(src);
  const int is_high_bitdepth = is_frame_high_bitdepth(frame);

  int64_t accum = 0;
  int count = 0;
  for (int i = 1; i < height - 1; ++i) {
    for (int j = 1; j < width - 1; ++j) {
      // Setup a small 3x3 matrix.
      const int center_idx = i * stride + j;
      int mat[3][3];
      for (int ii = -1; ii <= 1; ++ii) {
        for (int jj = -1; jj <= 1; ++jj) {
          const int idx = center_idx + ii * stride + jj;
          mat[ii + 1][jj + 1] = is_high_bitdepth ? src16[idx] : src[idx];
        }
      }
      // Compute sobel gradients.
      const int Gx = (mat[0][0] - mat[0][2]) + (mat[2][0] - mat[2][2]) +
                     2 * (mat[1][0] - mat[1][2]);
      const int Gy = (mat[0][0] - mat[2][0]) + (mat[0][2] - mat[2][2]) +
                     2 * (mat[0][1] - mat[2][1]);
      const int Ga = ROUND_POWER_OF_TWO(abs(Gx) + abs(Gy), bit_depth - 8);
      // Accumulate Laplacian.
      if (Ga < NOISE_ESTIMATION_EDGE_THRESHOLD) {  // Only count smooth pixels.
        const int v = 4 * mat[1][1] -
                      2 * (mat[0][1] + mat[2][1] + mat[1][0] + mat[1][2]) +
                      (mat[0][0] + mat[0][2] + mat[2][0] + mat[2][2]);
        accum += ROUND_POWER_OF_TWO(abs(v), bit_depth - 8);
        ++count;
      }
    }
  }

  // Return -1.0 (unreliable estimation) if there are too few smooth pixels.
  return (count < 16) ? -1.0 : (double)accum / (6 * count) * SQRT_PI_BY_2;
}

int av1_temporal_filter(AV1_COMP *cpi, const int filter_frame_lookahead_idx,
                        int *show_existing_arf) {
  // Basic informaton of the current frame.
  const GF_GROUP *const gf_group = &cpi->gf_group;
  const uint8_t group_idx = gf_group->index;
  const FRAME_UPDATE_TYPE update_type = gf_group->update_type[group_idx];
  // Filter one more ARF if the lookahead index is leq 7 (w.r.t. 9-th frame).
  // This frame is ALWAYS a show existing frame.
  const int is_second_arf = (update_type == INTNL_ARF_UPDATE) &&
                            (filter_frame_lookahead_idx >= 7) &&
                            cpi->sf.hl_sf.second_alt_ref_filtering;
  // TODO(anyone): Currently, we enforce the filtering strength on internal
  // ARFs except the second ARF to be zero. We should investigate in which case
  // it is more beneficial to use non-zero strength filtering.
  if (update_type == INTNL_ARF_UPDATE && !is_second_arf) {
    return 0;
  }

  // TODO(yunqing): For INTNL_ARF_UPDATE type, the following me initialization
  // is used somewhere unexpectedly. Should be resolved later.
  // Initialize errorperbit and sadperbit
  const int rdmult = av1_compute_rd_mult_based_on_qindex(cpi, TF_QINDEX);
  MvCostInfo *mv_cost_info = &cpi->td.mb.mv_cost_info;
  av1_set_error_per_bit(mv_cost_info, rdmult);
  av1_set_sad_per_bit(cpi, mv_cost_info, TF_QINDEX);
  av1_fill_mv_costs(cpi->common.fc,
                    cpi->common.features.cur_frame_force_integer_mv,
                    cpi->common.features.allow_high_precision_mv, mv_cost_info);

  // Setup frame buffer for filtering.
  YV12_BUFFER_CONFIG *frames[MAX_LAG_BUFFERS] = { NULL };
  int num_frames_for_filtering = 0;
  int filter_frame_idx = -1;
  tf_setup_filtering_buffer(cpi, filter_frame_lookahead_idx, is_second_arf,
                            frames, &num_frames_for_filtering,
                            &filter_frame_idx);

  // Estimate noise.
  const int bit_depth = cpi->common.seq_params.bit_depth;
  const int num_planes = av1_num_planes(&cpi->common);
  double noise_levels[MAX_MB_PLANE] = { 0 };
  for (int plane = 0; plane < num_planes; ++plane) {
    noise_levels[plane] = av1_estimate_noise_from_single_plane(
        frames[filter_frame_idx], plane, bit_depth);
  }

  // Set showable frame.
  if (filter_frame_lookahead_idx >= 0) {
    cpi->common.showable_frame =
        num_frames_for_filtering == 1 || is_second_arf ||
        (cpi->oxcf.enable_overlay == 0 || cpi->sf.hl_sf.disable_overlay_frames);
  }

  // Do filtering.
  const int is_key_frame = (filter_frame_lookahead_idx < 0);
  FRAME_DIFF diff = { 0, 0 };
  if (num_frames_for_filtering > 0 && frames[0] != NULL) {
    // Setup scaling factors. Scaling on each of the arnr frames is not
    // supported.
    // ARF is produced at the native frame size and resized when coded.
    struct scale_factors sf;
    av1_setup_scale_factors_for_frame(
        &sf, frames[0]->y_crop_width, frames[0]->y_crop_height,
        frames[0]->y_crop_width, frames[0]->y_crop_height);
    diff =
        tf_do_filtering(cpi, frames, num_frames_for_filtering, filter_frame_idx,
                        is_key_frame, TF_BLOCK_SIZE, &sf, noise_levels);
  }

  if (is_key_frame) {  // Key frame should always be filtered.
    return 1;
  }

  if ((show_existing_arf != NULL && cpi->sf.hl_sf.adaptive_overlay_encoding) ||
      is_second_arf) {
    const int frame_height = frames[filter_frame_idx]->y_crop_height;
    const int frame_width = frames[filter_frame_idx]->y_crop_width;
    const int block_height = block_size_high[TF_BLOCK_SIZE];
    const int block_width = block_size_wide[TF_BLOCK_SIZE];
    const int mb_rows = get_num_blocks(frame_height, block_height);
    const int mb_cols = get_num_blocks(frame_width, block_width);
    const int num_mbs = AOMMAX(1, mb_rows * mb_cols);
    const float mean = (float)diff.sum / num_mbs;
    const float std = (float)sqrt((float)diff.sse / num_mbs - mean * mean);

    aom_clear_system_state();
    // TODO(yunqing): This can be combined with TPL q calculation later.
    cpi->rc.base_frame_target = gf_group->bit_allocation[group_idx];
    av1_set_target_rate(cpi, cpi->common.width, cpi->common.height);
    int top_index = 0;
    int bottom_index = 0;
    const int q = av1_rc_pick_q_and_bounds(cpi, &cpi->rc, cpi->oxcf.width,
                                           cpi->oxcf.height, group_idx,
                                           &bottom_index, &top_index);
    const int ac_q = av1_ac_quant_QTX(q, 0, bit_depth);
    const float threshold = 0.7f * ac_q * ac_q;

    if (!is_second_arf) {
      *show_existing_arf = 0;
      if (mean < threshold && std < mean * 1.2) {
        *show_existing_arf = 1;
      }
      cpi->common.showable_frame |= *show_existing_arf;
    } else {
      // Use source frame if the filtered frame becomes very different.
      if (!(mean < threshold && std < mean * 1.2)) {
        return 0;
      }
    }
  }

  return 1;
}
