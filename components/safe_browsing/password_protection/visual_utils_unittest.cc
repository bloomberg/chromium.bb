// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/password_protection/visual_utils.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace visual_utils {

using ::testing::FloatEq;

TEST(VisualUtilsTest, TestSkColorToQuantizedColor) {
  // Test quantization
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 31)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 32)), 1u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 31, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 32, 0)), 8u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(31, 0, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(32, 0, 0)), 64u);

  // Test composition of RGB quantized values
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 255)), 7u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 255, 255)), 63u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(255, 255, 255)), 511u);
}

TEST(VisualUtilsTest, GetQuantizedR) {
  EXPECT_EQ(GetQuantizedR(0), 0);
  EXPECT_EQ(GetQuantizedR(64), 1);
  EXPECT_EQ(GetQuantizedR(448), 7);
}

TEST(VisualUtilsTest, GetQuantizedG) {
  EXPECT_EQ(GetQuantizedG(0), 0);
  EXPECT_EQ(GetQuantizedG(8), 1);
  EXPECT_EQ(GetQuantizedG(56), 7);
}

TEST(VisualUtilsTest, GetQuantizedB) {
  EXPECT_EQ(GetQuantizedB(0), 0);
  EXPECT_EQ(GetQuantizedB(1), 1);
  EXPECT_EQ(GetQuantizedB(7), 7);
}

TEST(VisualUtilsTest, GetHistogramForImageWhite) {
  VisualFeatures::ColorHistogram histogram;
  SkBitmap bitmap;
  // Initializes to white.
  bitmap.allocN32Pixels(200, 200);

  ASSERT_TRUE(GetHistogramForImage(bitmap, &histogram));
  ASSERT_EQ(histogram.bins_size(), 1);
  EXPECT_THAT(histogram.bins(0).centroid_x(),
              FloatEq(0.4975));  // All pixels are the same color, so centroid_x
                                 // is (0+1+...+199)/200/200 = 0.4975
  EXPECT_THAT(histogram.bins(0).centroid_y(), FloatEq(0.4975));
  EXPECT_EQ(histogram.bins(0).quantized_r(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_g(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_b(), 7);
  EXPECT_THAT(histogram.bins(0).weight(), FloatEq(1.0));
}

TEST(VisualUtilsTest, GetHistogramForImageHalfWhiteHalfBlack) {
  VisualFeatures::ColorHistogram histogram;
  SkBitmap bitmap;
  // Initializes to white.
  bitmap.allocN32Pixels(200, 200);

  // Draw black over half the image.
  for (int x = 0; x < 200; x++)
    for (int y = 0; y < 100; y++)
      *bitmap.getAddr32(x, y) = 0;

  ASSERT_TRUE(GetHistogramForImage(bitmap, &histogram));
  ASSERT_EQ(histogram.bins_size(), 2);

  EXPECT_THAT(histogram.bins(0).centroid_x(), FloatEq(0.4975));
  EXPECT_THAT(histogram.bins(0).centroid_y(), FloatEq(0.7475));
  EXPECT_EQ(histogram.bins(0).quantized_r(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_g(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_b(), 7);
  EXPECT_THAT(histogram.bins(0).weight(), FloatEq(0.5));

  EXPECT_THAT(histogram.bins(1).centroid_x(), FloatEq(0.4975));
  EXPECT_THAT(histogram.bins(1).centroid_y(), FloatEq(0.2475));
  EXPECT_EQ(histogram.bins(1).quantized_r(), 0);
  EXPECT_EQ(histogram.bins(1).quantized_g(), 0);
  EXPECT_EQ(histogram.bins(1).quantized_b(), 0);
  EXPECT_THAT(histogram.bins(1).weight(), FloatEq(0.5));
}

}  // namespace visual_utils
}  // namespace safe_browsing
