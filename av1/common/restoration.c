/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 *
 */

#include <math.h>

#include "./aom_config.h"
#include "./aom_dsp_rtcd.h"
#include "./aom_scale_rtcd.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/restoration.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_mem/aom_mem.h"

#include "aom_ports/mem.h"

const sgr_params_type sgr_params[SGRPROJ_PARAMS] = {
#if USE_HIGHPASS_IN_SGRPROJ
  // corner, edge, r2, eps2
  { -1, 2, 1, 1 }, { -1, 2, 1, 2 }, { -1, 2, 1, 3 }, { -1, 2, 1, 4 },
  { -1, 2, 1, 5 }, { -2, 3, 1, 2 }, { -2, 3, 1, 3 }, { -2, 3, 1, 4 },
  { -2, 3, 1, 5 }, { -2, 3, 1, 6 }, { -3, 4, 1, 3 }, { -3, 4, 1, 4 },
  { -3, 4, 1, 5 }, { -3, 4, 1, 6 }, { -3, 4, 1, 7 }, { -3, 4, 1, 8 }
#else
// r1, eps1, r2, eps2
#if MAX_RADIUS == 2
  { 2, 12, 1, 4 },  { 2, 15, 1, 6 },  { 2, 18, 1, 8 },  { 2, 20, 1, 9 },
  { 2, 22, 1, 10 }, { 2, 25, 1, 11 }, { 2, 35, 1, 12 }, { 2, 45, 1, 13 },
  { 2, 55, 1, 14 }, { 2, 65, 1, 15 }, { 2, 75, 1, 16 }, { 2, 30, 1, 6 },
  { 2, 50, 1, 12 }, { 2, 60, 1, 13 }, { 2, 70, 1, 14 }, { 2, 80, 1, 15 },
#else
  { 2, 12, 1, 4 },  { 2, 15, 1, 6 },  { 2, 18, 1, 8 },  { 2, 20, 1, 9 },
  { 2, 22, 1, 10 }, { 2, 25, 1, 11 }, { 2, 35, 1, 12 }, { 2, 45, 1, 13 },
  { 2, 55, 1, 14 }, { 2, 65, 1, 15 }, { 2, 75, 1, 16 }, { 3, 30, 1, 10 },
  { 3, 50, 1, 12 }, { 3, 50, 2, 25 }, { 3, 60, 2, 35 }, { 3, 70, 2, 45 },
#endif  // MAX_RADIUS == 2
#endif
};

int av1_alloc_restoration_struct(AV1_COMMON *cm, RestorationInfo *rst_info,
                                 int width, int height) {
  const int ntiles = av1_get_rest_ntiles(
      width, height, rst_info->restoration_tilesize, NULL, NULL);
  aom_free(rst_info->unit_info);
  CHECK_MEM_ERROR(
      cm, rst_info->unit_info,
      (RestorationUnitInfo *)aom_malloc(sizeof(*rst_info->unit_info) * ntiles));
  return ntiles;
}

void av1_free_restoration_struct(RestorationInfo *rst_info) {
  aom_free(rst_info->unit_info);
  rst_info->unit_info = NULL;
}

// TODO(debargha): This table can be substantially reduced since only a few
// values are actually used.
int sgrproj_mtable[MAX_EPS][MAX_NELEM];

static void GenSgrprojVtable() {
  int e, n;
  for (e = 1; e <= MAX_EPS; ++e)
    for (n = 1; n <= MAX_NELEM; ++n) {
      const int n2e = n * n * e;
      sgrproj_mtable[e - 1][n - 1] =
          (((1 << SGRPROJ_MTABLE_BITS) + n2e / 2) / n2e);
    }
}

void av1_loop_restoration_precal() { GenSgrprojVtable(); }

static void extend_frame_lowbd(uint8_t *data, int width, int height, int stride,
                               int border_horz, int border_vert) {
  uint8_t *data_p;
  int i;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    memset(data_p - border_horz, data_p[0], border_horz);
    memset(data_p + width, data_p[width - 1], border_horz);
  }
  data_p = data - border_horz;
  for (i = -border_vert; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p, width + 2 * border_horz);
  }
  for (i = height; i < height + border_vert; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           width + 2 * border_horz);
  }
}

#if CONFIG_HIGHBITDEPTH
static void extend_frame_highbd(uint16_t *data, int width, int height,
                                int stride, int border_horz, int border_vert) {
  uint16_t *data_p;
  int i, j;
  for (i = 0; i < height; ++i) {
    data_p = data + i * stride;
    for (j = -border_horz; j < 0; ++j) data_p[j] = data_p[0];
    for (j = width; j < width + border_horz; ++j) data_p[j] = data_p[width - 1];
  }
  data_p = data - border_horz;
  for (i = -border_vert; i < 0; ++i) {
    memcpy(data_p + i * stride, data_p,
           (width + 2 * border_horz) * sizeof(uint16_t));
  }
  for (i = height; i < height + border_vert; ++i) {
    memcpy(data_p + i * stride, data_p + (height - 1) * stride,
           (width + 2 * border_horz) * sizeof(uint16_t));
  }
}
#endif

void extend_frame(uint8_t *data, int width, int height, int stride,
                  int border_horz, int border_vert, int highbd) {
#if !CONFIG_HIGHBITDEPTH
  assert(highbd == 0);
  (void)highbd;
#else
  if (highbd)
    extend_frame_highbd(CONVERT_TO_SHORTPTR(data), width, height, stride,
                        border_horz, border_vert);
  else
#endif
  extend_frame_lowbd(data, width, height, stride, border_horz, border_vert);
}

static void copy_tile_lowbd(int width, int height, const uint8_t *src,
                            int src_stride, uint8_t *dst, int dst_stride) {
  for (int i = 0; i < height; ++i)
    memcpy(dst + i * dst_stride, src + i * src_stride, width);
}

#if CONFIG_HIGHBITDEPTH
static void copy_tile_highbd(int width, int height, const uint16_t *src,
                             int src_stride, uint16_t *dst, int dst_stride) {
  for (int i = 0; i < height; ++i)
    memcpy(dst + i * dst_stride, src + i * src_stride, width * sizeof(*dst));
}
#endif

static void copy_tile(int width, int height, const uint8_t *src, int src_stride,
                      uint8_t *dst, int dst_stride, int highbd) {
#if !CONFIG_HIGHBITDEPTH
  assert(highbd == 0);
  (void)highbd;
#else
  if (highbd)
    copy_tile_highbd(width, height, CONVERT_TO_SHORTPTR(src), src_stride,
                     CONVERT_TO_SHORTPTR(dst), dst_stride);
  else
#endif
  copy_tile_lowbd(width, height, src, src_stride, dst, dst_stride);
}

#if CONFIG_STRIPED_LOOP_RESTORATION
#define REAL_PTR(hbd, d) ((hbd) ? (uint8_t *)CONVERT_TO_SHORTPTR(d) : (d))

