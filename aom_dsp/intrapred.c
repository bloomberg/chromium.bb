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

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"

#define DST(x, y) dst[(x) + (y)*stride]
#define AVG3(a, b, c) (((a) + 2 * (b) + (c) + 2) >> 2)
#define AVG2(a, b) (((a) + (b) + 1) >> 1)

static INLINE void d207_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)above;
  // first column
  for (r = 0; r < bs - 1; ++r) dst[r * stride] = AVG2(left[r], left[r + 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // second column
  for (r = 0; r < bs - 2; ++r)
    dst[r * stride] = AVG3(left[r], left[r + 1], left[r + 2]);
  dst[(bs - 2) * stride] = AVG3(left[bs - 2], left[bs - 1], left[bs - 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // rest of last row
  for (c = 0; c < bs - 2; ++c) dst[(bs - 1) * stride + c] = left[bs - 1];

  for (r = bs - 2; r >= 0; --r)
    for (c = 0; c < bs - 2; ++c)
      dst[r * stride + c] = dst[(r + 1) * stride + c - 2];
}

static INLINE void d207e_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                   const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)above;

  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = c & 1 ? AVG3(left[(c >> 1) + r], left[(c >> 1) + r + 1],
                            left[(c >> 1) + r + 2])
                     : AVG2(left[(c >> 1) + r], left[(c >> 1) + r + 1]);
    }
    dst += stride;
  }
}

static INLINE void d63_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                 const uint8_t *above, const uint8_t *left) {
  int r, c;
  int size;
  (void)left;
  for (c = 0; c < bs; ++c) {
    dst[c] = AVG2(above[c], above[c + 1]);
    dst[stride + c] = AVG3(above[c], above[c + 1], above[c + 2]);
  }
  for (r = 2, size = bs - 2; r < bs; r += 2, --size) {
    memcpy(dst + (r + 0) * stride, dst + (r >> 1), size);
    memset(dst + (r + 0) * stride + size, above[bs - 1], bs - size);
    memcpy(dst + (r + 1) * stride, dst + stride + (r >> 1), size);
    memset(dst + (r + 1) * stride + size, above[bs - 1], bs - size);
  }
}

static INLINE void d63e_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)left;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = r & 1 ? AVG3(above[(r >> 1) + c], above[(r >> 1) + c + 1],
                            above[(r >> 1) + c + 2])
                     : AVG2(above[(r >> 1) + c], above[(r >> 1) + c + 1]);
    }
    dst += stride;
  }
}

static INLINE void d45_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                 const uint8_t *above, const uint8_t *left) {
  const uint8_t above_right = above[bs - 1];
  const uint8_t *const dst_row0 = dst;
  int x, size;
  (void)left;

  for (x = 0; x < bs - 1; ++x) {
    dst[x] = AVG3(above[x], above[x + 1], above[x + 2]);
  }
  dst[bs - 1] = above_right;
  dst += stride;
  for (x = 1, size = bs - 2; x < bs; ++x, --size) {
    memcpy(dst, dst_row0 + x, size);
    memset(dst + size, above_right, x + 1);
    dst += stride;
  }
}

static INLINE void d45e_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  (void)left;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = AVG3(above[r + c], above[r + c + 1],
                    above[r + c + 1 + (r + c + 2 < bs * 2)]);
    }
    dst += stride;
  }
}

static INLINE void d117_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;

  // first row
  for (c = 0; c < bs; c++) dst[c] = AVG2(above[c - 1], above[c]);
  dst += stride;

  // second row
  dst[0] = AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);
  dst += stride;

  // the rest of first col
  dst[0] = AVG3(above[-1], left[0], left[1]);
  for (r = 3; r < bs; ++r)
    dst[(r - 2) * stride] = AVG3(left[r - 3], left[r - 2], left[r - 1]);

  // the rest of the block
  for (r = 2; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-2 * stride + c - 1];
    dst += stride;
  }
}

static INLINE void d135_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int i;
#if CONFIG_TX64X64
#if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ > 7
  // silence a spurious -Warray-bounds warning, possibly related to:
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56273
  uint8_t border[133];
#else
  uint8_t border[64 + 64 - 1];  // outer border from bottom-left to top-right
#endif
#else
#if defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ > 7
  // silence a spurious -Warray-bounds warning, possibly related to:
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56273
  uint8_t border[69];
#else
  uint8_t border[32 + 32 - 1];  // outer border from bottom-left to top-right
