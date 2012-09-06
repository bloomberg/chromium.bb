// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/select_favicon_frames.h"

#include "ui/base/layout.h"
#include "ui/gfx/image/image_skia.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

using std::vector;

namespace {

vector<ui::ScaleFactor> Scale1x() {
  return vector<ui::ScaleFactor>(1, ui::SCALE_FACTOR_100P);
}

vector<ui::ScaleFactor> Scale1x2x() {
  vector<ui::ScaleFactor> scales;
  scales.push_back(ui::SCALE_FACTOR_100P);
  scales.push_back(ui::SCALE_FACTOR_200P);
  return scales;
}

SkBitmap MakeBitmap(SkColor color, int w, int h) {
  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, w, h);
  bitmap.allocPixels();
  bitmap.eraseColor(color);
  return bitmap;
}

SkColor GetColor(const gfx::ImageSkia& image, ui::ScaleFactor factor,
                 int x = -1, int y = -1) {
  const SkBitmap& bitmap = image.GetRepresentation(factor).sk_bitmap();
  if (x == -1)
    x = bitmap.width() / 2;
  if (y == -1)
    y = bitmap.width() / 2;
  bitmap.lockPixels();
  SkColor color = bitmap.getColor(x, y);
  bitmap.unlockPixels();
  return color;
}

SkColor GetColor1x(const gfx::ImageSkia& image) {
  return GetColor(image, ui::SCALE_FACTOR_100P);
}

SkColor GetColor2x(const gfx::ImageSkia& image) {
  return GetColor(image, ui::SCALE_FACTOR_200P);
}

TEST(SelectFaviconFramesTest, ZeroSizePicksLargest) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 16, 16));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 48, 48));
  bitmaps.push_back(MakeBitmap(SK_ColorBLUE, 32, 32));

  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x(), 0, NULL);
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  EXPECT_EQ(48, image.width());
  EXPECT_EQ(48, image.height());

  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
}

TEST(SelectFaviconFramesTest, _16From16) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 15, 15));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  bitmaps.push_back(MakeBitmap(SK_ColorBLUE, 17, 17));

  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x(), 16, NULL);
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
}

TEST(SelectFaviconFramesTest, _16From17) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 15, 15));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 17, 17));

  // Should resample from the bigger candidate.
  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x(), 16, NULL);
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
}

TEST(SelectFaviconFramesTest, _16From15) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 14, 14));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 15, 15));

  // If nothing else is available, should resample form the next smaller
  // candidate.
  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x(), 16, NULL);
  EXPECT_EQ(1u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
}

TEST(SelectFaviconFramesTest, _16From16_Scale2x_32_From_16) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));

  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x2x(), 16, NULL);
  EXPECT_EQ(2u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_200P));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
  EXPECT_EQ(SK_ColorGREEN, GetColor2x(image));
}

TEST(SelectFaviconFramesTest, _16From16_Scale2x_32_From_32) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  bitmaps.push_back(MakeBitmap(SK_ColorBLUE, 32, 32));

  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x2x(), 16, NULL);
  EXPECT_EQ(2u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_200P));
  EXPECT_EQ(16, image.width());
  EXPECT_EQ(16, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
  EXPECT_EQ(SK_ColorBLUE, GetColor2x(image));
}

TEST(SelectFaviconFramesTest, _24_48) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorRED, 25, 25));
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  bitmaps.push_back(MakeBitmap(SK_ColorBLUE, 32, 32));
  bitmaps.push_back(MakeBitmap(SK_ColorYELLOW, 49, 49));

  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x2x(), 24, NULL);
  EXPECT_EQ(2u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_200P));
  EXPECT_EQ(24, image.width());
  EXPECT_EQ(24, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 0, 0)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 3, 4)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 4, 4)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 19, 19)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 20, 19)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 23, 23)));

  EXPECT_EQ(SK_ColorBLUE, GetColor2x(image));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 0, 0)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 7, 8)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 8, 8)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 39, 39)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 40, 39)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 47, 47)));
}

TEST(SelectFaviconFramesTest, _48_From_16) {
  vector<SkBitmap> bitmaps;
  bitmaps.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));

  gfx::ImageSkia image = SelectFaviconFrames(bitmaps, Scale1x2x(), 24, NULL);
  EXPECT_EQ(2u, image.image_reps().size());
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_100P));
  ASSERT_TRUE(image.HasRepresentation(ui::SCALE_FACTOR_200P));
  EXPECT_EQ(24, image.width());
  EXPECT_EQ(24, image.height());
  EXPECT_EQ(SK_ColorGREEN, GetColor1x(image));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 0, 0)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 3, 4)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 4, 4)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 19, 19)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 20, 19)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_100P, 23, 23)));

  EXPECT_EQ(SK_ColorGREEN, GetColor2x(image));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 0, 0)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 7, 8)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 8, 8)));
  EXPECT_EQ(255u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 39, 39)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 40, 39)));
  EXPECT_EQ(0u, SkColorGetA(GetColor(image, ui::SCALE_FACTOR_200P, 47, 47)));
}

TEST(SelectFaviconFramesTest, ExactMatchBetterThanHugeBitmap) {
  float score1;
  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 255, 255));
  SelectFaviconFrames(bitmaps1, Scale1x2x(), 16, &score1);

  float score2;
  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 16, 16));
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 32, 32));
  SelectFaviconFrames(bitmaps2, Scale1x2x(), 16, &score2);

  EXPECT_GT(score2, score1);
}

TEST(SelectFaviconFramesTest, DownsamplingBetterThanUpsampling) {
  float score1;
  vector<SkBitmap> bitmaps1;
  bitmaps1.push_back(MakeBitmap(SK_ColorGREEN, 8, 8));
  SelectFaviconFrames(bitmaps1, Scale1x(), 16, &score1);

  float score2;
  vector<SkBitmap> bitmaps2;
  bitmaps2.push_back(MakeBitmap(SK_ColorGREEN, 24, 24));
  SelectFaviconFrames(bitmaps2, Scale1x(), 16, &score2);

  EXPECT_GT(score2, score1);
}

}
