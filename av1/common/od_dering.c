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
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "./config.h"
#endif

#include "./aom_dsp_rtcd.h"
#include "./av1_rtcd.h"
#include "./cdef.h"

/* Generated from gen_filter_tables.c. */
const int OD_DIRECTION_OFFSETS_TABLE[8][3] = {
  { -1 * OD_FILT_BSTRIDE + 1, -2 * OD_FILT_BSTRIDE + 2,
    -3 * OD_FILT_BSTRIDE + 3 },
  { 0 * OD_FILT_BSTRIDE + 1, -1 * OD_FILT_BSTRIDE + 2,
    -1 * OD_FILT_BSTRIDE + 3 },
  { 0 * OD_FILT_BSTRIDE + 1, 0 * OD_FILT_BSTRIDE + 2, 0 * OD_FILT_BSTRIDE + 3 },
  { 0 * OD_FILT_BSTRIDE + 1, 1 * OD_FILT_BSTRIDE + 2, 1 * OD_FILT_BSTRIDE + 3 },
  { 1 * OD_FILT_BSTRIDE + 1, 2 * OD_FILT_BSTRIDE + 2, 3 * OD_FILT_BSTRIDE + 3 },
  { 1 * OD_FILT_BSTRIDE + 0, 2 * OD_FILT_BSTRIDE + 1, 3 * OD_FILT_BSTRIDE + 1 },
  { 1 * OD_FILT_BSTRIDE + 0, 2 * OD_FILT_BSTRIDE + 0, 3 * OD_FILT_BSTRIDE + 0 },
  { 1 * OD_FILT_BSTRIDE + 0, 2 * OD_FILT_BSTRIDE - 1, 3 * OD_FILT_BSTRIDE - 1 },
};

/* Detect direction. 0 means 45-degree up-right, 2 is horizontal, and so on.
   The search minimizes the weighted variance along all the lines in a
   particular direction, i.e. the squared error between the input and a
   "predicted" block where each pixel is replaced by the average along a line
   in a particular direction. Since each direction have the same sum(x^2) term,
   that term is never computed. See Section 2, step 2, of:
   http://jmvalin.ca/notes/intra_paint.pdf */
int od_dir_find8_c(const uint16_t *img, int stride, int32_t *var,
                   int coeff_shift) {
  int i;
  int32_t cost[8] = { 0 };
  int partial[8][15] = { { 0 } };
  int32_t best_cost = 0;
  int best_dir = 0;
  /* Instead of dividing by n between 2 and 8, we multiply by 3*5*7*8/n.
     The output is then 840 times larger, but we don't care for finding
     the max. */
  static const int div_table[] = { 0, 840, 420, 280, 210, 168, 140, 120, 105 };
  for (i = 0; i < 8; i++) {
    int j;
    for (j = 0; j < 8; j++) {
      int x;
      /* We subtract 128 here to reduce the maximum range of the squared
         partial sums. */
      x = (img[i * stride + j] >> coeff_shift) - 128;
      partial[0][i + j] += x;
      partial[1][i + j / 2] += x;
      partial[2][i] += x;
      partial[3][3 + i - j / 2] += x;
      partial[4][7 + i - j] += x;
      partial[5][3 - i / 2 + j] += x;
      partial[6][j] += x;
      partial[7][i / 2 + j] += x;
    }
  }
  for (i = 0; i < 8; i++) {
    cost[2] += partial[2][i] * partial[2][i];
    cost[6] += partial[6][i] * partial[6][i];
  }
  cost[2] *= div_table[8];
  cost[6] *= div_table[8];
  for (i = 0; i < 7; i++) {
    cost[0] += (partial[0][i] * partial[0][i] +
                partial[0][14 - i] * partial[0][14 - i]) *
               div_table[i + 1];
    cost[4] += (partial[4][i] * partial[4][i] +
                partial[4][14 - i] * partial[4][14 - i]) *
               div_table[i + 1];
  }
  cost[0] += partial[0][7] * partial[0][7] * div_table[8];
  cost[4] += partial[4][7] * partial[4][7] * div_table[8];
  for (i = 1; i < 8; i += 2) {
    int j;
    for (j = 0; j < 4 + 1; j++) {
      cost[i] += partial[i][3 + j] * partial[i][3 + j];
    }
    cost[i] *= div_table[8];
    for (j = 0; j < 4 - 1; j++) {
      cost[i] += (partial[i][j] * partial[i][j] +
                  partial[i][10 - j] * partial[i][10 - j]) *
                 div_table[2 * j + 2];
    }
  }
  for (i = 0; i < 8; i++) {
    if (cost[i] > best_cost) {
      best_cost = cost[i];
      best_dir = i;
    }
  }
  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var = best_cost - cost[(best_dir + 4) & 7];
  /* We'd normally divide by 840, but dividing by 1024 is close enough
     for what we're going to do with this. */
  *var >>= 10;
  return best_dir;
}