#endif
#endif  // CONFIG_TX64X64

  // dst(bs, bs - 2)[0], i.e., border starting at bottom-left
  for (i = 0; i < bs - 2; ++i) {
    border[i] = AVG3(left[bs - 3 - i], left[bs - 2 - i], left[bs - 1 - i]);
  }
  border[bs - 2] = AVG3(above[-1], left[0], left[1]);
  border[bs - 1] = AVG3(left[0], above[-1], above[0]);
  border[bs - 0] = AVG3(above[-1], above[0], above[1]);
  // dst[0][2, size), i.e., remaining top border ascending
  for (i = 0; i < bs - 2; ++i) {
    border[bs + 1 + i] = AVG3(above[i], above[i + 1], above[i + 2]);
  }

  for (i = 0; i < bs; ++i) {
    memcpy(dst + i * stride, border + bs - 1 - i, bs);
  }
}

static INLINE void d153_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                  const uint8_t *above, const uint8_t *left) {
  int r, c;
  dst[0] = AVG2(above[-1], left[0]);
  for (r = 1; r < bs; r++) dst[r * stride] = AVG2(left[r - 1], left[r]);
  dst++;

  dst[0] = AVG3(left[0], above[-1], above[0]);
  dst[stride] = AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; r++)
    dst[r * stride] = AVG3(left[r - 2], left[r - 1], left[r]);
  dst++;

  for (c = 0; c < bs - 2; c++)
    dst[c] = AVG3(above[c - 1], above[c], above[c + 1]);
  dst += stride;

  for (r = 1; r < bs; ++r) {
    for (c = 0; c < bs - 2; c++) dst[c] = dst[-stride + c - 2];
    dst += stride;
  }
}

static INLINE void v_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                               const uint8_t *above, const uint8_t *left) {
  int r;
  (void)left;

  for (r = 0; r < bs; r++) {
    memcpy(dst, above, bs);
    dst += stride;
  }
}

static INLINE void h_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                               const uint8_t *above, const uint8_t *left) {
  int r;
  (void)above;

  for (r = 0; r < bs; r++) {
    memset(dst, left[r], bs);
    dst += stride;
  }
}

#if CONFIG_ALT_INTRA
static INLINE int abs_diff(int a, int b) { return (a > b) ? a - b : b - a; }

static INLINE uint16_t paeth_predictor_single(uint16_t left, uint16_t top,
                                              uint16_t top_left) {
  const int base = top + left - top_left;
  const int p_left = abs_diff(base, left);
  const int p_top = abs_diff(base, top);
  const int p_top_left = abs_diff(base, top_left);

  // Return nearest to base of left, top and top_left.
  return (p_left <= p_top && p_left <= p_top_left)
             ? left
             : (p_top <= p_top_left) ? top : top_left;
}

static INLINE void paeth_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                   const uint8_t *above, const uint8_t *left) {
  int r, c;
  const uint8_t ytop_left = above[-1];

  for (r = 0; r < bs; r++) {
    for (c = 0; c < bs; c++)
      dst[c] = (uint8_t)paeth_predictor_single(left[r], above[c], ytop_left);
    dst += stride;
  }
}

