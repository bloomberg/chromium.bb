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

#include "aom_mem/aom_mem.h"

#include "av1/common/reconinter.h"
#include "av1/common/scan.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/seg_common.h"

#if CONFIG_ALT_INTRA

const aom_prob av1_kf_y_mode_prob[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1] = {
  {
      // above = dc
      { 121, 30, 54, 128, 164, 158, 45, 41, 57, 91 },   // left = dc
      { 91, 38, 101, 102, 124, 141, 49, 48, 45, 73 },   // left = v
      { 66, 28, 27, 177, 225, 178, 32, 27, 52, 114 },   // left = h
      { 106, 23, 50, 101, 134, 148, 64, 50, 49, 107 },  // left = d45
      { 75, 24, 32, 118, 66, 143, 42, 28, 57, 74 },     // left = d135
      { 95, 24, 40, 142, 56, 141, 72, 121, 129, 255 },  // left = d117
      { 71, 14, 25, 126, 117, 201, 28, 21, 117, 89 },   // left = d153
      { 85, 16, 37, 110, 163, 178, 41, 28, 48, 134 },   // left = d207
      { 86, 25, 32, 83, 105, 133, 58, 81, 46, 95 },     // left = d63
      { 79, 25, 38, 75, 150, 255, 30, 49, 34, 51 },     // left = smooth
      { 68, 59, 48, 122, 193, 158, 43, 46, 46, 112 },   // left = paeth
  },
  {
      // above = v
      { 66, 21, 118, 111, 145, 107, 27, 50, 27, 54 },    // left = dc
      { 52, 25, 167, 81, 120, 101, 34, 55, 19, 32 },     // left = v
      { 56, 18, 72, 134, 208, 139, 31, 34, 27, 89 },     // left = h
      { 75, 21, 94, 88, 134, 123, 49, 57, 30, 68 },      // left = d45
      { 54, 18, 95, 96, 78, 107, 33, 49, 28, 65 },       // left = d135
      { 61, 19, 121, 131, 58, 101, 56, 143, 120, 255 },  // left = d117
      { 53, 13, 78, 103, 110, 147, 31, 41, 64, 77 },     // left = d153
      { 69, 14, 78, 93, 167, 121, 31, 39, 25, 113 },     // left = d207
      { 64, 18, 103, 79, 90, 108, 34, 73, 27, 69 },      // left = d63
      { 52, 20, 103, 61, 161, 255, 22, 42, 16, 35 },     // left = smooth
      { 50, 31, 124, 92, 161, 120, 50, 53, 23, 60 },     // left = paeth
  },
  {
      // above = h
      { 94, 29, 31, 158, 214, 178, 35, 31, 72, 111 },   // left = dc
      { 72, 37, 72, 149, 184, 177, 43, 40, 53, 105 },   // left = v
      { 53, 21, 14, 196, 242, 209, 29, 19, 55, 145 },   // left = h
      { 93, 36, 36, 104, 176, 166, 56, 37, 49, 141 },   // left = d45
      { 84, 32, 27, 124, 108, 143, 38, 36, 76, 134 },   // left = d135
      { 82, 31, 47, 142, 122, 161, 83, 73, 126, 255 },  // left = d117
      { 66, 16, 20, 133, 148, 210, 30, 17, 113, 104 },  // left = d153
      { 76, 16, 17, 129, 207, 181, 41, 20, 46, 163 },   // left = d207
      { 72, 38, 21, 100, 142, 171, 37, 70, 49, 111 },   // left = d63
      { 61, 30, 27, 115, 208, 255, 27, 31, 44, 63 },    // left = smooth
      { 53, 45, 29, 157, 222, 185, 49, 37, 55, 102 },   // left = paeth
  },
  {
      // above = d45
      { 96, 18, 37, 98, 138, 154, 68, 56, 59, 96 },    // left = dc
      { 73, 18, 92, 81, 125, 132, 75, 64, 27, 67 },    // left = v
      { 73, 17, 27, 128, 213, 154, 56, 44, 32, 105 },  // left = h
      { 101, 20, 21, 75, 138, 138, 82, 56, 23, 154 },  // left = d45
      { 71, 15, 33, 91, 70, 150, 62, 55, 38, 118 },    // left = d135
      { 80, 19, 38, 116, 69, 122, 88, 132, 92, 255 },  // left = d117
      { 68, 11, 22, 101, 116, 179, 52, 44, 85, 96 },   // left = d153
      { 101, 8, 59, 77, 151, 170, 53, 41, 35, 172 },   // left = d207
      { 82, 19, 24, 81, 172, 129, 82, 128, 43, 108 },  // left = d63
      { 66, 18, 42, 64, 143, 255, 52, 52, 25, 83 },    // left = smooth
      { 57, 24, 42, 85, 169, 145, 104, 71, 34, 86 },   // left = paeth
  },
  {
      // above = d135
      { 85, 15, 29, 113, 83, 176, 26, 29, 70, 110 },    // left = dc
      { 78, 28, 49, 111, 91, 141, 30, 42, 48, 75 },     // left = v
      { 56, 21, 16, 146, 190, 178, 23, 31, 49, 92 },    // left = h
      { 70, 19, 20, 65, 90, 173, 97, 36, 57, 98 },      // left = d45
      { 77, 14, 26, 110, 51, 156, 34, 35, 54, 74 },     // left = d135
      { 78, 18, 36, 153, 47, 131, 62, 102, 155, 255 },  // left = d117
      { 56, 11, 15, 115, 85, 196, 32, 45, 81, 96 },     // left = d153
      { 90, 18, 24, 95, 126, 159, 34, 31, 46, 136 },    // left = d207
      { 80, 23, 28, 90, 75, 141, 39, 50, 46, 87 },      // left = d63
      { 63, 22, 31, 91, 110, 255, 26, 43, 51, 51 },     // left = smooth
      { 66, 32, 31, 122, 145, 165, 40, 43, 56, 79 },    // left = paeth
  },
  {
      // above = d117
      { 81, 16, 61, 170, 74, 105, 54, 105, 113, 255 },  // left = dc
      { 74, 20, 86, 163, 64, 97, 65, 129, 101, 255 },   // left = v
      { 63, 15, 47, 168, 141, 176, 69, 77, 77, 255 },   // left = h
      { 70, 17, 59, 97, 78, 114, 74, 122, 80, 255 },    // left = d45
      { 78, 13, 50, 153, 34, 126, 75, 114, 120, 255 },  // left = d135
      { 72, 16, 69, 159, 28, 108, 63, 134, 107, 255 },  // left = d117
      { 66, 9, 47, 131, 79, 148, 41, 88, 105, 255 },    // left = d153
      { 78, 12, 60, 119, 105, 133, 47, 95, 63, 255 },   // left = d207
      { 82, 21, 58, 128, 61, 98, 64, 136, 91, 255 },    // left = d63
      { 23, 26, 28, 96, 85, 128, 51, 64, 85, 128 },     // left = smooth
      { 58, 27, 62, 162, 109, 151, 75, 106, 78, 255 },  // left = paeth
  },
  {
      // above = d153
      { 91, 18, 25, 121, 166, 173, 25, 25, 128, 102 },  // left = dc
      { 80, 27, 51, 111, 141, 147, 45, 38, 70, 85 },    // left = v
      { 53, 12, 11, 154, 197, 225, 17, 17, 74, 145 },   // left = h
      { 93, 27, 23, 111, 143, 188, 43, 39, 69, 112 },   // left = d45
      { 83, 15, 21, 118, 67, 178, 40, 33, 73, 92 },     // left = d135
      { 94, 13, 31, 132, 66, 110, 61, 82, 148, 255 },   // left = d117
      { 76, 9, 11, 96, 105, 201, 16, 13, 157, 97 },     // left = d153
      { 70, 10, 12, 100, 172, 201, 23, 17, 53, 158 },   // left = d207
      { 114, 25, 21, 104, 108, 163, 30, 47, 53, 111 },  // left = d63
      { 70, 16, 21, 80, 157, 255, 25, 30, 81, 69 },     // left = smooth
      { 87, 32, 26, 120, 191, 168, 32, 33, 70, 118 },   // left = paeth
  },
  {
      // above = d207
      { 98, 20, 39, 122, 168, 188, 38, 36, 54, 132 },   // left = dc
      { 81, 37, 62, 97, 122, 153, 38, 43, 36, 118 },    // left = v
      { 71, 21, 22, 154, 227, 183, 37, 31, 46, 140 },   // left = h
      { 90, 34, 19, 93, 144, 194, 65, 47, 41, 163 },    // left = d45
      { 78, 20, 27, 91, 93, 173, 57, 52, 49, 113 },     // left = d135
      { 79, 25, 45, 121, 101, 147, 69, 56, 122, 255 },  // left = d117
      { 73, 13, 19, 105, 122, 206, 40, 28, 91, 126 },   // left = d153
      { 101, 14, 22, 87, 153, 169, 33, 25, 26, 175 },   // left = d207
      { 81, 28, 23, 86, 115, 169, 48, 56, 41, 111 },    // left = d63
      { 70, 24, 30, 90, 180, 255, 38, 26, 36, 82 },     // left = smooth
      { 61, 37, 30, 94, 189, 163, 76, 50, 36, 127 },    // left = paeth
  },
  {
      // above = d63
      { 77, 13, 46, 86, 138, 117, 55, 88, 34, 68 },     // left = dc
      { 68, 17, 80, 64, 105, 108, 66, 115, 32, 45 },    // left = v
      { 62, 13, 37, 124, 210, 131, 46, 57, 28, 103 },   // left = h
      { 88, 15, 45, 73, 134, 145, 73, 101, 37, 87 },    // left = d45
      { 68, 16, 35, 78, 81, 133, 54, 71, 33, 67 },      // left = d135
      { 71, 16, 57, 108, 61, 135, 71, 184, 113, 255 },  // left = d117
      { 55, 10, 27, 69, 107, 158, 39, 76, 82, 95 },     // left = d153
      { 80, 9, 38, 78, 153, 145, 50, 63, 28, 123 },     // left = d207
      { 86, 12, 33, 49, 107, 135, 64, 134, 57, 89 },    // left = d63
      { 56, 19, 55, 60, 163, 255, 38, 84, 22, 36 },     // left = smooth
      { 53, 17, 60, 69, 151, 126, 73, 113, 26, 80 },    // left = paeth
  },
  {
      // above = smooth
      { 79, 16, 46, 89, 167, 255, 22, 36, 29, 42 },   // left = dc
      { 63, 22, 88, 71, 131, 255, 26, 41, 21, 35 },   // left = v
      { 51, 18, 28, 142, 232, 255, 26, 25, 25, 75 },  // left = h
      { 75, 18, 43, 70, 140, 255, 37, 49, 34, 89 },   // left = d45
      { 70, 14, 35, 87, 83, 255, 30, 36, 34, 50 },    // left = d135
      { 23, 26, 28, 96, 85, 128, 51, 64, 85, 128 },   // left = d117
      { 74, 12, 33, 83, 128, 255, 27, 33, 58, 68 },   // left = d153
      { 66, 11, 30, 77, 179, 255, 21, 27, 23, 113 },  // left = d207
      { 68, 22, 40, 65, 118, 255, 28, 61, 30, 50 },   // left = d63
      { 60, 18, 44, 69, 141, 255, 18, 32, 22, 40 },   // left = smooth
      { 52, 32, 54, 96, 194, 255, 33, 37, 25, 53 },   // left = paeth
  },
  {
      // above = paeth
      { 76, 47, 67, 123, 182, 150, 41, 52, 55, 97 },    // left = dc
      { 69, 40, 125, 102, 138, 138, 42, 55, 32, 70 },   // left = v
      { 46, 28, 27, 160, 232, 169, 34, 21, 32, 122 },   // left = h
      { 78, 35, 41, 99, 128, 124, 49, 43, 35, 111 },    // left = d45
      { 66, 28, 47, 100, 113, 145, 37, 40, 72, 93 },    // left = d135
      { 77, 37, 76, 134, 124, 124, 65, 122, 88, 255 },  // left = d117
      { 53, 23, 38, 108, 128, 204, 26, 32, 115, 114 },  // left = d153
      { 65, 20, 29, 101, 202, 186, 29, 24, 29, 188 },   // left = d207
      { 71, 24, 49, 81, 126, 151, 36, 65, 28, 93 },     // left = d63
      { 54, 36, 53, 94, 193, 255, 25, 38, 20, 64 },     // left = smooth
      { 52, 54, 60, 108, 176, 168, 47, 44, 50, 105 },   // left = paeth
  },
};

static const aom_prob default_if_y_probs[BLOCK_SIZE_GROUPS][INTRA_MODES - 1] = {
  { 88, 16, 47, 133, 143, 150, 70, 48, 84, 122 },  // block_size < 8x8
  { 75, 26, 51, 120, 158, 157, 44, 45, 56, 102 },  // block_size < 16x16
  { 73, 24, 60, 115, 184, 164, 26, 36, 32, 63 },   // block_size < 32x32
  { 96, 27, 50, 107, 221, 148, 16, 22, 14, 39 },   // block_size >= 32x32
};

static const aom_prob default_uv_probs[INTRA_MODES][INTRA_MODES - 1] = {
  { 199, 3, 79, 179, 220, 109, 38, 50, 68, 138 },   // y = dc
  { 17, 2, 219, 136, 131, 58, 21, 106, 23, 41 },    // y = v
  { 26, 1, 5, 244, 253, 138, 16, 21, 68, 205 },     // y = h
  { 183, 3, 66, 94, 195, 97, 101, 104, 41, 178 },   // y = d45
  { 178, 2, 36, 158, 99, 175, 21, 29, 105, 77 },    // y = d135
  { 154, 3, 65, 219, 40, 48, 45, 95, 146, 255 },    // y = d117
  { 167, 1, 16, 160, 214, 187, 10, 10, 200, 155 },  // y = d153
  { 154, 2, 18, 178, 238, 132, 25, 21, 34, 221 },   // y = d207
  { 153, 4, 76, 85, 157, 90, 38, 165, 46, 104 },    // y = d63
  { 163, 3, 68, 87, 190, 255, 19, 27, 25, 46 },     // y = smooth
  { 185, 7, 113, 171, 203, 57, 18, 69, 49, 104 },   // y = paeth
};

