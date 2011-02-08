// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_convert.h"
#include "media/base/yuv_convert_internal.h"
#include "media/base/yuv_row.h"

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <mmintrin.h>
#include <emmintrin.h>
#endif

namespace media {

// This is the final offset for the conversion from signed yuv values to
// unsigned values. It is arranged so that offset of 16 is applied to Y
// components and 128 is added to UV components for 2 pixels.
SIMD_ALIGNED(const int16 kYuvOffset[8]) = {16, 0, 128, 128, 16, 0, 128, 128};

void FastConvertRGB32ToYUVRow(const uint8* rgb_buf_1,
                              const uint8* rgb_buf_2,
                              uint8* y_buf_1,
                              uint8* y_buf_2,
                              uint8* u_buf,
                              uint8* v_buf,
                              int width) {
  const uint64* r_table = reinterpret_cast<uint64*>(kCoefficientsYuvR);
  const uint64* g_table = reinterpret_cast<uint64*>(kCoefficientsYuvR + 256);
  const uint64* b_table = reinterpret_cast<uint64*>(kCoefficientsYuvR + 512);
  uint16* y_row_1 = reinterpret_cast<uint16*>(y_buf_1);
  uint16* y_row_2 = reinterpret_cast<uint16*>(y_buf_2);
  const uint32* rgb_row_1 = reinterpret_cast<const uint32*>(rgb_buf_1);
  const uint32* rgb_row_2 = reinterpret_cast<const uint32*>(rgb_buf_2);

  SIMD_ALIGNED(int8 output_stage[8]);
  __m128i offset = _mm_load_si128(reinterpret_cast<const __m128i*>(kYuvOffset));

  for (int i = 0; i < width; i += 2) {
    // Load the first pixel (a).
    register unsigned int pixel = *rgb_row_1++;
    __m128i b_comp_a = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(b_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i g_comp_a = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(g_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i r_comp_a = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(r_table + (pixel & 0xFF)));

    // Load the first pixel (c) in the second row.
    pixel = *rgb_row_2++;
    __m128i b_comp_c = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(b_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i g_comp_c = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(g_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i r_comp_c = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(r_table + (pixel & 0xFF)));

    // Pack two pixels into one register.
    __m128i b_comp_ac = _mm_unpacklo_epi64(b_comp_a, b_comp_c);
    __m128i g_comp_ac = _mm_unpacklo_epi64(g_comp_a, g_comp_c);
    __m128i r_comp_ac = _mm_unpacklo_epi64(r_comp_a, r_comp_c);

    // Add the coefficients together.
    //                  127                  0
    // |yuv_ac| will be (Vc Uc 0 Yc Va Ua 0 Ya).
    __m128i yuv_ac = _mm_adds_epi16(r_comp_ac,
                                    _mm_adds_epi16(g_comp_ac, b_comp_ac));

    // Right shift 6 bits to perform divide by 64 and then add the offset.
    yuv_ac = _mm_srai_epi16(yuv_ac, 6);
    yuv_ac = _mm_adds_epi16(yuv_ac, offset);

    // Now perform on the second column on pixel (b).
    pixel = *rgb_row_1++;
    __m128i b_comp_b = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(b_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i g_comp_b = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(g_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i r_comp_b = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(r_table + (pixel & 0xFF)));

    // Load the second pixel (d) in the second row.
    pixel = *rgb_row_2++;
    __m128i b_comp_d = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(b_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i g_comp_d = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(g_table + (pixel & 0xFF)));
    pixel >>= 8;
    __m128i r_comp_d = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(r_table + (pixel & 0xFF)));

    // Pack two pixels into one register.
    __m128i b_comp_bd = _mm_unpacklo_epi64(b_comp_b, b_comp_d);
    __m128i g_comp_bd = _mm_unpacklo_epi64(g_comp_b, g_comp_d);
    __m128i r_comp_bd = _mm_unpacklo_epi64(r_comp_b, r_comp_d);

    // Add the coefficients together.
    //                   127                  0
    // |yuv_bd| will be (Vd Ud 0 Yd Vb Ub 0 Yb).
    __m128i yuv_bd = _mm_adds_epi16(r_comp_bd,
                                    _mm_adds_epi16(g_comp_bd, b_comp_bd));

    // Right shift 6 bits to perform divide by 64 and then add the offset.
    yuv_bd = _mm_srai_epi16(yuv_bd, 6);
    yuv_bd = _mm_adds_epi16(yuv_bd, offset);

    // |yuv_row_1| will have (Vb Va Ub Ua 0 0 Yb Ya) and
    // |yuv_row_2| will have (Vd Vc Ud Uc 0 0 Yd Yc).
    __m128i yuv_row_1 = _mm_unpacklo_epi16(yuv_ac, yuv_bd);
    __m128i yuv_row_2 = _mm_unpackhi_epi16(yuv_ac, yuv_bd);

    // |y_comp| will have (0 0 0 0 Yd Yc Yb Ya).
    __m128i y_comp = _mm_unpacklo_epi32(yuv_row_1, yuv_row_2);

    // Down size to 8 bits.
    y_comp = _mm_packus_epi16(y_comp, y_comp);

    // |uv_comp| will have (Vd Vc Vb Va Ud Uc Ub Ua).
    __m128i uv_comp = _mm_unpackhi_epi32(yuv_row_1, yuv_row_2);

    // Generate |unity| to become (1 1 1 1 1 1 1 1).
    __m128i unity = _mm_cmpeq_epi16(offset, offset);
    unity = _mm_srli_epi16(unity, 15);

    // |uv_comp| will have (Vc + Vd, Va + Vb, Uc + Ud, Ua + Ub).
    uv_comp = _mm_madd_epi16(uv_comp, unity);

    // Pack |uv_comp| into 16 bit signed integers.
    uv_comp = _mm_packs_epi32(uv_comp, uv_comp);

    // And then do a multiply-add again. r1 will have 4 32-bits integers.
    uv_comp = _mm_madd_epi16(uv_comp, unity);

    // Do a right shift to perform divide by 4.
    uv_comp = _mm_srai_epi32(uv_comp, 2);

    // And then pack twice to form 2 8-bits unsigned integers of U and V.
    uv_comp = _mm_packs_epi32(uv_comp, uv_comp);
    uv_comp = _mm_packus_epi16(uv_comp, uv_comp);

    // And then finally pack the output.
    __m128i output = _mm_unpacklo_epi32(y_comp, uv_comp);

    // Store the output.
    _mm_storel_epi64(reinterpret_cast<__m128i*>(output_stage), output);

    *y_row_1++ = *reinterpret_cast<uint16*>(output_stage);
    *y_row_2++ = *reinterpret_cast<uint16*>(output_stage + 2);
    *u_buf++ = output_stage[4];
    *v_buf++ = output_stage[5];
  }
}

extern void ConvertRGB32ToYUV_SSE2(const uint8* rgbframe,
                                   uint8* yplane,
                                   uint8* uplane,
                                   uint8* vplane,
                                   int width,
                                   int height,
                                   int rgbstride,
                                   int ystride,
                                   int uvstride) {
  // Make sure |width| is a multiple of 2.
  width = (width / 2) * 2;
  for (int i = 0; i < height; i += 2) {
    FastConvertRGB32ToYUVRow(rgbframe,
                             rgbframe + rgbstride,
                             yplane,
                             yplane + ystride,
                             uplane,
                             vplane,
                             width);
    rgbframe += 2 * rgbstride;
    yplane += 2 * ystride;
    uplane += uvstride;
    vplane += uvstride;
  }
}

}  // namespace media