// Weights are quadratic from 'bs' to '1'.
// Scale is same as 'bs'.
// TODO(urvang): Integerize the weights at a suitable precision.
#if CONFIG_TX64X64
static const double sm_weights_fwd[6][64] = {
#else
static const double sm_weights_fwd[5][32] = {
#endif  // CONFIG_TX64X64
  // bs = 2
  { 2, 1 },
  // bs = 4
  { 4, 2.33333, 1.33333, 1 },
  // bs = 8
  { 8, 6.14286, 4.57143, 3.28571, 2.28571, 1.57143, 1.14286, 1 },
  // bs = 16
  { 16, 14.0667, 12.2667, 10.6, 9.06667, 7.66667, 6.4, 5.26667, 4.26667, 3.4,
    2.66667, 2.06667, 1.6, 1.26667, 1.06667, 1 },
  // bs = 32
  { 32,      30.0323, 28.129,  26.2903, 24.5161, 22.8065, 21.1613, 19.5806,
    18.0645, 16.6129, 15.2258, 13.9032, 12.6452, 11.4516, 10.3226, 9.25806,
    8.25806, 7.32258, 6.45161, 5.64516, 4.90323, 4.22581, 3.6129,  3.06452,
    2.58065, 2.16129, 1.80645, 1.51613, 1.29032, 1.12903, 1.03226, 1 },
#if CONFIG_TX64X64
  // bs = 64
  { 64,      62.0159, 60.0635, 58.1429, 56.254,  54.3968, 52.5714, 50.7778,
    49.0159, 47.2857, 45.5873, 43.9206, 42.2857, 40.6825, 39.1111, 37.5714,
    36.0635, 34.5873, 33.1429, 31.7302, 30.3492, 29,      27.6825, 26.3968,
    25.1429, 23.9206, 22.7302, 21.5714, 20.4444, 19.3492, 18.2857, 17.254,
    16.254,  15.2857, 14.3492, 13.4444, 12.5714, 11.7302, 10.9206, 10.1429,
    9.39683, 8.68254, 8,       7.34921, 6.73016, 6.14286, 5.5873,  5.06349,
    4.57143, 4.11111, 3.68254, 3.28571, 2.92063, 2.5873,  2.28571, 2.01587,
    1.77778, 1.57143, 1.39683, 1.25397, 1.14286, 1.06349, 1.01587, 1 },
#endif  // CONFIG_TX64X64
};

#if CONFIG_TX64X64
static const double sm_weights_rev[6][64] = {
#else
static const double sm_weights_rev[5][32] = {
#endif  // CONFIG_TX64X64
  // bs = 2
  { 0, 1 },
  // bs = 4
  { 0, 1.66667, 2.66667, 3 },
  // bs = 8
  { 0, 1.85714, 3.42857, 4.71429, 5.71429, 6.42857, 6.85714, 7 },
  // bs = 16
  { 0, 1.93333, 3.73333, 5.4, 6.93333, 8.33333, 9.6, 10.7333, 11.7333, 12.6,
    13.3333, 13.9333, 14.4, 14.7333, 14.9333, 15 },
  // bs = 32
  { 0,       1.96774, 3.87097, 5.70968, 7.48387, 9.19355, 10.8387, 12.4194,
    13.9355, 15.3871, 16.7742, 18.0968, 19.3548, 20.5484, 21.6774, 22.7419,
    23.7419, 24.6774, 25.5484, 26.3548, 27.0968, 27.7742, 28.3871, 28.9355,
    29.4194, 29.8387, 30.1935, 30.4839, 30.7097, 30.871,  30.9677, 31 },
#if CONFIG_TX64X64
  // bs = 64
  { 0,       1.98413, 3.93651, 5.85714, 7.74603, 9.60317, 11.4286, 13.2222,
    14.9841, 16.7143, 18.4127, 20.0794, 21.7143, 23.3175, 24.8889, 26.4286,
    27.9365, 29.4127, 30.8571, 32.2698, 33.6508, 35,      36.3175, 37.6032,
    38.8571, 40.0794, 41.2698, 42.4286, 43.5556, 44.6508, 45.7143, 46.746,
    47.746,  48.7143, 49.6508, 50.5556, 51.4286, 52.2698, 53.0794, 53.8571,
    54.6032, 55.3175, 56,      56.6508, 57.2698, 57.8571, 58.4127, 58.9365,
    59.4286, 59.8889, 60.3175, 60.7143, 61.0794, 61.4127, 61.7143, 61.9841,
    62.2222, 62.4286, 62.6032, 62.746,  62.8571, 62.9365, 62.9841, 63 },
#endif  // CONFIG_TX64X64
};

static INLINE void smooth_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                    const uint8_t *above, const uint8_t *left) {
  const uint8_t below_pred = left[bs - 1];   // estimated by bottom-left pixel
  const uint8_t right_pred = above[bs - 1];  // estimated by top-right pixel
  const int arr_index = (int)lround(log2(bs)) - 1;
  const double *const fwd_weights = sm_weights_fwd[arr_index];
  const double *const rev_weights = sm_weights_rev[arr_index];
  const double scale = 2.0 * bs;
  int r;
  for (r = 0; r < bs; ++r) {
    int c;
    for (c = 0; c < bs; ++c) {
      const int pixels[] = { above[c], below_pred, left[r], right_pred };
      const double weights[] = { fwd_weights[r], rev_weights[r], fwd_weights[c],
                                 rev_weights[c] };
      double this_pred = 0;
      int i;
      for (i = 0; i < 4; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = clip_pixel(lround(this_pred / scale));
    }
    dst += stride;
  }
}

#else

static INLINE void tm_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                const uint8_t *above, const uint8_t *left) {
  int r, c;
  int ytop_left = above[-1];

  for (r = 0; r < bs; r++) {
    for (c = 0; c < bs; c++)
      dst[c] = clip_pixel(left[r] + above[c] - ytop_left);
    dst += stride;
  }
}
#endif  // CONFIG_ALT_INTRA

static INLINE void dc_128_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                    const uint8_t *above, const uint8_t *left) {
  int r;
  (void)above;
  (void)left;

  for (r = 0; r < bs; r++) {
    memset(dst, 128, bs);
    dst += stride;
  }
}