#else

const aom_prob av1_kf_y_mode_prob[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1] = {
  {
      // above = dc
      { 137, 30, 42, 148, 151, 207, 70, 52, 91 },   // left = dc
      { 92, 45, 102, 136, 116, 180, 74, 90, 100 },  // left = v
      { 73, 32, 19, 187, 222, 215, 46, 34, 100 },   // left = h
      { 91, 30, 32, 116, 121, 186, 93, 86, 94 },    // left = d45
      { 72, 35, 36, 149, 68, 206, 68, 63, 105 },    // left = d135
      { 73, 31, 28, 138, 57, 124, 55, 122, 151 },   // left = d117
      { 67, 23, 21, 140, 126, 197, 40, 37, 171 },   // left = d153
      { 86, 27, 28, 128, 154, 212, 45, 43, 53 },    // left = d207
      { 74, 32, 27, 107, 86, 160, 63, 134, 102 },   // left = d63
      { 59, 67, 44, 140, 161, 202, 78, 67, 119 }    // left = tm
  },
  {
      // above = v
      { 63, 36, 126, 146, 123, 158, 60, 90, 96 },   // left = dc
      { 43, 46, 168, 134, 107, 128, 69, 142, 92 },  // left = v
      { 44, 29, 68, 159, 201, 177, 50, 57, 77 },    // left = h
      { 58, 38, 76, 114, 97, 172, 78, 133, 92 },    // left = d45
      { 46, 41, 76, 140, 63, 184, 69, 112, 57 },    // left = d135
      { 38, 32, 85, 140, 46, 112, 54, 151, 133 },   // left = d117
      { 39, 27, 61, 131, 110, 175, 44, 75, 136 },   // left = d153
      { 52, 30, 74, 113, 130, 175, 51, 64, 58 },    // left = d207
      { 47, 35, 80, 100, 74, 143, 64, 163, 74 },    // left = d63
      { 36, 61, 116, 114, 128, 162, 80, 125, 82 }   // left = tm
  },
  {
      // above = h
      { 82, 26, 26, 171, 208, 204, 44, 32, 105 },  // left = dc
      { 55, 44, 68, 166, 179, 192, 57, 57, 108 },  // left = v
      { 42, 26, 11, 199, 241, 228, 23, 15, 85 },   // left = h
      { 68, 42, 19, 131, 160, 199, 55, 52, 83 },   // left = d45
      { 58, 50, 25, 139, 115, 232, 39, 52, 118 },  // left = d135
      { 50, 35, 33, 153, 104, 162, 64, 59, 131 },  // left = d117
      { 44, 24, 16, 150, 177, 202, 33, 19, 156 },  // left = d153
      { 55, 27, 12, 153, 203, 218, 26, 27, 49 },   // left = d207
      { 53, 49, 21, 110, 116, 168, 59, 80, 76 },   // left = d63
      { 38, 72, 19, 168, 203, 212, 50, 50, 107 }   // left = tm
  },
  {
      // above = d45
      { 103, 26, 36, 129, 132, 201, 83, 80, 93 },  // left = dc
      { 59, 38, 83, 112, 103, 162, 98, 136, 90 },  // left = v
      { 62, 30, 23, 158, 200, 207, 59, 57, 50 },   // left = h
      { 67, 30, 29, 84, 86, 191, 102, 91, 59 },    // left = d45
      { 60, 32, 33, 112, 71, 220, 64, 89, 104 },   // left = d135
      { 53, 26, 34, 130, 56, 149, 84, 120, 103 },  // left = d117
      { 53, 21, 23, 133, 109, 210, 56, 77, 172 },  // left = d153
      { 77, 19, 29, 112, 142, 228, 55, 66, 36 },   // left = d207
      { 61, 29, 29, 93, 97, 165, 83, 175, 162 },   // left = d63
      { 47, 47, 43, 114, 137, 181, 100, 99, 95 }   // left = tm
  },
  {
      // above = d135
      { 69, 23, 29, 128, 83, 199, 46, 44, 101 },   // left = dc
      { 53, 40, 55, 139, 69, 183, 61, 80, 110 },   // left = v
      { 40, 29, 19, 161, 180, 207, 43, 24, 91 },   // left = h
      { 60, 34, 19, 105, 61, 198, 53, 64, 89 },    // left = d45
      { 52, 31, 22, 158, 40, 209, 58, 62, 89 },    // left = d135
      { 44, 31, 29, 147, 46, 158, 56, 102, 198 },  // left = d117
      { 35, 19, 12, 135, 87, 209, 41, 45, 167 },   // left = d153
      { 55, 25, 21, 118, 95, 215, 38, 39, 66 },    // left = d207
      { 51, 38, 25, 113, 58, 164, 70, 93, 97 },    // left = d63
      { 47, 54, 34, 146, 108, 203, 72, 103, 151 }  // left = tm
  },
  {
      // above = d117
      { 64, 19, 37, 156, 66, 138, 49, 95, 133 },   // left = dc
      { 46, 27, 80, 150, 55, 124, 55, 121, 135 },  // left = v
      { 36, 23, 27, 165, 149, 166, 54, 64, 118 },  // left = h
      { 53, 21, 36, 131, 63, 163, 60, 109, 81 },   // left = d45
      { 40, 26, 35, 154, 40, 185, 51, 97, 123 },   // left = d135
      { 35, 19, 34, 179, 19, 97, 48, 129, 124 },   // left = d117
      { 36, 20, 26, 136, 62, 164, 33, 77, 154 },   // left = d153
      { 45, 18, 32, 130, 90, 157, 40, 79, 91 },    // left = d207
      { 45, 26, 28, 129, 45, 129, 49, 147, 123 },  // left = d63
      { 38, 44, 51, 136, 74, 162, 57, 97, 121 }    // left = tm
  },
  {
      // above = d153
      { 75, 17, 22, 136, 138, 185, 32, 34, 166 },  // left = dc
      { 56, 39, 58, 133, 117, 173, 48, 53, 187 },  // left = v
      { 35, 21, 12, 161, 212, 207, 20, 23, 145 },  // left = h
      { 56, 29, 19, 117, 109, 181, 55, 68, 112 },  // left = d45
      { 47, 29, 17, 153, 64, 220, 59, 51, 114 },   // left = d135
      { 46, 16, 24, 136, 76, 147, 41, 64, 172 },   // left = d117
      { 34, 17, 11, 108, 152, 187, 13, 15, 209 },  // left = d153
      { 51, 24, 14, 115, 133, 209, 32, 26, 104 },  // left = d207
      { 55, 30, 18, 122, 79, 179, 44, 88, 116 },   // left = d63
      { 37, 49, 25, 129, 168, 164, 41, 54, 148 }   // left = tm
  },
  {
      // above = d207
      { 82, 22, 32, 127, 143, 213, 39, 41, 70 },   // left = dc
      { 62, 44, 61, 123, 105, 189, 48, 57, 64 },   // left = v
      { 47, 25, 17, 175, 222, 220, 24, 30, 86 },   // left = h
      { 68, 36, 17, 106, 102, 206, 59, 74, 74 },   // left = d45
      { 57, 39, 23, 151, 68, 216, 55, 63, 58 },    // left = d135
      { 49, 30, 35, 141, 70, 168, 82, 40, 115 },   // left = d117
      { 51, 25, 15, 136, 129, 202, 38, 35, 139 },  // left = d153
      { 68, 26, 16, 111, 141, 215, 29, 28, 28 },   // left = d207
      { 59, 39, 19, 114, 75, 180, 77, 104, 42 },   // left = d63
      { 40, 61, 26, 126, 152, 206, 61, 59, 93 }    // left = tm
  },
  {
      // above = d63
      { 78, 23, 39, 111, 117, 170, 74, 124, 94 },   // left = dc
      { 48, 34, 86, 101, 92, 146, 78, 179, 134 },   // left = v
      { 47, 22, 24, 138, 187, 178, 68, 69, 59 },    // left = h
      { 56, 25, 33, 105, 112, 187, 95, 177, 129 },  // left = d45
      { 48, 31, 27, 114, 63, 183, 82, 116, 56 },    // left = d135
      { 43, 28, 37, 121, 63, 123, 61, 192, 169 },   // left = d117
      { 42, 17, 24, 109, 97, 177, 56, 76, 122 },    // left = d153
      { 58, 18, 28, 105, 139, 182, 70, 92, 63 },    // left = d207
      { 46, 23, 32, 74, 86, 150, 67, 183, 88 },     // left = d63
      { 36, 38, 48, 92, 122, 165, 88, 137, 91 }     // left = tm
  },
  {
      // above = tm
      { 65, 70, 60, 155, 159, 199, 61, 60, 81 },    // left = dc
      { 44, 78, 115, 132, 119, 173, 71, 112, 93 },  // left = v
      { 39, 38, 21, 184, 227, 206, 42, 32, 64 },    // left = h
      { 58, 47, 36, 124, 137, 193, 80, 82, 78 },    // left = d45
      { 49, 50, 35, 144, 95, 205, 63, 78, 59 },     // left = d135
      { 41, 53, 52, 148, 71, 142, 65, 128, 51 },    // left = d117
      { 40, 36, 28, 143, 143, 202, 40, 55, 137 },   // left = d153
      { 52, 34, 29, 129, 183, 227, 42, 35, 43 },    // left = d207
      { 42, 44, 44, 104, 105, 164, 64, 130, 80 },   // left = d63
      { 43, 81, 53, 140, 169, 204, 68, 84, 72 }     // left = tm
  }
};

// Default probabilities for signaling Intra mode for Y plane -- used only for
// inter frames. ('av1_kf_y_mode_prob' is used for intra-only frames).
// Context used: block size group.
static const aom_prob default_if_y_probs[BLOCK_SIZE_GROUPS][INTRA_MODES - 1] = {
  { 65, 32, 18, 144, 162, 194, 41, 51, 98 },   // block_size < 8x8
  { 132, 68, 18, 165, 217, 196, 45, 40, 78 },  // block_size < 16x16
  { 173, 80, 19, 176, 240, 193, 64, 35, 46 },  // block_size < 32x32
  { 221, 135, 38, 194, 248, 121, 96, 85, 29 }  // block_size >= 32x32
};

// Default probabilities for signaling Intra mode for UV plane -- common for
// both intra and inter frames.
// Context used: Intra mode used by Y plane of the same block.
static const aom_prob default_uv_probs[INTRA_MODES][INTRA_MODES - 1] = {
  { 120, 7, 76, 176, 208, 126, 28, 54, 103 },   // y = dc
  { 48, 12, 154, 155, 139, 90, 34, 117, 119 },  // y = v
  { 67, 6, 25, 204, 243, 158, 13, 21, 96 },     // y = h
  { 97, 5, 44, 131, 176, 139, 48, 68, 97 },     // y = d45
  { 83, 5, 42, 156, 111, 152, 26, 49, 152 },    // y = d135
  { 80, 5, 58, 178, 74, 83, 33, 62, 145 },      // y = d117
  { 86, 5, 32, 154, 192, 168, 14, 22, 163 },    // y = d153
  { 85, 5, 32, 156, 216, 148, 19, 29, 73 },     // y = d207
  { 77, 7, 64, 116, 132, 122, 37, 126, 120 },   // y = d63
  { 101, 21, 107, 181, 192, 103, 19, 67, 125 }  // y = tm
};

#endif  // CONFIG_ALT_INTRA

#if CONFIG_EXT_PARTITION_TYPES
static const aom_prob
    default_partition_probs[PARTITION_CONTEXTS][EXT_PARTITION_TYPES - 1] = {
      // 8x8 -> 4x4
      { 199, 122, 141, 128, 128, 128, 128 },  // a/l both not split
      { 147, 63, 159, 128, 128, 128, 128 },   // a split, l not split
      { 148, 133, 118, 128, 128, 128, 128 },  // l split, a not split
      { 121, 104, 114, 128, 128, 128, 128 },  // a/l both split
      // 16x16 -> 8x8
      { 174, 73, 87, 128, 128, 128, 128 },  // a/l both not split
      { 92, 41, 83, 128, 128, 128, 128 },   // a split, l not split
      { 82, 99, 50, 128, 128, 128, 128 },   // l split, a not split
      { 53, 39, 39, 128, 128, 128, 128 },   // a/l both split
      // 32x32 -> 16x16
      { 177, 58, 59, 128, 128, 128, 128 },  // a/l both not split
      { 68, 26, 63, 128, 128, 128, 128 },   // a split, l not split
      { 52, 79, 25, 128, 128, 128, 128 },   // l split, a not split
      { 17, 14, 12, 128, 128, 128, 128 },   // a/l both split
      // 64x64 -> 32x32
      { 222, 34, 30, 128, 128, 128, 128 },  // a/l both not split
      { 72, 16, 44, 128, 128, 128, 128 },   // a split, l not split
      { 58, 32, 12, 128, 128, 128, 128 },   // l split, a not split
      { 10, 7, 6, 128, 128, 128, 128 },     // a/l both split
#if CONFIG_EXT_PARTITION
      // 128x128 -> 64x64
      { 222, 34, 30, 128, 128, 128, 128 },  // a/l both not split
      { 72, 16, 44, 128, 128, 128, 128 },   // a split, l not split
      { 58, 32, 12, 128, 128, 128, 128 },   // l split, a not split
      { 10, 7, 6, 128, 128, 128, 128 },     // a/l both split
#endif                                      // CONFIG_EXT_PARTITION
#if CONFIG_UNPOISON_PARTITION_CTX
      { 0, 0, 141, 0, 0, 0, 0 },  // 8x8 -> 4x4
      { 0, 0, 87, 0, 0, 0, 0 },   // 16x16 -> 8x8
      { 0, 0, 59, 0, 0, 0, 0 },   // 32x32 -> 16x16
      { 0, 0, 30, 0, 0, 0, 0 },   // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 0, 30, 0, 0, 0, 0 },   // 128x128 -> 64x64
#endif                            // CONFIG_EXT_PARTITION
      { 0, 122, 0, 0, 0, 0, 0 },  // 8x8 -> 4x4
      { 0, 73, 0, 0, 0, 0, 0 },   // 16x16 -> 8x8
      { 0, 58, 0, 0, 0, 0, 0 },   // 32x32 -> 16x16
      { 0, 34, 0, 0, 0, 0, 0 },   // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 34, 0, 0, 0, 0, 0 },  // 128x128 -> 64x64
#endif                           // CONFIG_EXT_PARTITION
#endif                           // CONFIG_UNPOISON_PARTITION_CTX
    };
