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

// Utility functions used by encoder binaries.

#include <string.h>

#include "./encoder_util.h"
#include "aom/aom_integer.h"

#define mmin(a, b) ((a) < (b) ? (a) : (b))

#if CONFIG_HIGHBITDEPTH
// TODO(urvang): Refactor the highbd and lowbd version.
void aom_find_mismatch_high(const aom_image_t *const img1,
                            const aom_image_t *const img2, int yloc[4],
                            int uloc[4], int vloc[4]) {
  uint16_t *plane1, *plane2;
  uint32_t stride1, stride2;
  const uint32_t bsize = 64;
  const uint32_t bsizey = bsize >> img1->y_chroma_shift;
  const uint32_t bsizex = bsize >> img1->x_chroma_shift;
  const uint32_t c_w =
      (img1->d_w + img1->x_chroma_shift) >> img1->x_chroma_shift;
  const uint32_t c_h =
      (img1->d_h + img1->y_chroma_shift) >> img1->y_chroma_shift;
  int match = 1;
  uint32_t i, j;
  yloc[0] = yloc[1] = yloc[2] = yloc[3] = -1;
  plane1 = (uint16_t *)img1->planes[AOM_PLANE_Y];
  plane2 = (uint16_t *)img2->planes[AOM_PLANE_Y];
  stride1 = img1->stride[AOM_PLANE_Y] / 2;
  stride2 = img2->stride[AOM_PLANE_Y] / 2;
  for (i = 0, match = 1; match && i < img1->d_h; i += bsize) {
    for (j = 0; match && j < img1->d_w; j += bsize) {
      int k, l;
      const int si = mmin(i + bsize, img1->d_h) - i;
      const int sj = mmin(j + bsize, img1->d_w) - j;
      for (k = 0; match && k < si; ++k) {
        for (l = 0; match && l < sj; ++l) {
          if (*(plane1 + (i + k) * stride1 + j + l) !=
              *(plane2 + (i + k) * stride2 + j + l)) {
            yloc[0] = i + k;
            yloc[1] = j + l;
            yloc[2] = *(plane1 + (i + k) * stride1 + j + l);
            yloc[3] = *(plane2 + (i + k) * stride2 + j + l);
            match = 0;
            break;
          }
        }
      }
    }
  }

  uloc[0] = uloc[1] = uloc[2] = uloc[3] = -1;
  plane1 = (uint16_t *)img1->planes[AOM_PLANE_U];
  plane2 = (uint16_t *)img2->planes[AOM_PLANE_U];
  stride1 = img1->stride[AOM_PLANE_U] / 2;
  stride2 = img2->stride[AOM_PLANE_U] / 2;
  for (i = 0, match = 1; match && i < c_h; i += bsizey) {
    for (j = 0; match && j < c_w; j += bsizex) {
      int k, l;
      const int si = mmin(i + bsizey, c_h - i);
      const int sj = mmin(j + bsizex, c_w - j);
      for (k = 0; match && k < si; ++k) {
        for (l = 0; match && l < sj; ++l) {
          if (*(plane1 + (i + k) * stride1 + j + l) !=
              *(plane2 + (i + k) * stride2 + j + l)) {
            uloc[0] = i + k;
            uloc[1] = j + l;
            uloc[2] = *(plane1 + (i + k) * stride1 + j + l);
            uloc[3] = *(plane2 + (i + k) * stride2 + j + l);
            match = 0;
            break;
          }
        }
      }
    }
  }

  vloc[0] = vloc[1] = vloc[2] = vloc[3] = -1;
  plane1 = (uint16_t *)img1->planes[AOM_PLANE_V];
  plane2 = (uint16_t *)img2->planes[AOM_PLANE_V];
  stride1 = img1->stride[AOM_PLANE_V] / 2;
  stride2 = img2->stride[AOM_PLANE_V] / 2;
  for (i = 0, match = 1; match && i < c_h; i += bsizey) {
    for (j = 0; match && j < c_w; j += bsizex) {
      int k, l;
      const int si = mmin(i + bsizey, c_h - i);
      const int sj = mmin(j + bsizex, c_w - j);
      for (k = 0; match && k < si; ++k) {
        for (l = 0; match && l < sj; ++l) {
          if (*(plane1 + (i + k) * stride1 + j + l) !=
              *(plane2 + (i + k) * stride2 + j + l)) {
            vloc[0] = i + k;
            vloc[1] = j + l;
            vloc[2] = *(plane1 + (i + k) * stride1 + j + l);
            vloc[3] = *(plane2 + (i + k) * stride2 + j + l);
            match = 0;
            break;
          }
        }
      }
    }
  }
}
#endif