static INLINE void dc_left_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  (void)above;

  for (i = 0; i < bs; i++) sum += left[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    memset(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void dc_top_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                    const uint8_t *above, const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  (void)left;

  for (i = 0; i < bs; i++) sum += above[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    memset(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void dc_predictor(uint8_t *dst, ptrdiff_t stride, int bs,
                                const uint8_t *above, const uint8_t *left) {
  int i, r, expected_dc, sum = 0;
  const int count = 2 * bs;

  for (i = 0; i < bs; i++) {
    sum += above[i];
    sum += left[i];
  }

  expected_dc = (sum + (count >> 1)) / count;

  for (r = 0; r < bs; r++) {
    memset(dst, expected_dc, bs);
    dst += stride;
  }
}

void aom_he_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  const int H = above[-1];
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];

  memset(dst + stride * 0, AVG3(H, I, J), 2);
  memset(dst + stride * 1, AVG3(I, J, K), 2);
}

void aom_ve_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  const int H = above[-1];
  const int I = above[0];
  const int J = above[1];
  const int K = above[2];
  (void)left;

  dst[0] = AVG3(H, I, J);
  dst[1] = AVG3(I, J, K);
  memcpy(dst + stride * 1, dst, 2);
}

void aom_d207_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  (void)above;
  DST(0, 0) = AVG2(I, J);
  DST(0, 1) = AVG2(J, K);
  DST(1, 0) = AVG3(I, J, K);
  DST(1, 1) = AVG3(J, K, L);
}

void aom_d63_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  (void)left;
  DST(0, 0) = AVG2(A, B);
  DST(1, 0) = AVG2(B, C);
  DST(0, 1) = AVG3(A, B, C);
  DST(1, 1) = AVG3(B, C, D);
}

void aom_d63f_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  (void)left;
  DST(0, 0) = AVG2(A, B);
  DST(1, 0) = AVG2(B, C);
  DST(0, 1) = AVG3(A, B, C);
  DST(1, 1) = AVG3(B, C, D);
}

void aom_d45_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  (void)stride;
  (void)left;
  DST(0, 0) = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
  DST(1, 1) = AVG3(C, D, D);
}

void aom_d45e_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  (void)stride;
  (void)left;

  DST(0, 0) = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
  DST(1, 1) = AVG3(C, D, D);
}

void aom_d117_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  DST(0, 0) = AVG2(X, A);
  DST(1, 0) = AVG2(A, B);
  DST(0, 1) = AVG3(I, X, A);
  DST(1, 1) = AVG3(X, A, B);
}

void aom_d135_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  (void)stride;
  DST(0, 1) = AVG3(X, I, J);
  DST(1, 1) = DST(0, 0) = AVG3(A, X, I);
  DST(1, 0) = AVG3(B, A, X);
}

void aom_d153_predictor_2x2_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int X = above[-1];
  const int A = above[0];

  DST(0, 0) = AVG2(I, X);
  DST(0, 1) = AVG2(J, I);
  DST(1, 0) = AVG3(I, X, A);
  DST(1, 1) = AVG3(J, I, X);
}

void aom_he_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  const int H = above[-1];
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];

  memset(dst + stride * 0, AVG3(H, I, J), 4);
  memset(dst + stride * 1, AVG3(I, J, K), 4);
  memset(dst + stride * 2, AVG3(J, K, L), 4);
  memset(dst + stride * 3, AVG3(K, L, L), 4);
}

void aom_ve_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                            const uint8_t *above, const uint8_t *left) {
  const int H = above[-1];
  const int I = above[0];
  const int J = above[1];
  const int K = above[2];
  const int L = above[3];
  const int M = above[4];
  (void)left;

  dst[0] = AVG3(H, I, J);
  dst[1] = AVG3(I, J, K);
  dst[2] = AVG3(J, K, L);
  dst[3] = AVG3(K, L, M);
  memcpy(dst + stride * 1, dst, 4);
  memcpy(dst + stride * 2, dst, 4);
  memcpy(dst + stride * 3, dst, 4);
}

