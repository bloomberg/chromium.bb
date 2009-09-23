// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/skbitmap_operations.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"

namespace {

// Returns true if each channel of the given two colors are "close." This is
// used for comparing colors where rounding errors may cause off-by-one.
bool ColorsClose(uint32_t a, uint32_t b) {
  return abs(static_cast<int>(SkColorGetB(a) - SkColorGetB(b))) < 2 &&
         abs(static_cast<int>(SkColorGetG(a) - SkColorGetG(b))) < 2 &&
         abs(static_cast<int>(SkColorGetR(a) - SkColorGetR(b))) < 2 &&
         abs(static_cast<int>(SkColorGetA(a) - SkColorGetA(b))) < 2;
}

void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bmp->allocPixels();

  unsigned char* src_data =
      reinterpret_cast<unsigned char*>(bmp->getAddr32(0, 0));
  for (int i = 0; i < w * h; i++) {
    src_data[i * 4 + 0] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 1] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 2] = static_cast<unsigned char>(i % 255);
    src_data[i * 4 + 3] = static_cast<unsigned char>(i % 255);
  }
}

}  // namespace

// Blend two bitmaps together at 50% alpha and verify that the result
// is the middle-blend of the two.
TEST(SkBitmapOperationsTest, CreateBlendedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src_a;
  src_a.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src_a.allocPixels();

  SkBitmap src_b;
  src_b.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src_b.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src_a.getAddr32(x, y) = SkColorSetARGB(255, 0, i * 2 % 255, i % 255);
      *src_b.getAddr32(x, y) =
          SkColorSetARGB((255 - i) % 255, i % 255, i * 4 % 255, 0);
      i++;
    }
  }

  // Shift to red.
  SkBitmap blended = SkBitmapOperations::CreateBlendedBitmap(
    src_a, src_b, 0.5);
  SkAutoLockPixels srca_lock(src_a);
  SkAutoLockPixels srcb_lock(src_b);
  SkAutoLockPixels blended_lock(blended);

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      int i = y * src_w + x;
      EXPECT_EQ(static_cast<unsigned int>((255 + ((255 - i) % 255)) / 2),
                SkColorGetA(*blended.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(i % 255 / 2),
                SkColorGetR(*blended.getAddr32(x, y)));
      EXPECT_EQ((static_cast<unsigned int>((i * 2) % 255 + (i * 4) % 255) / 2),
                SkColorGetG(*blended.getAddr32(x, y)));
      EXPECT_EQ(static_cast<unsigned int>(i % 255 / 2),
                SkColorGetB(*blended.getAddr32(x, y)));
    }
  }
}

// Test our masking functions.
TEST(SkBitmapOperationsTest, CreateMaskedBitmap) {
  int src_w = 16, src_h = 16;

  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Generate alpha mask
  SkBitmap alpha;
  alpha.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  alpha.allocPixels();
  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *alpha.getAddr32(x, y) = SkColorSetARGB((i + 128) % 255,
                                              (i + 128) % 255,
                                              (i + 64) % 255,
                                              (i + 0) % 255);
      i++;
    }
  }

  SkBitmap masked = SkBitmapOperations::CreateMaskedBitmap(src, alpha);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels alpha_lock(alpha);
  SkAutoLockPixels masked_lock(masked);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      // Test that the alpha is equal.
      SkColor src_pixel = SkUnPreMultiply::PMColorToColor(*src.getAddr32(x, y));
      SkColor alpha_pixel =
          SkUnPreMultiply::PMColorToColor(*alpha.getAddr32(x, y));
      SkColor masked_pixel = *masked.getAddr32(x, y);

      int alpha_value = SkAlphaMul(SkColorGetA(src_pixel),
                                   SkColorGetA(alpha_pixel));
      SkColor expected_pixel = SkColorSetARGB(
          alpha_value,
          SkAlphaMul(SkColorGetR(src_pixel), alpha_value),
          SkAlphaMul(SkColorGetG(src_pixel), alpha_value),
          SkAlphaMul(SkColorGetB(src_pixel), alpha_value));

      EXPECT_TRUE(ColorsClose(expected_pixel, masked_pixel));
    }
  }
}