// With striped loop restoration, the filtering for each 64-pixel stripe gets
// most of its input from the output of CDEF (stored in data8), but pixels just
// above and below the stripe come straight from the deblocker. These have been
// stored away in separate buffers.
//
// This function modifies data8 (which was the output from CDEF) by copying in
// the boundary pixels. Before doing so, it saves the pixels that get
// overwritten into a temporary buffer. They will be restored again by
// restore_processing_stripe_boundary.
//
// limits gives the rectangular limits of the remaining stripes for the current
// restoration unit. rsb is the stored stripe boundaries (the saved output from
// the deblocker). stripe_height is the height of each stripe. ss_y is true if
// we're on a chroma plane with vertical subsampling. use_highbd is true if the
// data has 2 bytes per pixel. rlbs contain scratch buffers to hold the CDEF
// data (written back to the frame by restore_processing_stripe_boundary)
static int setup_processing_stripe_boundary(
    const RestorationTileLimits *limits, const RestorationStripeBoundaries *rsb,
    int stripe_height, int ss_y, int use_highbd, uint8_t *data8, int stride,
    RestorationLineBuffers *rlbs) {
  // Which stripe is this? limits->v_start is the top of the stripe in pixel
  // units, but we add tile_offset to get the number of pixels from the top of
  // the first stripe, which lies off the image.
  const int tile_offset = RESTORATION_TILE_OFFSET >> ss_y;
  const int stripe_index = (limits->v_start + tile_offset) / stripe_height;

  // Horizontal offsets within the line buffers. The buffer logically starts at
  // column -RESTORATION_EXTRA_HORZ. We'll start our copy from the column
  // limits->h_start - RESTORATION_EXTRA_HORZ and copy up to the column
  // limits->h_end + RESTORATION_EXTRA_HORZ.
  const int buf_stride = rsb->stripe_boundary_stride;
  const int buf_x0_off = limits->h_start;
  const int line_width =
      (limits->h_end - limits->h_start) + 2 * RESTORATION_EXTRA_HORZ;
  const int line_size = line_width << use_highbd;
  const int data_x0_off = limits->h_start - RESTORATION_EXTRA_HORZ;

  assert(CONFIG_HIGHBITDEPTH || !use_highbd);

  // Replace the pixels above the top of the stripe, unless this is the top of
  // the image.
  if (stripe_index > 0) {
    const int above_buf_y = 2 * (stripe_index - 1);
    uint8_t *data8_tl = data8 + (limits->v_start - 2) * stride + data_x0_off;

    for (int i = 0; i < 2; ++i) {
      const int buf_off = buf_x0_off + (above_buf_y + i) * buf_stride;
      const uint8_t *src = rsb->stripe_boundary_above + (buf_off << use_highbd);
      uint8_t *dst8 = data8_tl + i * stride;
      // Save old pixels, then replace with data from boundary_above_buf
      memcpy(rlbs->tmp_save_above[i], REAL_PTR(use_highbd, dst8), line_size);
      memcpy(REAL_PTR(use_highbd, dst8), src, line_size);
    }
  }

  // Replace the pixels below the bottom of the stripe if necessary. This might
  // not be needed if the stripe is less than stripe_height high (which might
  // happen on the bottom of a loop restoration unit), in which case
  // rows_needed_below might be negative.
  const int stripe_bottom = stripe_height * (1 + stripe_index) - tile_offset;
  const int rows_needed_below = AOMMIN(limits->v_end + 2 - stripe_bottom, 2);

  const int below_buf_y = 2 * stripe_index;
  uint8_t *data8_bl = data8 + stripe_bottom * stride + data_x0_off;

  for (int i = 0; i < rows_needed_below; ++i) {
    const int buf_off = buf_x0_off + (below_buf_y + i) * buf_stride;
    const uint8_t *src = rsb->stripe_boundary_below + (buf_off << use_highbd);
    uint8_t *dst8 = data8_bl + i * stride;
    // Save old pixels, then replace with data from boundary_below_buf
    memcpy(rlbs->tmp_save_below[i], REAL_PTR(use_highbd, dst8), line_size);
    memcpy(REAL_PTR(use_highbd, dst8), src, line_size);
  }

  // Finally, return the actual height of this stripe.
  return AOMMIN(limits->v_end, stripe_bottom) - limits->v_start;
}

// This function restores the boundary lines modified by
// setup_processing_stripe_boundary.
static void restore_processing_stripe_boundary(
    const RestorationTileLimits *limits, const RestorationLineBuffers *rlbs,
    int stripe_height, int ss_y, int use_highbd, uint8_t *data8, int stride) {
  const int tile_offset = RESTORATION_TILE_OFFSET >> ss_y;
  const int stripe_index = (limits->v_start + tile_offset) / stripe_height;

  const int line_width =
      (limits->h_end - limits->h_start) + 2 * RESTORATION_EXTRA_HORZ;
  const int line_size = line_width << use_highbd;
  const int data_x0_off = limits->h_start - RESTORATION_EXTRA_HORZ;

  assert(CONFIG_HIGHBITDEPTH || !use_highbd);

  if (stripe_index > 0) {
    uint8_t *data8_tl = data8 + (limits->v_start - 2) * stride + data_x0_off;
    for (int i = 0; i < 2; ++i) {
      uint8_t *dst8 = data8_tl + i * stride;
      // Save old pixels, then replace with data from boundary_above_buf
      memcpy(REAL_PTR(use_highbd, dst8), rlbs->tmp_save_above[i], line_size);
    }
  }

  const int stripe_bottom = stripe_height * (1 + stripe_index) - tile_offset;
  const int rows_needed_below = AOMMIN(limits->v_end + 2 - stripe_bottom, 2);

  uint8_t *data8_bl = data8 + stripe_bottom * stride + data_x0_off;

  for (int i = 0; i < rows_needed_below; ++i) {
    uint8_t *dst8 = data8_bl + i * stride;
    // Save old pixels, then replace with data from boundary_below_buf
    memcpy(REAL_PTR(use_highbd, dst8), rlbs->tmp_save_below[i], line_size);
  }
}
#undef REAL_PTR
#endif

static void stepdown_wiener_kernel(const InterpKernel orig, InterpKernel vert,
                                   int boundary_dist, int istop) {
  memcpy(vert, orig, sizeof(InterpKernel));
  switch (boundary_dist) {
    case 0:
      vert[WIENER_HALFWIN] += vert[2] + vert[1] + vert[0];
      vert[2] = vert[1] = vert[0] = 0;
      break;
    case 1:
      vert[2] += vert[1] + vert[0];
      vert[1] = vert[0] = 0;
      break;
    case 2:
      vert[1] += vert[0];
      vert[0] = 0;
      break;
    default: break;
  }
  if (!istop) {
    int tmp;
    tmp = vert[0];
    vert[0] = vert[WIENER_WIN - 1];
    vert[WIENER_WIN - 1] = tmp;
    tmp = vert[1];
    vert[1] = vert[WIENER_WIN - 2];
    vert[WIENER_WIN - 2] = tmp;
    tmp = vert[2];
    vert[2] = vert[WIENER_WIN - 3];
    vert[WIENER_WIN - 3] = tmp;
  }
}

#if USE_WIENER_HIGH_INTERMEDIATE_PRECISION
#define wiener_convolve8_add_src aom_convolve8_add_src_hip
#else
#define wiener_convolve8_add_src aom_convolve8_add_src
#endif

