/*
 *  Copyright (c) 2020, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>
#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"

int64_t aom_sse_neon(const uint8_t *a, int a_stride, const uint8_t *b,
                     int b_stride, int width, int height) {
  int addinc;
  uint8x8_t d0, d1;
  uint8_t dx;
  uint32x2_t d2, d3;
  uint8x16_t q0 = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
  uint32x4_t q8, q9;
  uint16x8_t q1, q6, q7;
  uint8x16_t q2, q3, q4, q5;
  uint32_t sse = 0;
  const uint16_t sse1 = 0;
  q1 = vld1q_dup_u16(&sse1);
  for (int y = 0; y < height; y++) {
    int x = width;
    while (x > 0) {
      addinc = width - x;
      q2 = vld1q_u8(a + addinc);
      q3 = vld1q_u8(b + addinc);
      if (x < 16) {
        dx = x;
        q4 = vld1q_dup_u8(&dx);
        q5 = vcltq_u8(q0, q4);
        q2 = vandq_u8(q2, q5);
        q3 = vandq_u8(q3, q5);
      }
      q4 = vabdq_u8(q2, q3);  // diff = abs(a[x] - b[x])
      d0 = vget_low_u8(q4);
      d1 = vget_high_u8(q4);
      q6 = vmlal_u8(q1, d0, d0);
      q7 = vmlal_u8(q1, d1, d1);
      q8 = vaddl_u16(vget_low_u16(q6), vget_high_u16(q6));
      q9 = vaddl_u16(vget_low_u16(q7), vget_high_u16(q7));

      d2 = vadd_u32(vget_low_u32(q8), vget_high_u32(q8));
      d3 = vadd_u32(vget_low_u32(q9), vget_high_u32(q9));
      sse += vget_lane_u32(d2, 0);
      sse += vget_lane_u32(d2, 1);
      sse += vget_lane_u32(d3, 0);
      sse += vget_lane_u32(d3, 1);
      x -= 16;
    }
    a += a_stride;
    b += b_stride;
  }
  return (int64_t)sse;
}
