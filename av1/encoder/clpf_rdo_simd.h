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

#include "./aom_dsp_rtcd.h"
#include "aom_dsp/aom_simd.h"
#include "aom_ports/mem.h"
#include "aom_ports/bitops.h"
#include "av1/common/clpf_simd_kernel.h"

SIMD_INLINE void clip_sides(v128 *c, v128 *d, v128 *e, v128 *f, int left,
                            int right) {
  DECLARE_ALIGNED(16, static const uint64_t,
                  c_shuff[]) = { 0x0504030201000000LL, 0x0d0c0b0a09080808LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  d_shuff[]) = { 0x0605040302010000LL, 0x0e0d0c0b0a090808LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  e_shuff[]) = { 0x0707060504030201LL, 0x0f0f0e0d0c0b0a09LL };
  DECLARE_ALIGNED(16, static const uint64_t,
                  f_shuff[]) = { 0x0707070605040302LL, 0x0f0f0f0e0d0c0b0aLL };

  if (!left) {  // Left clipping
    *c = v128_shuffle_8(*c, v128_load_aligned(c_shuff));
    *d = v128_shuffle_8(*d, v128_load_aligned(d_shuff));
  }
  if (!right) {  // Right clipping
    *e = v128_shuffle_8(*e, v128_load_aligned(e_shuff));
    *f = v128_shuffle_8(*f, v128_load_aligned(f_shuff));
  }
}

SIMD_INLINE void read_two_lines(const uint8_t *rec, const uint8_t *org,
                                int rstride, int ostride, int x0, int y0,
                                int bottom, int right, int y, v128 *o, v128 *r,
                                v128 *a, v128 *b, v128 *c, v128 *d, v128 *e,
                                v128 *f, v128 *g, v128 *h) {
  const v64 k1 = v64_load_aligned(org);
  const v64 k2 = v64_load_aligned(org + ostride);
  const v64 l1 = v64_load_aligned(rec);
  const v64 l2 = v64_load_aligned(rec + rstride);
  const v64 l3 = v64_load_aligned(rec - (y != -y0) * rstride);
  const v64 l4 = v64_load_aligned(rec + ((y != bottom) + 1) * rstride);
  *o = v128_from_v64(k1, k2);
  *r = v128_from_v64(l1, l2);
  *a = v128_from_v64(v64_load_aligned(rec - 2 * (y != -y0) * rstride), l3);
  *b = v128_from_v64(l3, l1);
  *g = v128_from_v64(l2, l4);
  *h = v128_from_v64(l4,
                     v64_load_aligned(rec + (2 * (y != bottom) + 1) * rstride));
  *c = v128_from_v64(v64_load_unaligned(rec - 2 * !!x0),
                     v64_load_unaligned(rec - 2 * !!x0 + rstride));
  *d = v128_from_v64(v64_load_unaligned(rec - !!x0),
                     v64_load_unaligned(rec - !!x0 + rstride));
  *e = v128_from_v64(v64_load_unaligned(rec + !!right),
                     v64_load_unaligned(rec + !!right + rstride));
  *f = v128_from_v64(v64_load_unaligned(rec + 2 * !!right),
                     v64_load_unaligned(rec + 2 * !!right + rstride));
  clip_sides(c, d, e, f, x0, right);
}

void SIMD_FUNC(aom_clpf_detect)(const uint8_t *rec, const uint8_t *org,
                                int rstride, int ostride, int x0, int y0,
                                int width, int height, int *sum0, int *sum1,
                                unsigned int strength, int size,
                                unsigned int dmp) {
  const int bottom = height - 2 - y0;
  const int right = width - 8 - x0;
  ssd128_internal ssd0 = v128_ssd_u8_init();
  ssd128_internal ssd1 = v128_ssd_u8_init();
  int y;

  if (size != 8) {  // Fallback to plain C
    aom_clpf_detect_c(rec, org, rstride, ostride, x0, y0, width, height, sum0,
                      sum1, strength, size, dmp);
    return;
  }

  rec += x0 + y0 * rstride;
  org += x0 + y0 * ostride;

  for (y = 0; y < 8; y += 2) {
    v128 a, b, c, d, e, f, g, h, o, r;
    read_two_lines(rec, org, rstride, ostride, x0, y0, bottom, right, y, &o, &r,
                   &a, &b, &c, &d, &e, &f, &g, &h);
    ssd0 = v128_ssd_u8(ssd0, o, r);
    ssd1 = v128_ssd_u8(ssd1, o,
                       calc_delta(r, a, b, c, d, e, f, g, h, strength, dmp));
    rec += rstride * 2;
    org += ostride * 2;
  }
  *sum0 += v128_ssd_u8_sum(ssd0);
  *sum1 += v128_ssd_u8_sum(ssd1);
}