#else
static const aom_prob
    default_partition_probs[PARTITION_CONTEXTS][PARTITION_TYPES - 1] = {
      // 8x8 -> 4x4
      { 199, 122, 141 },  // a/l both not split
      { 147, 63, 159 },   // a split, l not split
      { 148, 133, 118 },  // l split, a not split
      { 121, 104, 114 },  // a/l both split
      // 16x16 -> 8x8
      { 174, 73, 87 },  // a/l both not split
      { 92, 41, 83 },   // a split, l not split
      { 82, 99, 50 },   // l split, a not split
      { 53, 39, 39 },   // a/l both split
      // 32x32 -> 16x16
      { 177, 58, 59 },  // a/l both not split
      { 68, 26, 63 },   // a split, l not split
      { 52, 79, 25 },   // l split, a not split
      { 17, 14, 12 },   // a/l both split
      // 64x64 -> 32x32
      { 222, 34, 30 },  // a/l both not split
      { 72, 16, 44 },   // a split, l not split
      { 58, 32, 12 },   // l split, a not split
      { 10, 7, 6 },     // a/l both split
#if CONFIG_EXT_PARTITION
      // 128x128 -> 64x64
      { 222, 34, 30 },  // a/l both not split
      { 72, 16, 44 },   // a split, l not split
      { 58, 32, 12 },   // l split, a not split
      { 10, 7, 6 },     // a/l both split
#endif  // CONFIG_EXT_PARTITION
#if CONFIG_UNPOISON_PARTITION_CTX
      { 0, 0, 141 },    // 8x8 -> 4x4
      { 0, 0, 87 },     // 16x16 -> 8x8
      { 0, 0, 59 },     // 32x32 -> 16x16
      { 0, 0, 30 },     // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 0, 30 },     // 128x128 -> 64x64
#endif  // CONFIG_EXT_PARTITION
      { 0, 122, 0 },    // 8x8 -> 4x4
      { 0, 73, 0 },     // 16x16 -> 8x8
      { 0, 58, 0 },     // 32x32 -> 16x16
      { 0, 34, 0 },     // 64x64 -> 32x32
#if CONFIG_EXT_PARTITION
      { 0, 34, 0 },     // 128x128 -> 64x64
#endif  // CONFIG_EXT_PARTITION
#endif  // CONFIG_UNPOISON_PARTITION_CTX
    };
#endif  // CONFIG_EXT_PARTITION_TYPES

#if CONFIG_REF_MV
static const aom_prob default_newmv_prob[NEWMV_MODE_CONTEXTS] = {
  200, 180, 150, 150, 110, 70, 60,
};

static const aom_prob default_zeromv_prob[ZEROMV_MODE_CONTEXTS] = {
  192, 64,
};

static const aom_prob default_refmv_prob[REFMV_MODE_CONTEXTS] = {
  220, 220, 200, 200, 180, 128, 30, 220, 30,
};

static const aom_prob default_drl_prob[DRL_MODE_CONTEXTS] = { 128, 160, 180,
                                                              128, 160 };

#if CONFIG_EXT_INTER
static const aom_prob default_new2mv_prob = 180;
#endif  // CONFIG_EXT_INTER
#endif  // CONFIG_REF_MV

static const aom_prob
    default_inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1] = {
#if CONFIG_EXT_INTER
      // TODO(zoeliu): To adjust the initial default probs
      { 2, 173, 34, 173 },  // 0 = both zero mv
      { 7, 145, 85, 145 },  // 1 = one zero mv + one a predicted mv
      { 7, 166, 63, 166 },  // 2 = two predicted mvs
      { 7, 94, 66, 128 },   // 3 = one predicted/zero and one new mv
      { 8, 64, 46, 128 },   // 4 = two new mvs
      { 17, 81, 31, 128 },  // 5 = one intra neighbour + x
      { 25, 29, 30, 96 },   // 6 = two intra neighbours
#else
      { 2, 173, 34 },  // 0 = both zero mv
      { 7, 145, 85 },  // 1 = one zero mv + one a predicted mv
      { 7, 166, 63 },  // 2 = two predicted mvs
      { 7, 94, 66 },   // 3 = one predicted/zero and one new mv
      { 8, 64, 46 },   // 4 = two new mvs
      { 17, 81, 31 },  // 5 = one intra neighbour + x
      { 25, 29, 30 },  // 6 = two intra neighbours
#endif  // CONFIG_EXT_INTER
    };

#if CONFIG_EXT_INTER
static const aom_prob default_inter_compound_mode_probs
    [INTER_MODE_CONTEXTS][INTER_COMPOUND_MODES - 1] = {
      { 2, 173, 68, 192, 64, 192, 128, 180, 180 },   // 0 = both zero mv
      { 7, 145, 160, 192, 64, 192, 128, 180, 180 },  // 1 = 1 zero + 1 predicted
      { 7, 166, 126, 192, 64, 192, 128, 180, 180 },  // 2 = two predicted mvs
      { 7, 94, 132, 192, 64, 192, 128, 180, 180 },   // 3 = 1 pred/zero, 1 new
      { 8, 64, 64, 192, 64, 192, 128, 180, 180 },    // 4 = two new mvs
      { 17, 81, 52, 192, 64, 192, 128, 180, 180 },   // 5 = one intra neighbour
      { 25, 29, 50, 192, 64, 192, 128, 180, 180 },   // 6 = two intra neighbours
    };

#if CONFIG_COMPOUND_SEGMENT
static const aom_prob
    default_compound_type_probs[BLOCK_SIZES][COMPOUND_TYPES - 1] = {
#if CONFIG_CB4X4
      { 255, 255 }, { 255, 255 }, { 255, 255 },
#endif
      { 208, 200 }, { 208, 200 }, { 208, 200 }, { 208, 200 }, { 208, 200 },
      { 208, 200 }, { 216, 200 }, { 216, 200 }, { 216, 200 }, { 224, 200 },
      { 224, 200 }, { 240, 200 }, { 240, 200 },
#if CONFIG_EXT_PARTITION
      { 255, 200 }, { 255, 200 }, { 255, 200 },
#endif  // CONFIG_EXT_PARTITION
    };
#else  // !CONFIG_COMPOUND_SEGMENT
static const aom_prob
    default_compound_type_probs[BLOCK_SIZES][COMPOUND_TYPES - 1] = {
#if CONFIG_CB4X4
      { 208 }, { 208 }, { 208 },
#endif
      { 208 }, { 208 }, { 208 }, { 208 }, { 208 }, { 208 }, { 216 },
      { 216 }, { 216 }, { 224 }, { 224 }, { 240 }, { 240 },
#if CONFIG_EXT_PARTITION
      { 255 }, { 255 }, { 255 },
#endif  // CONFIG_EXT_PARTITION
    };
#endif  // CONFIG_COMPOUND_SEGMENT

static const aom_prob default_interintra_prob[BLOCK_SIZE_GROUPS] = {
  208, 208, 208, 208,
};

static const aom_prob
    default_interintra_mode_prob[BLOCK_SIZE_GROUPS][INTERINTRA_MODES - 1] = {
      { 65, 32, 18, 144, 162, 194, 41, 51, 98 },   // block_size < 8x8
      { 132, 68, 18, 165, 217, 196, 45, 40, 78 },  // block_size < 16x16
      { 173, 80, 19, 176, 240, 193, 64, 35, 46 },  // block_size < 32x32
      { 221, 135, 38, 194, 248, 121, 96, 85, 29 }  // block_size >= 32x32
    };

static const aom_prob default_wedge_interintra_prob[BLOCK_SIZES] = {
#if CONFIG_CB4X4
  208, 208, 208,
#endif
  208, 208, 208, 208, 208, 208, 216, 216, 216, 224, 224, 224, 240,
#if CONFIG_EXT_PARTITION
  208, 208, 208
#endif  // CONFIG_EXT_PARTITION
};
#endif  // CONFIG_EXT_INTER

// Change this section appropriately once warped motion is supported
#if CONFIG_MOTION_VAR && !CONFIG_WARPED_MOTION
const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)] = {
  -SIMPLE_TRANSLATION, -OBMC_CAUSAL
};
static const aom_prob default_motion_mode_prob[BLOCK_SIZES][MOTION_MODES - 1] =
    {
#if CONFIG_CB4X4
      { 255 }, { 255 }, { 255 },
#endif
      { 255 }, { 255 }, { 255 }, { 151 }, { 153 }, { 144 }, { 178 },
      { 165 }, { 160 }, { 207 }, { 195 }, { 168 }, { 244 },
#if CONFIG_EXT_PARTITION
      { 252 }, { 252 }, { 252 },
#endif  // CONFIG_EXT_PARTITION
    };

#elif !CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION

const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)] = {
  -SIMPLE_TRANSLATION, -WARPED_CAUSAL
};

static const aom_prob default_motion_mode_prob[BLOCK_SIZES][MOTION_MODES - 1] =
    {
#if CONFIG_CB4X4
      { 255 }, { 255 }, { 255 },
#endif
      { 255 }, { 255 }, { 255 }, { 151 }, { 153 }, { 144 }, { 178 },
      { 165 }, { 160 }, { 207 }, { 195 }, { 168 }, { 244 },
#if CONFIG_EXT_PARTITION
      { 252 }, { 252 }, { 252 },
#endif  // CONFIG_EXT_PARTITION
    };

#elif CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION

const aom_tree_index av1_motion_mode_tree[TREE_SIZE(MOTION_MODES)] = {
  -SIMPLE_TRANSLATION, 2, -OBMC_CAUSAL, -WARPED_CAUSAL,
};
static const aom_prob default_motion_mode_prob[BLOCK_SIZES][MOTION_MODES - 1] =
    {
#if CONFIG_CB4X4
      { 255, 200 }, { 255, 200 }, { 255, 200 },
#endif
      { 255, 200 }, { 255, 200 }, { 255, 200 }, { 151, 200 }, { 153, 200 },
      { 144, 200 }, { 178, 200 }, { 165, 200 }, { 160, 200 }, { 207, 200 },
      { 195, 200 }, { 168, 200 }, { 244, 200 },
#if CONFIG_EXT_PARTITION
      { 252, 200 }, { 252, 200 }, { 252, 200 },
#endif  // CONFIG_EXT_PARTITION
    };

// Probability for the case that only 1 additional motion mode is allowed
static const aom_prob default_obmc_prob[BLOCK_SIZES] = {
#if CONFIG_CB4X4
  255, 255, 255,
#endif
  255, 255, 255, 151, 153, 144, 178, 165, 160, 207, 195, 168, 244,
#if CONFIG_EXT_PARTITION
  252, 252, 252,
#endif  // CONFIG_EXT_PARTITION
};
#endif

#if CONFIG_DELTA_Q
static const aom_prob default_delta_q_probs[DELTA_Q_PROBS] = { 220, 220, 220 };
#if CONFIG_EC_MULTISYMBOL
static const aom_cdf_prob default_delta_q_cdf[CDF_SIZE(DELTA_Q_PROBS + 1)] = {
  28160, 32120, 32677, 32768, 0
};
#endif
#endif
#if CONFIG_EC_MULTISYMBOL
int av1_intra_mode_ind[INTRA_MODES];
int av1_intra_mode_inv[INTRA_MODES];
int av1_inter_mode_ind[INTER_MODES];
int av1_inter_mode_inv[INTER_MODES];
#endif

/* Array indices are identical to previously-existing INTRAMODECONTEXTNODES. */
#if CONFIG_ALT_INTRA
const aom_tree_index av1_intra_mode_tree[TREE_SIZE(INTRA_MODES)] = {
  -DC_PRED,   2,            /* 0 = DC_NODE */
  -TM_PRED,   4,            /* 1 = TM_NODE */
  -V_PRED,    6,            /* 2 = V_NODE */
  8,          12,           /* 3 = COM_NODE */
  -H_PRED,    10,           /* 4 = H_NODE */
  -D135_PRED, -D117_PRED,   /* 5 = D135_NODE */
  -D45_PRED,  14,           /* 6 = D45_NODE */
  -D63_PRED,  16,           /* 7 = D63_NODE */
  -D153_PRED, 18,           /* 8 = D153_NODE */
  -D207_PRED, -SMOOTH_PRED, /* 9 = D207_NODE */
};
#else
const aom_tree_index av1_intra_mode_tree[TREE_SIZE(INTRA_MODES)] = {
  -DC_PRED,   2,          /* 0 = DC_NODE */
  -TM_PRED,   4,          /* 1 = TM_NODE */
  -V_PRED,    6,          /* 2 = V_NODE */
  8,          12,         /* 3 = COM_NODE */
  -H_PRED,    10,         /* 4 = H_NODE */
  -D135_PRED, -D117_PRED, /* 5 = D135_NODE */
  -D45_PRED,  14,         /* 6 = D45_NODE */
  -D63_PRED,  16,         /* 7 = D63_NODE */
  -D153_PRED, -D207_PRED  /* 8 = D153_NODE */
};
#endif  // CONFIG_ALT_INTRA

const aom_tree_index av1_inter_mode_tree[TREE_SIZE(INTER_MODES)] = {
  -INTER_OFFSET(ZEROMV),    2,
  -INTER_OFFSET(NEARESTMV), 4,
#if CONFIG_EXT_INTER
  -INTER_OFFSET(NEARMV),    6,
  -INTER_OFFSET(NEWMV),     -INTER_OFFSET(NEWFROMNEARMV)
#else
  -INTER_OFFSET(NEARMV),    -INTER_OFFSET(NEWMV)
#endif  // CONFIG_EXT_INTER
};

