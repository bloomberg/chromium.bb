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
// 64-bits system has more registers and so saving the pointers to the tables
// will be faster. But on 32-bits system we want to save registers so calculate
// the offset each time.
#if defined(ARCH_CPU_X86_64)
  const uint64* r_table = reinterpret_cast<uint64*>(kCoefficientsYuvR);
  const uint64* g_table = reinterpret_cast<uint64*>(kCoefficientsYuvR + 256);
  const uint64* b_table = reinterpret_cast<uint64*>(kCoefficientsYuvR + 512);
#define R_TABLE r_table
#define G_TABLE g_table
#define B_TABLE b_table
#else // On 32-bits system we compute the offset.
  const uint64* r_table = reinterpret_cast<uint64*>(kCoefficientsYuvR);
#define R_TABLE r_table
#define G_TABLE (r_table + 256)
#define B_TABLE (r_table + 512)
#endif

  uint16* y_row_1 = reinterpret_cast<uint16*>(y_buf_1);
  uint16* y_row_2 = reinterpret_cast<uint16*>(y_buf_2);
  __m128i offset = _mm_load_si128(reinterpret_cast<const __m128i*>(kYuvOffset));

  for (; width > 1; width -= 2) {
// MSVC generates very different intrinsic code for reading that affects
// performance significantly so we define two different ways.
#if defined(COMPILER_MSVC)
uint32 pixel;
#define READ_RGB(buf) pixel = *reinterpret_cast<const uint32*>(buf); buf += 4;
#define READ_B(buf) (pixel & 0xFF)
#define READ_G(buf) ((pixel >> 8) & 0xFF)
#define READ_R(buf) ((pixel >> 16) & 0xFF)
#define READ_A(buf)
#else
#define READ_RGB(buf)
#define READ_B(buf) (*buf++)
#define READ_G(buf) (*buf++)
#define READ_R(buf) (*buf++)
#define READ_A(buf) (buf++)
#endif
    // Load the first pixel (a) in the first row.
    READ_RGB(rgb_buf_1);
    __m128i b_comp_a = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(B_TABLE + READ_B(rgb_buf_1)));
    __m128i g_comp_a = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(G_TABLE + READ_G(rgb_buf_1)));
    __m128i r_comp_a = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(R_TABLE + READ_R(rgb_buf_1)));
    READ_A(rgb_buf_1);

    // Load the second pixel (b) in the first row.
    READ_RGB(rgb_buf_1);
    __m128i b_comp_b = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(B_TABLE + READ_B(rgb_buf_1)));
    __m128i g_comp_b = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(G_TABLE + READ_G(rgb_buf_1)));
    __m128i r_comp_b = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(R_TABLE + READ_R(rgb_buf_1)));
    READ_A(rgb_buf_1);

    // Pack two pixels into one register.
    __m128i b_comp_ab = _mm_unpacklo_epi64(b_comp_a, b_comp_b);
    __m128i g_comp_ab = _mm_unpacklo_epi64(g_comp_a, g_comp_b);
    __m128i r_comp_ab = _mm_unpacklo_epi64(r_comp_a, r_comp_b);

    // Add the coefficients together.
    //                  127                  0
    // |yuv_ab| will be (Vb Ub 0 Yb Va Ua 0 Ya).
    __m128i yuv_ab = _mm_adds_epi16(r_comp_ab,
                                    _mm_adds_epi16(g_comp_ab, b_comp_ab));

    // Right shift 6 bits to perform divide by 64 and then add the offset.
    yuv_ab = _mm_srai_epi16(yuv_ab, 6);
    yuv_ab = _mm_adds_epi16(yuv_ab, offset);

    // Do a shuffle to obtain (Vb Ub Va Ua 0 Yb 0 Ya).
    yuv_ab = _mm_shuffle_epi32(yuv_ab, 0xD8);

    // Load the first pixel (c) in the second row.
    READ_RGB(rgb_buf_2);
    __m128i b_comp_c = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(B_TABLE + READ_B(rgb_buf_2)));
    __m128i g_comp_c = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(G_TABLE + READ_G(rgb_buf_2)));
    __m128i r_comp_c = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(R_TABLE + READ_R(rgb_buf_2)));
    READ_A(rgb_buf_2);

    // Load the second pixel (d) in the second row.
    READ_RGB(rgb_buf_2);
    __m128i b_comp_d = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(B_TABLE + READ_B(rgb_buf_2)));
    __m128i g_comp_d = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(G_TABLE + READ_G(rgb_buf_2)));
    __m128i r_comp_d = _mm_loadl_epi64(
        reinterpret_cast<const __m128i*>(R_TABLE + READ_R(rgb_buf_2)));
    READ_A(rgb_buf_2);

    // Pack two pixels into one register.
    __m128i b_comp_cd = _mm_unpacklo_epi64(b_comp_c, b_comp_d);
    __m128i g_comp_cd = _mm_unpacklo_epi64(g_comp_c, g_comp_d);
    __m128i r_comp_cd = _mm_unpacklo_epi64(r_comp_c, r_comp_d);

    // Add the coefficients together.
    //                   127                  0
    // |yuv_cd| will be (Vd Ud 0 Yd Vc Uc 0 Yc).
    __m128i yuv_cd = _mm_adds_epi16(r_comp_cd,
                                    _mm_adds_epi16(g_comp_cd, b_comp_cd));

    // Right shift 6 bits to perform divide by 64 and then add the offset.
    yuv_cd = _mm_srai_epi16(yuv_cd, 6);
    yuv_cd = _mm_adds_epi16(yuv_cd, offset);

    // Do a shuffle to obtain (Vd Ud Vc Uc 0 Yd 0 Yc).
    yuv_cd = _mm_shuffle_epi32(yuv_cd, 0xD8);

    // This will form (0 Yd 0 Yc 0 Yb 0 Ya).
    __m128i y_comp = _mm_unpacklo_epi64(yuv_ab, yuv_cd);

    // Pack to 8-bits Y values.
    y_comp = _mm_packs_epi32(y_comp, y_comp);
    y_comp = _mm_packus_epi16(y_comp, y_comp);

    // And then store.
    *y_row_1++ = _mm_extract_epi16(y_comp, 0);
    *y_row_2++ = _mm_extract_epi16(y_comp, 1);

    // Find the average between pixel a and c then move to lower 64bit.
    __m128i uv_comp = _mm_avg_epu16(yuv_ab, yuv_cd);
    uv_comp = _mm_unpackhi_epi64(uv_comp, uv_comp);

    // Shuffle and then find average again.
    __m128i uv_comp_2 = _mm_shuffle_epi32(uv_comp, 0x01);
    uv_comp = _mm_avg_epu16(uv_comp, uv_comp_2);

    // Pack to 8-bits then store.
    uv_comp = _mm_packus_epi16(uv_comp, uv_comp);
    int uv = _mm_extract_epi16(uv_comp, 0);

    *u_buf++ = uv & 0xff;
    *v_buf++ = uv >> 8;
  }
}

#undef R_TABLE
#undef G_TABLE
#undef B_TABLE
#undef READ_RGB
#undef READ_R
#undef READ_G
#undef READ_B
#undef READ_A

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