/* Smooth in the direction detected. */
int od_filter_dering_direction_8x8_c(uint16_t *y, int ystride,
                                     const uint16_t *in, int threshold,
                                     int dir) {
  int i;
  int j;
  int k;
  static const int taps[3] = { 3, 2, 1 };
  int total_abs = 0;
  for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
      int16_t sum;
      int16_t xx;
      int16_t yy;
      xx = in[i * OD_FILT_BSTRIDE + j];
      sum = 0;
      for (k = 0; k < 3; k++) {
        int16_t p0;
        int16_t p1;
        p0 = in[i * OD_FILT_BSTRIDE + j + OD_DIRECTION_OFFSETS_TABLE[dir][k]] -
             xx;
        p1 = in[i * OD_FILT_BSTRIDE + j - OD_DIRECTION_OFFSETS_TABLE[dir][k]] -
             xx;
        if (abs(p0) < threshold) sum += taps[k] * p0;
        if (abs(p1) < threshold) sum += taps[k] * p1;
      }
      sum = (sum + 8) >> 4;
      total_abs += abs(sum);
      yy = xx + sum;
      y[i * ystride + j] = yy;
    }
  }
  return (total_abs + 8) >> 4;
}

/* Smooth in the direction detected. */
int od_filter_dering_direction_4x4_c(uint16_t *y, int ystride,
                                     const uint16_t *in, int threshold,
                                     int dir) {
  int i;
  int j;
  int k;
  static const int taps[2] = { 4, 1 };
  int total_abs = 0;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
      int16_t sum;
      int16_t xx;
      int16_t yy;
      xx = in[i * OD_FILT_BSTRIDE + j];
      sum = 0;
      for (k = 0; k < 2; k++) {
        int16_t p0;
        int16_t p1;
        p0 = in[i * OD_FILT_BSTRIDE + j + OD_DIRECTION_OFFSETS_TABLE[dir][k]] -
             xx;
        p1 = in[i * OD_FILT_BSTRIDE + j - OD_DIRECTION_OFFSETS_TABLE[dir][k]] -
             xx;
        if (abs(p0) < threshold) sum += taps[k] * p0;
        if (abs(p1) < threshold) sum += taps[k] * p1;
      }
      sum = (sum + 8) >> 4;
      total_abs += abs(sum);
      yy = xx + sum;
      y[i * ystride + j] = yy;
    }
  }
  return (total_abs + 2) >> 2;
}

/* This table approximates x^0.16 with the index being log2(x). It is clamped
   to [-.5, 3]. The table is computed as:
   round(256*min(3, max(.5, 1.08*(sqrt(2)*2.^([0:17]+8)/256/256).^.16))) */
static const int16_t OD_THRESH_TABLE_Q8[18] = {
  128, 134, 150, 168, 188, 210, 234, 262, 292,
  327, 365, 408, 455, 509, 569, 635, 710, 768,
};

/* Compute deringing filter threshold for an 8x8 block based on the
   directional variance difference. A high variance difference means that we
   have a highly directional pattern (e.g. a high contrast edge), so we can
   apply more deringing. A low variance means that we either have a low
   contrast edge, or a non-directional texture, so we want to be careful not
   to blur. */