#if CONFIG_EXT_INTER
/* clang-format off */
const aom_tree_index av1_interintra_mode_tree[TREE_SIZE(INTERINTRA_MODES)] = {
  -II_DC_PRED, 2,                   /* 0 = II_DC_NODE     */
  -II_TM_PRED, 4,                   /* 1 = II_TM_NODE     */
  -II_V_PRED, 6,                    /* 2 = II_V_NODE      */
  8, 12,                            /* 3 = II_COM_NODE    */
  -II_H_PRED, 10,                   /* 4 = II_H_NODE      */
  -II_D135_PRED, -II_D117_PRED,     /* 5 = II_D135_NODE   */
  -II_D45_PRED, 14,                 /* 6 = II_D45_NODE    */
  -II_D63_PRED, 16,                 /* 7 = II_D63_NODE    */
  -II_D153_PRED, -II_D207_PRED      /* 8 = II_D153_NODE   */
};

const aom_tree_index av1_inter_compound_mode_tree
    [TREE_SIZE(INTER_COMPOUND_MODES)] = {
  -INTER_COMPOUND_OFFSET(ZERO_ZEROMV), 2,
  -INTER_COMPOUND_OFFSET(NEAREST_NEARESTMV), 4,
  6, -INTER_COMPOUND_OFFSET(NEW_NEWMV),
  8, 12,
  -INTER_COMPOUND_OFFSET(NEAR_NEARMV), 10,
  -INTER_COMPOUND_OFFSET(NEAREST_NEARMV),
      -INTER_COMPOUND_OFFSET(NEAR_NEARESTMV),
  14, 16,
  -INTER_COMPOUND_OFFSET(NEAREST_NEWMV), -INTER_COMPOUND_OFFSET(NEW_NEARESTMV),
  -INTER_COMPOUND_OFFSET(NEAR_NEWMV), -INTER_COMPOUND_OFFSET(NEW_NEARMV)
};

#if CONFIG_COMPOUND_SEGMENT
const aom_tree_index av1_compound_type_tree[TREE_SIZE(COMPOUND_TYPES)] = {
  -COMPOUND_AVERAGE, 2, -COMPOUND_WEDGE, -COMPOUND_SEG
};
#else  // !CONFIG_COMPOUND_SEGMENT
const aom_tree_index av1_compound_type_tree[TREE_SIZE(COMPOUND_TYPES)] = {
  -COMPOUND_AVERAGE, -COMPOUND_WEDGE
};
#endif  // CONFIG_COMPOUND_SEGMENT
/* clang-format on */
#endif  // CONFIG_EXT_INTER

const aom_tree_index av1_partition_tree[TREE_SIZE(PARTITION_TYPES)] = {
  -PARTITION_NONE, 2, -PARTITION_HORZ, 4, -PARTITION_VERT, -PARTITION_SPLIT
};

#if CONFIG_EXT_PARTITION_TYPES
/* clang-format off */
const aom_tree_index av1_ext_partition_tree[TREE_SIZE(EXT_PARTITION_TYPES)] = {
  -PARTITION_NONE, 2,
  6, 4,
  8, -PARTITION_SPLIT,
  -PARTITION_HORZ, 10,
  -PARTITION_VERT, 12,
  -PARTITION_HORZ_A, -PARTITION_HORZ_B,
  -PARTITION_VERT_A, -PARTITION_VERT_B
};
/* clang-format on */
#endif  // CONFIG_EXT_PARTITION_TYPES

static const aom_prob default_intra_inter_p[INTRA_INTER_CONTEXTS] = {
  9, 102, 187, 225
};

static const aom_prob default_comp_inter_p[COMP_INTER_CONTEXTS] = {
  239, 183, 119, 96, 41
};

#if CONFIG_EXT_REFS
static const aom_prob default_comp_ref_p[REF_CONTEXTS][FWD_REFS - 1] = {
  // TODO(zoeliu): To adjust the initial prob values.
  { 33, 16, 16 },
  { 77, 74, 74 },
  { 142, 142, 142 },
  { 172, 170, 170 },
  { 238, 247, 247 }
};
static const aom_prob default_comp_bwdref_p[REF_CONTEXTS][BWD_REFS - 1] = {
  { 16 }, { 74 }, { 142 }, { 170 }, { 247 }
};
#else
static const aom_prob default_comp_ref_p[REF_CONTEXTS][COMP_REFS - 1] = {
  { 50 }, { 126 }, { 123 }, { 221 }, { 226 }
};
#endif  // CONFIG_EXT_REFS

static const aom_prob default_single_ref_p[REF_CONTEXTS][SINGLE_REFS - 1] = {
#if CONFIG_EXT_REFS
  { 33, 16, 16, 16, 16 },
  { 77, 74, 74, 74, 74 },
  { 142, 142, 142, 142, 142 },
  { 172, 170, 170, 170, 170 },
  { 238, 247, 247, 247, 247 }
#else
  { 33, 16 }, { 77, 74 }, { 142, 142 }, { 172, 170 }, { 238, 247 }
#endif  // CONFIG_EXT_REFS
};

#if CONFIG_PALETTE

// Tree to code palette size (number of colors in a palette) and the
// corresponding probabilities for Y and UV planes.
const aom_tree_index av1_palette_size_tree[TREE_SIZE(PALETTE_SIZES)] = {
  -TWO_COLORS,  2, -THREE_COLORS, 4,  -FOUR_COLORS,  6,
  -FIVE_COLORS, 8, -SIX_COLORS,   10, -SEVEN_COLORS, -EIGHT_COLORS,
};

// TODO(huisu): tune these probs
const aom_prob
    av1_default_palette_y_size_prob[PALETTE_BLOCK_SIZES][PALETTE_SIZES - 1] = {
      { 96, 89, 100, 64, 77, 130 },   { 22, 15, 44, 16, 34, 82 },
      { 30, 19, 57, 18, 38, 86 },     { 94, 36, 104, 23, 43, 92 },
      { 116, 76, 107, 46, 65, 105 },  { 112, 82, 94, 40, 70, 112 },
      { 147, 124, 123, 58, 69, 103 }, { 180, 113, 136, 49, 45, 114 },
      { 107, 70, 87, 49, 154, 156 },  { 98, 105, 142, 63, 64, 152 },
#if CONFIG_EXT_PARTITION
      { 98, 105, 142, 63, 64, 152 },  { 98, 105, 142, 63, 64, 152 },
      { 98, 105, 142, 63, 64, 152 },
#endif  // CONFIG_EXT_PARTITION
    };

const aom_prob
    av1_default_palette_uv_size_prob[PALETTE_BLOCK_SIZES][PALETTE_SIZES - 1] = {
      { 160, 196, 228, 213, 175, 230 }, { 87, 148, 208, 141, 166, 163 },
      { 72, 151, 204, 139, 155, 161 },  { 78, 135, 171, 104, 120, 173 },
      { 59, 92, 131, 78, 92, 142 },     { 75, 118, 149, 84, 90, 128 },
      { 89, 87, 92, 66, 66, 128 },      { 67, 53, 54, 55, 66, 93 },
      { 120, 130, 83, 171, 75, 214 },   { 72, 55, 66, 68, 79, 107 },
#if CONFIG_EXT_PARTITION
      { 72, 55, 66, 68, 79, 107 },      { 72, 55, 66, 68, 79, 107 },
      { 72, 55, 66, 68, 79, 107 },
#endif  // CONFIG_EXT_PARTITION
    };

// When palette mode is enabled, following probability tables indicate the
// probabilities to code the "is_palette" bit (i.e. the bit that indicates
// if this block uses palette mode or DC_PRED mode).
const aom_prob av1_default_palette_y_mode_prob
    [PALETTE_BLOCK_SIZES][PALETTE_Y_MODE_CONTEXTS] = {
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
      { 240, 180, 100 },
#if CONFIG_EXT_PARTITION
      { 240, 180, 100 }, { 240, 180, 100 }, { 240, 180, 100 },
#endif  // CONFIG_EXT_PARTITION
    };

const aom_prob av1_default_palette_uv_mode_prob[PALETTE_UV_MODE_CONTEXTS] = {
  253, 229
};

// Trees to code palette color indices (for various palette sizes), and the
// corresponding probability tables for Y and UV planes.
const aom_tree_index
    av1_palette_color_index_tree[PALETTE_SIZES][TREE_SIZE(PALETTE_COLORS)] = {
      { // 2 colors
        -PALETTE_COLOR_ONE, -PALETTE_COLOR_TWO },
      { // 3 colors
        -PALETTE_COLOR_ONE, 2, -PALETTE_COLOR_TWO, -PALETTE_COLOR_THREE },
      { // 4 colors
        -PALETTE_COLOR_ONE, 2, -PALETTE_COLOR_TWO, 4, -PALETTE_COLOR_THREE,
        -PALETTE_COLOR_FOUR },
      { // 5 colors
        -PALETTE_COLOR_ONE, 2, -PALETTE_COLOR_TWO, 4, -PALETTE_COLOR_THREE, 6,
        -PALETTE_COLOR_FOUR, -PALETTE_COLOR_FIVE },
      { // 6 colors
        -PALETTE_COLOR_ONE, 2, -PALETTE_COLOR_TWO, 4, -PALETTE_COLOR_THREE, 6,
        -PALETTE_COLOR_FOUR, 8, -PALETTE_COLOR_FIVE, -PALETTE_COLOR_SIX },
      { // 7 colors
        -PALETTE_COLOR_ONE, 2, -PALETTE_COLOR_TWO, 4, -PALETTE_COLOR_THREE, 6,
        -PALETTE_COLOR_FOUR, 8, -PALETTE_COLOR_FIVE, 10, -PALETTE_COLOR_SIX,
        -PALETTE_COLOR_SEVEN },
      { // 8 colors
        -PALETTE_COLOR_ONE, 2, -PALETTE_COLOR_TWO, 4, -PALETTE_COLOR_THREE, 6,
        -PALETTE_COLOR_FOUR, 8, -PALETTE_COLOR_FIVE, 10, -PALETTE_COLOR_SIX, 12,
        -PALETTE_COLOR_SEVEN, -PALETTE_COLOR_EIGHT },
    };

// Note: Has to be non-zero to avoid any asserts triggering.
#define UNUSED_PROB 128