void aom_d207_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  (void)above;
  DST(0, 0) = AVG2(I, J);
  DST(2, 0) = DST(0, 1) = AVG2(J, K);
  DST(2, 1) = DST(0, 2) = AVG2(K, L);
  DST(1, 0) = AVG3(I, J, K);
  DST(3, 0) = DST(1, 1) = AVG3(J, K, L);
  DST(3, 1) = DST(1, 2) = AVG3(K, L, L);
  DST(3, 2) = DST(2, 2) = DST(0, 3) = DST(1, 3) = DST(2, 3) = DST(3, 3) = L;
}

void aom_d63_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  (void)left;
  DST(0, 0) = AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);
  DST(3, 2) = AVG2(E, F);  // differs from vp8

  DST(0, 1) = AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
  DST(3, 3) = AVG3(E, F, G);  // differs from vp8
}

void aom_d63f_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  const int H = above[7];
  (void)left;
  DST(0, 0) = AVG2(A, B);
  DST(1, 0) = DST(0, 2) = AVG2(B, C);
  DST(2, 0) = DST(1, 2) = AVG2(C, D);
  DST(3, 0) = DST(2, 2) = AVG2(D, E);
  DST(3, 2) = AVG3(E, F, G);

  DST(0, 1) = AVG3(A, B, C);
  DST(1, 1) = DST(0, 3) = AVG3(B, C, D);
  DST(2, 1) = DST(1, 3) = AVG3(C, D, E);
  DST(3, 1) = DST(2, 3) = AVG3(D, E, F);
  DST(3, 3) = AVG3(F, G, H);
}

void aom_d45_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                             const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  const int H = above[7];
  (void)stride;
  (void)left;
  DST(0, 0) = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
  DST(3, 3) = H;  // differs from vp8
}

void aom_d45e_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  const int E = above[4];
  const int F = above[5];
  const int G = above[6];
  const int H = above[7];
  (void)stride;
  (void)left;
  DST(0, 0) = AVG3(A, B, C);
  DST(1, 0) = DST(0, 1) = AVG3(B, C, D);
  DST(2, 0) = DST(1, 1) = DST(0, 2) = AVG3(C, D, E);
  DST(3, 0) = DST(2, 1) = DST(1, 2) = DST(0, 3) = AVG3(D, E, F);
  DST(3, 1) = DST(2, 2) = DST(1, 3) = AVG3(E, F, G);
  DST(3, 2) = DST(2, 3) = AVG3(F, G, H);
  DST(3, 3) = AVG3(G, H, H);
}

void aom_d117_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  DST(0, 0) = DST(1, 2) = AVG2(X, A);
  DST(1, 0) = DST(2, 2) = AVG2(A, B);
  DST(2, 0) = DST(3, 2) = AVG2(B, C);
  DST(3, 0) = AVG2(C, D);

  DST(0, 3) = AVG3(K, J, I);
  DST(0, 2) = AVG3(J, I, X);
  DST(0, 1) = DST(1, 3) = AVG3(I, X, A);
  DST(1, 1) = DST(2, 3) = AVG3(X, A, B);
  DST(2, 1) = DST(3, 3) = AVG3(A, B, C);
  DST(3, 1) = AVG3(B, C, D);
}

void aom_d135_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];
  const int D = above[3];
  (void)stride;
  DST(0, 3) = AVG3(J, K, L);
  DST(1, 3) = DST(0, 2) = AVG3(I, J, K);
  DST(2, 3) = DST(1, 2) = DST(0, 1) = AVG3(X, I, J);
  DST(3, 3) = DST(2, 2) = DST(1, 1) = DST(0, 0) = AVG3(A, X, I);
  DST(3, 2) = DST(2, 1) = DST(1, 0) = AVG3(B, A, X);
  DST(3, 1) = DST(2, 0) = AVG3(C, B, A);
  DST(3, 0) = AVG3(D, C, B);
}

