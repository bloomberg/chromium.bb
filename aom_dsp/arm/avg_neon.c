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

#include <arm_neon.h>
#include <assert.h>

#include "config/aom_dsp_rtcd.h"
#include "config/aom_config.h"

#include "aom/aom_integer.h"

static INLINE unsigned int horizontal_add_u16x8(const uint16x8_t v_16x8) {
  const uint32x4_t a = vpaddlq_u16(v_16x8);
  const uint64x2_t b = vpaddlq_u32(a);
  const uint32x2_t c = vadd_u32(vreinterpret_u32_u64(vget_low_u64(b)),
                                vreinterpret_u32_u64(vget_high_u64(b)));
  return vget_lane_u32(c, 0);
}

// coeff: 16 bits, dynamic range [-32640, 32640].
// length: value range {16, 64, 256, 1024}.
int aom_satd_neon(const int16_t *coeff, int length) {
  const int16x4_t zero = vdup_n_s16(0);
  int32x4_t accum = vdupq_n_s32(0);

  do {
    const int16x8_t src0 = vld1q_s16(coeff);
    const int16x8_t src8 = vld1q_s16(coeff + 8);
    accum = vabal_s16(accum, vget_low_s16(src0), zero);
    accum = vabal_s16(accum, vget_high_s16(src0), zero);
    accum = vabal_s16(accum, vget_low_s16(src8), zero);
    accum = vabal_s16(accum, vget_high_s16(src8), zero);
    length -= 16;
    coeff += 16;
  } while (length != 0);

  {
    // satd: 26 bits, dynamic range [-32640 * 1024, 32640 * 1024]
    const int64x2_t s0 = vpaddlq_s32(accum);  // cascading summation of 'accum'.
    const int32x2_t s1 = vadd_s32(vreinterpret_s32_s64(vget_low_s64(s0)),
                                  vreinterpret_s32_s64(vget_high_s64(s0)));
    const int satd = vget_lane_s32(s1, 0);
    return satd;
  }
}

void aom_minmax_8x8_neon(const uint8_t *a, int a_stride, const uint8_t *b,
                         int b_stride, int *min, int *max) {
  // Load and concatenate.
  const uint8x16_t a01 = vcombine_u8(vld1_u8(a), vld1_u8(a + a_stride));
  const uint8x16_t a23 =
      vcombine_u8(vld1_u8(a + 2 * a_stride), vld1_u8(a + 3 * a_stride));
  const uint8x16_t a45 =
      vcombine_u8(vld1_u8(a + 4 * a_stride), vld1_u8(a + 5 * a_stride));
  const uint8x16_t a67 =
      vcombine_u8(vld1_u8(a + 6 * a_stride), vld1_u8(a + 7 * a_stride));

  const uint8x16_t b01 = vcombine_u8(vld1_u8(b), vld1_u8(b + b_stride));
  const uint8x16_t b23 =
      vcombine_u8(vld1_u8(b + 2 * b_stride), vld1_u8(b + 3 * b_stride));
  const uint8x16_t b45 =
      vcombine_u8(vld1_u8(b + 4 * b_stride), vld1_u8(b + 5 * b_stride));
  const uint8x16_t b67 =
      vcombine_u8(vld1_u8(b + 6 * b_stride), vld1_u8(b + 7 * b_stride));

  // Absolute difference.
  const uint8x16_t ab01_diff = vabdq_u8(a01, b01);
  const uint8x16_t ab23_diff = vabdq_u8(a23, b23);
  const uint8x16_t ab45_diff = vabdq_u8(a45, b45);
  const uint8x16_t ab67_diff = vabdq_u8(a67, b67);

  // Max values between the Q vectors.
  const uint8x16_t ab0123_max = vmaxq_u8(ab01_diff, ab23_diff);
  const uint8x16_t ab4567_max = vmaxq_u8(ab45_diff, ab67_diff);
  const uint8x16_t ab0123_min = vminq_u8(ab01_diff, ab23_diff);
  const uint8x16_t ab4567_min = vminq_u8(ab45_diff, ab67_diff);

  const uint8x16_t ab07_max = vmaxq_u8(ab0123_max, ab4567_max);
  const uint8x16_t ab07_min = vminq_u8(ab0123_min, ab4567_min);

  // Split to D and start doing pairwise.
  uint8x8_t ab_max = vmax_u8(vget_high_u8(ab07_max), vget_low_u8(ab07_max));
  uint8x8_t ab_min = vmin_u8(vget_high_u8(ab07_min), vget_low_u8(ab07_min));

  // Enough runs of vpmax/min propogate the max/min values to every position.
  ab_max = vpmax_u8(ab_max, ab_max);
  ab_min = vpmin_u8(ab_min, ab_min);

  ab_max = vpmax_u8(ab_max, ab_max);
  ab_min = vpmin_u8(ab_min, ab_min);

  ab_max = vpmax_u8(ab_max, ab_max);
  ab_min = vpmin_u8(ab_min, ab_min);

  *min = *max = 0;  // Clear high bits
  // Store directly to avoid costly neon->gpr transfer.
  vst1_lane_u8((uint8_t *)max, ab_max, 0);
  vst1_lane_u8((uint8_t *)min, ab_min, 0);
}