const aom_prob av1_default_palette_y_color_index_prob
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][PALETTE_COLORS - 1] = {
      {
          // 2 colors
          { 231, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
          { UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
          { 69, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 224, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
          { 249, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 3 colors
          { 219, 124, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 91, 191, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 34, 237, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 184, 118, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 252, 124, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
      },
      {
          // 4 colors
          { 204, 87, 97, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 74, 144, 129, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 52, 191, 134, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 151, 85, 147, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 248, 60, 115, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 5 colors
          { 218, 69, 62, 106, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 76, 143, 89, 127, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 21, 233, 94, 131, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 172, 72, 89, 112, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 253, 66, 65, 128, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 6 colors
          { 190, 60, 47, 54, 74, UNUSED_PROB, UNUSED_PROB },
          { 62, 106, 51, 95, 110, UNUSED_PROB, UNUSED_PROB },
          { 52, 180, 69, 72, 107, UNUSED_PROB, UNUSED_PROB },
          { 156, 83, 72, 83, 101, UNUSED_PROB, UNUSED_PROB },
          { 245, 45, 37, 52, 91, UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 7 colors
          { 206, 56, 42, 42, 53, 85, UNUSED_PROB },
          { 70, 100, 45, 68, 77, 94, UNUSED_PROB },
          { 57, 169, 51, 62, 74, 119, UNUSED_PROB },
          { 172, 76, 71, 40, 59, 76, UNUSED_PROB },
          { 248, 47, 36, 53, 61, 110, UNUSED_PROB },
      },
      {
          // 8 colors
          { 208, 52, 38, 34, 34, 44, 66 },
          { 52, 107, 34, 73, 69, 82, 87 },
          { 28, 208, 53, 43, 62, 70, 102 },
          { 184, 64, 45, 37, 37, 69, 105 },
          { 251, 18, 31, 45, 47, 61, 104 },
      },
    };

const aom_prob av1_default_palette_uv_color_index_prob
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][PALETTE_COLORS - 1] = {
      {
          // 2 colors
          { 233, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
          { UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
          { 69, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 240, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
          { 248, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 3 colors
          { 216, 128, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 110, 171, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 40, 239, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 191, 104, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
          { 247, 134, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB,
            UNUSED_PROB },
      },
      {
          // 4 colors
          { 202, 89, 132, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 90, 132, 136, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 63, 195, 149, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 152, 84, 152, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 241, 87, 136, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 5 colors
          { 209, 54, 82, 134, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 94, 173, 180, 93, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 10, 251, 127, 84, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 183, 20, 150, 47, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
          { 252, 73, 111, 150, UNUSED_PROB, UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 6 colors
          { 192, 67, 59, 46, 184, UNUSED_PROB, UNUSED_PROB },
          { 59, 92, 61, 100, 130, UNUSED_PROB, UNUSED_PROB },
          { 49, 162, 68, 91, 150, UNUSED_PROB, UNUSED_PROB },
          { 133, 29, 36, 153, 101, UNUSED_PROB, UNUSED_PROB },
          { 247, 71, 44, 90, 129, UNUSED_PROB, UNUSED_PROB },
      },
      {
          // 7 colors
          { 182, 62, 80, 78, 46, 116, UNUSED_PROB },
          { 59, 62, 39, 81, 65, 99, UNUSED_PROB },
          { 54, 177, 48, 58, 93, 104, UNUSED_PROB },
          { 137, 79, 54, 55, 44, 134, UNUSED_PROB },
          { 239, 82, 79, 44, 69, 71, UNUSED_PROB },
      },
      {
          // 8 colors
          { 172, 53, 27, 67, 30, 79, 113 },
          { 63, 57, 45, 81, 62, 35, 47 },
          { 51, 200, 36, 47, 82, 165, 129 },
          { 141, 100, 47, 29, 33, 37, 129 },
          { 236, 42, 50, 91, 24, 154, 65 },
      },
    };

#undef UNUSED_PROB

#define MAX_COLOR_CONTEXT_HASH 8
// Negative values are invalid
static const int palette_color_index_context_lookup[MAX_COLOR_CONTEXT_HASH +
                                                    1] = { -1, -1, 0, -1, -1,
                                                           4,  3,  2, 1 };

#endif  // CONFIG_PALETTE

// The transform size is coded as an offset to the smallest transform
// block size.
const aom_tree_index av1_tx_size_tree[MAX_TX_DEPTH][TREE_SIZE(TX_SIZES)] = {
  {
      // Max tx_size is 8X8
      -0, -1,
  },
  {
      // Max tx_size is 16X16
      -0, 2, -1, -2,
  },
  {
      // Max tx_size is 32X32
      -0, 2, -1, 4, -2, -3,
  },
#if CONFIG_TX64X64
  {
      // Max tx_size is 64X64
      -0, 2, -1, 4, -2, 6, -3, -4,
  },
#endif  // CONFIG_TX64X64
};

static const aom_prob default_tx_size_prob[MAX_TX_DEPTH][TX_SIZE_CONTEXTS]
                                          [MAX_TX_DEPTH] = {
                                            {
                                                // Max tx_size is 8X8
                                                { 100 },
                                                { 66 },
                                            },
                                            {
                                                // Max tx_size is 16X16
                                                { 20, 152 },
                                                { 15, 101 },
                                            },
                                            {
                                                // Max tx_size is 32X32
                                                { 3, 136, 37 },
                                                { 5, 52, 13 },
                                            },
#if CONFIG_TX64X64
                                            {
                                                // Max tx_size is 64X64
                                                { 1, 64, 136, 127 },
                                                { 1, 32, 52, 67 },
                                            },
#endif  // CONFIG_TX64X64
                                          };

#if CONFIG_LOOP_RESTORATION
const aom_tree_index
    av1_switchable_restore_tree[TREE_SIZE(RESTORE_SWITCHABLE_TYPES)] = {
      -RESTORE_NONE, 2, -RESTORE_WIENER, -RESTORE_SGRPROJ,
    };

static const aom_prob
    default_switchable_restore_prob[RESTORE_SWITCHABLE_TYPES - 1] = {
      32, 128,
    };
#endif  // CONFIG_LOOP_RESTORATION

#if CONFIG_PALETTE
#define NUM_PALETTE_NEIGHBORS 3  // left, top-left and top.
int av1_get_palette_color_index_context(const uint8_t *color_map, int stride,
                                        int r, int c, int palette_size,
                                        uint8_t *color_order, int *color_idx) {
  int i;
  // The +10 below should not be needed. But we get a warning "array subscript
  // is above array bounds [-Werror=array-bounds]" without it, possibly due to
  // this (or similar) bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
  int scores[PALETTE_MAX_SIZE + 10];
  const int weights[NUM_PALETTE_NEIGHBORS] = { 2, 1, 2 };
  const int hash_multipliers[NUM_PALETTE_NEIGHBORS] = { 1, 2, 2 };
  int color_index_ctx_hash;
  int color_index_ctx;
  int color_neighbors[NUM_PALETTE_NEIGHBORS];
  int inverse_color_order[PALETTE_MAX_SIZE];
  assert(palette_size <= PALETTE_MAX_SIZE);
  assert(r > 0 || c > 0);

  // Get color indices of neighbors.
  color_neighbors[0] = (c - 1 >= 0) ? color_map[r * stride + c - 1] : -1;
  color_neighbors[1] =
      (c - 1 >= 0 && r - 1 >= 0) ? color_map[(r - 1) * stride + c - 1] : -1;
  color_neighbors[2] = (r - 1 >= 0) ? color_map[(r - 1) * stride + c] : -1;

  for (i = 0; i < PALETTE_MAX_SIZE; ++i) {
    color_order[i] = i;
    inverse_color_order[i] = i;
  }
  memset(scores, 0, PALETTE_MAX_SIZE * sizeof(scores[0]));
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    if (color_neighbors[i] >= 0) {
      scores[color_neighbors[i]] += weights[i];
    }
  }

  // Get the top NUM_PALETTE_NEIGHBORS scores (sorted from large to small).
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    int max = scores[i];
    int max_idx = i;
    int j;
    for (j = i + 1; j < palette_size; ++j) {
      if (scores[j] > max) {
        max = scores[j];
        max_idx = j;
      }
    }
    if (max_idx != i) {
      // Move the score at index 'max_idx' to index 'i', and shift the scores
      // from 'i' to 'max_idx - 1' by 1.
      const int max_score = scores[max_idx];
      const uint8_t max_color_order = color_order[max_idx];
      int k;
      for (k = max_idx; k > i; --k) {
        scores[k] = scores[k - 1];
        color_order[k] = color_order[k - 1];
        inverse_color_order[color_order[k]] = k;
      }
      scores[i] = max_score;
      color_order[i] = max_color_order;
      inverse_color_order[color_order[i]] = i;
    }
  }

  // Get hash value of context.
  color_index_ctx_hash = 0;
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    color_index_ctx_hash += scores[i] * hash_multipliers[i];
  }
  assert(color_index_ctx_hash > 0);
  assert(color_index_ctx_hash <= MAX_COLOR_CONTEXT_HASH);

  // Lookup context from hash.
  color_index_ctx = palette_color_index_context_lookup[color_index_ctx_hash];
  assert(color_index_ctx >= 0);
  assert(color_index_ctx < PALETTE_COLOR_INDEX_CONTEXTS);

  if (color_idx != NULL) {
    *color_idx = inverse_color_order[color_map[r * stride + c]];
  }
  return color_index_ctx;
}
#undef NUM_PALETTE_NEIGHBORS
#undef MAX_COLOR_CONTEXT_HASH

#endif  // CONFIG_PALETTE

#if CONFIG_VAR_TX
static const aom_prob default_txfm_partition_probs[TXFM_PARTITION_CONTEXTS] = {
  250, 231, 212, 241, 166, 66, 241, 230, 135, 243, 154, 64, 248, 161, 63, 128,
};
#endif

static const aom_prob default_skip_probs[SKIP_CONTEXTS] = { 192, 128, 64 };

#if CONFIG_DUAL_FILTER
static const aom_prob default_switchable_interp_prob
    [SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS - 1] = {
      { 235, 192, 128 }, { 36, 243, 48 },   { 34, 16, 128 },
      { 34, 16, 128 },   { 149, 160, 128 }, { 235, 192, 128 },
      { 36, 243, 48 },   { 34, 16, 128 },   { 34, 16, 128 },
      { 149, 160, 128 }, { 235, 192, 128 }, { 36, 243, 48 },
      { 34, 16, 128 },   { 34, 16, 128 },   { 149, 160, 128 },
      { 235, 192, 128 }, { 36, 243, 48 },   { 34, 16, 128 },
      { 34, 16, 128 },   { 149, 160, 128 },
    };
#else   // CONFIG_DUAL_FILTER
static const aom_prob default_switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS]
                                                    [SWITCHABLE_FILTERS - 1] = {
                                                      { 235, 162 },
                                                      { 36, 255 },
                                                      { 34, 3 },
                                                      { 149, 144 },
                                                    };
#endif  // CONFIG_DUAL_FILTER

#if CONFIG_EXT_TX
/* clang-format off */
const aom_tree_index av1_ext_tx_inter_tree[EXT_TX_SETS_INTER]
                                           [TREE_SIZE(TX_TYPES)] = {
  { // ToDo(yaowu): remove used entry 0.
    0
  }, {
    -IDTX, 2,
    4, 14,
    6, 8,
    -V_DCT, -H_DCT,
    10, 12,
    -V_ADST, -H_ADST,
    -V_FLIPADST, -H_FLIPADST,
    -DCT_DCT, 16,
    18, 24,
    20, 22,
    -ADST_DCT, -DCT_ADST,
    -FLIPADST_DCT, -DCT_FLIPADST,
    26, 28,
    -ADST_ADST, -FLIPADST_FLIPADST,
    -ADST_FLIPADST, -FLIPADST_ADST
  }, {
    -IDTX, 2,
    4, 6,
    -V_DCT, -H_DCT,
    -DCT_DCT, 8,
    10, 16,
    12, 14,
    -ADST_DCT, -DCT_ADST,
    -FLIPADST_DCT, -DCT_FLIPADST,
    18, 20,
    -ADST_ADST, -FLIPADST_FLIPADST,
    -ADST_FLIPADST, -FLIPADST_ADST
  }, {
    -IDTX, -DCT_DCT,
  }
};

const aom_tree_index av1_ext_tx_intra_tree[EXT_TX_SETS_INTRA]
                                           [TREE_SIZE(TX_TYPES)] = {
  {  // ToDo(yaowu): remove unused entry 0.
    0
  }, {
    -IDTX, 2,
    -DCT_DCT, 4,
    6, 8,
    -V_DCT, -H_DCT,
    -ADST_ADST, 10,
    -ADST_DCT, -DCT_ADST,
  }, {
    -IDTX, 2,
    -DCT_DCT, 4,
    -ADST_ADST, 6,
    -ADST_DCT, -DCT_ADST,
  }
};
/* clang-format on */

static const aom_prob
    default_inter_ext_tx_prob[EXT_TX_SETS_INTER][EXT_TX_SIZES][TX_TYPES - 1] = {
      {
// ToDo(yaowu): remove unused entry 0.
#if CONFIG_CB4X4
          { 0 },
#endif
          { 0 },
          { 0 },
          { 0 },
          { 0 },
      },
      {
#if CONFIG_CB4X4
          { 0 },
#endif
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
          { 10, 24, 30, 128, 128, 128, 128, 112, 160, 128, 128, 128, 128, 128,
            128 },
      },
      {
#if CONFIG_CB4X4
          { 0 },
#endif
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
          { 10, 30, 128, 112, 160, 128, 128, 128, 128, 128, 128 },
      },
      {
#if CONFIG_CB4X4
          { 0 },
#endif
          { 12 },
          { 12 },
          { 12 },
          { 12 },
      }
    };

// TODO(urvang): 3rd context should be tx_type instead of intra mode just like
// the baseline.
static const aom_prob
    default_intra_ext_tx_prob[EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
                             [TX_TYPES - 1] = {
                               {
// ToDo(yaowu): remove unused entry 0.
#if CONFIG_CB4X4
                                   {
                                       { 0 },
                                   },
#endif
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
                                       { 0 },
#if CONFIG_ALT_INTRA
                                       { 0 },
#endif  // CONFIG_ALT_INTRA
                                   },
                               },
                               {
#if CONFIG_CB4X4
                                   {
                                       { 0 },
                                   },
#endif
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 8, 224, 32, 128, 64, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 9, 200, 32, 128, 64, 128 },
                                       { 8, 8, 32, 128, 224, 128 },
                                       { 10, 32, 32, 128, 16, 192 },
                                       { 10, 32, 32, 128, 16, 64 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 23, 32, 128, 80, 176 },
                                       { 10, 32, 32, 128, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 32, 128, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                               },
                               {
#if CONFIG_CB4X4
                                   {
                                       { 0 },
                                   },
#endif
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
                                       { 10, 32, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
                                       { 10, 32, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
                                       { 10, 32, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                                   {
                                       { 8, 224, 64, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 9, 200, 64, 128 },
                                       { 8, 8, 224, 128 },
                                       { 10, 32, 16, 192 },
                                       { 10, 32, 16, 64 },
                                       { 10, 23, 80, 176 },
                                       { 10, 23, 80, 176 },
                                       { 10, 32, 16, 64 },
#if CONFIG_ALT_INTRA
                                       { 10, 32, 16, 64 },
#endif  // CONFIG_ALT_INTRA
                                   },
                               },
                             };
#else

/* clang-format off */
const aom_tree_index av1_ext_tx_tree[TREE_SIZE(TX_TYPES)] = {
  -DCT_DCT, 2,
  -ADST_ADST, 4,
  -ADST_DCT, -DCT_ADST
};
/* clang-format on */

int av1_ext_tx_ind[TX_TYPES];
int av1_ext_tx_inv[TX_TYPES];

static const aom_prob
    default_intra_ext_tx_prob[EXT_TX_SIZES][TX_TYPES][TX_TYPES - 1] = {
#if CONFIG_CB4X4
      { { 240, 85, 128 }, { 4, 1, 248 }, { 4, 1, 8 }, { 4, 248, 128 } },
#endif
      { { 240, 85, 128 }, { 4, 1, 248 }, { 4, 1, 8 }, { 4, 248, 128 } },
      { { 244, 85, 128 }, { 8, 2, 248 }, { 8, 2, 8 }, { 8, 248, 128 } },
      { { 248, 85, 128 }, { 16, 4, 248 }, { 16, 4, 8 }, { 16, 248, 128 } },
    };

static const aom_prob default_inter_ext_tx_prob[EXT_TX_SIZES][TX_TYPES - 1] = {
#if CONFIG_CB4X4
  { 160, 85, 128 },
#endif
  { 160, 85, 128 },
  { 176, 85, 128 },
  { 192, 85, 128 },
};
#endif  // CONFIG_EXT_TX

#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
static const aom_prob
    default_intra_filter_probs[INTRA_FILTERS + 1][INTRA_FILTERS - 1] = {
      { 98, 63, 60 }, { 98, 82, 80 }, { 94, 65, 103 },
      { 49, 25, 24 }, { 72, 38, 50 },
    };
const aom_tree_index av1_intra_filter_tree[TREE_SIZE(INTRA_FILTERS)] = {
  -INTRA_FILTER_LINEAR,      2, -INTRA_FILTER_8TAP, 4, -INTRA_FILTER_8TAP_SHARP,
  -INTRA_FILTER_8TAP_SMOOTH,
};
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA

#if CONFIG_FILTER_INTRA
static const aom_prob default_filter_intra_probs[2] = { 230, 230 };
#endif  // CONFIG_FILTER_INTRA

#if CONFIG_SUPERTX
static const aom_prob
    default_supertx_prob[PARTITION_SUPERTX_CONTEXTS][TX_SIZES] = {
#if CONFIG_CB4X4
#if CONFIG_TX64X64
      { 1, 1, 160, 160, 170, 180 }, { 1, 1, 200, 200, 210, 220 },
#else
      { 1, 1, 160, 160, 170 }, { 1, 1, 200, 200, 210 },
#endif  // CONFIG_TX64X64
#else
#if CONFIG_TX64X64
      { 1, 160, 160, 170, 180 }, { 1, 200, 200, 210, 220 },
#else
      { 1, 160, 160, 170 }, { 1, 200, 200, 210 },
#endif  // CONFIG_CB4X4
#endif  // CONFIG_TX64X64
    };
#endif  // CONFIG_SUPERTX

// FIXME(someone) need real defaults here
static const aom_prob default_segment_tree_probs[SEG_TREE_PROBS] = {
  128, 128, 128, 128, 128, 128, 128
};
// clang-format off
static const aom_prob default_segment_pred_probs[PREDICTION_PROBS] = {
  128, 128, 128
};
// clang-format on

#if CONFIG_EC_MULTISYMBOL
static const aom_cdf_prob
    default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS]
                                 [CDF_SIZE(SWITCHABLE_FILTERS)] = {
                                   { 30080, 31781, 32768, 0 },
                                   { 4608, 32658, 32768, 0 },
                                   { 4352, 4685, 32768, 0 },
                                   { 19072, 26776, 32768, 0 },
                                 };

static const aom_cdf_prob default_seg_tree_cdf[CDF_SIZE(MAX_SEGMENTS)] = {
  4096, 8192, 12288, 16384, 20480, 24576, 28672, 32768, 0
};

static const aom_cdf_prob
    default_tx_size_cdf[MAX_TX_DEPTH][TX_SIZE_CONTEXTS][CDF_SIZE(MAX_TX_DEPTH +
                                                                 1)] = {
      { { 12800, 32768, 0 }, { 8448, 32768, 0 } },
      { { 2560, 20496, 32768, 0 }, { 1920, 14091, 32768, 0 } },
      { { 384, 17588, 19782, 32768, 0 }, { 640, 7166, 8466, 32768, 0 } },
#if CONFIG_TX64X64
      { { 128, 8288, 21293, 26986, 32768, 0 },
        { 128, 4208, 10009, 15965, 32768, 0 } },
#endif
    };

static const aom_cdf_prob default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
    INTRA_MODES)] = {
  { 8320, 11376, 12880, 19959, 23072, 24067, 25461, 26917, 29157, 32768, 0 },
  { 16896, 21112, 21932, 27852, 28667, 28916, 29593, 30089, 30905, 32768, 0 },
  { 22144, 25464, 26006, 30364, 30583, 30655, 31183, 31400, 31646, 32768, 0 },
  { 28288, 30650, 30964, 32288, 32308, 32331, 32495, 32586, 32607, 32768, 0 },
};

static const aom_cdf_prob default_uv_mode_cdf[INTRA_MODES][CDF_SIZE(
    INTRA_MODES)] = {
  { 15360, 15836, 20863, 27513, 28269, 29048, 29455, 30154, 31206, 32768, 0 },
  { 6144, 7392, 22657, 25981, 26965, 28779, 29309, 30890, 31763, 32768, 0 },
  { 8576, 9143, 11450, 27575, 28108, 28438, 28658, 28995, 30410, 32768, 0 },
  { 12416, 12814, 16244, 22057, 23492, 24700, 26213, 27954, 29778, 32768, 0 },
  { 10624, 11057, 14619, 19415, 23134, 25679, 26399, 27618, 30676, 32768, 0 },
  { 10240, 10680, 15684, 19118, 21856, 27563, 28234, 29332, 31278, 32768, 0 },
  { 11008, 11433, 14100, 22522, 24365, 25330, 25737, 26341, 30433, 32768, 0 },
  { 10880, 11308, 13991, 23645, 24679, 25433, 25977, 26746, 28463, 32768, 0 },
  { 9856, 10483, 16054, 19959, 21708, 23628, 24949, 28797, 30658, 32768, 0 },
  { 12928, 14556, 22168, 27789, 28543, 29663, 29893, 30645, 31682, 32768, 0 },
};

static const aom_cdf_prob
    default_partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(PARTITION_TYPES)] = {
      { 25472, 28949, 31052, 32768, 0 }, { 18816, 22250, 28783, 32768, 0 },
      { 18944, 26126, 29188, 32768, 0 }, { 15488, 22508, 27077, 32768, 0 },
      { 22272, 25265, 27815, 32768, 0 }, { 11776, 15138, 20854, 32768, 0 },
      { 10496, 19109, 21777, 32768, 0 }, { 6784, 10743, 14098, 32768, 0 },
      { 22656, 24947, 26749, 32768, 0 }, { 8704, 11148, 16469, 32768, 0 },
      { 6656, 14714, 16477, 32768, 0 },  { 2176, 3849, 5205, 32768, 0 },
      { 28416, 28994, 29436, 32768, 0 }, { 9216, 10688, 14483, 32768, 0 },
      { 7424, 10592, 11632, 32768, 0 },  { 1280, 2141, 2859, 32768, 0 },
    };

static const aom_cdf_prob
    default_inter_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(INTER_MODES)] = {
      { 256, 22227, 23627, 32768, 0 }, { 896, 18948, 23537, 32768, 0 },
      { 896, 21563, 24320, 32768, 0 }, { 896, 12599, 17799, 32768, 0 },
      { 1024, 8960, 13238, 32768, 0 }, { 2176, 11856, 14388, 32768, 0 },
      { 3200, 6550, 9622, 32768, 0 },
    };

#if !CONFIG_EXT_TX
static const aom_cdf_prob
    default_intra_ext_tx_cdf[EXT_TX_SIZES][TX_TYPES][CDF_SIZE(TX_TYPES)] = {
      { { 30720, 31400, 32084, 32768, 0 },
        { 512, 638, 31764, 32768, 0 },
        { 512, 638, 1642, 32768, 0 },
        { 512, 31760, 32264, 32768, 0 } },
      { { 31232, 31742, 32255, 32768, 0 },
        { 1024, 1272, 31784, 32768, 0 },
        { 1024, 1272, 2256, 32768, 0 },
        { 1024, 31776, 32272, 32768, 0 } },
      { { 31744, 32084, 32426, 32768, 0 },
        { 2048, 2528, 31823, 32768, 0 },
        { 2048, 2528, 3473, 32768, 0 },
        { 2048, 31808, 32288, 32768, 0 } },
    };

static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SIZES][CDF_SIZE(TX_TYPES)] = {
      { 20480, 24560, 28664, 32768, 0 },
      { 22528, 25928, 29348, 32768, 0 },
      { 24576, 27296, 30032, 32768, 0 },
    };
#endif

// CDF version of 'av1_kf_y_mode_prob'.
const aom_cdf_prob av1_kf_y_mode_cdf[INTRA_MODES][INTRA_MODES][CDF_SIZE(
    INTRA_MODES)] = {
  { { 17536, 19321, 21527, 25360, 27516, 28026, 29323, 30023, 30999, 32768, 0 },
    { 11776, 15466, 22360, 24865, 26991, 27889, 29299, 30519, 31398, 32768, 0 },
    { 9344, 12272, 13793, 25813, 27359, 27654, 28573, 29130, 30551, 32768, 0 },
    { 11648, 14123, 16454, 19948, 22780, 23846, 27087, 28995, 30380, 32768, 0 },
    { 9216, 12436, 15295, 17996, 24006, 25465, 27405, 28725, 30383, 32768, 0 },
    { 9344, 12181, 14433, 16634, 20355, 24317, 26133, 29295, 31344, 32768, 0 },
    { 8576, 10750, 12556, 17996, 22315, 23609, 25040, 26157, 30573, 32768, 0 },
    { 11008, 13303, 15432, 20646, 23506, 24100, 25624, 26824, 28055, 32768, 0 },
    { 9472, 12384, 14534, 17094, 20257, 22155, 24767, 28955, 30474, 32768, 0 },
    { 7552, 14152, 17352, 22654, 25123, 25783, 27911, 29182, 30849, 32768,
      0 } },
  { { 8064, 11538, 21987, 24941, 26913, 28136, 29222, 30469, 31331, 32768, 0 },
    { 5504, 10403, 25080, 26762, 27933, 29104, 30092, 31576, 32004, 32768, 0 },
    { 5632, 8706, 15097, 23714, 25344, 26072, 27380, 28580, 29840, 32768, 0 },
    { 7424, 11186, 17593, 20154, 22974, 24351, 26916, 29956, 30967, 32768, 0 },
    { 5888, 10193, 16895, 19031, 23735, 25576, 27514, 29813, 30471, 32768, 0 },
    { 4864, 8352, 16459, 18062, 21263, 25378, 26937, 30376, 31619, 32768, 0 },
    { 4992, 7922, 13842, 18004, 21779, 23527, 25115, 27357, 30232, 32768, 0 },
    { 6656, 9716, 16379, 20053, 22487, 23613, 25437, 27270, 28516, 32768, 0 },
    { 6016, 9674, 16891, 18684, 21147, 23093, 25512, 30132, 30894, 32768, 0 },
    { 4608, 11318, 21038, 23650, 25303, 26262, 28295, 30479, 31212, 32768,
      0 } },
  { { 10496, 12758, 14790, 24547, 26342, 26799, 27825, 28443, 30217, 32768, 0 },
    { 7040, 11462, 17121, 24215, 26504, 27267, 28492, 29444, 30846, 32768, 0 },
    { 5376, 8158, 9215, 26451, 27407, 27524, 27995, 28275, 29767, 32768, 0 },
    { 8704, 12652, 14145, 20101, 22879, 23675, 25629, 27079, 28923, 32768, 0 },
    { 7424, 12374, 14366, 18855, 23842, 24358, 25639, 27087, 29706, 32768, 0 },
    { 6400, 10005, 12939, 17753, 22206, 24790, 26785, 28164, 30520, 32768, 0 },
    { 5632, 8176, 9713, 19053, 22343, 23222, 24453, 25070, 29761, 32768, 0 },
    { 7040, 9754, 10833, 21229, 23540, 23943, 24839, 25675, 27033, 32768, 0 },
    { 6784, 11758, 13481, 17236, 20210, 21768, 24303, 26948, 28676, 32768, 0 },
    { 4864, 12712, 14201, 23863, 25952, 26386, 27632, 28635, 30362, 32768,
      0 } },
  { { 13184, 15173, 17647, 21576, 24474, 25267, 27699, 29283, 30549, 32768, 0 },
    { 7552, 11295, 18257, 20811, 23213, 24606, 27731, 30407, 31237, 32768, 0 },
    { 7936, 10846, 12816, 22436, 24614, 25130, 26890, 28199, 29091, 32768, 0 },
    { 8576, 11411, 13830, 15918, 18996, 20044, 25114, 27835, 28972, 32768, 0 },
    { 7680, 10816, 13646, 15966, 21162, 22012, 24701, 27506, 29644, 32768, 0 },
    { 6784, 9423, 12524, 14773, 19447, 22804, 26073, 29211, 30642, 32768, 0 },
    { 6784, 8916, 11059, 15861, 21174, 22338, 24620, 27071, 30899, 32768, 0 },
    { 9856, 11557, 13960, 18525, 21788, 22189, 24462, 26603, 27470, 32768, 0 },
    { 7808, 10636, 13143, 15844, 18698, 20272, 24323, 30096, 31787, 32768, 0 },
    { 6016, 10928, 14596, 18926, 21586, 22688, 26626, 29001, 30399, 32768,
      0 } },
  { { 8832, 10983, 13451, 16582, 21656, 23109, 24845, 26207, 28796, 32768, 0 },
    { 6784, 10844, 15554, 18073, 22954, 24901, 26776, 28649, 30419, 32768, 0 },
    { 5120, 8252, 10072, 20108, 23535, 24346, 25761, 26418, 28675, 32768, 0 },
    { 7680, 11012, 12627, 14595, 19462, 20888, 23348, 25703, 28159, 32768, 0 },
    { 6656, 9818, 11790, 13813, 22731, 24737, 26557, 28061, 29697, 32768, 0 },
    { 5632, 8918, 11620, 13802, 19950, 23764, 25734, 28537, 31809, 32768, 0 },
    { 4480, 6580, 7808, 12281, 19375, 20970, 22860, 24602, 29929, 32768, 0 },
    { 7040, 9553, 11457, 15102, 20291, 21280, 22985, 24475, 26613, 32768, 0 },
    { 6528, 10423, 12605, 14621, 19031, 21505, 24585, 27558, 29532, 32768, 0 },
    { 6016, 11659, 14463, 18867, 23653, 24903, 27115, 29389, 31382, 32768,
      0 } },
  { { 8192, 10016, 13304, 16362, 21107, 25165, 26620, 28901, 30910, 32768, 0 },
    { 5888, 8723, 16237, 18318, 22002, 25923, 27394, 29934, 31428, 32768, 0 },
    { 4608, 7138, 9841, 18442, 22447, 24618, 26337, 27945, 30168, 32768, 0 },
    { 6784, 8916, 12270, 14851, 19886, 22759, 25105, 28368, 29760, 32768, 0 },
    { 5120, 7928, 11324, 13340, 21205, 24224, 25926, 28518, 30560, 32768, 0 },
    { 4480, 6580, 10058, 11237, 16807, 25937, 27218, 30015, 31348, 32768, 0 },
    { 4608, 6808, 9445, 12446, 18461, 21835, 23244, 26109, 30115, 32768, 0 },
    { 5760, 7659, 10798, 14720, 19157, 21955, 23645, 26460, 28702, 32768, 0 },
    { 5760, 8503, 11157, 13071, 17594, 22047, 24099, 29077, 30850, 32768, 0 },
    { 4864, 9660, 14264, 17105, 21528, 24094, 26025, 28580, 30559, 32768, 0 } },
  { { 9600, 11139, 12998, 18660, 22158, 23501, 24659, 25736, 30296, 32768, 0 },
    { 7168, 11068, 15984, 19969, 23169, 24704, 26216, 27572, 31368, 32768, 0 },
    { 4480, 6801, 8018, 20908, 23071, 23583, 24301, 25062, 29427, 32768, 0 },
    { 7168, 10068, 11753, 15843, 19742, 21358, 23809, 26189, 29067, 32768, 0 },
    { 6016, 9047, 10622, 13931, 22462, 23858, 25911, 27277, 29722, 32768, 0 },
    { 5888, 7568, 9931, 13533, 18431, 22063, 23777, 26025, 30555, 32768, 0 },
    { 4352, 6239, 7379, 13739, 16917, 18090, 18835, 19651, 30360, 32768, 0 },
    { 6528, 8988, 10288, 15534, 19495, 20386, 21934, 23034, 26988, 32768, 0 },
    { 7040, 10055, 11652, 14757, 19622, 21715, 23615, 26761, 29483, 32768, 0 },
    { 4736, 10102, 12315, 19078, 21348, 22621, 24246, 26044, 29931, 32768,
      0 } },
  { { 10496, 12410, 14955, 19891, 23137, 23792, 25159, 26378, 28125, 32768, 0 },
    { 7936, 12204, 17104, 20191, 23468, 24630, 26156, 27628, 28913, 32768, 0 },
    { 6016, 8629, 10232, 23591, 25349, 25637, 26306, 27063, 28980, 32768, 0 },
    { 8704, 12088, 13461, 16646, 20516, 21455, 24062, 26579, 28368, 32768, 0 },
    { 7296, 11177, 13117, 16196, 23378, 24708, 26440, 27997, 29078, 32768, 0 },
    { 6272, 9377, 12575, 15616, 20919, 23697, 26603, 27566, 29903, 32768, 0 },
    { 6528, 9091, 10478, 16445, 21081, 22320, 23871, 25087, 29258, 32768, 0 },
    { 8704, 11148, 12499, 17340, 20656, 21288, 22588, 23701, 24693, 32768, 0 },
    { 7552, 11394, 12980, 15562, 19942, 21792, 25093, 28211, 28959, 32768, 0 },
    { 5120, 11708, 13847, 19377, 22421, 23160, 25449, 27136, 29182, 32768,
      0 } },
  { { 9984, 12031, 15190, 18673, 21422, 22812, 25690, 29118, 30458, 32768, 0 },
    { 6144, 9680, 17436, 19610, 21820, 23485, 26313, 30826, 31843, 32768, 0 },
    { 6016, 8315, 10607, 19333, 21572, 22553, 25266, 27288, 28551, 32768, 0 },
    { 7168, 9668, 12646, 16257, 19648, 20899, 25304, 30465, 31625, 32768, 0 },
    { 6144, 9368, 11836, 14130, 19153, 21157, 24876, 28452, 29396, 32768, 0 },
    { 5504, 8486, 11996, 14412, 17968, 21814, 24424, 30682, 32059, 32768, 0 },
    { 5376, 7195, 9592, 13331, 17569, 19460, 22371, 25458, 28942, 32768, 0 },
    { 7424, 9206, 11783, 16456, 19253, 20390, 23775, 27007, 28425, 32768, 0 },
    { 5888, 8303, 11361, 13440, 15848, 17549, 21532, 29564, 30665, 32768, 0 },
    { 4608, 8788, 13284, 16621, 18983, 20286, 24577, 28960, 30314, 32768, 0 } },
  { { 8320, 15005, 19168, 24282, 26707, 27402, 28681, 29639, 30629, 32768, 0 },
    { 5632, 13900, 22376, 24867, 26804, 27734, 29130, 30722, 31465, 32768, 0 },
    { 4992, 9115, 11055, 24893, 26316, 26661, 27663, 28301, 29418, 32768, 0 },
    { 7424, 12077, 14987, 19596, 22615, 23600, 26465, 28484, 29789, 32768, 0 },
    { 6272, 11447, 14362, 18204, 23418, 24715, 26697, 28547, 29520, 32768, 0 },
    { 5248, 10946, 15379, 18167, 22197, 25432, 27295, 30031, 30576, 32768, 0 },
    { 5120, 9008, 11607, 18210, 22327, 23427, 24887, 26580, 29892, 32768, 0 },
    { 6656, 10124, 12689, 19922, 22480, 22807, 24441, 25579, 26787, 32768, 0 },
    { 5376, 10084, 13983, 17113, 19996, 21614, 24403, 28651, 29938, 32768, 0 },
    { 5504, 14131, 17989, 23324, 25513, 26071, 27850, 29464, 30393, 32768,
      0 } },
};
#endif  // CONFIG_EC_MULTISYMBOL

static void init_mode_probs(FRAME_CONTEXT *fc) {
  av1_copy(fc->uv_mode_prob, default_uv_probs);
  av1_copy(fc->y_mode_prob, default_if_y_probs);
  av1_copy(fc->switchable_interp_prob, default_switchable_interp_prob);
  av1_copy(fc->partition_prob, default_partition_probs);
  av1_copy(fc->intra_inter_prob, default_intra_inter_p);
  av1_copy(fc->comp_inter_prob, default_comp_inter_p);
  av1_copy(fc->comp_ref_prob, default_comp_ref_p);
#if CONFIG_EXT_REFS
  av1_copy(fc->comp_bwdref_prob, default_comp_bwdref_p);
#endif  // CONFIG_EXT_REFS
  av1_copy(fc->single_ref_prob, default_single_ref_p);
  av1_copy(fc->tx_size_probs, default_tx_size_prob);
#if CONFIG_VAR_TX
  av1_copy(fc->txfm_partition_prob, default_txfm_partition_probs);
#endif
  av1_copy(fc->skip_probs, default_skip_probs);
#if CONFIG_REF_MV
  av1_copy(fc->newmv_prob, default_newmv_prob);
  av1_copy(fc->zeromv_prob, default_zeromv_prob);
  av1_copy(fc->refmv_prob, default_refmv_prob);
  av1_copy(fc->drl_prob, default_drl_prob);
#if CONFIG_EXT_INTER
  fc->new2mv_prob = default_new2mv_prob;
#endif  // CONFIG_EXT_INTER
#endif  // CONFIG_REF_MV
  av1_copy(fc->inter_mode_probs, default_inter_mode_probs);
#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  av1_copy(fc->motion_mode_prob, default_motion_mode_prob);
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
  av1_copy(fc->obmc_prob, default_obmc_prob);
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
#if CONFIG_EXT_INTER
  av1_copy(fc->inter_compound_mode_probs, default_inter_compound_mode_probs);
  av1_copy(fc->compound_type_prob, default_compound_type_probs);
  av1_copy(fc->interintra_prob, default_interintra_prob);
  av1_copy(fc->interintra_mode_prob, default_interintra_mode_prob);
  av1_copy(fc->wedge_interintra_prob, default_wedge_interintra_prob);
#endif  // CONFIG_EXT_INTER
#if CONFIG_SUPERTX
  av1_copy(fc->supertx_prob, default_supertx_prob);
#endif  // CONFIG_SUPERTX
  av1_copy(fc->seg.tree_probs, default_segment_tree_probs);
  av1_copy(fc->seg.pred_probs, default_segment_pred_probs);
#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  av1_copy(fc->intra_filter_probs, default_intra_filter_probs);
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  av1_copy(fc->filter_intra_probs, default_filter_intra_probs);
#endif  // CONFIG_FILTER_INTRA
  av1_copy(fc->inter_ext_tx_prob, default_inter_ext_tx_prob);
  av1_copy(fc->intra_ext_tx_prob, default_intra_ext_tx_prob);
#if CONFIG_LOOP_RESTORATION
  av1_copy(fc->switchable_restore_prob, default_switchable_restore_prob);
#endif  // CONFIG_LOOP_RESTORATION
#if CONFIG_EC_MULTISYMBOL
  av1_copy(fc->y_mode_cdf, default_if_y_mode_cdf);
  av1_copy(fc->uv_mode_cdf, default_uv_mode_cdf);
  av1_copy(fc->switchable_interp_cdf, default_switchable_interp_cdf);
  av1_copy(fc->partition_cdf, default_partition_cdf);
  av1_copy(fc->inter_mode_cdf, default_inter_mode_cdf);
#if !CONFIG_EXT_TX
  av1_copy(fc->intra_ext_tx_cdf, default_intra_ext_tx_cdf);
  av1_copy(fc->inter_ext_tx_cdf, default_inter_ext_tx_cdf);
#endif
  av1_copy(fc->seg.tree_cdf, default_seg_tree_cdf);
  av1_copy(fc->tx_size_cdf, default_tx_size_cdf);
#endif
#if CONFIG_DELTA_Q
  av1_copy(fc->delta_q_prob, default_delta_q_probs);
#if CONFIG_EC_MULTISYMBOL
  av1_copy(fc->delta_q_cdf, default_delta_q_cdf);
#endif
#endif
}

#if CONFIG_EC_MULTISYMBOL
int av1_switchable_interp_ind[SWITCHABLE_FILTERS];
int av1_switchable_interp_inv[SWITCHABLE_FILTERS];

void av1_set_mode_cdfs(struct AV1Common *cm) {
  FRAME_CONTEXT *fc = cm->fc;
  int i, j;
  if (cm->seg.enabled && cm->seg.update_map) {
    av1_tree_to_cdf(av1_segment_tree, cm->fc->seg.tree_probs,
                    cm->fc->seg.tree_cdf);
  }

  for (i = 0; i < INTRA_MODES; ++i)
    av1_tree_to_cdf(av1_intra_mode_tree, fc->uv_mode_prob[i],
                    fc->uv_mode_cdf[i]);

  for (i = 0; i < PARTITION_CONTEXTS; ++i)
    av1_tree_to_cdf(av1_partition_tree, fc->partition_prob[i],
                    fc->partition_cdf[i]);

  for (i = 0; i < INTRA_MODES; ++i)
    for (j = 0; j < INTRA_MODES; ++j)
      av1_tree_to_cdf(av1_intra_mode_tree, cm->kf_y_prob[i][j],
                      cm->fc->kf_y_cdf[i][j]);

  for (j = 0; j < SWITCHABLE_FILTER_CONTEXTS; ++j)
    av1_tree_to_cdf(av1_switchable_interp_tree, fc->switchable_interp_prob[j],
                    fc->switchable_interp_cdf[j]);

  for (i = 0; i < INTER_MODE_CONTEXTS; ++i)
    av1_tree_to_cdf(av1_inter_mode_tree, fc->inter_mode_probs[i],
                    fc->inter_mode_cdf[i]);

  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i)
    av1_tree_to_cdf(av1_intra_mode_tree, fc->y_mode_prob[i], fc->y_mode_cdf[i]);

#if !CONFIG_EXT_TX
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i)
    for (j = 0; j < TX_TYPES; ++j)
      av1_tree_to_cdf(av1_ext_tx_tree, fc->intra_ext_tx_prob[i][j],
                      fc->intra_ext_tx_cdf[i][j]);

  for (i = TX_4X4; i < EXT_TX_SIZES; ++i)
    av1_tree_to_cdf(av1_ext_tx_tree, fc->inter_ext_tx_prob[i],
                    fc->inter_ext_tx_cdf[i]);
#endif
  for (i = 0; i < MAX_TX_DEPTH; i++) {
    for (j = 0; j < TX_SIZE_CONTEXTS; j++) {
      av1_tree_to_cdf(av1_tx_size_tree[i], fc->tx_size_probs[i][j],
                      fc->tx_size_cdf[i][j]);
    }
  }
}
#endif

#if CONFIG_DUAL_FILTER
const aom_tree_index av1_switchable_interp_tree[TREE_SIZE(SWITCHABLE_FILTERS)] =
    {
      -EIGHTTAP_REGULAR, 2, 4, -MULTITAP_SHARP, -EIGHTTAP_SMOOTH,
      -EIGHTTAP_SMOOTH2,
    };
#else
const aom_tree_index av1_switchable_interp_tree[TREE_SIZE(SWITCHABLE_FILTERS)] =
    { -EIGHTTAP_REGULAR, 2, -EIGHTTAP_SMOOTH, -MULTITAP_SHARP };
#endif  // CONFIG_DUAL_FILTER

void av1_adapt_inter_frame_probs(AV1_COMMON *cm) {
  int i, j;
  FRAME_CONTEXT *fc = cm->fc;
  const FRAME_CONTEXT *pre_fc = &cm->frame_contexts[cm->frame_context_idx];
  const FRAME_COUNTS *counts = &cm->counts;

  for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
    fc->intra_inter_prob[i] = av1_mode_mv_merge_probs(
        pre_fc->intra_inter_prob[i], counts->intra_inter[i]);
  for (i = 0; i < COMP_INTER_CONTEXTS; i++)
    fc->comp_inter_prob[i] = av1_mode_mv_merge_probs(pre_fc->comp_inter_prob[i],
                                                     counts->comp_inter[i]);

#if CONFIG_EXT_REFS
  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (FWD_REFS - 1); j++)
      fc->comp_ref_prob[i][j] = mode_mv_merge_probs(pre_fc->comp_ref_prob[i][j],
                                                    counts->comp_ref[i][j]);
  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (BWD_REFS - 1); j++)
      fc->comp_bwdref_prob[i][j] = mode_mv_merge_probs(
          pre_fc->comp_bwdref_prob[i][j], counts->comp_bwdref[i][j]);
#else
  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (COMP_REFS - 1); j++)
      fc->comp_ref_prob[i][j] = mode_mv_merge_probs(pre_fc->comp_ref_prob[i][j],
                                                    counts->comp_ref[i][j]);
#endif  // CONFIG_EXT_REFS

  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < (SINGLE_REFS - 1); j++)
      fc->single_ref_prob[i][j] = av1_mode_mv_merge_probs(
          pre_fc->single_ref_prob[i][j], counts->single_ref[i][j]);

#if CONFIG_REF_MV
  for (i = 0; i < NEWMV_MODE_CONTEXTS; ++i)
    fc->newmv_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->newmv_prob[i], counts->newmv_mode[i]);
  for (i = 0; i < ZEROMV_MODE_CONTEXTS; ++i)
    fc->zeromv_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->zeromv_prob[i], counts->zeromv_mode[i]);
  for (i = 0; i < REFMV_MODE_CONTEXTS; ++i)
    fc->refmv_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->refmv_prob[i], counts->refmv_mode[i]);

  for (i = 0; i < DRL_MODE_CONTEXTS; ++i)
    fc->drl_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->drl_prob[i], counts->drl_mode[i]);