static void wiener_filter_stripe(const RestorationUnitInfo *rui,
                                 int stripe_width, int stripe_height,
                                 int procunit_width, const uint8_t *src,
                                 int src_stride, uint8_t *dst, int dst_stride,
                                 int32_t *tmpbuf, int bit_depth) {
  (void)tmpbuf;
  (void)bit_depth;
  assert(bit_depth == 8);

  const int mid_height =
      stripe_height - (WIENER_HALFWIN - WIENER_BORDER_VERT) * 2;
  assert(mid_height > 0);
  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, (stripe_width - j + 15) & ~15);
    const uint8_t *src_p = src + j;
    uint8_t *dst_p = dst + j;
    for (int b = 0; b < WIENER_HALFWIN - WIENER_BORDER_VERT; ++b) {
      InterpKernel vertical_top;
      stepdown_wiener_kernel(rui->wiener_info.vfilter, vertical_top,
                             WIENER_BORDER_VERT + b, 1);
      wiener_convolve8_add_src(src_p, src_stride, dst_p, dst_stride,
                               rui->wiener_info.hfilter, 16, vertical_top, 16,
                               w, 1);
      src_p += src_stride;
      dst_p += dst_stride;
    }

    wiener_convolve8_add_src(src_p, src_stride, dst_p, dst_stride,
                             rui->wiener_info.hfilter, 16,
                             rui->wiener_info.vfilter, 16, w, mid_height);
    src_p += src_stride * mid_height;
    dst_p += dst_stride * mid_height;

    for (int b = WIENER_HALFWIN - WIENER_BORDER_VERT - 1; b >= 0; --b) {
      InterpKernel vertical_bot;
      stepdown_wiener_kernel(rui->wiener_info.vfilter, vertical_bot,
                             WIENER_BORDER_VERT + b, 0);
      wiener_convolve8_add_src(src_p, src_stride, dst_p, dst_stride,
                               rui->wiener_info.hfilter, 16, vertical_bot, 16,
                               w, 1);
      src_p += src_stride;
      dst_p += dst_stride;
    }
  }
}

/* Calculate windowed sums (if sqr=0) or sums of squares (if sqr=1)
   over the input. The window is of size (2r + 1)x(2r + 1), and we
   specialize to r = 1, 2, 3. A default function is used for r > 3.

   Each loop follows the same format: We keep a window's worth of input
   in individual variables and select data out of that as appropriate.
*/
static void boxsum1(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c;

  // Vertical sum over 3-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 1) * src_stride + j]
        // b = src[(i    ) * src_stride + j]
        // c = src[(i + 1) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];

      dst[j] = a + b;
      for (i = 1; i < height - 2; ++i) {
        dst[i * dst_stride + j] = a + b + c;
        a = b;
        b = c;
        c = src[(i + 2) * src_stride + j] * src[(i + 2) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c;
      dst[(i + 1) * dst_stride + j] = b + c;
    }
  }

  // Horizontal sum over 3-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];

    dst[i * dst_stride] = a + b;
    for (j = 1; j < width - 2; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 1)]
      // b = src[i * src_stride + (j    )]
      // c = src[i * src_stride + (j + 1)]
      dst[i * dst_stride + j] = a + b + c;
      a = b;
      b = c;
      c = dst[i * dst_stride + (j + 2)];
    }
    dst[i * dst_stride + j] = a + b + c;
    dst[i * dst_stride + (j + 1)] = b + c;
  }
}

static void boxsum2(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e;

  // Vertical sum over 5-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        // Loop invariant: At the start of each iteration,
        // a = src[(i - 2) * src_stride + j]
        // b = src[(i - 1) * src_stride + j]
        // c = src[(i    ) * src_stride + j]
        // d = src[(i + 1) * src_stride + j]
        // e = src[(i + 2) * src_stride + j]
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[src_stride + j] * src[src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];

      dst[j] = a + b + c;
      dst[dst_stride + j] = a + b + c + d;
      for (i = 2; i < height - 3; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e;
        a = b;
        b = c;
        c = d;
        d = e;
        e = src[(i + 3) * src_stride + j] * src[(i + 3) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e;
      dst[(i + 1) * dst_stride + j] = b + c + d + e;
      dst[(i + 2) * dst_stride + j] = c + d + e;
    }
  }

  // Horizontal sum over 5-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];

    dst[i * dst_stride] = a + b + c;
    dst[i * dst_stride + 1] = a + b + c + d;
    for (j = 2; j < width - 3; ++j) {
      // Loop invariant: At the start of each iteration,
      // a = src[i * src_stride + (j - 2)]
      // b = src[i * src_stride + (j - 1)]
      // c = src[i * src_stride + (j    )]
      // d = src[i * src_stride + (j + 1)]
      // e = src[i * src_stride + (j + 2)]
      dst[i * dst_stride + j] = a + b + c + d + e;
      a = b;
      b = c;
      c = d;
      d = e;
      e = dst[i * dst_stride + (j + 3)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e;
    dst[i * dst_stride + (j + 1)] = b + c + d + e;
    dst[i * dst_stride + (j + 2)] = c + d + e;
  }
}

static void boxsum3(int32_t *src, int width, int height, int src_stride,
                    int sqr, int32_t *dst, int dst_stride) {
  int i, j, a, b, c, d, e, f, g;

  // Vertical sum over 7-pixel regions, from src into dst.
  if (!sqr) {
    for (j = 0; j < width; ++j) {
      a = src[j];
      b = src[1 * src_stride + j];
      c = src[2 * src_stride + j];
      d = src[3 * src_stride + j];
      e = src[4 * src_stride + j];
      f = src[5 * src_stride + j];
      g = src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  } else {
    for (j = 0; j < width; ++j) {
      a = src[j] * src[j];
      b = src[1 * src_stride + j] * src[1 * src_stride + j];
      c = src[2 * src_stride + j] * src[2 * src_stride + j];
      d = src[3 * src_stride + j] * src[3 * src_stride + j];
      e = src[4 * src_stride + j] * src[4 * src_stride + j];
      f = src[5 * src_stride + j] * src[5 * src_stride + j];
      g = src[6 * src_stride + j] * src[6 * src_stride + j];

      dst[j] = a + b + c + d;
      dst[dst_stride + j] = a + b + c + d + e;
      dst[2 * dst_stride + j] = a + b + c + d + e + f;
      for (i = 3; i < height - 4; ++i) {
        dst[i * dst_stride + j] = a + b + c + d + e + f + g;
        a = b;
        b = c;
        c = d;
        d = e;
        e = f;
        f = g;
        g = src[(i + 4) * src_stride + j] * src[(i + 4) * src_stride + j];
      }
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      dst[(i + 1) * dst_stride + j] = b + c + d + e + f + g;
      dst[(i + 2) * dst_stride + j] = c + d + e + f + g;
      dst[(i + 3) * dst_stride + j] = d + e + f + g;
    }
  }

  // Horizontal sum over 7-pixel regions of dst
  for (i = 0; i < height; ++i) {
    a = dst[i * dst_stride];
    b = dst[i * dst_stride + 1];
    c = dst[i * dst_stride + 2];
    d = dst[i * dst_stride + 3];
    e = dst[i * dst_stride + 4];
    f = dst[i * dst_stride + 5];
    g = dst[i * dst_stride + 6];

    dst[i * dst_stride] = a + b + c + d;
    dst[i * dst_stride + 1] = a + b + c + d + e;
    dst[i * dst_stride + 2] = a + b + c + d + e + f;
    for (j = 3; j < width - 4; ++j) {
      dst[i * dst_stride + j] = a + b + c + d + e + f + g;
      a = b;
      b = c;
      c = d;
      d = e;
      e = f;
      f = g;
      g = dst[i * dst_stride + (j + 4)];
    }
    dst[i * dst_stride + j] = a + b + c + d + e + f + g;
    dst[i * dst_stride + (j + 1)] = b + c + d + e + f + g;
    dst[i * dst_stride + (j + 2)] = c + d + e + f + g;
    dst[i * dst_stride + (j + 3)] = d + e + f + g;
  }
}

// Generic version for any r. To be removed after experiments are done.
static void boxsumr(int32_t *src, int width, int height, int src_stride, int r,
                    int sqr, int32_t *dst, int dst_stride) {
  int32_t *tmp = aom_malloc(width * height * sizeof(*tmp));
  int tmp_stride = width;
  int i, j;
  if (sqr) {
    for (j = 0; j < width; ++j) tmp[j] = src[j] * src[j];
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] +
            src[i * src_stride + j] * src[i * src_stride + j];
  } else {
    memcpy(tmp, src, sizeof(*tmp) * width);
    for (j = 0; j < width; ++j)
      for (i = 1; i < height; ++i)
        tmp[i * tmp_stride + j] =
            tmp[(i - 1) * tmp_stride + j] + src[i * src_stride + j];
  }
  for (i = 0; i <= r; ++i)
    memcpy(&dst[i * dst_stride], &tmp[(i + r) * tmp_stride],
           sizeof(*tmp) * width);
  for (i = r + 1; i < height - r; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] =
          tmp[(i + r) * tmp_stride + j] - tmp[(i - r - 1) * tmp_stride + j];
  for (i = height - r; i < height; ++i)
    for (j = 0; j < width; ++j)
      dst[i * dst_stride + j] = tmp[(height - 1) * tmp_stride + j] -
                                tmp[(i - r - 1) * tmp_stride + j];

  for (i = 0; i < height; ++i) tmp[i * tmp_stride] = dst[i * dst_stride];
  for (i = 0; i < height; ++i)
    for (j = 1; j < width; ++j)
      tmp[i * tmp_stride + j] =
          tmp[i * tmp_stride + j - 1] + dst[i * src_stride + j];

  for (j = 0; j <= r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] = tmp[i * tmp_stride + j + r];
  for (j = r + 1; j < width - r; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + j + r] - tmp[i * tmp_stride + j - r - 1];
  for (j = width - r; j < width; ++j)
    for (i = 0; i < height; ++i)
      dst[i * dst_stride + j] =
          tmp[i * tmp_stride + width - 1] - tmp[i * tmp_stride + j - r - 1];
  aom_free(tmp);
}

static void boxsum(int32_t *src, int width, int height, int src_stride, int r,
                   int sqr, int32_t *dst, int dst_stride) {
  if (r == 1)
    boxsum1(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 2)
    boxsum2(src, width, height, src_stride, sqr, dst, dst_stride);
  else if (r == 3)
    boxsum3(src, width, height, src_stride, sqr, dst, dst_stride);
  else
    boxsumr(src, width, height, src_stride, r, sqr, dst, dst_stride);
}

static void boxnum(int width, int height, int r, int8_t *num, int num_stride) {
  int i, j;
  for (i = 0; i <= r; ++i) {
    for (j = 0; j <= r; ++j) {
      num[i * num_stride + j] = (r + 1 + i) * (r + 1 + j);
      num[i * num_stride + (width - 1 - j)] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + j] = num[i * num_stride + j];
      num[(height - 1 - i) * num_stride + (width - 1 - j)] =
          num[i * num_stride + j];
    }
  }
  for (j = 0; j <= r; ++j) {
    const int val = (2 * r + 1) * (r + 1 + j);
    for (i = r + 1; i < height - r; ++i) {
      num[i * num_stride + j] = val;
      num[i * num_stride + (width - 1 - j)] = val;
    }
  }
  for (i = 0; i <= r; ++i) {
    const int val = (2 * r + 1) * (r + 1 + i);
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = val;
      num[(height - 1 - i) * num_stride + j] = val;
    }
  }
  for (i = r + 1; i < height - r; ++i) {
    for (j = r + 1; j < width - r; ++j) {
      num[i * num_stride + j] = (2 * r + 1) * (2 * r + 1);
    }
  }
}