SIMD_INLINE void calc_delta_multi(v128 r, v128 o, v128 a, v128 b, v128 c,
                                  v128 d, v128 e, v128 f, v128 g, v128 h,
                                  ssd128_internal *ssd1, ssd128_internal *ssd2,
                                  ssd128_internal *ssd3, unsigned int dmp) {
  *ssd1 = v128_ssd_u8(*ssd1, o, calc_delta(r, a, b, c, d, e, f, g, h, 1, dmp));
  *ssd2 = v128_ssd_u8(*ssd2, o, calc_delta(r, a, b, c, d, e, f, g, h, 2, dmp));
  *ssd3 = v128_ssd_u8(*ssd3, o, calc_delta(r, a, b, c, d, e, f, g, h, 4, dmp));
}

// Test multiple filter strengths at once.
void SIMD_FUNC(aom_clpf_detect_multi)(const uint8_t *rec, const uint8_t *org,
                                      int rstride, int ostride, int x0, int y0,
                                      int width, int height, int *sum, int size,
                                      unsigned int dmp) {
  const int bottom = height - 2 - y0;
  const int right = width - 8 - x0;
  ssd128_internal ssd0 = v128_ssd_u8_init();
  ssd128_internal ssd1 = v128_ssd_u8_init();
  ssd128_internal ssd2 = v128_ssd_u8_init();
  ssd128_internal ssd3 = v128_ssd_u8_init();
  int y;

  if (size != 8) {  // Fallback to plain C
    aom_clpf_detect_multi_c(rec, org, rstride, ostride, x0, y0, width, height,
                            sum, size, dmp);
    return;
  }

  rec += x0 + y0 * rstride;
  org += x0 + y0 * ostride;

  for (y = 0; y < 8; y += 2) {
    v128 a, b, c, d, e, f, g, h, o, r;
    read_two_lines(rec, org, rstride, ostride, x0, y0, bottom, right, y, &o, &r,
                   &a, &b, &c, &d, &e, &f, &g, &h);
    ssd0 = v128_ssd_u8(ssd0, o, r);
    calc_delta_multi(r, o, a, b, c, d, e, f, g, h, &ssd1, &ssd2, &ssd3, dmp);
    rec += 2 * rstride;
    org += 2 * ostride;
  }
  sum[0] += v128_ssd_u8_sum(ssd0);
  sum[1] += v128_ssd_u8_sum(ssd1);
  sum[2] += v128_ssd_u8_sum(ssd2);
  sum[3] += v128_ssd_u8_sum(ssd3);
}

#if CONFIG_AOM_HIGHBITDEPTH
SIMD_INLINE void read_two_lines_hbd(const uint16_t *rec, const uint16_t *org,
                                    int rstride, int ostride, int x0, int y0,
                                    int bottom, int right, int y, v128 *o,
                                    v128 *r, v128 *a, v128 *b, v128 *c, v128 *d,
                                    v128 *e, v128 *f, v128 *g, v128 *h,
                                    int shift) {
  const v128 k1 = v128_shr_u16(v128_load_aligned(org), shift);
  const v128 k2 = v128_shr_u16(v128_load_aligned(org + ostride), shift);
  const v128 l1 = v128_shr_u16(v128_load_aligned(rec), shift);
  const v128 l2 = v128_shr_u16(v128_load_aligned(rec + rstride), shift);
  const v128 l3 =
      v128_shr_u16(v128_load_aligned(rec - (y != -y0) * rstride), shift);
  const v128 l4 = v128_shr_u16(
      v128_load_aligned(rec + ((y != bottom) + 1) * rstride), shift);
  *o = v128_unziplo_8(k1, k2);
  *r = v128_unziplo_8(l1, l2);
  *a = v128_unziplo_8(
      v128_shr_u16(v128_load_aligned(rec - 2 * (y != -y0) * rstride), shift),
      l3);
  *b = v128_unziplo_8(l3, l1);
  *g = v128_unziplo_8(l2, l4);
  *h = v128_unziplo_8(
      l4,
      v128_shr_u16(v128_load_unaligned(rec + (2 * (y != bottom) + 1) * rstride),
                   shift));
  *c = v128_unziplo_8(
      v128_shr_u16(v128_load_unaligned(rec - 2 * !!x0), shift),
      v128_shr_u16(v128_load_unaligned(rec - 2 * !!x0 + rstride), shift));
  *d = v128_unziplo_8(
      v128_shr_u16(v128_load_unaligned(rec - !!x0), shift),
      v128_shr_u16(v128_load_unaligned(rec - !!x0 + rstride), shift));
  *e = v128_unziplo_8(
      v128_shr_u16(v128_load_unaligned(rec + !!right), shift),
      v128_shr_u16(v128_load_unaligned(rec + !!right + rstride), shift));
  *f = v128_unziplo_8(
      v128_shr_u16(v128_load_unaligned(rec + 2 * !!right), shift),
      v128_shr_u16(v128_load_unaligned(rec + 2 * !!right + rstride), shift));
  clip_sides(c, d, e, f, x0, right);
}

