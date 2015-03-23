// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See the following specification for details on the ETC1 format:
// https://www.khronos.org/registry/gles/extensions/OES/OES_compressed_ETC1_RGB8_texture.txt

#include "cc/resources/texture_compressor_etc1.h"

#include <string.h>
#include <limits>

#include "base/logging.h"

// Defining the following macro will cause the error metric function to weigh
// each color channel differently depending on how the human eye can perceive
// them. This can give a slight improvement in image quality at the cost of a
// performance hit.
// #define USE_PERCEIVED_ERROR_METRIC

namespace {

template <typename T>
inline T clamp(T val, T min, T max) {
  return val < min ? min : (val > max ? max : val);
}

inline uint8_t round_to_5_bits(float val) {
  return clamp<uint8_t>(val * 31.0f / 255.0f + 0.5f, 0, 31);
}

inline uint8_t round_to_4_bits(float val) {
  return clamp<uint8_t>(val * 15.0f / 255.0f + 0.5f, 0, 15);
}

union Color {
  struct BgraColorType {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t a;
  } channels;
  uint8_t components[4];
  uint32_t bits;
};

/*
 * Codeword tables.
 * See: Table 3.17.2
 */
static const int16_t g_codeword_tables[8][4] = {{-8, -2, 2, 8},
                                                {-17, -5, 5, 17},
                                                {-29, -9, 9, 29},
                                                {-42, -13, 13, 42},
                                                {-60, -18, 18, 60},
                                                {-80, -24, 24, 80},
                                                {-106, -33, 33, 106},
                                                {-183, -47, 47, 183}};

/*
 * Maps modifier indices to pixel index values.
 * See: Table 3.17.3
 */
static const uint8_t g_mod_to_pix[4] = {3, 2, 0, 1};

/*
 * The ETC1 specification index texels as follows:
 *
 * [a][e][i][m]     [ 0][ 4][ 8][12]
 * [b][f][j][n] <-> [ 1][ 5][ 9][13]
 * [c][g][k][o]     [ 2][ 6][10][14]
 * [d][h][l][p]     [ 3][ 7][11][15]
 *
 * However, when extracting sub blocks from BGRA data the natural array
 * indexing order ends up different:
 *
 * vertical0: [a][e][b][f]  horizontal0: [a][e][i][m]
 *            [c][g][d][h]               [b][f][j][n]
 * vertical1: [i][m][j][n]  horizontal1: [c][g][k][o]
 *            [k][o][l][p]               [d][h][l][p]
 *
 * In order to translate from the natural array indices in a sub block to the
 * indices (number) used by specification and hardware we use this table.
 */
static const uint8_t g_idx_to_num[4][8] = {
    {0, 4, 1, 5, 2, 6, 3, 7},        // Vertical block 0.
    {8, 12, 9, 13, 10, 14, 11, 15},  // Vertical block 1.
    {0, 4, 8, 12, 1, 5, 9, 13},      // Horizontal block 0.
    {2, 6, 10, 14, 3, 7, 11, 15}     // Horizontal block 1.
};

inline void WriteColors444(uint8_t* block,
                           const Color& color0,
                           const Color& color1) {
  block[0] = (color0.channels.r & 0xf0) | (color1.channels.r >> 4);
  block[1] = (color0.channels.g & 0xf0) | (color1.channels.g >> 4);
  block[2] = (color0.channels.b & 0xf0) | (color1.channels.b >> 4);
}

inline void WriteColors555(uint8_t* block,
                           const Color& color0,
                           const Color& color1) {
  // Table for conversion to 3-bit two complement format.
  static const uint8_t two_compl_trans_table[8] = {
      4,  // -4 (100b)
      5,  // -3 (101b)
      6,  // -2 (110b)
      7,  // -1 (111b)
      0,  //  0 (000b)
      1,  //  1 (001b)
      2,  //  2 (010b)
      3,  //  3 (011b)
  };

  int16_t delta_r =
      static_cast<int16_t>(color1.channels.r >> 3) - (color0.channels.r >> 3);
  int16_t delta_g =
      static_cast<int16_t>(color1.channels.g >> 3) - (color0.channels.g >> 3);
  int16_t delta_b =
      static_cast<int16_t>(color1.channels.b >> 3) - (color0.channels.b >> 3);
  DCHECK(delta_r >= -4 && delta_r <= 3);
  DCHECK(delta_g >= -4 && delta_g <= 3);
  DCHECK(delta_b >= -4 && delta_b <= 3);

  block[0] = (color0.channels.r & 0xf8) | two_compl_trans_table[delta_r + 4];
  block[1] = (color0.channels.g & 0xf8) | two_compl_trans_table[delta_g + 4];
  block[2] = (color0.channels.b & 0xf8) | two_compl_trans_table[delta_b + 4];
}

inline void WriteCodewordTable(uint8_t* block,
                               uint8_t sub_block_id,
                               uint8_t table) {
  DCHECK_LT(sub_block_id, 2);
  DCHECK_LT(table, 8);

  uint8_t shift = (2 + (3 - sub_block_id * 3));
  block[3] &= ~(0x07 << shift);
  block[3] |= table << shift;
}

inline void WritePixelData(uint8_t* block, uint32_t pixel_data) {
  block[4] |= pixel_data >> 24;
  block[5] |= (pixel_data >> 16) & 0xff;
  block[6] |= (pixel_data >> 8) & 0xff;
  block[7] |= pixel_data & 0xff;
}

inline void WriteFlip(uint8_t* block, bool flip) {
  block[3] &= ~0x01;
  block[3] |= static_cast<uint8_t>(flip);
}

inline void WriteDiff(uint8_t* block, bool diff) {
  block[3] &= ~0x02;
  block[3] |= static_cast<uint8_t>(diff) << 1;
}

/**
 * Compress and rounds BGR888 into BGR444. The resulting BGR444 color is
 * expanded to BGR888 as it would be in hardware after decompression. The
 * actual 444-bit data is available in the four most significant bits of each
 * channel.
 */
inline Color MakeColor444(const float* bgr) {
  uint8_t b4 = round_to_4_bits(bgr[0]);
  uint8_t g4 = round_to_4_bits(bgr[1]);
  uint8_t r4 = round_to_4_bits(bgr[2]);
  Color bgr444;
  bgr444.channels.b = (b4 << 4) | b4;
  bgr444.channels.g = (g4 << 4) | g4;
  bgr444.channels.r = (r4 << 4) | r4;
  return bgr444;
}

/**
 * Compress and rounds BGR888 into BGR555. The resulting BGR555 color is
 * expanded to BGR888 as it would be in hardware after decompression. The
 * actual 555-bit data is available in the five most significant bits of each
 * channel.
 */
inline Color MakeColor555(const float* bgr) {
  uint8_t b5 = round_to_5_bits(bgr[0]);
  uint8_t g5 = round_to_5_bits(bgr[1]);
  uint8_t r5 = round_to_5_bits(bgr[2]);
  Color bgr555;
  bgr555.channels.b = (b5 << 3) | (b5 >> 2);
  bgr555.channels.g = (g5 << 3) | (g5 >> 2);
  bgr555.channels.r = (r5 << 3) | (r5 >> 2);
  return bgr555;
}

/**
 * Constructs a color from a given base color and luminance value.
 */
inline Color MakeColor(const Color& base, int16_t lum) {
  int b = static_cast<int>(base.channels.b) + lum;
  int g = static_cast<int>(base.channels.g) + lum;
  int r = static_cast<int>(base.channels.r) + lum;
  Color color;
  color.channels.b = static_cast<uint8_t>(clamp(b, 0, 255));
  color.channels.g = static_cast<uint8_t>(clamp(g, 0, 255));
  color.channels.r = static_cast<uint8_t>(clamp(r, 0, 255));
  return color;
}

/**
 * Calculates the error metric for two colors. A small error signals that the
 * colors are similar to each other, a large error the signals the opposite.
 */
inline uint32_t GetColorError(const Color& u, const Color& v) {
#ifdef USE_PERCEIVED_ERROR_METRIC
  float delta_b = static_cast<float>(u.channels.b) - v.channels.b;
  float delta_g = static_cast<float>(u.channels.g) - v.channels.g;
  float delta_r = static_cast<float>(u.channels.r) - v.channels.r;
  return static_cast<uint32_t>(0.299f * delta_b * delta_b +
                               0.587f * delta_g * delta_g +
                               0.114f * delta_r * delta_r);
#else
  int delta_b = static_cast<int>(u.channels.b) - v.channels.b;
  int delta_g = static_cast<int>(u.channels.g) - v.channels.g;
  int delta_r = static_cast<int>(u.channels.r) - v.channels.r;
  return delta_b * delta_b + delta_g * delta_g + delta_r * delta_r;
#endif
}

void GetAverageColor(const Color* src, float* avg_color) {
  uint32_t sum_b = 0, sum_g = 0, sum_r = 0;

  for (unsigned int i = 0; i < 8; ++i) {
    sum_b += src[i].channels.b;
    sum_g += src[i].channels.g;
    sum_r += src[i].channels.r;
  }

  const float kInv8 = 1.0f / 8.0f;
  avg_color[0] = static_cast<float>(sum_b) * kInv8;
  avg_color[1] = static_cast<float>(sum_g) * kInv8;
  avg_color[2] = static_cast<float>(sum_r) * kInv8;
}

void ComputeLuminance(uint8_t* block,
                      const Color* src,
                      const Color& base,
                      int sub_block_id,
                      const uint8_t* idx_to_num_tab) {
  uint32_t best_tbl_err = std::numeric_limits<uint32_t>::max();
  uint8_t best_tbl_idx = 0;
  uint8_t best_mod_idx[8][8];  // [table][texel]

  // Try all codeword tables to find the one giving the best results for this
  // block.
  for (unsigned int tbl_idx = 0; tbl_idx < 8; ++tbl_idx) {
    // Pre-compute all the candidate colors; combinations of the base color and
    // all available luminance values.
    Color candidate_color[4];  // [modifier]
    for (unsigned int mod_idx = 0; mod_idx < 4; ++mod_idx) {
      int16_t lum = g_codeword_tables[tbl_idx][mod_idx];
      candidate_color[mod_idx] = MakeColor(base, lum);
    }

    uint32_t tbl_err = 0;

    for (unsigned int i = 0; i < 8; ++i) {
      // Try all modifiers in the current table to find which one gives the
      // smallest error.
      uint32_t best_mod_err = std::numeric_limits<uint32_t>::max();
      for (unsigned int mod_idx = 0; mod_idx < 4; ++mod_idx) {
        const Color& color = candidate_color[mod_idx];

        uint32_t mod_err = GetColorError(src[i], color);
        if (mod_err < best_mod_err) {
          best_mod_idx[tbl_idx][i] = mod_idx;
          best_mod_err = mod_err;

          if (mod_err == 0)
            break;  // We cannot do any better than this.
        }
      }

      tbl_err += best_mod_err;
      if (tbl_err > best_tbl_err)
        break;  // We're already doing worse than the best table so skip.
    }

    if (tbl_err < best_tbl_err) {
      best_tbl_err = tbl_err;
      best_tbl_idx = tbl_idx;

      if (tbl_err == 0)
        break;  // We cannot do any better than this.
    }
  }

  WriteCodewordTable(block, sub_block_id, best_tbl_idx);

  uint32_t pix_data = 0;

  for (unsigned int i = 0; i < 8; ++i) {
    uint8_t mod_idx = best_mod_idx[best_tbl_idx][i];
    uint8_t pix_idx = g_mod_to_pix[mod_idx];

    uint32_t lsb = pix_idx & 0x1;
    uint32_t msb = pix_idx >> 1;

    // Obtain the texel number as specified in the standard.
    int texel_num = idx_to_num_tab[i];
    pix_data |= msb << (texel_num + 16);
    pix_data |= lsb << (texel_num);
  }

  WritePixelData(block, pix_data);
}

/**
 * Tries to compress the block under the assumption that it's a single color
 * block. If it's not the function will bail out without writing anything to
 * the destination buffer.
 */
bool TryCompressSolidBlock(uint8_t* dst, const Color* src) {
  for (unsigned int i = 1; i < 16; ++i) {
    if (src[i].bits != src[0].bits)
      return false;
  }

  // Clear destination buffer so that we can "or" in the results.
  memset(dst, 0, 8);

  float src_color_float[3] = {static_cast<float>(src->channels.b),
                              static_cast<float>(src->channels.g),
                              static_cast<float>(src->channels.r)};
  Color base = MakeColor555(src_color_float);

  WriteDiff(dst, true);
  WriteFlip(dst, false);
  WriteColors555(dst, base, base);

  uint8_t best_tbl_idx = 0;
  uint8_t best_mod_idx = 0;
  uint32_t best_mod_err = std::numeric_limits<uint32_t>::max();

  // Try all codeword tables to find the one giving the best results for this
  // block.
  for (unsigned int tbl_idx = 0; tbl_idx < 8; ++tbl_idx) {
    // Try all modifiers in the current table to find which one gives the
    // smallest error.
    for (unsigned int mod_idx = 0; mod_idx < 4; ++mod_idx) {
      int16_t lum = g_codeword_tables[tbl_idx][mod_idx];
      const Color& color = MakeColor(base, lum);

      uint32_t mod_err = GetColorError(*src, color);
      if (mod_err < best_mod_err) {
        best_tbl_idx = tbl_idx;
        best_mod_idx = mod_idx;
        best_mod_err = mod_err;

        if (mod_err == 0)
          break;  // We cannot do any better than this.
      }
    }

    if (best_mod_err == 0)
      break;
  }

  WriteCodewordTable(dst, 0, best_tbl_idx);
  WriteCodewordTable(dst, 1, best_tbl_idx);

  uint8_t pix_idx = g_mod_to_pix[best_mod_idx];
  uint32_t lsb = pix_idx & 0x1;
  uint32_t msb = pix_idx >> 1;

  uint32_t pix_data = 0;
  for (unsigned int i = 0; i < 2; ++i) {
    for (unsigned int j = 0; j < 8; ++j) {
      // Obtain the texel number as specified in the standard.
      int texel_num = g_idx_to_num[i][j];
      pix_data |= msb << (texel_num + 16);
      pix_data |= lsb << (texel_num);
    }
  }

  WritePixelData(dst, pix_data);
  return true;
}

void CompressBlock(uint8_t* dst, const Color* ver_src, const Color* hor_src) {
  if (TryCompressSolidBlock(dst, ver_src))
    return;

  const Color* sub_block_src[4] = {ver_src, ver_src + 8, hor_src, hor_src + 8};

  Color sub_block_avg[4];
  bool use_differential[2] = {true, true};

  // Compute the average color for each sub block and determine if differential
  // coding can be used.
  for (unsigned int i = 0, j = 1; i < 4; i += 2, j += 2) {
    float avg_color_0[3];
    GetAverageColor(sub_block_src[i], avg_color_0);
    Color avg_color_555_0 = MakeColor555(avg_color_0);

    float avg_color_1[3];
    GetAverageColor(sub_block_src[j], avg_color_1);
    Color avg_color_555_1 = MakeColor555(avg_color_1);

    for (unsigned int light_idx = 0; light_idx < 3; ++light_idx) {
      int u = avg_color_555_0.components[light_idx] >> 3;
      int v = avg_color_555_1.components[light_idx] >> 3;

      int component_diff = v - u;
      if (component_diff < -4 || component_diff > 3) {
        use_differential[i / 2] = false;
        sub_block_avg[i] = MakeColor444(avg_color_0);
        sub_block_avg[j] = MakeColor444(avg_color_1);
      } else {
        sub_block_avg[i] = avg_color_555_0;
        sub_block_avg[j] = avg_color_555_1;
      }
    }
  }

  // Compute the error of each sub block before adjusting for luminance. These
  // error values are later used for determining if we should flip the sub
  // block or not.
  uint32_t sub_block_err[4] = {0};
  for (unsigned int i = 0; i < 4; ++i) {
    for (unsigned int j = 0; j < 8; ++j) {
      sub_block_err[i] += GetColorError(sub_block_avg[i], sub_block_src[i][j]);
    }
  }

  bool flip =
      sub_block_err[2] + sub_block_err[3] < sub_block_err[0] + sub_block_err[1];

  // Clear destination buffer so that we can "or" in the results.
  memset(dst, 0, 8);

  WriteDiff(dst, use_differential[!!flip]);
  WriteFlip(dst, flip);

  uint8_t sub_block_off_0 = flip ? 2 : 0;
  uint8_t sub_block_off_1 = sub_block_off_0 + 1;

  if (use_differential[!!flip]) {
    WriteColors555(dst, sub_block_avg[sub_block_off_0],
                   sub_block_avg[sub_block_off_1]);
  } else {
    WriteColors444(dst, sub_block_avg[sub_block_off_0],
                   sub_block_avg[sub_block_off_1]);
  }

  // Compute luminance for the first sub block.
  ComputeLuminance(dst, sub_block_src[sub_block_off_0],
                   sub_block_avg[sub_block_off_0], 0,
                   g_idx_to_num[sub_block_off_0]);
  // Compute luminance for the second sub block.
  ComputeLuminance(dst, sub_block_src[sub_block_off_1],
                   sub_block_avg[sub_block_off_1], 1,
                   g_idx_to_num[sub_block_off_1]);
}

}  // namespace

