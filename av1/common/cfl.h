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

#ifndef AV1_COMMON_CFL_H_
#define AV1_COMMON_CFL_H_

#include "av1/common/blockd.h"

typedef void (*cfl_subsample_lbd_fn)(const uint8_t *input, int input_stride,
                                     int16_t *output_q3, int width, int height);

typedef void (*cfl_subsample_hbd_fn)(const uint16_t *input, int input_stride,
                                     int16_t *output_q3, int width, int height);

typedef void (*cfl_subtract_average_fn)(int16_t *pred_buf_q3);

typedef void (*cfl_predict_lbd_fn)(const int16_t *pred_buf_q3, uint8_t *dst,
                                   int dst_stride, TX_SIZE tx_size,
                                   int alpha_q3);

typedef void (*cfl_predict_hbd_fn)(const int16_t *pred_buf_q3, uint16_t *dst,
                                   int dst_stride, TX_SIZE tx_size,
                                   int alpha_q3, int bd);

static INLINE CFL_ALLOWED_TYPE is_cfl_allowed(const MB_MODE_INFO *mbmi) {
  const BLOCK_SIZE bsize = mbmi->sb_type;
  assert(bsize < BLOCK_SIZES_ALL);
#if CONFIG_EXT_PARTITION_TYPES && CONFIG_RECT_TX_EXT_INTRA
  return (CFL_ALLOWED_TYPE)(block_size_wide[bsize] <= 32 &&
                            block_size_high[bsize] <= 32);
#else
  return (CFL_ALLOWED_TYPE)(bsize <= CFL_MAX_BLOCK_SIZE);
#endif  // CONFIG_EXT_PARTITION_TYPES && CONFIG_RECT_TX_EXT_INTRA
}

static INLINE int get_scaled_luma_q0(int alpha_q3, int16_t pred_buf_q3) {
  int scaled_luma_q6 = alpha_q3 * pred_buf_q3;
  return ROUND_POWER_OF_TWO_SIGNED(scaled_luma_q6, 6);
}

static INLINE CFL_PRED_TYPE get_cfl_pred_type(PLANE_TYPE plane) {
  assert(plane > 0);
  return (CFL_PRED_TYPE)(plane - 1);
}

void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       TX_SIZE tx_size, int plane);

void cfl_store_block(MACROBLOCKD *const xd, BLOCK_SIZE bsize, TX_SIZE tx_size);

void cfl_store_tx(MACROBLOCKD *const xd, int row, int col, TX_SIZE tx_size,
                  BLOCK_SIZE bsize);

void cfl_store_dc_pred(MACROBLOCKD *const xd, const uint8_t *input,
                       CFL_PRED_TYPE pred_plane, int width);

void cfl_load_dc_pred(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                      TX_SIZE tx_size, CFL_PRED_TYPE pred_plane);

// TODO(ltrudeau) Remove this when HBD 420 SIMD is added
void cfl_luma_subsampling_420_hbd_c(const uint16_t *input, int input_stride,
                                    int16_t *output_q3, int width, int height);

// TODO(ltrudeau) Remove this when HBD 422 SIMD is added
void cfl_luma_subsampling_422_hbd_c(const uint16_t *input, int input_stride,
                                    int16_t *output_q3, int width, int height);

// TODO(ltrudeau) Remove this when HBD 444 SIMD is added
void cfl_luma_subsampling_444_hbd_c(const uint16_t *input, int input_stride,
                                    int16_t *output_q3, int width, int height);

// TODO(ltrudeau) Remove this when LBD 422 SIMD is added
void cfl_luma_subsampling_422_lbd_c(const uint8_t *input, int input_stride,
                                    int16_t *output_q3, int width, int height);
// TODO(ltrudeau) Remove this when LBD 444 SIMD is added
void cfl_luma_subsampling_444_lbd_c(const uint8_t *input, int input_stride,
                                    int16_t *output_q3, int width, int height);

#define CFL_GET_SUBSAMPLE_FUNCTION(arch)                                   \
  cfl_subsample_lbd_fn get_subsample_lbd_fn_##arch(int sub_x, int sub_y) { \
    if (sub_x == 1)                                                        \
      return (sub_y == 1) ? cfl_luma_subsampling_420_lbd_##arch            \
                          : cfl_luma_subsampling_422_lbd_c;                \
    return cfl_luma_subsampling_444_lbd_c;                                 \
  }                                                                        \
  cfl_subsample_hbd_fn get_subsample_hbd_fn_##arch(int sub_x, int sub_y) { \
    if (sub_x == 1)                                                        \
      return (sub_y == 1) ? cfl_luma_subsampling_420_hbd_c                 \
                          : cfl_luma_subsampling_422_hbd_c;                \
    return cfl_luma_subsampling_444_hbd_c;                                 \
  }

// Null function used for invalid tx_sizes
static INLINE void cfl_subtract_average_null(int16_t *pred_buf_q3) {
  (void)pred_buf_q3;
  assert(0);
}

