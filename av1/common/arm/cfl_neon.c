/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <arm_neon.h>

#include "./av1_rtcd.h"

#include "av1/common/cfl.h"

static INLINE void vldsubstq_s16(int16_t *buf, int16x8_t sub) {
  vst1q_s16(buf, vsubq_s16(vld1q_s16(buf), sub));
}

static INLINE uint16x8_t vldaddq_u16(const uint16_t *buf, size_t offset) {
  return vaddq_u16(vld1q_u16(buf), vld1q_u16(buf + offset));
}

// Load half of a vector and duplicated in other half
static INLINE uint8x8_t vldh_dup_u8(const uint8_t *ptr) {
  return vreinterpret_u8_u32(vld1_dup_u32((const uint32_t *)ptr));
}

// Store half of a vector.
static INLINE void vsth_s16(int16_t *ptr, int16x4_t val) {
  *((uint32_t *)ptr) = vreinterpret_u32_s16(val)[0];
}

static void cfl_luma_subsampling_420_lbd_neon(const uint8_t *input,
                                              int input_stride,
                                              int16_t *pred_buf_q3, int width,
                                              int height) {
  const int16_t *end = pred_buf_q3 + (height >> 1) * CFL_BUF_LINE;
  const int luma_stride = input_stride << 1;
  do {
    if (width == 4) {
      const uint16x4_t top = vpaddl_u8(vldh_dup_u8(input));
      const uint16x4_t sum = vpadal_u8(top, vldh_dup_u8(input + input_stride));
      vsth_s16(pred_buf_q3, vshl_n_s16(vreinterpret_s16_u16(sum), 1));
    } else if (width == 8) {
      const uint16x4_t top = vpaddl_u8(vld1_u8(input));
      const uint16x4_t sum = vpadal_u8(top, vld1_u8(input + input_stride));
      vst1_s16(pred_buf_q3, vshl_n_s16(vreinterpret_s16_u16(sum), 1));
    } else {
      const uint16x8_t top = vpaddlq_u8(vld1q_u8(input));
      const uint16x8_t sum = vpadalq_u8(top, vld1q_u8(input + input_stride));
      vst1q_s16(pred_buf_q3, vshlq_n_s16(vreinterpretq_s16_u16(sum), 1));
      if (width == 32) {
        const uint16x8_t next_top = vpaddlq_u8(vld1q_u8(input + 16));
        const uint16x8_t next_sum =
            vpadalq_u8(next_top, vld1q_u8(input + 16 + input_stride));
        vst1q_s16(pred_buf_q3 + 8,
                  vshlq_n_s16(vreinterpretq_s16_u16(next_sum), 1));
      }
    }
    input += luma_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

static void cfl_luma_subsampling_422_lbd_neon(const uint8_t *input,
                                              int input_stride,
                                              int16_t *pred_buf_q3, int width,
                                              int height) {
  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    if (width == 4) {
      const uint16x4_t top = vpaddl_u8(vldh_dup_u8(input));
      vsth_s16(pred_buf_q3, vshl_n_s16(vreinterpret_s16_u16(top), 2));
    } else if (width == 8) {
      const uint16x4_t top = vpaddl_u8(vld1_u8(input));
      vst1_s16(pred_buf_q3, vshl_n_s16(vreinterpret_s16_u16(top), 2));
    } else {
      const uint16x8_t top = vpaddlq_u8(vld1q_u8(input));
      vst1q_s16(pred_buf_q3, vshlq_n_s16(vreinterpretq_s16_u16(top), 2));
      if (width == 32) {
        const uint16x8_t next_top = vpaddlq_u8(vld1q_u8(input + 16));
        vst1q_s16(pred_buf_q3 + 8,
                  vshlq_n_s16(vreinterpretq_s16_u16(next_top), 2));
      }
    }
    input += input_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

static void cfl_luma_subsampling_444_lbd_neon(const uint8_t *input,
                                              int input_stride,
                                              int16_t *pred_buf_q3, int width,
                                              int height) {
  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    if (width == 4) {
      const uint16x8_t top = vshll_n_u8(vldh_dup_u8(input), 3);
      vst1_s16(pred_buf_q3, vreinterpret_s16_u16(vget_low_u16(top)));
    } else if (width == 8) {
      const uint16x8_t top = vshll_n_u8(vld1_u8(input), 3);
      vst1q_s16(pred_buf_q3, vreinterpretq_s16_u16(top));
    } else {
      const uint8x16_t top = vld1q_u8(input);
      vst1q_s16(pred_buf_q3,
                vreinterpretq_s16_u16(vshll_n_u8(vget_low_u8(top), 3)));
      vst1q_s16(pred_buf_q3 + 8,
                vreinterpretq_s16_u16(vshll_n_u8(vget_high_u8(top), 3)));
      if (width == 32) {
        const uint8x16_t next_top = vld1q_u8(input + 16);
        vst1q_s16(pred_buf_q3 + 16,
                  vreinterpretq_s16_u16(vshll_n_u8(vget_low_u8(next_top), 3)));
        vst1q_s16(pred_buf_q3 + 24,
                  vreinterpretq_s16_u16(vshll_n_u8(vget_high_u8(next_top), 3)));
      }
    }
    input += input_stride;
  } while ((pred_buf_q3 += CFL_BUF_LINE) < end);
}

CFL_GET_SUBSAMPLE_FUNCTION(neon)

static INLINE void subtract_average_neon(int16_t *pred_buf, int width,
                                         int height, int round_offset,
                                         const int num_pel_log2) {
  const int16_t *const end = pred_buf + height * CFL_BUF_LINE;
  const uint16_t *const sum_end = (uint16_t *)end;

  // Round offset is not needed, because NEON will handle the rounding.
  (void)round_offset;

  // To optimize the use of the CPU pipeline, we process 4 rows per iteration
  const int step = 4 * CFL_BUF_LINE;

  // At this stage, the prediction buffer contains scaled reconstructed luma
  // pixels, which are positive integer and only require 15 bits. By using
  // unsigned integer for the sum, we can do one addition operation inside 16
  // bits (8 lanes) before having to convert to 32 bits (4 lanes).
  const uint16_t *sum_buf = (uint16_t *)pred_buf;
  uint32x4_t sum_32x4 = { 0, 0, 0, 0 };
  do {
    // For all widths, we load, add and combine the data so it fits in 4 lanes.
    if (width == 4) {
      const uint16x4_t a0 =
          vadd_u16(vld1_u16(sum_buf), vld1_u16(sum_buf + CFL_BUF_LINE));
      const uint16x4_t a1 = vadd_u16(vld1_u16(sum_buf + 2 * CFL_BUF_LINE),
                                     vld1_u16(sum_buf + 3 * CFL_BUF_LINE));
      sum_32x4 = vaddq_u32(sum_32x4, vaddl_u16(a0, a1));
    } else if (width == 8) {
      const uint16x8_t a0 = vldaddq_u16(sum_buf, CFL_BUF_LINE);
      const uint16x8_t a1 =
          vldaddq_u16(sum_buf + 2 * CFL_BUF_LINE, CFL_BUF_LINE);
      sum_32x4 = vpadalq_u16(sum_32x4, a0);
      sum_32x4 = vpadalq_u16(sum_32x4, a1);
    } else {
      const uint16x8_t row0 = vldaddq_u16(sum_buf, 8);
      const uint16x8_t row1 = vldaddq_u16(sum_buf + CFL_BUF_LINE, 8);
      const uint16x8_t row2 = vldaddq_u16(sum_buf + 2 * CFL_BUF_LINE, 8);
      const uint16x8_t row3 = vldaddq_u16(sum_buf + 3 * CFL_BUF_LINE, 8);
      sum_32x4 = vpadalq_u16(sum_32x4, row0);
      sum_32x4 = vpadalq_u16(sum_32x4, row1);
      sum_32x4 = vpadalq_u16(sum_32x4, row2);
      sum_32x4 = vpadalq_u16(sum_32x4, row3);

      if (width == 32) {
        const uint16x8_t row0_1 = vldaddq_u16(sum_buf + 16, 8);
        const uint16x8_t row1_1 = vldaddq_u16(sum_buf + CFL_BUF_LINE + 16, 8);
        const uint16x8_t row2_1 =
            vldaddq_u16(sum_buf + 2 * CFL_BUF_LINE + 16, 8);
        const uint16x8_t row3_1 =
            vldaddq_u16(sum_buf + 3 * CFL_BUF_LINE + 16, 8);

        sum_32x4 = vpadalq_u16(sum_32x4, row0_1);
        sum_32x4 = vpadalq_u16(sum_32x4, row1_1);
        sum_32x4 = vpadalq_u16(sum_32x4, row2_1);
        sum_32x4 = vpadalq_u16(sum_32x4, row3_1);
      }
    }
  } while ((sum_buf += step) < sum_end);

  // Permute and add in such a way that each lane contains the block sum.
  // [A+C+B+D, B+D+A+C, C+A+D+B, D+B+C+A]
#if __ARM_ARCH >= 8
  sum_32x4 = vpaddq_u32(sum_32x4, sum_32x4);
  sum_32x4 = vpaddq_u32(sum_32x4, sum_32x4);
#else
  uint32x4_t flip =
      vcombine_u32(vget_high_u32(sum_32x4), vget_low_u32(sum_32x4));
  sum_32x4 = vaddq_u32(sum_32x4, flip);
  sum_32x4 = vaddq_u32(sum_32x4, vrev64q_u32(sum_32x4));
#endif

  // Computing the average could be done using scalars, but getting off the NEON
  // engine introduces latency, so we use vqrshrn.
  int16x4_t avg_16x4;
  // Constant propagation makes for some ugly code.
  switch (num_pel_log2) {
    case 4: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 4)); break;
    case 5: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 5)); break;
    case 6: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 6)); break;
    case 7: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 7)); break;
    case 8: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 8)); break;
    case 9: avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 9)); break;
    case 10:
      avg_16x4 = vreinterpret_s16_u16(vqrshrn_n_u32(sum_32x4, 10));
      break;
    default: assert(0);
  }

  if (width == 4) {
    do {
      vst1_s16(pred_buf, vsub_s16(vld1_s16(pred_buf), avg_16x4));
    } while ((pred_buf += CFL_BUF_LINE) < end);
  } else {
    const int16x8_t avg_16x8 = vcombine_s16(avg_16x4, avg_16x4);
    do {
      vldsubstq_s16(pred_buf, avg_16x8);
      vldsubstq_s16(pred_buf + CFL_BUF_LINE, avg_16x8);
      vldsubstq_s16(pred_buf + 2 * CFL_BUF_LINE, avg_16x8);
      vldsubstq_s16(pred_buf + 3 * CFL_BUF_LINE, avg_16x8);

      if (width > 8) {
        vldsubstq_s16(pred_buf + 8, avg_16x8);
        vldsubstq_s16(pred_buf + CFL_BUF_LINE + 8, avg_16x8);
        vldsubstq_s16(pred_buf + 2 * CFL_BUF_LINE + 8, avg_16x8);
        vldsubstq_s16(pred_buf + 3 * CFL_BUF_LINE + 8, avg_16x8);
      }
      if (width == 32) {
        vldsubstq_s16(pred_buf + 16, avg_16x8);
        vldsubstq_s16(pred_buf + 24, avg_16x8);
        vldsubstq_s16(pred_buf + CFL_BUF_LINE + 16, avg_16x8);
        vldsubstq_s16(pred_buf + CFL_BUF_LINE + 24, avg_16x8);
        vldsubstq_s16(pred_buf + 2 * CFL_BUF_LINE + 16, avg_16x8);
        vldsubstq_s16(pred_buf + 2 * CFL_BUF_LINE + 24, avg_16x8);
        vldsubstq_s16(pred_buf + 3 * CFL_BUF_LINE + 16, avg_16x8);
        vldsubstq_s16(pred_buf + 3 * CFL_BUF_LINE + 24, avg_16x8);
      }
    } while ((pred_buf += step) < end);
  }
}

CFL_SUB_AVG_FN(neon)