static INLINE int od_adjust_thresh(int threshold, int32_t var) {
  int v1;
  /* We use the variance of 8x8 blocks to adjust the threshold. */
  v1 = OD_MINI(32767, var >> 6);
  return (threshold * OD_THRESH_TABLE_Q8[OD_ILOG(v1)] + 128) >> 8;
}

void copy_8x8_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src,
                               int sstride) {
  int i, j;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++) dst[i * dstride + j] = src[i * sstride + j];
}

void copy_4x4_16bit_to_16bit_c(uint16_t *dst, int dstride, const uint16_t *src,
                               int sstride) {
  int i, j;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++) dst[i * dstride + j] = src[i * sstride + j];
}

void copy_dering_16bit_to_16bit(uint16_t *dst, int dstride, uint16_t *src,
                                dering_list *dlist, int dering_count,
                                BLOCK_SIZE bsize) {
  int bi, bx, by;
  const int mi_size_l2 = (bsize == BLOCK_8X8) ? MI_SIZE_LOG2 : MI_SIZE_LOG2 - 1;

  if (bsize == BLOCK_8X8) {
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_8x8_16bit_to_16bit(
          &dst[(by << mi_size_l2) * dstride + (bx << mi_size_l2)], dstride,
          &src[bi << (2 * 3)], 8);
    }
  } else {
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_16bit(
          &dst[(by << mi_size_l2) * dstride + (bx << mi_size_l2)], dstride,
          &src[bi << (2 * 2)], 4);
    }
  }
}

void copy_8x8_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src,
                              int sstride) {
  int i, j;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      dst[i * dstride + j] = (uint8_t)src[i * sstride + j];
}

void copy_4x4_16bit_to_8bit_c(uint8_t *dst, int dstride, const uint16_t *src,
                              int sstride) {
  int i, j;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      dst[i * dstride + j] = (uint8_t)src[i * sstride + j];
}

static void copy_dering_16bit_to_8bit(uint8_t *dst, int dstride,
                                      const uint16_t *src, dering_list *dlist,
                                      int dering_count, int bsize) {
  int bi, bx, by;
  if (bsize == 3) {
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_8x8_16bit_to_8bit(&dst[(by << 3) * dstride + (bx << 3)], dstride,
                             &src[bi << 2 * bsize], 1 << bsize);
    }
  } else {
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      copy_4x4_16bit_to_8bit(&dst[(by << 2) * dstride + (bx << 2)], dstride,
                             &src[bi << 2 * bsize], 1 << bsize);
    }
  }
}

