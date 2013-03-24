// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/render_surface_filters.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperation.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFilterOperations.h"

using namespace WebKit;

namespace cc {
namespace {

// Checks whether op can be combined with a following color matrix.
bool IsCombined(const WebFilterOperation& op) {
  WebFilterOperations filters;
  filters.append(op);
  // brightness(0.0f) is identity.
  filters.append(WebFilterOperation::createBrightnessFilter(0.0f));
  WebFilterOperations optimized = RenderSurfaceFilters::Optimize(filters);
  return optimized.size() == 1;
}

TEST(RenderSurfaceFiltersTest, TestColorMatrixFiltersCombined) {
  // Several filters should always combine for any amount between 0 and 1:
  // grayscale, saturate, invert, contrast, opacity.
  EXPECT_TRUE(IsCombined(WebFilterOperation::createGrayscaleFilter(0.0f)));
  // Note that we use 0.3f to avoid "argument is truncated from 'double' to
  // 'float'" warnings on Windows. 0.5f is exactly representable as a float, so
  // there is no warning.
  EXPECT_TRUE(IsCombined(WebFilterOperation::createGrayscaleFilter(0.3f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createGrayscaleFilter(0.5f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createGrayscaleFilter(1.0f)));

  EXPECT_TRUE(IsCombined(WebFilterOperation::createSaturateFilter(0.0f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createSaturateFilter(0.3f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createSaturateFilter(0.5)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createSaturateFilter(1.0f)));

  EXPECT_TRUE(IsCombined(WebFilterOperation::createInvertFilter(0.0f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createInvertFilter(0.3f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createInvertFilter(0.5)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createInvertFilter(1.0f)));

  EXPECT_TRUE(IsCombined(WebFilterOperation::createContrastFilter(0.0f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createContrastFilter(0.3f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createContrastFilter(0.5)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createContrastFilter(1.0f)));

  EXPECT_TRUE(IsCombined(WebFilterOperation::createOpacityFilter(0.0f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createOpacityFilter(0.3f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createOpacityFilter(0.5)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createOpacityFilter(1.0f)));

  // Brightness combines when amount <= 1
  EXPECT_TRUE(IsCombined(WebFilterOperation::createBrightnessFilter(0.5)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createBrightnessFilter(1.0f)));
  EXPECT_FALSE(IsCombined(WebFilterOperation::createBrightnessFilter(1.5)));

  // SaturatingBrightness combines only when amount == 0
  EXPECT_TRUE(
      IsCombined(WebFilterOperation::createSaturatingBrightnessFilter(0.0f)));
  EXPECT_FALSE(
      IsCombined(WebFilterOperation::createSaturatingBrightnessFilter(0.5)));
  EXPECT_FALSE(
      IsCombined(WebFilterOperation::createSaturatingBrightnessFilter(1.0f)));

  // Several filters should never combine: blur, drop-shadow.
  EXPECT_FALSE(IsCombined(WebFilterOperation::createBlurFilter(3.0f)));
  EXPECT_FALSE(IsCombined(WebFilterOperation::createDropShadowFilter(
      WebPoint(2, 2), 3.0f, 0xffffffff)));

  // sepia and hue may or may not combine depending on the value.
  EXPECT_TRUE(IsCombined(WebFilterOperation::createSepiaFilter(0.0f)));
  EXPECT_FALSE(IsCombined(WebFilterOperation::createSepiaFilter(1.0f)));
  EXPECT_TRUE(IsCombined(WebFilterOperation::createHueRotateFilter(0.0f)));
  EXPECT_FALSE(IsCombined(WebFilterOperation::createHueRotateFilter(180.0f)));

  float matrix1[20] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, };
  EXPECT_TRUE(IsCombined(WebFilterOperation::createColorMatrixFilter(matrix1)));

  float matrix2[20] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, };
  EXPECT_FALSE(
      IsCombined(WebFilterOperation::createColorMatrixFilter(matrix2)));

  float matrix3[20] = { 0.25f, 0.0f, 0.0f, 0.0f, 255.0f * 0.75f,
                        0.0f,  1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f,  0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f,  0.0f, 0.0f, 1.0f, 0.0f, };
  EXPECT_TRUE(IsCombined(WebFilterOperation::createColorMatrixFilter(matrix3)));

  float matrix4[20] = { -0.25f, 0.75f, 0.0f, 0.0f, 255.0f * 0.25f,
                         0.0f,  1.0f,  0.0f, 0.0f, 0.0f,
                         0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
                         0.0f,  0.0f,  0.0f, 1.0f, 0.0f, };
  EXPECT_TRUE(IsCombined(WebFilterOperation::createColorMatrixFilter(matrix4)));
}

TEST(RenderSurfaceFiltersTest, TestOptimize) {
  WebFilterOperation combines(WebFilterOperation::createBrightnessFilter(1.0f));
  WebFilterOperation doesnt_combine(
      WebFilterOperation::createBrightnessFilter(1.5f));

  WebFilterOperations filters;
  WebFilterOperations optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(0u, optimized.size());

  filters.append(combines);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(1u, optimized.size());

  filters.append(combines);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(1u, optimized.size());

  filters.append(doesnt_combine);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(1u, optimized.size());

  filters.append(combines);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(2u, optimized.size());

  filters.append(doesnt_combine);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(2u, optimized.size());

  filters.append(doesnt_combine);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(3u, optimized.size());
}

}  // namespace
}  // namespace cc