#if CONFIG_EXT_INTER
  fc->new2mv_prob =
      av1_mode_mv_merge_probs(pre_fc->new2mv_prob, counts->new2mv_mode);
#endif  // CONFIG_EXT_INTER
#else
  for (i = 0; i < INTER_MODE_CONTEXTS; i++)
    aom_tree_merge_probs(av1_inter_mode_tree, pre_fc->inter_mode_probs[i],
                         counts->inter_mode[i], fc->inter_mode_probs[i]);
#endif

#if CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION
  for (i = BLOCK_8X8; i < BLOCK_SIZES; ++i)
    aom_tree_merge_probs(av1_motion_mode_tree, pre_fc->motion_mode_prob[i],
                         counts->motion_mode[i], fc->motion_mode_prob[i]);
#if CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
  for (i = BLOCK_8X8; i < BLOCK_SIZES; ++i)
    fc->obmc_prob[i] =
        av1_mode_mv_merge_probs(pre_fc->obmc_prob[i], counts->obmc[i]);
#endif  // CONFIG_MOTION_VAR && CONFIG_WARPED_MOTION
#endif  // CONFIG_MOTION_VAR || CONFIG_WARPED_MOTION

#if CONFIG_SUPERTX
  for (i = 0; i < PARTITION_SUPERTX_CONTEXTS; ++i) {
    for (j = TX_8X8; j < TX_SIZES; ++j) {
      fc->supertx_prob[i][j] = av1_mode_mv_merge_probs(
          pre_fc->supertx_prob[i][j], counts->supertx[i][j]);
    }
  }
