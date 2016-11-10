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

#ifndef AV1_COMMON_MV_H_
#define AV1_COMMON_MV_H_

#include "av1/common/common.h"
#include "aom_dsp/aom_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mv {
  int16_t row;
  int16_t col;
} MV;

typedef union int_mv {
  uint32_t as_int;
  MV as_mv;
} int_mv; /* facilitates faster equality tests and copies */

typedef struct mv32 {
  int32_t row;
  int32_t col;
} MV32;

#if CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION
// Bits of precision used for the model
#define WARPEDMODEL_PREC_BITS 12
#define WARPEDMODEL_ROW3HOMO_PREC_BITS 14

// Bits of subpel precision for warped interpolation
#define WARPEDPIXEL_PREC_BITS 6
#define WARPEDPIXEL_PREC_SHIFTS (1 << WARPEDPIXEL_PREC_BITS)

// Taps for ntap filter
#define WARPEDPIXEL_FILTER_TAPS 6

// Precision of filter taps
#define WARPEDPIXEL_FILTER_BITS 7

#define WARPEDDIFF_PREC_BITS (WARPEDMODEL_PREC_BITS - WARPEDPIXEL_PREC_BITS)

/* clang-format off */
typedef enum {
  IDENTITY = 0,     // identity transformation, 0-parameter
  TRANSLATION = 1,  // translational motion 2-parameter
  ROTZOOM = 2,      // simplified affine with rotation and zoom only, 4-parameter
  AFFINE = 3,       // affine, 6-parameter
  HOMOGRAPHY = 4,   // homography, 8-parameter
  TRANS_TYPES = 5,
} TransformationType;
/* clang-format on */

// Number of types used for global motion (must be <= TRANS_TYPES)
#define GLOBAL_TRANS_TYPES 3

// number of parameters used by each transformation in TransformationTypes
static const int n_trans_model_params[TRANS_TYPES] = { 0, 2, 4, 6, 8 };

// The order of values in the wmmat matrix below is best described
// by the homography:
// [x'     (m2 m3 m0   [x
//  y'  =   m4 m5 m1 *  y
//  1]      m6 m7 1)    1]
typedef struct {
  TransformationType wmtype;
  int32_t wmmat[8];
} WarpedMotionParams;
#endif  // CONFIG_GLOBAL_MOTION || CONFIG_WARPED_MOTION

#if CONFIG_GLOBAL_MOTION
// ALPHA here refers to parameters a and b in rotzoom model:
// | a   b|
// |-b   a|
//
// and a, b, c, d in affine model:
// | a   b|
// | c   d|
//
// Anything ending in PREC_BITS is the number of bits of precision
// to maintain when converting from double to integer.
//
// The ABS parameters are used to create an upper and lower bound
// for each parameter. In other words, after a parameter is integerized
// it is clamped between -(1 << ABS_XXX_BITS) and (1 << ABS_XXX_BITS).
//
// XXX_PREC_DIFF and XXX_DECODE_FACTOR
// are computed once here to prevent repetitive
// computation on the decoder side. These are
// to allow the global motion parameters to be encoded in a lower
// precision than the warped model precision. This means that they
// need to be changed to warped precision when they are decoded.
//
// XX_MIN, XX_MAX are also computed to avoid repeated computation

#define GM_TRANS_PREC_BITS 3
#define GM_TRANS_PREC_DIFF (WARPEDMODEL_PREC_BITS - GM_TRANS_PREC_BITS)
#define GM_TRANS_DECODE_FACTOR (1 << GM_TRANS_PREC_DIFF)

#define GM_ALPHA_PREC_BITS 12
#define GM_ALPHA_PREC_DIFF (WARPEDMODEL_PREC_BITS - GM_ALPHA_PREC_BITS)
#define GM_ALPHA_DECODE_FACTOR (1 << GM_ALPHA_PREC_DIFF)

#define GM_ROW3HOMO_PREC_BITS 14
#define GM_ROW3HOMO_PREC_DIFF \
  (WARPEDMODEL_ROW3HOMO_PREC_BITS - GM_ROW3HOMO_PREC_BITS)
#define GM_ROW3HOMO_DECODE_FACTOR (1 << GM_ROW3HOMO_PREC_DIFF)

#define GM_ABS_TRANS_BITS 9
#define GM_ABS_ALPHA_BITS 9
#define GM_ABS_ROW3HOMO_BITS 9

#define GM_TRANS_MAX (1 << GM_ABS_TRANS_BITS)
#define GM_ALPHA_MAX (1 << GM_ABS_ALPHA_BITS)
#define GM_ROW3HOMO_MAX (1 << GM_ABS_ROW3HOMO_BITS)

#define GM_TRANS_MIN -GM_TRANS_MAX
#define GM_ALPHA_MIN -GM_ALPHA_MAX
#define GM_ROW3HOMO_MIN -GM_ROW3HOMO_MAX

// Convert a global motion translation vector (which may have more bits than a
// regular motion vector) into a motion vector
static INLINE int_mv gm_get_motion_vector(const WarpedMotionParams *gm) {
  int_mv res;
  res.as_mv.row = (int16_t)ROUND_POWER_OF_TWO_SIGNED(gm->wmmat[1],
                                                     WARPEDMODEL_PREC_BITS - 3);
  res.as_mv.col = (int16_t)ROUND_POWER_OF_TWO_SIGNED(gm->wmmat[0],
                                                     WARPEDMODEL_PREC_BITS - 3);
  return res;
}

static INLINE TransformationType get_gmtype(const WarpedMotionParams *gm) {
  // if (gm->wmmat[6] != 0 || gm->wmmat[7] != 0) return HOMOGRAPHY;
  if (gm->wmmat[5] == (1 << WARPEDMODEL_PREC_BITS) && !gm->wmmat[4] &&
      gm->wmmat[2] == (1 << WARPEDMODEL_PREC_BITS) && !gm->wmmat[3]) {
    return ((!gm->wmmat[1] && !gm->wmmat[0]) ? IDENTITY : TRANSLATION);
  }
  if (gm->wmmat[2] == gm->wmmat[5] && gm->wmmat[3] == -gm->wmmat[4])
    return ROTZOOM;
  else
    return AFFINE;
}

static INLINE void set_default_gmparams(WarpedMotionParams *gm) {
  static const int32_t default_gm_params[8] = {
    0, 0, (1 << WARPEDMODEL_PREC_BITS), 0, 0, (1 << WARPEDMODEL_PREC_BITS), 0, 0
  };
  memcpy(gm->wmmat, default_gm_params, sizeof(gm->wmmat));
  gm->wmtype = IDENTITY;
}
#endif  // CONFIG_GLOBAL_MOTION

#if CONFIG_REF_MV
typedef struct candidate_mv {
  int_mv this_mv;
  int_mv comp_mv;
  int_mv pred_mv[2];
  int weight;
} CANDIDATE_MV;
#endif

static INLINE int is_zero_mv(const MV *mv) {
  return *((const uint32_t *)mv) == 0;
}

static INLINE int is_equal_mv(const MV *a, const MV *b) {
  return *((const uint32_t *)a) == *((const uint32_t *)b);
}

static INLINE void clamp_mv(MV *mv, int min_col, int max_col, int min_row,
                            int max_row) {
  mv->col = clamp(mv->col, min_col, max_col);
  mv->row = clamp(mv->row, min_row, max_row);
}

static INLINE int mv_has_subpel(const MV *mv) {
  return (mv->row & SUBPEL_MASK) || (mv->col & SUBPEL_MASK);
}
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_MV_H_