void aom_find_mismatch(const aom_image_t *const img1,
                       const aom_image_t *const img2, int yloc[4], int uloc[4],
                       int vloc[4]) {
  const uint32_t bsize = 64;
  const uint32_t bsizey = bsize >> img1->y_chroma_shift;
  const uint32_t bsizex = bsize >> img1->x_chroma_shift;
  const uint32_t c_w =
      (img1->d_w + img1->x_chroma_shift) >> img1->x_chroma_shift;
  const uint32_t c_h =
      (img1->d_h + img1->y_chroma_shift) >> img1->y_chroma_shift;
  int match = 1;
  uint32_t i, j;
  yloc[0] = yloc[1] = yloc[2] = yloc[3] = -1;
  for (i = 0, match = 1; match && i < img1->d_h; i += bsize) {
    for (j = 0; match && j < img1->d_w; j += bsize) {
      int k, l;
      const int si = mmin(i + bsize, img1->d_h) - i;
      const int sj = mmin(j + bsize, img1->d_w) - j;
      for (k = 0; match && k < si; ++k) {
        for (l = 0; match && l < sj; ++l) {
          if (*(img1->planes[AOM_PLANE_Y] +
                (i + k) * img1->stride[AOM_PLANE_Y] + j + l) !=
              *(img2->planes[AOM_PLANE_Y] +
                (i + k) * img2->stride[AOM_PLANE_Y] + j + l)) {
            yloc[0] = i + k;
            yloc[1] = j + l;
            yloc[2] = *(img1->planes[AOM_PLANE_Y] +
                        (i + k) * img1->stride[AOM_PLANE_Y] + j + l);
            yloc[3] = *(img2->planes[AOM_PLANE_Y] +
                        (i + k) * img2->stride[AOM_PLANE_Y] + j + l);
            match = 0;
            break;
          }
        }
      }
    }
  }

  uloc[0] = uloc[1] = uloc[2] = uloc[3] = -1;
  for (i = 0, match = 1; match && i < c_h; i += bsizey) {
    for (j = 0; match && j < c_w; j += bsizex) {
      int k, l;
      const int si = mmin(i + bsizey, c_h - i);
      const int sj = mmin(j + bsizex, c_w - j);
      for (k = 0; match && k < si; ++k) {
        for (l = 0; match && l < sj; ++l) {
          if (*(img1->planes[AOM_PLANE_U] +
                (i + k) * img1->stride[AOM_PLANE_U] + j + l) !=
              *(img2->planes[AOM_PLANE_U] +
                (i + k) * img2->stride[AOM_PLANE_U] + j + l)) {
            uloc[0] = i + k;
            uloc[1] = j + l;
            uloc[2] = *(img1->planes[AOM_PLANE_U] +
                        (i + k) * img1->stride[AOM_PLANE_U] + j + l);
            uloc[3] = *(img2->planes[AOM_PLANE_U] +
                        (i + k) * img2->stride[AOM_PLANE_U] + j + l);
            match = 0;
            break;
          }
        }
      }
    }
  }
  // TODO(urvang): Refactor the 3 for loops for Y, U, V?
  vloc[0] = vloc[1] = vloc[2] = vloc[3] = -1;
  for (i = 0, match = 1; match && i < c_h; i += bsizey) {
    for (j = 0; match && j < c_w; j += bsizex) {
      int k, l;
      const int si = mmin(i + bsizey, c_h - i);
      const int sj = mmin(j + bsizex, c_w - j);
      for (k = 0; match && k < si; ++k) {
        for (l = 0; match && l < sj; ++l) {
          if (*(img1->planes[AOM_PLANE_V] +
                (i + k) * img1->stride[AOM_PLANE_V] + j + l) !=
              *(img2->planes[AOM_PLANE_V] +
                (i + k) * img2->stride[AOM_PLANE_V] + j + l)) {
            vloc[0] = i + k;
            vloc[1] = j + l;
            vloc[2] = *(img1->planes[AOM_PLANE_V] +
                        (i + k) * img1->stride[AOM_PLANE_V] + j + l);
            vloc[3] = *(img2->planes[AOM_PLANE_V] +
                        (i + k) * img2->stride[AOM_PLANE_V] + j + l);
            match = 0;
            break;
          }
        }
      }
    }
  }
}

int aom_compare_img(const aom_image_t *const img1,
                    const aom_image_t *const img2) {
  uint32_t l_w = img1->d_w;
  uint32_t c_w = (img1->d_w + img1->x_chroma_shift) >> img1->x_chroma_shift;
  const uint32_t c_h =
      (img1->d_h + img1->y_chroma_shift) >> img1->y_chroma_shift;
  uint32_t i;
  int match = 1;

  match &= (img1->fmt == img2->fmt);
  match &= (img1->d_w == img2->d_w);
  match &= (img1->d_h == img2->d_h);
#if CONFIG_HIGHBITDEPTH
  if (img1->fmt & AOM_IMG_FMT_HIGHBITDEPTH) {
    l_w *= 2;
    c_w *= 2;
  }
#endif

  for (i = 0; i < img1->d_h; ++i)
    match &= (memcmp(img1->planes[AOM_PLANE_Y] + i * img1->stride[AOM_PLANE_Y],
                     img2->planes[AOM_PLANE_Y] + i * img2->stride[AOM_PLANE_Y],
                     l_w) == 0);

  for (i = 0; i < c_h; ++i)
    match &= (memcmp(img1->planes[AOM_PLANE_U] + i * img1->stride[AOM_PLANE_U],
                     img2->planes[AOM_PLANE_U] + i * img2->stride[AOM_PLANE_U],
                     c_w) == 0);

  for (i = 0; i < c_h; ++i)
    match &= (memcmp(img1->planes[AOM_PLANE_V] + i * img1->stride[AOM_PLANE_V],
                     img2->planes[AOM_PLANE_V] + i * img2->stride[AOM_PLANE_V],
                     c_w) == 0);

  return match;
}