namespace cc {

void TextureCompressorETC1::Compress(const uint8_t* src,
                                     uint8_t* dst,
                                     int width,
                                     int height,
                                     Quality quality) {
  DCHECK(width >= 4 && (width & 3) == 0);
  DCHECK(height >= 4 && (height & 3) == 0);

  Color ver_blocks[16];
  Color hor_blocks[16];

  for (int y = 0; y < height; y += 4, src += width * 4 * 4) {
    for (int x = 0; x < width; x += 4, dst += 8) {
      const Color* row0 = reinterpret_cast<const Color*>(src + x * 4);
      const Color* row1 = row0 + width;
      const Color* row2 = row1 + width;
      const Color* row3 = row2 + width;

      memcpy(ver_blocks, row0, 8);
      memcpy(ver_blocks + 2, row1, 8);
      memcpy(ver_blocks + 4, row2, 8);
      memcpy(ver_blocks + 6, row3, 8);
      memcpy(ver_blocks + 8, row0 + 2, 8);
      memcpy(ver_blocks + 10, row1 + 2, 8);
      memcpy(ver_blocks + 12, row2 + 2, 8);
      memcpy(ver_blocks + 14, row3 + 2, 8);

      memcpy(hor_blocks, row0, 16);
      memcpy(hor_blocks + 4, row1, 16);
      memcpy(hor_blocks + 8, row2, 16);
      memcpy(hor_blocks + 12, row3, 16);

      CompressBlock(dst, ver_blocks, hor_blocks);
    }
  }
}

}  // namespace cc