void decode_xq(const int *xqd, int *xq) {
  xq[0] = xqd[0];
  xq[1] = (1 << SGRPROJ_PRJ_BITS) - xq[0] - xqd[1];
}

const int32_t x_by_xplus1[256] = {
  0,   128, 171, 192, 205, 213, 219, 224, 228, 230, 233, 235, 236, 238, 239,
  240, 241, 242, 243, 243, 244, 244, 245, 245, 246, 246, 247, 247, 247, 247,
  248, 248, 248, 248, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250,
  250, 251, 251, 251, 251, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252,
  252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253,
  253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
  254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
  256,
};

const int32_t one_by_x[MAX_NELEM] = {
  4096, 2048, 1365, 1024, 819, 683, 585, 512, 455, 410, 372, 341, 315,
  293,  273,  256,  241,  228, 216, 205, 195, 186, 178, 171, 164,
#if MAX_RADIUS > 2
  158,  152,  146,  141,  137, 132, 128, 124, 120, 117, 114, 111, 108,
  105,  102,  100,  98,   95,  93,  91,  89,  87,  85,  84
#endif  // MAX_RADIUS > 2
};

static void av1_selfguided_restoration_internal(int32_t *dgd, int width,
                                                int height, int dgd_stride,
                                                int32_t *dst, int dst_stride,
                                                int bit_depth, int r, int eps) {
  const int width_ext = width + 2 * SGRPROJ_BORDER_HORZ;
  const int height_ext = height + 2 * SGRPROJ_BORDER_VERT;
  const int num_stride = width_ext;
  // Adjusting the stride of A and B here appears to avoid bad cache effects,
  // leading to a significant speed improvement.
  // We also align the stride to a multiple of 16 bytes, for consistency
  // with the SIMD version of this function.
  int buf_stride = ((width_ext + 3) & ~3) + 16;
  int32_t A_[RESTORATION_PROC_UNIT_PELS];
  int32_t B_[RESTORATION_PROC_UNIT_PELS];
  int32_t *A = A_;
  int32_t *B = B_;
  int8_t num_[RESTORATION_PROC_UNIT_PELS];
  int8_t *num = num_ + SGRPROJ_BORDER_VERT * num_stride + SGRPROJ_BORDER_HORZ;
  int i, j;

  // Don't filter tiles with dimensions < 5 on any axis
  if ((width < 5) || (height < 5)) return;

  boxsum(dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ,
         width_ext, height_ext, dgd_stride, r, 0, B, buf_stride);
  boxsum(dgd - dgd_stride * SGRPROJ_BORDER_VERT - SGRPROJ_BORDER_HORZ,
         width_ext, height_ext, dgd_stride, r, 1, A, buf_stride);
  boxnum(width_ext, height_ext, r, num_, num_stride);
  assert(r <= 3);
  A += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  B += SGRPROJ_BORDER_VERT * buf_stride + SGRPROJ_BORDER_HORZ;
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * buf_stride + j;
      const int n = num[i * num_stride + j];

      // a < 2^16 * n < 2^22 regardless of bit depth
      uint32_t a = ROUND_POWER_OF_TWO(A[k], 2 * (bit_depth - 8));
      // b < 2^8 * n < 2^14 regardless of bit depth
      uint32_t b = ROUND_POWER_OF_TWO(B[k], bit_depth - 8);

      // Each term in calculating p = a * n - b * b is < 2^16 * n^2 < 2^28,
      // and p itself satisfies p < 2^14 * n^2 < 2^26.
      // Note: Sometimes, in high bit depth, we can end up with a*n < b*b.
      // This is an artefact of rounding, and can only happen if all pixels
      // are (almost) identical, so in this case we saturate to p=0.
      uint32_t p = (a * n < b * b) ? 0 : a * n - b * b;
      uint32_t s = sgrproj_mtable[eps - 1][n - 1];

      // p * s < (2^14 * n^2) * round(2^20 / n^2 eps) < 2^34 / eps < 2^32
      // as long as eps >= 4. So p * s fits into a uint32_t, and z < 2^12
      // (this holds even after accounting for the rounding in s)
      const uint32_t z = ROUND_POWER_OF_TWO(p * s, SGRPROJ_MTABLE_BITS);

      A[k] = x_by_xplus1[AOMMIN(z, 255)];  // < 2^8

      // SGRPROJ_SGR - A[k] < 2^8, B[k] < 2^(bit_depth) * n,
      // one_by_x[n - 1] = round(2^12 / n)
      // => the product here is < 2^(20 + bit_depth) <= 2^32,
      // and B[k] is set to a value < 2^(8 + bit depth)
      B[k] = (int32_t)ROUND_POWER_OF_TWO((uint32_t)(SGRPROJ_SGR - A[k]) *
                                             (uint32_t)B[k] *
                                             (uint32_t)one_by_x[n - 1],
                                         SGRPROJ_RECIP_BITS);
    }
  }
  i = 0;
  j = 0;
  {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k + buf_stride] + A[k + buf_stride + 1];
    const int32_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k + buf_stride] + B[k + buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k + buf_stride] + A[k + buf_stride - 1];
    const int32_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k + buf_stride] + B[k + buf_stride - 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k + 1] + 2 * A[k - buf_stride] + A[k - buf_stride + 1];
    const int32_t b =
        3 * B[k] + 2 * B[k + 1] + 2 * B[k - buf_stride] + B[k - buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a =
        3 * A[k] + 2 * A[k - 1] + 2 * A[k - buf_stride] + A[k - buf_stride - 1];
    const int32_t b =
        3 * B[k] + 2 * B[k - 1] + 2 * B[k - buf_stride] + B[k - buf_stride - 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k + buf_stride] +
                      A[k + buf_stride - 1] + A[k + buf_stride + 1];
    const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k + buf_stride] +
                      B[k + buf_stride - 1] + B[k + buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - 1] + A[k + 1]) + A[k - buf_stride] +
                      A[k - buf_stride - 1] + A[k - buf_stride + 1];
    const int32_t b = B[k] + 2 * (B[k - 1] + B[k + 1]) + B[k - buf_stride] +
                      B[k - buf_stride - 1] + B[k - buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                      A[k + 1] + A[k - buf_stride + 1] + A[k + buf_stride + 1];
    const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                      B[k + 1] + B[k - buf_stride + 1] + B[k + buf_stride + 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * buf_stride + j;
    const int l = i * dgd_stride + j;
    const int m = i * dst_stride + j;
    const int nb = 3;
    const int32_t a = A[k] + 2 * (A[k - buf_stride] + A[k + buf_stride]) +
                      A[k - 1] + A[k - buf_stride - 1] + A[k + buf_stride - 1];
    const int32_t b = B[k] + 2 * (B[k - buf_stride] + B[k + buf_stride]) +
                      B[k - 1] + B[k - buf_stride - 1] + B[k + buf_stride - 1];
    const int32_t v = a * dgd[l] + b;
    dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * buf_stride + j;
      const int l = i * dgd_stride + j;
      const int m = i * dst_stride + j;
      const int nb = 5;
      const int32_t a =
          (A[k] + A[k - 1] + A[k + 1] + A[k - buf_stride] + A[k + buf_stride]) *
              4 +
          (A[k - 1 - buf_stride] + A[k - 1 + buf_stride] +
           A[k + 1 - buf_stride] + A[k + 1 + buf_stride]) *
              3;
      const int32_t b =
          (B[k] + B[k - 1] + B[k + 1] + B[k - buf_stride] + B[k + buf_stride]) *
              4 +
          (B[k - 1 - buf_stride] + B[k - 1 + buf_stride] +
           B[k + 1 - buf_stride] + B[k + 1 + buf_stride]) *
              3;
      const int32_t v = a * dgd[l] + b;
      dst[m] = ROUND_POWER_OF_TWO(v, SGRPROJ_SGR_BITS + nb - SGRPROJ_RST_BITS);
    }
  }
}