void aom_d153_predictor_4x4_c(uint8_t *dst, ptrdiff_t stride,
                              const uint8_t *above, const uint8_t *left) {
  const int I = left[0];
  const int J = left[1];
  const int K = left[2];
  const int L = left[3];
  const int X = above[-1];
  const int A = above[0];
  const int B = above[1];
  const int C = above[2];

  DST(0, 0) = DST(2, 1) = AVG2(I, X);
  DST(0, 1) = DST(2, 2) = AVG2(J, I);
  DST(0, 2) = DST(2, 3) = AVG2(K, J);
  DST(0, 3) = AVG2(L, K);

  DST(3, 0) = AVG3(A, B, C);
  DST(2, 0) = AVG3(X, A, B);
  DST(1, 0) = DST(3, 1) = AVG3(I, X, A);
  DST(1, 1) = DST(3, 2) = AVG3(J, I, X);
  DST(1, 2) = DST(3, 3) = AVG3(K, J, I);
  DST(1, 3) = AVG3(L, K, J);
}

#if CONFIG_AOM_HIGHBITDEPTH
static INLINE void highbd_d207_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)above;
  (void)bd;

  // First column.
  for (r = 0; r < bs - 1; ++r) {
    dst[r * stride] = AVG2(left[r], left[r + 1]);
  }
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // Second column.
  for (r = 0; r < bs - 2; ++r) {
    dst[r * stride] = AVG3(left[r], left[r + 1], left[r + 2]);
  }
  dst[(bs - 2) * stride] = AVG3(left[bs - 2], left[bs - 1], left[bs - 1]);
  dst[(bs - 1) * stride] = left[bs - 1];
  dst++;

  // Rest of last row.
  for (c = 0; c < bs - 2; ++c) dst[(bs - 1) * stride + c] = left[bs - 1];

  for (r = bs - 2; r >= 0; --r) {
    for (c = 0; c < bs - 2; ++c)
      dst[r * stride + c] = dst[(r + 1) * stride + c - 2];
  }
}

static INLINE void highbd_d207e_predictor(uint16_t *dst, ptrdiff_t stride,
                                          int bs, const uint16_t *above,
                                          const uint16_t *left, int bd) {
  int r, c;
  (void)above;
  (void)bd;

  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = c & 1 ? AVG3(left[(c >> 1) + r], left[(c >> 1) + r + 1],
                            left[(c >> 1) + r + 2])
                     : AVG2(left[(c >> 1) + r], left[(c >> 1) + r + 1]);
    }
    dst += stride;
  }
}

static INLINE void highbd_d63_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  int r, c;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = r & 1 ? AVG3(above[(r >> 1) + c], above[(r >> 1) + c + 1],
                            above[(r >> 1) + c + 2])
                     : AVG2(above[(r >> 1) + c], above[(r >> 1) + c + 1]);
    }
    dst += stride;
  }
}

#define highbd_d63e_predictor highbd_d63_predictor

static INLINE void highbd_d45_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                        const uint16_t *above,
                                        const uint16_t *left, int bd) {
  int r, c;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = r + c + 2 < bs * 2
                   ? AVG3(above[r + c], above[r + c + 1], above[r + c + 2])
                   : above[bs * 2 - 1];
    }
    dst += stride;
  }
}

static INLINE void highbd_d45e_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; ++r) {
    for (c = 0; c < bs; ++c) {
      dst[c] = AVG3(above[r + c], above[r + c + 1],
                    above[r + c + 1 + (r + c + 2 < bs * 2)]);
    }
    dst += stride;
  }
}

static INLINE void highbd_d117_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)bd;

  // first row
  for (c = 0; c < bs; c++) dst[c] = AVG2(above[c - 1], above[c]);
  dst += stride;

  // second row
  dst[0] = AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);
  dst += stride;

  // the rest of first col
  dst[0] = AVG3(above[-1], left[0], left[1]);
  for (r = 3; r < bs; ++r)
    dst[(r - 2) * stride] = AVG3(left[r - 3], left[r - 2], left[r - 1]);

  // the rest of the block
  for (r = 2; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-2 * stride + c - 1];
    dst += stride;
  }
}

static INLINE void highbd_d135_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)bd;
  dst[0] = AVG3(left[0], above[-1], above[0]);
  for (c = 1; c < bs; c++) dst[c] = AVG3(above[c - 2], above[c - 1], above[c]);

  dst[stride] = AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; ++r)
    dst[r * stride] = AVG3(left[r - 2], left[r - 1], left[r]);

  dst += stride;
  for (r = 1; r < bs; ++r) {
    for (c = 1; c < bs; c++) dst[c] = dst[-stride + c - 1];
    dst += stride;
  }
}

