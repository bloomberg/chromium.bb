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

#ifndef AOM_DSP_PROB_H_
#define AOM_DSP_PROB_H_

#include <assert.h>
#include <stdio.h>

#include "./aom_config.h"
#include "./aom_dsp_common.h"

#include "aom_ports/bitops.h"
#include "aom_ports/mem.h"

#include "aom_dsp/entcode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t aom_prob;

// TODO(negge): Rename this aom_prob once we remove vpxbool.
typedef uint16_t aom_cdf_prob;

#define CDF_SIZE(x) ((x) + 1)

#define CDF_PROB_BITS 15
#define CDF_PROB_TOP (1 << CDF_PROB_BITS)
#define CDF_INIT_TOP 32768
#define CDF_SHIFT (15 - CDF_PROB_BITS)
/*The value stored in an iCDF is CDF_PROB_TOP minus the actual cumulative
  probability (an "inverse" CDF).
  This function converts from one representation to the other (and is its own
  inverse).*/
#define AOM_ICDF(x) (CDF_PROB_TOP - (x))

#if CDF_SHIFT == 0

#define AOM_CDF2(a0) AOM_ICDF(a0), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF3(a0, a1) AOM_ICDF(a0), AOM_ICDF(a1), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF4(a0, a1, a2) \
  AOM_ICDF(a0), AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF5(a0, a1, a2, a3) \
  AOM_ICDF(a0)                   \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF6(a0, a1, a2, a3, a4)                        \
  AOM_ICDF(a0)                                              \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF7(a0, a1, a2, a3, a4, a5)                                  \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF8(a0, a1, a2, a3, a4, a5, a6)                              \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF9(a0, a1, a2, a3, a4, a5, a6, a7)                          \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF10(a0, a1, a2, a3, a4, a5, a6, a7, a8)                     \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)                 \
  AOM_ICDF(a0)                                                            \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5), \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9),             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)               \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11)          \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(a11), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF14(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12)     \
  AOM_ICDF(a0)                                                               \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),    \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10), \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF15(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
  AOM_ICDF(a0)                                                                \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),     \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10),  \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(a13), AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, \
                  a14)                                                        \
  AOM_ICDF(a0)                                                                \
  , AOM_ICDF(a1), AOM_ICDF(a2), AOM_ICDF(a3), AOM_ICDF(a4), AOM_ICDF(a5),     \
      AOM_ICDF(a6), AOM_ICDF(a7), AOM_ICDF(a8), AOM_ICDF(a9), AOM_ICDF(a10),  \
      AOM_ICDF(a11), AOM_ICDF(a12), AOM_ICDF(a13), AOM_ICDF(a14),             \
      AOM_ICDF(CDF_PROB_TOP), 0