void av1_selfguided_restoration_c(const uint8_t *dgd, int width, int height,
                                  int stride, int32_t *dst, int dst_stride,
                                  int r, int eps) {
  int32_t dgd32_[RESTORATION_PROC_UNIT_PELS];
  const int dgd32_stride = width + 2 * SGRPROJ_BORDER_HORZ;
  int32_t *dgd32 =
      dgd32_ + dgd32_stride * SGRPROJ_BORDER_VERT + SGRPROJ_BORDER_HORZ;
  int i, j;
  for (i = -SGRPROJ_BORDER_VERT; i < height + SGRPROJ_BORDER_VERT; ++i) {
    for (j = -SGRPROJ_BORDER_HORZ; j < width + SGRPROJ_BORDER_HORZ; ++j) {
      dgd32[i * dgd32_stride + j] = dgd[i * stride + j];
    }
  }
  av1_selfguided_restoration_internal(dgd32, width, height, dgd32_stride, dst,
                                      dst_stride, 8, r, eps);
}

void av1_highpass_filter_c(const uint8_t *dgd, int width, int height,
                           int stride, int32_t *dst, int dst_stride, int corner,
                           int edge) {
  int i, j;
  const int center = (1 << SGRPROJ_RST_BITS) - 4 * (corner + edge);

  i = 0;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride + 1] + dgd[k + 1] + dgd[k + stride] + dgd[k]);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride - 1] + dgd[k - 1] + dgd[k + stride] + dgd[k]);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride + 1] + dgd[k + 1] + dgd[k - stride] + dgd[k]);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride - 1] + dgd[k - 1] + dgd[k - stride] + dgd[k]);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k + stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k + stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k - stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k - stride - 1] + dgd[k - stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k + 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride + 1] + dgd[k - stride + 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k + 1]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride + 1] + dgd[k + stride + 1]);
    }
  }
}

void apply_selfguided_restoration_c(const uint8_t *dat, int width, int height,
                                    int stride, int eps, const int *xqd,
                                    uint8_t *dst, int dst_stride,
                                    int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int i, j;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
#if USE_HIGHPASS_IN_SGRPROJ
  av1_highpass_filter_c(dat, width, height, stride, flt1, width,
                        sgr_params[eps].corner, sgr_params[eps].edge);
#else
  av1_selfguided_restoration_c(dat, width, height, stride, flt1, width,
                               sgr_params[eps].r1, sgr_params[eps].e1);
#endif  // USE_HIGHPASS_IN_SGRPROJ
  av1_selfguided_restoration_c(dat, width, height, stride, flt2, width,
                               sgr_params[eps].r2, sgr_params[eps].e2);
  decode_xq(xqd, xq);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = clip_pixel(w);
    }
  }
}

static void sgrproj_filter_stripe(const RestorationUnitInfo *rui,
                                  int stripe_width, int stripe_height,
                                  int procunit_width, const uint8_t *src,
                                  int src_stride, uint8_t *dst, int dst_stride,
                                  int32_t *tmpbuf, int bit_depth) {
  (void)bit_depth;
  assert(bit_depth == 8);

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, stripe_width - j);
    apply_selfguided_restoration(src + j, w, stripe_height, src_stride,
                                 rui->sgrproj_info.ep, rui->sgrproj_info.xqd,
                                 dst + j, dst_stride, tmpbuf);
  }
}

#if CONFIG_HIGHBITDEPTH
#if USE_WIENER_HIGH_INTERMEDIATE_PRECISION
#define wiener_highbd_convolve8_add_src aom_highbd_convolve8_add_src_hip
#else
#define wiener_highbd_convolve8_add_src aom_highbd_convolve8_add_src
#endif