// Make sure that when shifting a bitmap without any shift parameters,
// the end result is close enough to the original (rounding errors
// notwithstanding).
TEST(SkBitmapOperationsTest, CreateHSLShiftedBitmapToSame) {
  int src_w = 4, src_h = 4;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkColorSetARGB(i + 128 % 255,
          i + 128 % 255, i + 64 % 255, i + 0 % 255);
      i++;
    }
  }

  color_utils::HSL hsl = { -1, -1, -1 };

  SkBitmap shifted = SkBitmapOperations::CreateHSLShiftedBitmap(src, hsl);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels shifted_lock(shifted);

  for (int y = 0; y < src_w; y++) {
    for (int x = 0; x < src_h; x++) {
      SkColor src_pixel = *src.getAddr32(x, y);
      SkColor shifted_pixel = *shifted.getAddr32(x, y);
      EXPECT_TRUE(ColorsClose(src_pixel, shifted_pixel));
    }
  }
}

// Shift a blue bitmap to red.
TEST(SkBitmapOperationsTest, CreateHSLShiftedBitmapHueOnly) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  src.setConfig(SkBitmap::kARGB_8888_Config, src_w, src_h);
  src.allocPixels();

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      *src.getAddr32(x, y) = SkColorSetARGB(255, 0, 0, i % 255);
      i++;
    }
  }

  // Shift to red.
  color_utils::HSL hsl = { 0, -1, -1 };

  SkBitmap shifted = SkBitmapOperations::CreateHSLShiftedBitmap(src, hsl);

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels shifted_lock(shifted);

  for (int y = 0, i = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_TRUE(ColorsClose(*shifted.getAddr32(x, y),
                              SkColorSetARGB(255, i % 255, 0, 0)));
      i++;
    }
  }
}

// Test our cropping.
TEST(SkBitmapOperationsTest, CreateCroppedBitmap) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(src, 4, 4,
                                                              8, 8);
  ASSERT_EQ(8, cropped.width());
  ASSERT_EQ(8, cropped.height());

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels cropped_lock(cropped);
  for (int y = 4; y < 12; y++) {
    for (int x = 4; x < 12; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32(x - 4, y - 4));
    }
  }
}

// Test whether our cropping correctly wraps across image boundaries.
TEST(SkBitmapOperationsTest, CreateCroppedBitmapWrapping) {
  int src_w = 16, src_h = 16;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
      src, src_w / 2, src_h / 2, src_w, src_h);
  ASSERT_EQ(src_w, cropped.width());
  ASSERT_EQ(src_h, cropped.height());

  SkAutoLockPixels src_lock(src);
  SkAutoLockPixels cropped_lock(cropped);
  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_EQ(*src.getAddr32(x, y),
                *cropped.getAddr32((x + src_w / 2) % src_w,
                                   (y + src_h / 2) % src_h));
    }
  }
}