#endif  // CONFIG_SUPERTX

#if CONFIG_EXT_INTER
  for (i = 0; i < INTER_MODE_CONTEXTS; i++)
    aom_tree_merge_probs(
        av1_inter_compound_mode_tree, pre_fc->inter_compound_mode_probs[i],
        counts->inter_compound_mode[i], fc->inter_compound_mode_probs[i]);
  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
    if (is_interintra_allowed_bsize_group(i))
      fc->interintra_prob[i] = av1_mode_mv_merge_probs(
          pre_fc->interintra_prob[i], counts->interintra[i]);
  }
  for (i = 0; i < BLOCK_SIZE_GROUPS; i++) {
    aom_tree_merge_probs(
        av1_interintra_mode_tree, pre_fc->interintra_mode_prob[i],
        counts->interintra_mode[i], fc->interintra_mode_prob[i]);
  }
  for (i = 0; i < BLOCK_SIZES; ++i) {
    if (is_interintra_allowed_bsize(i) && is_interintra_wedge_used(i))
      fc->wedge_interintra_prob[i] = av1_mode_mv_merge_probs(
          pre_fc->wedge_interintra_prob[i], counts->wedge_interintra[i]);
  }

  for (i = 0; i < BLOCK_SIZES; ++i) {
    aom_tree_merge_probs(av1_compound_type_tree, pre_fc->compound_type_prob[i],
                         counts->compound_interinter[i],
                         fc->compound_type_prob[i]);
  }