#else
#define AOM_CDF2(a0)                                       \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 2) + \
            ((CDF_INIT_TOP - 2) >> 1)) /                   \
               ((CDF_INIT_TOP - 2)) +                      \
           1)                                              \
  , AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF3(a0, a1)                                     \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 3) +   \
            ((CDF_INIT_TOP - 3) >> 1)) /                     \
               ((CDF_INIT_TOP - 3)) +                        \
           1)                                                \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 3) + \
              ((CDF_INIT_TOP - 3) >> 1)) /                   \
                 ((CDF_INIT_TOP - 3)) +                      \
             2),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF4(a0, a1, a2)                                   \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 4) +     \
            ((CDF_INIT_TOP - 4) >> 1)) /                       \
               ((CDF_INIT_TOP - 4)) +                          \
           1)                                                  \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 4) +   \
              ((CDF_INIT_TOP - 4) >> 1)) /                     \
                 ((CDF_INIT_TOP - 4)) +                        \
             2),                                               \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 4) + \
                ((CDF_INIT_TOP - 4) >> 1)) /                   \
                   ((CDF_INIT_TOP - 4)) +                      \
               3),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF5(a0, a1, a2, a3)                               \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) +     \
            ((CDF_INIT_TOP - 5) >> 1)) /                       \
               ((CDF_INIT_TOP - 5)) +                          \
           1)                                                  \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) +   \
              ((CDF_INIT_TOP - 5) >> 1)) /                     \
                 ((CDF_INIT_TOP - 5)) +                        \
             2),                                               \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) + \
                ((CDF_INIT_TOP - 5) >> 1)) /                   \
                   ((CDF_INIT_TOP - 5)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 5) + \
                ((CDF_INIT_TOP - 5) >> 1)) /                   \
                   ((CDF_INIT_TOP - 5)) +                      \
               4),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF6(a0, a1, a2, a3, a4)                           \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) +     \
            ((CDF_INIT_TOP - 6) >> 1)) /                       \
               ((CDF_INIT_TOP - 6)) +                          \
           1)                                                  \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) +   \
              ((CDF_INIT_TOP - 6) >> 1)) /                     \
                 ((CDF_INIT_TOP - 6)) +                        \
             2),                                               \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) + \
                ((CDF_INIT_TOP - 6) >> 1)) /                   \
                   ((CDF_INIT_TOP - 6)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) + \
                ((CDF_INIT_TOP - 6) >> 1)) /                   \
                   ((CDF_INIT_TOP - 6)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 6) + \
                ((CDF_INIT_TOP - 6) >> 1)) /                   \
                   ((CDF_INIT_TOP - 6)) +                      \
               5),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF7(a0, a1, a2, a3, a4, a5)                       \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) +     \
            ((CDF_INIT_TOP - 7) >> 1)) /                       \
               ((CDF_INIT_TOP - 7)) +                          \
           1)                                                  \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) +   \
              ((CDF_INIT_TOP - 7) >> 1)) /                     \
                 ((CDF_INIT_TOP - 7)) +                        \
             2),                                               \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               5),                                             \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 7) + \
                ((CDF_INIT_TOP - 7) >> 1)) /                   \
                   ((CDF_INIT_TOP - 7)) +                      \
               6),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF8(a0, a1, a2, a3, a4, a5, a6)                   \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) +     \
            ((CDF_INIT_TOP - 8) >> 1)) /                       \
               ((CDF_INIT_TOP - 8)) +                          \
           1)                                                  \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) +   \
              ((CDF_INIT_TOP - 8) >> 1)) /                     \
                 ((CDF_INIT_TOP - 8)) +                        \
             2),                                               \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               5),                                             \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               6),                                             \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 8) + \
                ((CDF_INIT_TOP - 8) >> 1)) /                   \
                   ((CDF_INIT_TOP - 8)) +                      \
               7),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF9(a0, a1, a2, a3, a4, a5, a6, a7)               \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) +     \
            ((CDF_INIT_TOP - 9) >> 1)) /                       \
               ((CDF_INIT_TOP - 9)) +                          \
           1)                                                  \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) +   \
              ((CDF_INIT_TOP - 9) >> 1)) /                     \
                 ((CDF_INIT_TOP - 9)) +                        \
             2),                                               \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               3),                                             \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               4),                                             \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               5),                                             \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               6),                                             \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               7),                                             \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 9) + \
                ((CDF_INIT_TOP - 9) >> 1)) /                   \
                   ((CDF_INIT_TOP - 9)) +                      \
               8),                                             \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF10(a0, a1, a2, a3, a4, a5, a6, a7, a8)           \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) +     \
            ((CDF_INIT_TOP - 10) >> 1)) /                       \
               ((CDF_INIT_TOP - 10)) +                          \
           1)                                                   \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) +   \
              ((CDF_INIT_TOP - 10) >> 1)) /                     \
                 ((CDF_INIT_TOP - 10)) +                        \
             2),                                                \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               3),                                              \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               4),                                              \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               5),                                              \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               6),                                              \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               7),                                              \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               8),                                              \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 10) + \
                ((CDF_INIT_TOP - 10) >> 1)) /                   \
                   ((CDF_INIT_TOP - 10)) +                      \
               9),                                              \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF11(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9)        \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +      \
            ((CDF_INIT_TOP - 11) >> 1)) /                        \
               ((CDF_INIT_TOP - 11)) +                           \
           1)                                                    \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +    \
              ((CDF_INIT_TOP - 11) >> 1)) /                      \
                 ((CDF_INIT_TOP - 11)) +                         \
             2),                                                 \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               3),                                               \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               4),                                               \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               5),                                               \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               6),                                               \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               7),                                               \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               8),                                               \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) +  \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               9),                                               \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 11) + \
                ((CDF_INIT_TOP - 11) >> 1)) /                    \
                   ((CDF_INIT_TOP - 11)) +                       \
               10),                                              \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF12(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)    \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +       \
            ((CDF_INIT_TOP - 12) >> 1)) /                         \
               ((CDF_INIT_TOP - 12)) +                            \
           1)                                                     \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +     \
              ((CDF_INIT_TOP - 12) >> 1)) /                       \
                 ((CDF_INIT_TOP - 12)) +                          \
             2),                                                  \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               3),                                                \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               4),                                                \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               5),                                                \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               6),                                                \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               7),                                                \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               8),                                                \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +   \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               9),                                                \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) +  \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               10),                                               \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 12) + \
                ((CDF_INIT_TOP - 12) >> 1)) /                     \
                   ((CDF_INIT_TOP - 12)) +                        \
               11),                                               \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF13(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11) \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +         \
            ((CDF_INIT_TOP - 13) >> 1)) /                           \
               ((CDF_INIT_TOP - 13)) +                              \
           1)                                                       \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +       \
              ((CDF_INIT_TOP - 13) >> 1)) /                         \
                 ((CDF_INIT_TOP - 13)) +                            \
             2),                                                    \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               3),                                                  \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               4),                                                  \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               5),                                                  \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               6),                                                  \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               7),                                                  \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               8),                                                  \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +     \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               9),                                                  \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +    \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               10),                                                 \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +   \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               11),                                                 \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 13) +   \
                ((CDF_INIT_TOP - 13) >> 1)) /                       \
                   ((CDF_INIT_TOP - 13)) +                          \
               12),                                                 \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF14(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12) \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +              \
            ((CDF_INIT_TOP - 14) >> 1)) /                                \
               ((CDF_INIT_TOP - 14)) +                                   \
           1)                                                            \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +            \
              ((CDF_INIT_TOP - 14) >> 1)) /                              \
                 ((CDF_INIT_TOP - 14)) +                                 \
             2),                                                         \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               3),                                                       \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               4),                                                       \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               5),                                                       \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               6),                                                       \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               7),                                                       \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               8),                                                       \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +          \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               9),                                                       \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +         \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               10),                                                      \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +        \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               11),                                                      \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +        \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               12),                                                      \
      AOM_ICDF((((a12)-13) * ((CDF_INIT_TOP >> CDF_SHIFT) - 14) +        \
                ((CDF_INIT_TOP - 14) >> 1)) /                            \
                   ((CDF_INIT_TOP - 14)) +                               \
               13),                                                      \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF15(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13) \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +                   \
            ((CDF_INIT_TOP - 15) >> 1)) /                                     \
               ((CDF_INIT_TOP - 15)) +                                        \
           1)                                                                 \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +                 \
              ((CDF_INIT_TOP - 15) >> 1)) /                                   \
                 ((CDF_INIT_TOP - 15)) +                                      \
             2),                                                              \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               3),                                                            \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               4),                                                            \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               5),                                                            \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               6),                                                            \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               7),                                                            \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               8),                                                            \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +               \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               9),                                                            \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +              \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               10),                                                           \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               11),                                                           \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               12),                                                           \
      AOM_ICDF((((a12)-13) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               13),                                                           \
      AOM_ICDF((((a13)-14) * ((CDF_INIT_TOP >> CDF_SHIFT) - 15) +             \
                ((CDF_INIT_TOP - 15) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 15)) +                                    \
               14),                                                           \
      AOM_ICDF(CDF_PROB_TOP), 0
#define AOM_CDF16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, \
                  a14)                                                        \
  AOM_ICDF((((a0)-1) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +                   \
            ((CDF_INIT_TOP - 16) >> 1)) /                                     \
               ((CDF_INIT_TOP - 16)) +                                        \
           1)                                                                 \
  , AOM_ICDF((((a1)-2) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +                 \
              ((CDF_INIT_TOP - 16) >> 1)) /                                   \
                 ((CDF_INIT_TOP - 16)) +                                      \
             2),                                                              \
      AOM_ICDF((((a2)-3) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               3),                                                            \
      AOM_ICDF((((a3)-4) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               4),                                                            \
      AOM_ICDF((((a4)-5) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               5),                                                            \
      AOM_ICDF((((a5)-6) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               6),                                                            \
      AOM_ICDF((((a6)-7) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               7),                                                            \
      AOM_ICDF((((a7)-8) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               8),                                                            \
      AOM_ICDF((((a8)-9) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +               \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               9),                                                            \
      AOM_ICDF((((a9)-10) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +              \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               10),                                                           \
      AOM_ICDF((((a10)-11) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               11),                                                           \
      AOM_ICDF((((a11)-12) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               12),                                                           \
      AOM_ICDF((((a12)-13) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               13),                                                           \
      AOM_ICDF((((a13)-14) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               14),                                                           \
      AOM_ICDF((((a14)-15) * ((CDF_INIT_TOP >> CDF_SHIFT) - 16) +             \
                ((CDF_INIT_TOP - 16) >> 1)) /                                 \
                   ((CDF_INIT_TOP - 16)) +                                    \
               15),                                                           \
      AOM_ICDF(CDF_PROB_TOP), 0

#endif

#define MAX_PROB 255

#define BR_NODE 1

#define aom_prob_half ((aom_prob)128)

typedef int8_t aom_tree_index;

#define TREE_SIZE(leaf_count) (-2 + 2 * (leaf_count))

#define MODE_MV_COUNT_SAT 20

/* We build coding trees compactly in arrays.
   Each node of the tree is a pair of aom_tree_indices.
   Array index often references a corresponding probability table.
   Index <= 0 means done encoding/decoding and value = -Index,
   Index > 0 means need another bit, specification at index.
   Nonnegative indices are always even;  processing begins at node 0. */

typedef const aom_tree_index aom_tree[];

static INLINE aom_prob get_prob(unsigned int num, unsigned int den) {
  assert(den != 0);
  {
    const int p = (int)(((uint64_t)num * 256 + (den >> 1)) / den);
    // (p > 255) ? 255 : (p < 1) ? 1 : p;
    const int clipped_prob = p | ((255 - p) >> 23) | (p == 0);
    return (aom_prob)clipped_prob;
  }
}

static INLINE void update_cdf(aom_cdf_prob *cdf, int val, int nsymbs) {
  int rate;
  const int rate2 = 5;
  int i, tmp;
  int diff;

#if 1
  // static const int nsymbs2speed[17] = { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
  // 3, 3, 3, 3, 4 };
  // static const int nsymbs2speed[17] = { 0, 0, 1, 1, 2, 2, 2, 2, 2,
  //                                       2, 2, 2, 3, 3, 3, 3, 3 };
  static const int nsymbs2speed[17] = { 0, 0, 1, 1, 2, 2, 2, 2, 2,
                                        2, 2, 2, 2, 2, 2, 2, 2 };
  assert(nsymbs < 17);
  rate = 3 + (cdf[nsymbs] > 15) + (cdf[nsymbs] > 31) +
         nsymbs2speed[nsymbs];  // + get_msb(nsymbs);
  tmp = AOM_ICDF(0);
  (void)rate2;
  (void)diff;

  // Single loop (faster)
  for (i = 0; i < nsymbs - 1; ++i) {
    tmp = (i == val) ? 0 : tmp;
#if 1
    if (tmp < cdf[i]) {
      cdf[i] -= ((cdf[i] - tmp) >> rate);
    } else {
      cdf[i] += ((tmp - cdf[i]) >> rate);
    }
#else
    cdf[i] += ((tmp - cdf[i]) >> rate);
#endif
  }

#else
  for (i = 0; i < nsymbs; ++i) {
    tmp = (i + 1) << rate2;
    cdf[i] -= ((cdf[i] - tmp) >> rate);
  }
  diff = CDF_PROB_TOP - cdf[nsymbs - 1];

  for (i = val; i < nsymbs; ++i) {
    cdf[i] += diff;
  }
#endif
  cdf[nsymbs] += (cdf[nsymbs] < 32);
}

#if CONFIG_LV_MAP
static INLINE void update_bin(aom_cdf_prob *cdf, int val, int nsymbs) {
  update_cdf(cdf, val, nsymbs);
}
#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_DSP_PROB_H_