void od_dering(uint8_t *dst, int dstride, uint16_t *y, uint16_t *in, int xdec,
               int dir[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS], int *dirinit,
               int var[OD_DERING_NBLOCKS][OD_DERING_NBLOCKS], int pli,
               dering_list *dlist, int dering_count, int level,
               int clpf_strength, int clpf_damping, int coeff_shift,
               int skip_dering, int hbd) {
  int bi;
  int bx;
  int by;
  int bsize;

  // TODO(stemidts): We might be good with fewer strengths and different
  // strengths for chroma.  Perhaps reduce CDEF_STRENGTH_BITS to 5 and
  // DERING_STRENGTHS to 8 and use the following tables:
  // static int level_table[DERING_STRENGTHS] = {0, 1, 3, 7, 14, 24, 39, 63};
  // static int level_table_uv[DERING_STRENGTHS] = {0, 1, 2, 5, 8, 12, 18, 25};
  // For now, use 21 strengths and the same for luma and chroma.
  static int level_table[DERING_STRENGTHS] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 24, 28, 33, 39, 46, 54, 63
  };
  static int level_table_uv[DERING_STRENGTHS] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 24, 28, 33, 39, 46, 54, 63
  };

  int threshold = (pli ? level_table_uv : level_table)[level] << coeff_shift;
  const int mi_size_l2 = xdec ? MI_SIZE_LOG2 - 1 : MI_SIZE_LOG2;
  od_filter_dering_direction_func filter_dering_direction[OD_DERINGSIZES] = {
    od_filter_dering_direction_4x4, od_filter_dering_direction_8x8
  };
  bsize = OD_DERING_SIZE_LOG2 - xdec;
  if (!skip_dering) {
    if (pli == 0) {
      if (!dirinit || !*dirinit) {
        for (bi = 0; bi < dering_count; bi++) {
          by = dlist[bi].by;
          bx = dlist[bi].bx;
          dir[by][bx] =
              od_dir_find8(&in[MI_SIZE * by * OD_FILT_BSTRIDE + MI_SIZE * bx],
                           OD_FILT_BSTRIDE, &var[by][bx], coeff_shift);
        }
        if (dirinit) *dirinit = 1;
      }
      for (bi = 0; bi < dering_count; bi++) {
        by = dlist[bi].by;
        bx = dlist[bi].bx;
        /* Deringing orthogonal to the direction uses a tighter threshold
           because we want to be conservative. We've presumably already
           achieved some deringing, so the amount of change is expected
           to be low. Also, since we might be filtering across an edge, we
           want to make sure not to blur it. That being said, we might want
           to be a little bit more aggressive on pure horizontal/vertical
           since the ringing there tends to be directional, so it doesn't
           get removed by the directional filtering. */
        (filter_dering_direction[bsize - OD_LOG_BSIZE0])(
            &y[bi << 2 * bsize], 1 << bsize,
            &in[(by * OD_FILT_BSTRIDE << mi_size_l2) + (bx << mi_size_l2)],
            od_adjust_thresh(threshold, var[by][bx]), dir[by][bx]);
      }
    } else {
      for (bi = 0; bi < dering_count; bi++) {
        by = dlist[bi].by;
        bx = dlist[bi].bx;
        (filter_dering_direction[bsize - OD_LOG_BSIZE0])(
            &y[bi << 2 * bsize], 1 << bsize,
            &in[(by * OD_FILT_BSTRIDE << mi_size_l2) + (bx << mi_size_l2)],
            threshold, dir[by][bx]);
      }
    }
  }

  if (clpf_strength) {
    if (threshold && !skip_dering)
      copy_dering_16bit_to_16bit(in, OD_FILT_BSTRIDE, y, dlist, dering_count,
                                 xdec ? BLOCK_4X4 : BLOCK_8X8);
    for (bi = 0; bi < dering_count; bi++) {
      by = dlist[bi].by;
      bx = dlist[bi].bx;
      int py = by << mi_size_l2;
      int px = bx << mi_size_l2;

      if (!dst || hbd) {
        // 16 bit destination if high bitdepth or 8 bit destination not given
        (!threshold || (dir[by][bx] < 4 && dir[by][bx]) ? aom_clpf_block_hbd
                                                        : aom_clpf_hblock_hbd)(
            dst ? (uint16_t *)dst + py * dstride + px : &y[bi << 2 * bsize],
            in + py * OD_FILT_BSTRIDE + px, dst && hbd ? dstride : 1 << bsize,
            OD_FILT_BSTRIDE, 1 << bsize, 1 << bsize,
            clpf_strength << coeff_shift, clpf_damping + coeff_shift);
      } else {
        // Do clpf and write the result to an 8 bit destination
        (!threshold || (dir[by][bx] < 4 && dir[by][bx]) ? aom_clpf_block
                                                        : aom_clpf_hblock)(
            dst + py * dstride + px, in + py * OD_FILT_BSTRIDE + px, dstride,
            OD_FILT_BSTRIDE, 1 << bsize, 1 << bsize,
            clpf_strength << coeff_shift, clpf_damping + coeff_shift);
      }
    }
  } else {
    // No clpf, so copy instead
    if (hbd) {
      copy_dering_16bit_to_16bit((uint16_t *)dst, dstride, y, dlist,
                                 dering_count, 3 - xdec);
    } else {
      copy_dering_16bit_to_8bit(dst, dstride, y, dlist, dering_count, bsize);
    }
  }
}