static void wiener_filter_stripe_highbd(const RestorationUnitInfo *rui,
                                        int stripe_width, int stripe_height,
                                        int procunit_width, const uint8_t *src8,
                                        int src_stride, uint8_t *dst8,
                                        int dst_stride, int32_t *tmpbuf,
                                        int bit_depth) {
  (void)tmpbuf;

  const int mid_height =
      stripe_height - (WIENER_HALFWIN - WIENER_BORDER_VERT) * 2;
  assert(mid_height > 0);

  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, (stripe_width - j + 15) & ~15);
    const uint8_t *src8_p = src8 + j;
    uint8_t *dst8_p = dst8 + j;

    for (int b = 0; b < WIENER_HALFWIN - WIENER_BORDER_VERT; ++b) {
      InterpKernel vertical_top;
      stepdown_wiener_kernel(rui->wiener_info.vfilter, vertical_top,
                             WIENER_BORDER_VERT + b, 1);
      wiener_highbd_convolve8_add_src(src8_p, src_stride, dst8_p, dst_stride,
                                      rui->wiener_info.hfilter, 16,
                                      vertical_top, 16, w, 1, bit_depth);
      src8_p += src_stride;
      dst8_p += dst_stride;
    }
    assert(stripe_height > (WIENER_HALFWIN - WIENER_BORDER_VERT) * 2);
    wiener_highbd_convolve8_add_src(
        src8_p, src_stride, dst8_p, dst_stride, rui->wiener_info.hfilter, 16,
        rui->wiener_info.vfilter, 16, w, mid_height, bit_depth);
    src8_p += src_stride * (mid_height);
    dst8_p += dst_stride * (mid_height);
    for (int b = WIENER_HALFWIN - WIENER_BORDER_VERT - 1; b >= 0; --b) {
      InterpKernel vertical_bot;
      stepdown_wiener_kernel(rui->wiener_info.vfilter, vertical_bot,
                             WIENER_BORDER_VERT + b, 0);
      wiener_highbd_convolve8_add_src(src8_p, src_stride, dst8_p, dst_stride,
                                      rui->wiener_info.hfilter, 16,
                                      vertical_bot, 16, w, 1, bit_depth);
      src8_p += src_stride;
      dst8_p += dst_stride;
    }
  }
}

void av1_selfguided_restoration_highbd_c(const uint16_t *dgd, int width,
                                         int height, int stride, int32_t *dst,
                                         int dst_stride, int bit_depth, int r,
                                         int eps) {
  int32_t dgd32_[RESTORATION_PROC_UNIT_PELS];
  const int dgd32_stride = width + 2 * SGRPROJ_BORDER_HORZ;
  int32_t *dgd32 =
      dgd32_ + dgd32_stride * SGRPROJ_BORDER_VERT + SGRPROJ_BORDER_HORZ;
  int i, j;
  for (i = -SGRPROJ_BORDER_VERT; i < height + SGRPROJ_BORDER_VERT; ++i) {
    for (j = -SGRPROJ_BORDER_HORZ; j < width + SGRPROJ_BORDER_HORZ; ++j) {
      dgd32[i * dgd32_stride + j] = dgd[i * stride + j];
    }
  }
  av1_selfguided_restoration_internal(dgd32, width, height, dgd32_stride, dst,
                                      dst_stride, bit_depth, r, eps);
}

void av1_highpass_filter_highbd_c(const uint16_t *dgd, int width, int height,
                                  int stride, int32_t *dst, int dst_stride,
                                  int corner, int edge) {
  int i, j;
  const int center = (1 << SGRPROJ_RST_BITS) - 4 * (corner + edge);

  i = 0;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride + 1] + dgd[k + 1] + dgd[k + stride] + dgd[k]);
  }
  i = 0;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k + stride] + dgd[k] * 2) +
        corner * (dgd[k + stride - 1] + dgd[k - 1] + dgd[k + stride] + dgd[k]);
  }
  i = height - 1;
  j = 0;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k + 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride + 1] + dgd[k + 1] + dgd[k - stride] + dgd[k]);
  }
  i = height - 1;
  j = width - 1;
  {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] =
        center * dgd[k] + edge * (dgd[k - 1] + dgd[k - stride] + dgd[k] * 2) +
        corner * (dgd[k - stride - 1] + dgd[k - 1] + dgd[k - stride] + dgd[k]);
  }
  i = 0;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k + stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k + stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  i = height - 1;
  for (j = 1; j < width - 1; ++j) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - 1] + dgd[k - stride] + dgd[k + 1] + dgd[k]) +
             corner * (dgd[k - stride - 1] + dgd[k - stride + 1] + dgd[k - 1] +
                       dgd[k + 1]);
  }
  j = 0;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k + 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride + 1] + dgd[k - stride + 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  j = width - 1;
  for (i = 1; i < height - 1; ++i) {
    const int k = i * stride + j;
    const int l = i * dst_stride + j;
    dst[l] = center * dgd[k] +
             edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k]) +
             corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                       dgd[k - stride] + dgd[k + stride]);
  }
  for (i = 1; i < height - 1; ++i) {
    for (j = 1; j < width - 1; ++j) {
      const int k = i * stride + j;
      const int l = i * dst_stride + j;
      dst[l] =
          center * dgd[k] +
          edge * (dgd[k - stride] + dgd[k - 1] + dgd[k + stride] + dgd[k + 1]) +
          corner * (dgd[k + stride - 1] + dgd[k - stride - 1] +
                    dgd[k - stride + 1] + dgd[k + stride + 1]);
    }
  }
}

void apply_selfguided_restoration_highbd_c(const uint16_t *dat, int width,
                                           int height, int stride,
                                           int bit_depth, int eps,
                                           const int *xqd, uint16_t *dst,
                                           int dst_stride, int32_t *tmpbuf) {
  int xq[2];
  int32_t *flt1 = tmpbuf;
  int32_t *flt2 = flt1 + RESTORATION_TILEPELS_MAX;
  int i, j;
  assert(width * height <= RESTORATION_TILEPELS_MAX);
#if USE_HIGHPASS_IN_SGRPROJ
  av1_highpass_filter_highbd_c(dat, width, height, stride, flt1, width,
                               sgr_params[eps].corner, sgr_params[eps].edge);
#else
  av1_selfguided_restoration_highbd_c(dat, width, height, stride, flt1, width,
                                      bit_depth, sgr_params[eps].r1,
                                      sgr_params[eps].e1);
#endif  // USE_HIGHPASS_IN_SGRPROJ
  av1_selfguided_restoration_highbd_c(dat, width, height, stride, flt2, width,
                                      bit_depth, sgr_params[eps].r2,
                                      sgr_params[eps].e2);
  decode_xq(xqd, xq);
  for (i = 0; i < height; ++i) {
    for (j = 0; j < width; ++j) {
      const int k = i * width + j;
      const int l = i * stride + j;
      const int m = i * dst_stride + j;
      const int32_t u = ((int32_t)dat[l] << SGRPROJ_RST_BITS);
      const int32_t f1 = (int32_t)flt1[k] - u;
      const int32_t f2 = (int32_t)flt2[k] - u;
      const int32_t v = xq[0] * f1 + xq[1] * f2 + (u << SGRPROJ_PRJ_BITS);
      const int16_t w =
          (int16_t)ROUND_POWER_OF_TWO(v, SGRPROJ_PRJ_BITS + SGRPROJ_RST_BITS);
      dst[m] = (uint16_t)clip_pixel_highbd(w, bit_depth);
    }
  }
}

static void sgrproj_filter_stripe_highbd(const RestorationUnitInfo *rui,
                                         int stripe_width, int stripe_height,
                                         int procunit_width,
                                         const uint8_t *src8, int src_stride,
                                         uint8_t *dst8, int dst_stride,
                                         int32_t *tmpbuf, int bit_depth) {
  for (int j = 0; j < stripe_width; j += procunit_width) {
    int w = AOMMIN(procunit_width, stripe_width - j);
    const uint16_t *data_p = CONVERT_TO_SHORTPTR(src8) + j;
    uint16_t *dst_p = CONVERT_TO_SHORTPTR(dst8) + j;
    apply_selfguided_restoration_highbd(
        data_p, w, stripe_height, src_stride, bit_depth, rui->sgrproj_info.ep,
        rui->sgrproj_info.xqd, dst_p, dst_stride, tmpbuf);
  }
}
#endif  // CONFIG_HIGHBITDEPTH