TEST(SkBitmapOperationsTest, DownsampleByTwo) {
  // Use an odd-sized bitmap to make sure the edge cases where there isn't a
  // 2x2 block of pixels is handled correctly.
  // Here's the ARGB example
  //
  //    50% transparent green             opaque 50% blue           white
  //        80008000                         FF000080              FFFFFFFF
  //
  //    50% transparent red               opaque 50% gray           black
  //        80800000                         80808080              FF000000
  //
  //         black                            white                50% gray
  //        FF000000                         FFFFFFFF              FF808080
  //
  // The result of this computation should be:
  //        A0404040  FF808080
  //        FF808080  FF808080
  SkBitmap input;
  input.setConfig(SkBitmap::kARGB_8888_Config, 3, 3);
  input.allocPixels();

  // The color order may be different, but we don't care (the channels are
  // trated the same).
  *input.getAddr32(0, 0) = 0x80008000;
  *input.getAddr32(1, 0) = 0xFF000080;
  *input.getAddr32(2, 0) = 0xFFFFFFFF;
  *input.getAddr32(0, 1) = 0x80800000;
  *input.getAddr32(1, 1) = 0x80808080;
  *input.getAddr32(2, 1) = 0xFF000000;
  *input.getAddr32(0, 2) = 0xFF000000;
  *input.getAddr32(1, 2) = 0xFFFFFFFF;
  *input.getAddr32(2, 2) = 0xFF808080;

  SkBitmap result = SkBitmapOperations::DownsampleByTwo(input);
  EXPECT_EQ(2, result.width());
  EXPECT_EQ(2, result.height());

  // Some of the values are off-by-one due to rounding.
  SkAutoLockPixels lock(result);
  EXPECT_EQ(0x9f404040, *result.getAddr32(0, 0));
  EXPECT_EQ(0xFF7f7f7f, *result.getAddr32(1, 0));
  EXPECT_EQ(0xFF7f7f7f, *result.getAddr32(0, 1));
  EXPECT_EQ(0xFF808080, *result.getAddr32(1, 1));
}

// Test edge cases for DownsampleByTwo.
TEST(SkBitmapOperationsTest, DownsampleByTwoSmall) {
  SkPMColor reference = 0xFF4080FF;

  // Test a 1x1 bitmap.
  SkBitmap one_by_one;
  one_by_one.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
  one_by_one.allocPixels();
  *one_by_one.getAddr32(0, 0) = reference;
  SkBitmap result = SkBitmapOperations::DownsampleByTwo(one_by_one);
  SkAutoLockPixels lock1(result);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(1, result.height());
  EXPECT_EQ(reference, *result.getAddr32(0, 0));

  // Test an n by 1 bitmap.
  SkBitmap one_by_n;
  one_by_n.setConfig(SkBitmap::kARGB_8888_Config, 300, 1);
  one_by_n.allocPixels();
  result = SkBitmapOperations::DownsampleByTwo(one_by_n);
  SkAutoLockPixels lock2(result);
  EXPECT_EQ(300, result.width());
  EXPECT_EQ(1, result.height());

  // Test a 1 by n bitmap.
  SkBitmap n_by_one;
  n_by_one.setConfig(SkBitmap::kARGB_8888_Config, 1, 300);
  n_by_one.allocPixels();
  result = SkBitmapOperations::DownsampleByTwo(n_by_one);
  SkAutoLockPixels lock3(result);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(300, result.height());

  // Test an empty bitmap
  SkBitmap empty;
  result = SkBitmapOperations::DownsampleByTwo(empty);
  EXPECT_TRUE(result.isNull());
  EXPECT_EQ(0, result.width());
  EXPECT_EQ(0, result.height());
}

// Here we assume DownsampleByTwo works correctly (it's tested above) and
// just make sure that the wrapper function does the right thing.
TEST(SkBitmapOperationsTest, DownsampleByTwoUntilSize) {
  // First make sure a "too small" bitmap doesn't get modified at all.
  SkBitmap too_small;
  too_small.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
  too_small.allocPixels();
  SkBitmap result = SkBitmapOperations::DownsampleByTwoUntilSize(
      too_small, 16, 16);
  EXPECT_EQ(10, result.width());
  EXPECT_EQ(10, result.height());

  // Now make sure giving it a 0x0 target returns something reasonable.
  result = SkBitmapOperations::DownsampleByTwoUntilSize(too_small, 0, 0);
  EXPECT_EQ(1, result.width());
  EXPECT_EQ(1, result.height());

  // Test multiple steps of downsampling.
  SkBitmap large;
  large.setConfig(SkBitmap::kARGB_8888_Config, 100, 43);
  large.allocPixels();
  result = SkBitmapOperations::DownsampleByTwoUntilSize(large, 6, 6);

  // The result should be divided in half 100x43 -> 50x22 -> 25x11
  EXPECT_EQ(25, result.width());
  EXPECT_EQ(11, result.height());
}