static INLINE void highbd_d153_predictor(uint16_t *dst, ptrdiff_t stride,
                                         int bs, const uint16_t *above,
                                         const uint16_t *left, int bd) {
  int r, c;
  (void)bd;
  dst[0] = AVG2(above[-1], left[0]);
  for (r = 1; r < bs; r++) dst[r * stride] = AVG2(left[r - 1], left[r]);
  dst++;

  dst[0] = AVG3(left[0], above[-1], above[0]);
  dst[stride] = AVG3(above[-1], left[0], left[1]);
  for (r = 2; r < bs; r++)
    dst[r * stride] = AVG3(left[r - 2], left[r - 1], left[r]);
  dst++;

  for (c = 0; c < bs - 2; c++)
    dst[c] = AVG3(above[c - 1], above[c], above[c + 1]);
  dst += stride;

  for (r = 1; r < bs; ++r) {
    for (c = 0; c < bs - 2; c++) dst[c] = dst[-stride + c - 2];
    dst += stride;
  }
}

static INLINE void highbd_v_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  int r;
  (void)left;
  (void)bd;
  for (r = 0; r < bs; r++) {
    memcpy(dst, above, bs * sizeof(uint16_t));
    dst += stride;
  }
}

static INLINE void highbd_h_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                      const uint16_t *above,
                                      const uint16_t *left, int bd) {
  int r;
  (void)above;
  (void)bd;
  for (r = 0; r < bs; r++) {
    aom_memset16(dst, left[r], bs);
    dst += stride;
  }
}

#if CONFIG_ALT_INTRA
static INLINE void highbd_paeth_predictor(uint16_t *dst, ptrdiff_t stride,
                                          int bs, const uint16_t *above,
                                          const uint16_t *left, int bd) {
  int r, c;
  const uint16_t ytop_left = above[-1];
  (void)bd;

  for (r = 0; r < bs; r++) {
    for (c = 0; c < bs; c++)
      dst[c] = paeth_predictor_single(left[r], above[c], ytop_left);
    dst += stride;
  }
}

static INLINE void highbd_smooth_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bs, const uint16_t *above,
                                           const uint16_t *left, int bd) {
  const uint16_t below_pred = left[bs - 1];   // estimated by bottom-left pixel
  const uint16_t right_pred = above[bs - 1];  // estimated by top-right pixel
  const int arr_index = (int)lround(log2(bs)) - 1;
  const double *const fwd_weights = sm_weights_fwd[arr_index];
  const double *const rev_weights = sm_weights_rev[arr_index];
  const double scale = 2.0 * bs;
  int r;
  for (r = 0; r < bs; ++r) {
    int c;
    for (c = 0; c < bs; ++c) {
      const int pixels[] = { above[c], below_pred, left[r], right_pred };
      const double weights[] = { fwd_weights[r], rev_weights[r], fwd_weights[c],
                                 rev_weights[c] };
      double this_pred = 0;
      int i;
      for (i = 0; i < 4; ++i) {
        this_pred += weights[i] * pixels[i];
      }
      dst[c] = clip_pixel_highbd(lround(this_pred / scale), bd);
    }
    dst += stride;
  }
}

#else
static INLINE void highbd_tm_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int r, c;
  int ytop_left = above[-1];
  (void)bd;

  for (r = 0; r < bs; r++) {
    for (c = 0; c < bs; c++)
      dst[c] = clip_pixel_highbd(left[r] + above[c] - ytop_left, bd);
    dst += stride;
  }
}
#endif  // CONFIG_ALT_INTRA

static INLINE void highbd_dc_128_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bs, const uint16_t *above,
                                           const uint16_t *left, int bd) {
  int r;
  (void)above;
  (void)left;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, 128 << (bd - 8), bs);
    dst += stride;
  }
}

static INLINE void highbd_dc_left_predictor(uint16_t *dst, ptrdiff_t stride,
                                            int bs, const uint16_t *above,
                                            const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  (void)above;
  (void)bd;

  for (i = 0; i < bs; i++) sum += left[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void highbd_dc_top_predictor(uint16_t *dst, ptrdiff_t stride,
                                           int bs, const uint16_t *above,
                                           const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  (void)left;
  (void)bd;

  for (i = 0; i < bs; i++) sum += above[i];
  expected_dc = (sum + (bs >> 1)) / bs;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, expected_dc, bs);
    dst += stride;
  }
}