typedef void (*stripe_filter_fun)(const RestorationUnitInfo *rui,
                                  int stripe_width, int stripe_height,
                                  int procunit_width, const uint8_t *src,
                                  int src_stride, uint8_t *dst, int dst_stride,
                                  int32_t *tmpbuf, int bit_depth);

#if CONFIG_HIGHBITDEPTH
#define NUM_STRIPE_FILTERS 4
#else
#define NUM_STRIPE_FILTERS 2
#endif

static const stripe_filter_fun stripe_filters[NUM_STRIPE_FILTERS] = {
  wiener_filter_stripe, sgrproj_filter_stripe,
#if CONFIG_HIGHBITDEPTH
  wiener_filter_stripe_highbd, sgrproj_filter_stripe_highbd
#endif  // CONFIG_HIGHBITDEPTH
};

void av1_loop_restoration_filter_unit(const RestorationTileLimits *limits,
                                      const RestorationUnitInfo *rui,
#if CONFIG_STRIPED_LOOP_RESTORATION
                                      const RestorationStripeBoundaries *rsb,
                                      RestorationLineBuffers *rlbs, int ss_y,
#endif
                                      int procunit_width, int procunit_height,
                                      int highbd, int bit_depth, uint8_t *data8,
                                      int stride, uint8_t *dst8, int dst_stride,
                                      int32_t *tmpbuf) {
  RestorationType unit_rtype = rui->restoration_type;

  int unit_h = limits->v_end - limits->v_start;
  int unit_w = limits->h_end - limits->h_start;
  uint8_t *data8_tl = data8 + limits->v_start * stride + limits->h_start;
  uint8_t *dst8_tl = dst8 + limits->v_start * dst_stride + limits->h_start;

  if (unit_rtype == RESTORE_NONE) {
    copy_tile(unit_w, unit_h, data8_tl, stride, dst8_tl, dst_stride, highbd);
    return;
  }

  const int filter_idx = 2 * highbd + (unit_rtype == RESTORE_SGRPROJ);
  assert(filter_idx < NUM_STRIPE_FILTERS);
  const stripe_filter_fun stripe_filter = stripe_filters[filter_idx];

// Convolve the whole tile one stripe at a time
#if CONFIG_STRIPED_LOOP_RESTORATION
  RestorationTileLimits remaining_stripes = *limits;
#endif
  int i = 0;
  while (i < unit_h) {
#if CONFIG_STRIPED_LOOP_RESTORATION
    remaining_stripes.v_start = limits->v_start + i;
    int h = setup_processing_stripe_boundary(&remaining_stripes, rsb,
                                             procunit_height, ss_y, highbd,
                                             data8, stride, rlbs);
    if (unit_rtype == RESTORE_WIENER) h = ALIGN_POWER_OF_TWO(h, 1);
#else
    const int h = AOMMIN(procunit_height, (unit_h - i + 15) & ~15);
#endif

    stripe_filter(rui, unit_w, h, procunit_width, data8_tl + i * stride, stride,
                  dst8_tl + i * dst_stride, dst_stride, tmpbuf, bit_depth);

#if CONFIG_STRIPED_LOOP_RESTORATION
    restore_processing_stripe_boundary(
        &remaining_stripes, rlbs, procunit_height, ss_y, highbd, data8, stride);
#endif

    i += h;
  }
}

struct restore_borders {
  int hborder, vborder;
};

static const struct restore_borders restore_borders[RESTORE_TYPES] = {
  { 0, 0 },
  { WIENER_BORDER_HORZ, WIENER_BORDER_VERT },
  { SGRPROJ_BORDER_HORZ, SGRPROJ_BORDER_VERT },
  { RESTORATION_BORDER_HORZ, RESTORATION_BORDER_VERT }
};

void av1_loop_restoration_filter_frame(YV12_BUFFER_CONFIG *frame,
                                       AV1_COMMON *cm, RestorationInfo *rsi,
                                       int components_pattern,
                                       YV12_BUFFER_CONFIG *dst) {
  YV12_BUFFER_CONFIG dst_;

  typedef void (*copy_fun)(const YV12_BUFFER_CONFIG *src,
                           YV12_BUFFER_CONFIG *dst);
  static const copy_fun copy_funs[3] = { aom_yv12_copy_y, aom_yv12_copy_u,
                                         aom_yv12_copy_v };

  for (int plane = 0; plane < 3; ++plane) {
    if ((components_pattern == 1 << plane) &&
        (rsi[plane].frame_restoration_type == RESTORE_NONE)) {
      if (dst) copy_funs[plane](frame, dst);
      return;
    }
  }
  if (components_pattern ==
      ((1 << AOM_PLANE_Y) | (1 << AOM_PLANE_U) | (1 << AOM_PLANE_V))) {
    // All components
    if (rsi[0].frame_restoration_type == RESTORE_NONE &&
        rsi[1].frame_restoration_type == RESTORE_NONE &&
        rsi[2].frame_restoration_type == RESTORE_NONE) {
      if (dst) aom_yv12_copy_frame(frame, dst);
      return;
    }
  }

  if (!dst) {
    dst = &dst_;
    memset(dst, 0, sizeof(YV12_BUFFER_CONFIG));
    if (aom_realloc_frame_buffer(dst, frame->y_crop_width, frame->y_crop_height,
                                 cm->subsampling_x, cm->subsampling_y,
#if CONFIG_HIGHBITDEPTH
                                 cm->use_highbitdepth,
#endif
                                 AOM_BORDER_IN_PIXELS, cm->byte_alignment, NULL,
                                 NULL, NULL) < 0)
      aom_internal_error(&cm->error, AOM_CODEC_MEM_ERROR,
                         "Failed to allocate restoration dst buffer");
  }

#if CONFIG_STRIPED_LOOP_RESTORATION
  RestorationLineBuffers rlbs;
#endif
#if CONFIG_HIGHBITDEPTH
  const int bit_depth = cm->bit_depth;
  const int highbd = cm->use_highbitdepth;
#else
  const int bit_depth = 8;
  const int highbd = 0;
#endif

  for (int plane = 0; plane < 3; ++plane) {
    if (!((components_pattern >> plane) & 1)) continue;
    const RestorationInfo *prsi = &rsi[plane];
    RestorationType rtype = prsi->frame_restoration_type;
    if (rtype == RESTORE_NONE) {
      copy_funs[plane](frame, dst);
      continue;
    }

    const int is_uv = plane > 0;
    const int ss_y = is_uv && cm->subsampling_y;

    const int plane_width = frame->crop_widths[is_uv];
    const int plane_height = frame->crop_heights[is_uv];

    int nhtiles, nvtiles;
    const int ntiles =
        av1_get_rest_ntiles(plane_width, plane_height,
                            prsi->restoration_tilesize, &nhtiles, &nvtiles);

    const struct restore_borders *borders =
        &restore_borders[prsi->frame_restoration_type];
    extend_frame(frame->buffers[plane], plane_width, plane_height,
                 frame->strides[is_uv], borders->hborder, borders->vborder,
                 highbd);

    for (int tile_idx = 0; tile_idx < ntiles; ++tile_idx) {
      RestorationTileLimits limits = av1_get_rest_tile_limits(
          tile_idx, nhtiles, nvtiles, prsi->restoration_tilesize, plane_width,
          plane_height, ss_y);

      av1_loop_restoration_filter_unit(
          &limits, &prsi->unit_info[tile_idx],
#if CONFIG_STRIPED_LOOP_RESTORATION
          &prsi->boundaries, &rlbs, ss_y,
#endif
          prsi->procunit_width, prsi->procunit_height, highbd, bit_depth,
          frame->buffers[plane], frame->strides[is_uv], dst->buffers[plane],
          dst->strides[is_uv], cm->rst_tmpbuf);
    }
  }

  if (dst == &dst_) {
    for (int plane = 0; plane < 3; ++plane) {
      if ((components_pattern >> plane) & 1) {
        copy_funs[plane](dst, frame);
      }
    }
    aom_free_frame_buffer(dst);
  }
}

