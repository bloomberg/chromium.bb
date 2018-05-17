/*
 *  Copyright (c) 2018, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AV1_COMMON_ARM_MEM_NEON_H_
#define AV1_COMMON_ARM_MEM_NEON_H_

#include <arm_neon.h>

static INLINE void store_row2_u8_8x8(uint8_t *s, int p, const uint8x8_t s0,
                                     const uint8x8_t s1) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
}

static INLINE void load_u8_8x8(const uint8_t *s, ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3,
                               uint8x8_t *const s4, uint8x8_t *const s5,
                               uint8x8_t *const s6, uint8x8_t *const s7) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
  s += p;
  *s4 = vld1_u8(s);
  s += p;
  *s5 = vld1_u8(s);
  s += p;
  *s6 = vld1_u8(s);
  s += p;
  *s7 = vld1_u8(s);
}

static INLINE void load_u8_8x4(const uint8_t *s, const ptrdiff_t p,
                               uint8x8_t *const s0, uint8x8_t *const s1,
                               uint8x8_t *const s2, uint8x8_t *const s3) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
}

static INLINE void load_s16_8x8(const uint8_t *s, const ptrdiff_t p,
                                uint8x8_t *const s0, uint8x8_t *const s1,
                                uint8x8_t *const s2, uint8x8_t *const s3) {
  *s0 = vld1_u8(s);
  s += p;
  *s1 = vld1_u8(s);
  s += p;
  *s2 = vld1_u8(s);
  s += p;
  *s3 = vld1_u8(s);
}

static INLINE void store_u8_8x8(uint8_t *s, ptrdiff_t p, const uint8x8_t s0,
                                const uint8x8_t s1, const uint8x8_t s2,
                                const uint8x8_t s3, const uint8x8_t s4,
                                const uint8x8_t s5, const uint8x8_t s6,
                                const uint8x8_t s7) {
  vst1_u8(s, s0);
  s += p;
  vst1_u8(s, s1);
  s += p;
  vst1_u8(s, s2);
  s += p;
  vst1_u8(s, s3);
  s += p;
  vst1_u8(s, s4);
  s += p;
  vst1_u8(s, s5);
  s += p;
  vst1_u8(s, s6);
  s += p;
  vst1_u8(s, s7);
}

static INLINE void store_u16_8x8(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x8_t s0, const uint16x8_t s1,
                                 const uint16x8_t s2, const uint16x8_t s3,
                                 const uint16x8_t s4, const uint16x8_t s5,
                                 const uint16x8_t s6, const uint16x8_t s7) {
  vst1q_u16(s, s0);
  s += dst_stride;
  vst1q_u16(s, s1);
  s += dst_stride;
  vst1q_u16(s, s2);
  s += dst_stride;
  vst1q_u16(s, s3);
  s += dst_stride;
  vst1q_u16(s, s4);
  s += dst_stride;
  vst1q_u16(s, s5);
  s += dst_stride;
  vst1q_u16(s, s6);
  s += dst_stride;
  vst1q_u16(s, s7);
}

static INLINE void store_u16_8x4(uint16_t *s, ptrdiff_t dst_stride,
                                 const uint16x8_t s0, const uint16x8_t s1,
                                 const uint16x8_t s2, const uint16x8_t s3) {
  vst1q_u16(s, s0);
  s += dst_stride;
  vst1q_u16(s, s1);
  s += dst_stride;
  vst1q_u16(s, s2);
  s += dst_stride;
  vst1q_u16(s, s3);
}

#endif  // AV1_COMMON_ARM_MEM_NEON_H_