#define CFL_SUB_AVG_X(arch, width, height, round_offset, num_pel_log2)        \
  static void subtract_average_##width##x##height##_x(int16_t *pred_buf_q3) { \
    subtract_average_##arch(pred_buf_q3, width, height, round_offset,         \
                            num_pel_log2);                                    \
  }

#if CONFIG_TX64X64
#define CFL_SUB_AVG_FN(arch)                                                \
  CFL_SUB_AVG_X(arch, 4, 4, 8, 4)                                           \
  CFL_SUB_AVG_X(arch, 4, 8, 16, 5)                                          \
  CFL_SUB_AVG_X(arch, 4, 16, 32, 6)                                         \
  CFL_SUB_AVG_X(arch, 8, 4, 16, 5)                                          \
  CFL_SUB_AVG_X(arch, 8, 8, 32, 6)                                          \
  CFL_SUB_AVG_X(arch, 8, 16, 64, 7)                                         \
  CFL_SUB_AVG_X(arch, 8, 32, 128, 8)                                        \
  CFL_SUB_AVG_X(arch, 16, 4, 32, 6)                                         \
  CFL_SUB_AVG_X(arch, 16, 8, 64, 7)                                         \
  CFL_SUB_AVG_X(arch, 16, 16, 128, 8)                                       \
  CFL_SUB_AVG_X(arch, 16, 32, 256, 9)                                       \
  CFL_SUB_AVG_X(arch, 32, 8, 128, 8)                                        \
  CFL_SUB_AVG_X(arch, 32, 16, 256, 9)                                       \
  CFL_SUB_AVG_X(arch, 32, 32, 512, 10)                                      \
  cfl_subtract_average_fn get_subtract_average_fn_##arch(TX_SIZE tx_size) { \
    static const cfl_subtract_average_fn sub_avg[TX_SIZES_ALL] = {          \
      subtract_average_4x4_x,    /* 4x4 */                                  \
      subtract_average_8x8_x,    /* 8x8 */                                  \
      subtract_average_16x16_x,  /* 16x16 */                                \
      subtract_average_32x32_x,  /* 32x32 */                                \
      cfl_subtract_average_null, /* 64x64 (invalid CFL size) */             \
      subtract_average_4x8_x,    /* 4x8 */                                  \
      subtract_average_8x4_x,    /* 8x4 */                                  \
      subtract_average_8x16_x,   /* 8x16 */                                 \
      subtract_average_16x8_x,   /* 16x8 */                                 \
      subtract_average_16x32_x,  /* 16x32 */                                \
      subtract_average_32x16_x,  /* 32x16 */                                \
      cfl_subtract_average_null, /* 32x64 (invalid CFL size) */             \
      cfl_subtract_average_null, /* 64x32 (invalid CFL size) */             \
      subtract_average_4x16_x,   /* 4x16 (invalid CFL size) */              \
      subtract_average_16x4_x,   /* 16x4 (invalid CFL size) */              \
      subtract_average_8x32_x,   /* 8x32 (invalid CFL size) */              \
      subtract_average_32x8_x,   /* 32x8 (invalid CFL size) */              \
      cfl_subtract_average_null, /* 16x64 (invalid CFL size) */             \
      cfl_subtract_average_null, /* 64x16 (invalid CFL size) */             \
    };                                                                      \
    /* Modulo TX_SIZES_ALL to ensure that an attacker won't be able to */   \
    /* index the function pointer array out of bounds. */                   \
    return sub_avg[tx_size % TX_SIZES_ALL];                                 \
  }
#else
#define CFL_SUB_AVG_FN(arch)                                                \
  cfl_subtract_average_fn get_subtract_average_fn_##arch(TX_SIZE tx_size) { \
    static const cfl_subtract_average_fn sub_avg[TX_SIZES_ALL] = {          \
      subtract_average_4x4_x,   /* 4x4 */                                   \
      subtract_average_8x8_x,   /* 8x8 */                                   \
      subtract_average_16x16_x, /* 16x16 */                                 \
      subtract_average_32x32_x, /* 32x32 */                                 \
      subtract_average_4x8_x,   /* 4x8 */                                   \
      subtract_average_8x4_x,   /* 8x4 */                                   \
      subtract_average_8x16_x,  /* 8x16 */                                  \
      subtract_average_16x8_x,  /* 16x8 */                                  \
      subtract_average_16x32_x, /* 16x32 */                                 \
      subtract_average_32x16_x, /* 32x16 */                                 \
      subtract_average_4x16_x,  /* 4x16 (invalid CFL size) */               \
      subtract_average_16x4_x,  /* 16x4 (invalid CFL size) */               \
      subtract_average_8x32_x,  /* 8x32 (invalid CFL size) */               \
      subtract_average_32x8_x,  /* 32x8 (invalid CFL size) */               \
    };                                                                      \
    /* Modulo TX_SIZES_ALL to ensure that an attacker won't be able to */   \
    /* index the function pointer array out of bounds. */                   \
    return sub_avg[tx_size % TX_SIZES_ALL];                                 \
  }
#endif

#endif  // AV1_COMMON_CFL_H_