#endif  // CONFIG_EXT_INTER

  for (i = 0; i < BLOCK_SIZE_GROUPS; i++)
    aom_tree_merge_probs(av1_intra_mode_tree, pre_fc->y_mode_prob[i],
                         counts->y_mode[i], fc->y_mode_prob[i]);

  if (cm->interp_filter == SWITCHABLE) {
    for (i = 0; i < SWITCHABLE_FILTER_CONTEXTS; i++)
      aom_tree_merge_probs(
          av1_switchable_interp_tree, pre_fc->switchable_interp_prob[i],
          counts->switchable_interp[i], fc->switchable_interp_prob[i]);
  }
}

void av1_adapt_intra_frame_probs(AV1_COMMON *cm) {
  int i, j;
  FRAME_CONTEXT *fc = cm->fc;
  const FRAME_CONTEXT *pre_fc = &cm->frame_contexts[cm->frame_context_idx];
  const FRAME_COUNTS *counts = &cm->counts;

  if (cm->tx_mode == TX_MODE_SELECT) {
    for (i = 0; i < MAX_TX_DEPTH; ++i) {
      for (j = 0; j < TX_SIZE_CONTEXTS; ++j)
        aom_tree_merge_probs(av1_tx_size_tree[i], pre_fc->tx_size_probs[i][j],
                             counts->tx_size[i][j], fc->tx_size_probs[i][j]);
    }
  }

#if CONFIG_VAR_TX
  if (cm->tx_mode == TX_MODE_SELECT) {
    for (i = 0; i < TXFM_PARTITION_CONTEXTS; ++i)
      fc->txfm_partition_prob[i] = av1_mode_mv_merge_probs(
          pre_fc->txfm_partition_prob[i], counts->txfm_partition[i]);
  }
#endif

  for (i = 0; i < SKIP_CONTEXTS; ++i)
    fc->skip_probs[i] =
        av1_mode_mv_merge_probs(pre_fc->skip_probs[i], counts->skip[i]);

#if CONFIG_EXT_TX
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    int s;
    for (s = 1; s < EXT_TX_SETS_INTER; ++s) {
      if (use_inter_ext_tx_for_txsize[s][i]) {
        aom_tree_merge_probs(
            av1_ext_tx_inter_tree[s], pre_fc->inter_ext_tx_prob[s][i],
            counts->inter_ext_tx[s][i], fc->inter_ext_tx_prob[s][i]);
      }
    }
    for (s = 1; s < EXT_TX_SETS_INTRA; ++s) {
      if (use_intra_ext_tx_for_txsize[s][i]) {
        for (j = 0; j < INTRA_MODES; ++j)
          aom_tree_merge_probs(
              av1_ext_tx_intra_tree[s], pre_fc->intra_ext_tx_prob[s][i][j],
              counts->intra_ext_tx[s][i][j], fc->intra_ext_tx_prob[s][i][j]);
      }
    }
  }
#else
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    for (j = 0; j < TX_TYPES; ++j) {
      aom_tree_merge_probs(av1_ext_tx_tree, pre_fc->intra_ext_tx_prob[i][j],
                           counts->intra_ext_tx[i][j],
                           fc->intra_ext_tx_prob[i][j]);
    }
  }
  for (i = TX_4X4; i < EXT_TX_SIZES; ++i) {
    aom_tree_merge_probs(av1_ext_tx_tree, pre_fc->inter_ext_tx_prob[i],
                         counts->inter_ext_tx[i], fc->inter_ext_tx_prob[i]);
  }
#endif  // CONFIG_EXT_TX

  if (cm->seg.temporal_update) {
    for (i = 0; i < PREDICTION_PROBS; i++)
      fc->seg.pred_probs[i] = av1_mode_mv_merge_probs(pre_fc->seg.pred_probs[i],
                                                      counts->seg.pred[i]);

    aom_tree_merge_probs(av1_segment_tree, pre_fc->seg.tree_probs,
                         counts->seg.tree_mispred, fc->seg.tree_probs);
  } else {
    aom_tree_merge_probs(av1_segment_tree, pre_fc->seg.tree_probs,
                         counts->seg.tree_total, fc->seg.tree_probs);
  }

  for (i = 0; i < INTRA_MODES; ++i)
    aom_tree_merge_probs(av1_intra_mode_tree, pre_fc->uv_mode_prob[i],
                         counts->uv_mode[i], fc->uv_mode_prob[i]);

#if CONFIG_EXT_PARTITION_TYPES
  aom_tree_merge_probs(av1_partition_tree, pre_fc->partition_prob[0],
                       counts->partition[0], fc->partition_prob[0]);
  for (i = 1; i < PARTITION_CONTEXTS_PRIMARY; i++)
    aom_tree_merge_probs(av1_ext_partition_tree, pre_fc->partition_prob[i],
                         counts->partition[i], fc->partition_prob[i]);
#else
  for (i = 0; i < PARTITION_CONTEXTS_PRIMARY; i++) {
    aom_tree_merge_probs(av1_partition_tree, pre_fc->partition_prob[i],
                         counts->partition[i], fc->partition_prob[i]);
  }
#endif  // CONFIG_EXT_PARTITION_TYPES
#if CONFIG_UNPOISON_PARTITION_CTX
  for (i = PARTITION_CONTEXTS_PRIMARY;
       i < PARTITION_CONTEXTS_PRIMARY + PARTITION_BLOCK_SIZES; ++i) {
    unsigned int ct[2] = { counts->partition[i][PARTITION_VERT],
                           counts->partition[i][PARTITION_SPLIT] };
    assert(counts->partition[i][PARTITION_NONE] == 0);
    assert(counts->partition[i][PARTITION_HORZ] == 0);
    assert(fc->partition_prob[i][PARTITION_NONE] == 0);
    assert(fc->partition_prob[i][PARTITION_HORZ] == 0);
    fc->partition_prob[i][PARTITION_VERT] =
        av1_mode_mv_merge_probs(pre_fc->partition_prob[i][PARTITION_VERT], ct);
  }
  for (i = PARTITION_CONTEXTS_PRIMARY + PARTITION_BLOCK_SIZES;
       i < PARTITION_CONTEXTS_PRIMARY + 2 * PARTITION_BLOCK_SIZES; ++i) {
    unsigned int ct[2] = { counts->partition[i][PARTITION_HORZ],
                           counts->partition[i][PARTITION_SPLIT] };
    assert(counts->partition[i][PARTITION_NONE] == 0);
    assert(counts->partition[i][PARTITION_VERT] == 0);
    assert(fc->partition_prob[i][PARTITION_NONE] == 0);
    assert(fc->partition_prob[i][PARTITION_VERT] == 0);
    fc->partition_prob[i][PARTITION_HORZ] =
        av1_mode_mv_merge_probs(pre_fc->partition_prob[i][PARTITION_HORZ], ct);
  }
#endif
#if CONFIG_DELTA_Q
  for (i = 0; i < DELTA_Q_PROBS; ++i)
    fc->delta_q_prob[i] =
        mode_mv_merge_probs(pre_fc->delta_q_prob[i], counts->delta_q[i]);
#endif
#if CONFIG_EXT_INTRA
#if CONFIG_INTRA_INTERP
  for (i = 0; i < INTRA_FILTERS + 1; ++i) {
    aom_tree_merge_probs(av1_intra_filter_tree, pre_fc->intra_filter_probs[i],
                         counts->intra_filter[i], fc->intra_filter_probs[i]);
  }
#endif  // CONFIG_INTRA_INTERP
#endif  // CONFIG_EXT_INTRA
#if CONFIG_FILTER_INTRA
  for (i = 0; i < PLANE_TYPES; ++i) {
    fc->filter_intra_probs[i] = av1_mode_mv_merge_probs(
        pre_fc->filter_intra_probs[i], counts->filter_intra[i]);
  }
#endif  // CONFIG_FILTER_INTRA
}

static void set_default_lf_deltas(struct loopfilter *lf) {
  lf->mode_ref_delta_enabled = 1;
  lf->mode_ref_delta_update = 1;

  lf->ref_deltas[INTRA_FRAME] = 1;
  lf->ref_deltas[LAST_FRAME] = 0;
#if CONFIG_EXT_REFS
  lf->ref_deltas[LAST2_FRAME] = lf->ref_deltas[LAST_FRAME];
  lf->ref_deltas[LAST3_FRAME] = lf->ref_deltas[LAST_FRAME];
  lf->ref_deltas[BWDREF_FRAME] = lf->ref_deltas[LAST_FRAME];
#endif  // CONFIG_EXT_REFS
  lf->ref_deltas[GOLDEN_FRAME] = -1;
  lf->ref_deltas[ALTREF_FRAME] = -1;

  lf->mode_deltas[0] = 0;
  lf->mode_deltas[1] = 0;
}

void av1_setup_past_independence(AV1_COMMON *cm) {
  // Reset the segment feature data to the default stats:
  // Features disabled, 0, with delta coding (Default state).
  struct loopfilter *const lf = &cm->lf;

  int i;
  av1_clearall_segfeatures(&cm->seg);
  cm->seg.abs_delta = SEGMENT_DELTADATA;

  if (cm->last_frame_seg_map && !cm->frame_parallel_decode)
    memset(cm->last_frame_seg_map, 0, (cm->mi_rows * cm->mi_cols));

  if (cm->current_frame_seg_map)
    memset(cm->current_frame_seg_map, 0, (cm->mi_rows * cm->mi_cols));

  // Reset the mode ref deltas for loop filter
  av1_zero(lf->last_ref_deltas);
  av1_zero(lf->last_mode_deltas);
  set_default_lf_deltas(lf);

  // To force update of the sharpness
  lf->last_sharpness_level = -1;

  av1_default_coef_probs(cm);
  init_mode_probs(cm->fc);
  av1_init_mv_probs(cm);
#if CONFIG_ADAPT_SCAN
  av1_init_scan_order(cm);
#endif
  av1_convolve_init(cm);
  cm->fc->initialized = 1;

  if (cm->frame_type == KEY_FRAME || cm->error_resilient_mode ||
      cm->reset_frame_context == RESET_FRAME_CONTEXT_ALL) {
    // Reset all frame contexts.
    for (i = 0; i < FRAME_CONTEXTS; ++i) cm->frame_contexts[i] = *cm->fc;
  } else if (cm->reset_frame_context == RESET_FRAME_CONTEXT_CURRENT) {
    // Reset only the frame context specified in the frame header.
    cm->frame_contexts[cm->frame_context_idx] = *cm->fc;
  }

  // prev_mip will only be allocated in encoder.
  if (frame_is_intra_only(cm) && cm->prev_mip && !cm->frame_parallel_decode)
    memset(cm->prev_mip, 0,
           cm->mi_stride * (cm->mi_rows + 1) * sizeof(*cm->prev_mip));

  cm->frame_context_idx = 0;
}