int av1_loop_restoration_corners_in_sb(const struct AV1Common *cm, int plane,
                                       int mi_row, int mi_col, BLOCK_SIZE bsize,
                                       int *rcol0, int *rcol1, int *rrow0,
                                       int *rrow1, int *nhtiles) {
  assert(rcol0 && rcol1 && rrow0 && rrow1 && nhtiles);

  if (bsize != cm->sb_size) return 0;

#if CONFIG_FRAME_SUPERRES
  const int frame_w = cm->superres_upscaled_width;
  const int frame_h = cm->superres_upscaled_height;
  const int mi_to_px = MI_SIZE * SCALE_NUMERATOR;
  const int denom = cm->superres_scale_denominator;
#else
  const int frame_w = cm->width;
  const int frame_h = cm->height;
  const int mi_to_px = MI_SIZE;
  const int denom = 1;
#endif  // CONFIG_FRAME_SUPERRES

  const int ss_x = plane > 0 && cm->subsampling_x != 0;
  const int ss_y = plane > 0 && cm->subsampling_y != 0;

  const int ss_frame_w = (frame_w + ss_x) >> ss_x;
  const int ss_frame_h = (frame_h + ss_y) >> ss_y;

  const int rtile_size = cm->rst_info[plane].restoration_tilesize;

  int nvtiles;
  av1_get_rest_ntiles(ss_frame_w, ss_frame_h, rtile_size, nhtiles, &nvtiles);

  const int rnd = rtile_size * denom - 1;

  // rcol0/rrow0 should be the first column/row of rtiles that doesn't start
  // left/below of mi_col/mi_row. For this calculation, we need to round up the
  // division (if the sb starts at rtile column 10.1, the first matching rtile
  // has column index 11)
  *rcol0 = (mi_col * mi_to_px + rnd) / (rtile_size * denom);
  *rrow0 = (mi_row * mi_to_px + rnd) / (rtile_size * denom);

  // rcol1/rrow1 is the equivalent calculation, but for the superblock
  // below-right. There are some slightly strange boundary effects. First, we
  // need to clamp to nhtiles/nvtiles for the case where it appears there are,
  // say, 2.4 restoration tiles horizontally. There we need a maximum mi_row1
  // of 2 because tile 1 gets extended.
  //
  // Second, if mi_col1 >= cm->mi_cols then we must manually set *rcol1 to
  // nhtiles. This is needed whenever the frame's width rounded up to the next
  // toplevel superblock is smaller than nhtiles * rtile_w. The same logic is
  // needed for rows.
  const int mi_row1 = mi_row + mi_size_high[bsize];
  const int mi_col1 = mi_col + mi_size_wide[bsize];

  if (mi_col1 >= cm->mi_cols)
    *rcol1 = *nhtiles;
  else
    *rcol1 =
        AOMMIN(*nhtiles, (mi_col1 * mi_to_px + rnd) / (rtile_size * denom));

  if (mi_row1 >= cm->mi_rows)
    *rrow1 = nvtiles;
  else
    *rrow1 = AOMMIN(nvtiles, (mi_row1 * mi_to_px + rnd) / (rtile_size * denom));

  return *rcol0 < *rcol1 && *rrow0 < *rrow1;
}

#if CONFIG_STRIPED_LOOP_RESTORATION

// Extend to left and right
static void extend_line(uint8_t *buf, int width, int extend,
                        int use_highbitdepth) {
  int i;
  if (use_highbitdepth) {
    uint16_t val, *buf16 = (uint16_t *)buf;
    val = buf16[0];
    for (i = 0; i < extend; i++) buf16[-1 - i] = val;
    val = buf16[width - 1];
    for (i = 0; i < extend; i++) buf16[width + i] = val;
  } else {
    uint8_t val;
    val = buf[0];
    for (i = 0; i < extend; i++) buf[-1 - i] = val;
    val = buf[width - 1];
    for (i = 0; i < extend; i++) buf[width + i] = val;
  }
}

// For each 64 pixel high stripe, save 4 scan lines to be used as boundary in
// the loop restoration process. The lines are saved in
// rst_internal.stripe_boundary_lines
void av1_loop_restoration_save_boundary_lines(const YV12_BUFFER_CONFIG *frame,
                                              AV1_COMMON *cm) {
  for (int p = 0; p < MAX_MB_PLANE; ++p) {
    const int is_uv = p > 0;
    const uint8_t *src_buf = frame->buffers[p];
    const int src_width = frame->crop_widths[is_uv];
    const int src_height = frame->crop_heights[is_uv];
    const int src_stride = frame->strides[is_uv];
    const int stripe_height = 64 >> (is_uv && cm->subsampling_y);
    const int stripe_offset = (56 >> (is_uv && cm->subsampling_y)) - 2;

    RestorationStripeBoundaries *boundaries = &cm->rst_info[p].boundaries;
    uint8_t *boundary_above_buf = boundaries->stripe_boundary_above;
    uint8_t *boundary_below_buf = boundaries->stripe_boundary_below;
    const int boundary_stride = boundaries->stripe_boundary_stride;
#if CONFIG_HIGHBITDEPTH
    const int use_highbitdepth = cm->use_highbitdepth;
    if (use_highbitdepth) {
      src_buf = (uint8_t *)CONVERT_TO_SHORTPTR(src_buf);
    }
#else
    const int use_highbitdepth = 0;
#endif
    src_buf += (stripe_offset * src_stride) << use_highbitdepth;
    boundary_above_buf += RESTORATION_EXTRA_HORZ << use_highbitdepth;
    boundary_below_buf += RESTORATION_EXTRA_HORZ << use_highbitdepth;
    // Loop over stripes
    for (int stripe_y = stripe_offset; stripe_y < src_height;
         stripe_y += stripe_height) {
      // Save 2 lines above the LR stripe (offset -9, -10)
      for (int yy = 0; yy < 2; yy++) {
        if (stripe_y + yy < src_height) {
          memcpy(boundary_above_buf, src_buf, src_width << use_highbitdepth);
          extend_line(boundary_above_buf, src_width, RESTORATION_EXTRA_HORZ,
                      use_highbitdepth);
          src_buf += src_stride << use_highbitdepth;
          boundary_above_buf += boundary_stride << use_highbitdepth;
        }
      }
      // Save 2 lines below the LR stripe (offset 56,57)
      for (int yy = 2; yy < 4; yy++) {
        if (stripe_y + yy < src_height) {
          memcpy(boundary_below_buf, src_buf, src_width << use_highbitdepth);
          extend_line(boundary_below_buf, src_width, RESTORATION_EXTRA_HORZ,
                      use_highbitdepth);
          src_buf += src_stride << use_highbitdepth;
          boundary_below_buf += boundary_stride << use_highbitdepth;
        }
      }
      // jump to next stripe
      src_buf += ((stripe_height - 4) * src_stride) << use_highbitdepth;
    }
  }
}

#endif  // CONFIG_STRIPED_LOOP_RESTORATION