void SIMD_FUNC(aom_clpf_detect_hbd)(const uint16_t *rec, const uint16_t *org,
                                    int rstride, int ostride, int x0, int y0,
                                    int width, int height, int *sum0, int *sum1,
                                    unsigned int strength, int size,
                                    unsigned int bitdepth,
                                    unsigned int damping) {
  const int shift = bitdepth - 8;
  const int bottom = height - 2 - y0;
  const int right = width - 8 - x0;
  ssd128_internal ssd0 = v128_ssd_u8_init();
  ssd128_internal ssd1 = v128_ssd_u8_init();
  int y;

  if (size != 8) {  // Fallback to plain C
    aom_clpf_detect_hbd_c(rec, org, rstride, ostride, x0, y0, width, height,
                          sum0, sum1, strength, size, bitdepth, damping);
    return;
  }

  rec += x0 + y0 * rstride;
  org += x0 + y0 * ostride;

  for (y = 0; y < 8; y += 2) {
    v128 a, b, c, d, e, f, g, h, o, r;
    read_two_lines_hbd(rec, org, rstride, ostride, x0, y0, bottom, right, y, &o,
                       &r, &a, &b, &c, &d, &e, &f, &g, &h, shift);
    ssd0 = v128_ssd_u8(ssd0, o, r);
    ssd1 = v128_ssd_u8(ssd1, o, calc_delta(r, a, b, c, d, e, f, g, h,
                                           strength >> shift, damping));
    rec += rstride * 2;
    org += ostride * 2;
  }
  *sum0 += v128_ssd_u8_sum(ssd0);
  *sum1 += v128_ssd_u8_sum(ssd1);
}

void SIMD_FUNC(aom_clpf_detect_multi_hbd)(const uint16_t *rec,
                                          const uint16_t *org, int rstride,
                                          int ostride, int x0, int y0,
                                          int width, int height, int *sum,
                                          int size, unsigned int bitdepth,
                                          unsigned int damping) {
  const int bottom = height - 2 - y0;
  const int right = width - 8 - x0;
  ssd128_internal ssd0 = v128_ssd_u8_init();
  ssd128_internal ssd1 = v128_ssd_u8_init();
  ssd128_internal ssd2 = v128_ssd_u8_init();
  ssd128_internal ssd3 = v128_ssd_u8_init();
  int y;

  if (size != 8) {  // Fallback to plain C
    aom_clpf_detect_multi_hbd_c(rec, org, rstride, ostride, x0, y0, width,
                                height, sum, size, bitdepth, damping);
    return;
  }

  rec += x0 + y0 * rstride;
  org += x0 + y0 * ostride;

  for (y = 0; y < 8; y += 2) {
    v128 a, b, c, d, e, f, g, h, o, r;
    read_two_lines_hbd(rec, org, rstride, ostride, x0, y0, bottom, right, y, &o,
                       &r, &a, &b, &c, &d, &e, &f, &g, &h, bitdepth - 8);
    ssd0 = v128_ssd_u8(ssd0, o, r);
    calc_delta_multi(r, o, a, b, c, d, e, f, g, h, &ssd1, &ssd2, &ssd3,
                     damping);
    rec += rstride * 2;
    org += ostride * 2;
  }
  sum[0] += v128_ssd_u8_sum(ssd0);
  sum[1] += v128_ssd_u8_sum(ssd1);
  sum[2] += v128_ssd_u8_sum(ssd2);
  sum[3] += v128_ssd_u8_sum(ssd3);
}
#endif