static INLINE void highbd_dc_predictor(uint16_t *dst, ptrdiff_t stride, int bs,
                                       const uint16_t *above,
                                       const uint16_t *left, int bd) {
  int i, r, expected_dc, sum = 0;
  const int count = 2 * bs;
  (void)bd;

  for (i = 0; i < bs; i++) {
    sum += above[i];
    sum += left[i];
  }

  expected_dc = (sum + (count >> 1)) / count;

  for (r = 0; r < bs; r++) {
    aom_memset16(dst, expected_dc, bs);
    dst += stride;
  }
}
#endif  // CONFIG_AOM_HIGHBITDEPTH

// This serves as a wrapper function, so that all the prediction functions
// can be unified and accessed as a pointer array. Note that the boundary
// above and left are not necessarily used all the time.
#define intra_pred_sized(type, size)                        \
  void aom_##type##_predictor_##size##x##size##_c(          \
      uint8_t *dst, ptrdiff_t stride, const uint8_t *above, \
      const uint8_t *left) {                                \
    type##_predictor(dst, stride, size, above, left);       \
  }

#if CONFIG_AOM_HIGHBITDEPTH
#define intra_pred_highbd_sized(type, size)                        \
  void aom_highbd_##type##_predictor_##size##x##size##_c(          \
      uint16_t *dst, ptrdiff_t stride, const uint16_t *above,      \
      const uint16_t *left, int bd) {                              \
    highbd_##type##_predictor(dst, stride, size, above, left, bd); \
  }

/* clang-format off */
#if CONFIG_TX64X64
#define intra_pred_allsizes(type) \
  intra_pred_sized(type, 2) \
  intra_pred_sized(type, 4) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32) \
  intra_pred_sized(type, 64) \
  intra_pred_highbd_sized(type, 4) \
  intra_pred_highbd_sized(type, 8) \
  intra_pred_highbd_sized(type, 16) \
  intra_pred_highbd_sized(type, 32) \
  intra_pred_highbd_sized(type, 64)

#define intra_pred_above_4x4(type) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32) \
  intra_pred_sized(type, 64) \
  intra_pred_highbd_sized(type, 4) \
  intra_pred_highbd_sized(type, 8) \
  intra_pred_highbd_sized(type, 16) \
  intra_pred_highbd_sized(type, 32) \
  intra_pred_highbd_sized(type, 64)
#else  // CONFIG_TX64X64
#define intra_pred_allsizes(type) \
  intra_pred_sized(type, 2) \
  intra_pred_sized(type, 4) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32) \
  intra_pred_highbd_sized(type, 4) \
  intra_pred_highbd_sized(type, 8) \
  intra_pred_highbd_sized(type, 16) \
  intra_pred_highbd_sized(type, 32)

#define intra_pred_above_4x4(type) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32) \
  intra_pred_highbd_sized(type, 4) \
  intra_pred_highbd_sized(type, 8) \
  intra_pred_highbd_sized(type, 16) \
  intra_pred_highbd_sized(type, 32)
#endif  // CONFIG_TX64X64

#else

#if CONFIG_TX64X64
#define intra_pred_allsizes(type) \
  intra_pred_sized(type, 2) \
  intra_pred_sized(type, 4) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32) \
  intra_pred_sized(type, 64)

#define intra_pred_above_4x4(type) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32) \
  intra_pred_sized(type, 64)
#else  // CONFIG_TX64X64
#define intra_pred_allsizes(type) \
  intra_pred_sized(type, 2) \
  intra_pred_sized(type, 4) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32)

#define intra_pred_above_4x4(type) \
  intra_pred_sized(type, 8) \
  intra_pred_sized(type, 16) \
  intra_pred_sized(type, 32)
#endif  // CONFIG_TX64X64
#endif  // CONFIG_AOM_HIGHBITDEPTH

intra_pred_above_4x4(d207)
intra_pred_above_4x4(d63)
intra_pred_above_4x4(d45)
intra_pred_allsizes(d207e)
intra_pred_allsizes(d63e)
intra_pred_above_4x4(d45e)
intra_pred_above_4x4(d117)
intra_pred_above_4x4(d135)
intra_pred_above_4x4(d153)
intra_pred_allsizes(v)
intra_pred_allsizes(h)
#if CONFIG_ALT_INTRA
intra_pred_allsizes(paeth)
intra_pred_allsizes(smooth)
#else
intra_pred_allsizes(tm)
#endif  // CONFIG_ALT_INTRA
intra_pred_allsizes(dc_128)
intra_pred_allsizes(dc_left)
intra_pred_allsizes(dc_top)
intra_pred_allsizes(dc)
/* clang-format on */
#undef intra_pred_allsizes
