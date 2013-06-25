// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/render_surface_filters.h"

#include "cc/output/filter_operation.h"
#include "cc/output/filter_operations.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

// Checks whether op can be combined with a following color matrix.
bool IsCombined(const FilterOperation& op) {
  FilterOperations filters;
  filters.Append(op);
  // brightness(0.0f) is identity.
  filters.Append(FilterOperation::CreateBrightnessFilter(0.0f));
  FilterOperations optimized = RenderSurfaceFilters::Optimize(filters);
  return optimized.size() == 1;
}

TEST(RenderSurfaceFiltersTest, TestColorMatrixFiltersCombined) {
  // Several filters should always combine for any amount between 0 and 1:
  // grayscale, saturate, invert, contrast, opacity.
  EXPECT_TRUE(IsCombined(FilterOperation::CreateGrayscaleFilter(0.0f)));
  // Note that we use 0.3f to avoid "argument is truncated from 'double' to
  // 'float'" warnings on Windows. 0.5f is exactly representable as a float, so
  // there is no warning.
  EXPECT_TRUE(IsCombined(FilterOperation::CreateGrayscaleFilter(0.3f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateGrayscaleFilter(0.5f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateGrayscaleFilter(1.0f)));

  EXPECT_TRUE(IsCombined(FilterOperation::CreateSaturateFilter(0.0f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateSaturateFilter(0.3f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateSaturateFilter(0.5)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateSaturateFilter(1.0f)));

  EXPECT_TRUE(IsCombined(FilterOperation::CreateInvertFilter(0.0f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateInvertFilter(0.3f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateInvertFilter(0.5)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateInvertFilter(1.0f)));

  EXPECT_TRUE(IsCombined(FilterOperation::CreateContrastFilter(0.0f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateContrastFilter(0.3f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateContrastFilter(0.5)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateContrastFilter(1.0f)));

  EXPECT_TRUE(IsCombined(FilterOperation::CreateOpacityFilter(0.0f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateOpacityFilter(0.3f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateOpacityFilter(0.5)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateOpacityFilter(1.0f)));

  // Brightness combines when amount <= 1
  EXPECT_TRUE(IsCombined(FilterOperation::CreateBrightnessFilter(0.5)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateBrightnessFilter(1.0f)));
  EXPECT_FALSE(IsCombined(FilterOperation::CreateBrightnessFilter(1.5)));

  // SaturatingBrightness combines only when amount == 0
  EXPECT_TRUE(
      IsCombined(FilterOperation::CreateSaturatingBrightnessFilter(0.0f)));
  EXPECT_FALSE(
      IsCombined(FilterOperation::CreateSaturatingBrightnessFilter(0.5)));
  EXPECT_FALSE(
      IsCombined(FilterOperation::CreateSaturatingBrightnessFilter(1.0f)));

  // Several filters should never combine: blur, drop-shadow.
  EXPECT_FALSE(IsCombined(FilterOperation::CreateBlurFilter(3.0f)));
  EXPECT_FALSE(IsCombined(FilterOperation::CreateDropShadowFilter(
      gfx::Point(2, 2), 3.0f, 0xffffffff)));

  // sepia and hue may or may not combine depending on the value.
  EXPECT_TRUE(IsCombined(FilterOperation::CreateSepiaFilter(0.0f)));
  EXPECT_FALSE(IsCombined(FilterOperation::CreateSepiaFilter(1.0f)));
  EXPECT_TRUE(IsCombined(FilterOperation::CreateHueRotateFilter(0.0f)));
  EXPECT_FALSE(IsCombined(FilterOperation::CreateHueRotateFilter(180.0f)));

  float matrix1[20] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, };
  EXPECT_TRUE(IsCombined(FilterOperation::CreateColorMatrixFilter(matrix1)));

  float matrix2[20] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, };
  EXPECT_FALSE(
      IsCombined(FilterOperation::CreateColorMatrixFilter(matrix2)));

  float matrix3[20] = { 0.25f, 0.0f, 0.0f, 0.0f, 255.0f * 0.75f,
                        0.0f,  1.0f, 0.0f, 0.0f, 0.0f,
                        0.0f,  0.0f, 1.0f, 0.0f, 0.0f,
                        0.0f,  0.0f, 0.0f, 1.0f, 0.0f, };
  EXPECT_TRUE(IsCombined(FilterOperation::CreateColorMatrixFilter(matrix3)));

  float matrix4[20] = { -0.25f, 0.75f, 0.0f, 0.0f, 255.0f * 0.25f,
                         0.0f,  1.0f,  0.0f, 0.0f, 0.0f,
                         0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
                         0.0f,  0.0f,  0.0f, 1.0f, 0.0f, };
  EXPECT_TRUE(IsCombined(FilterOperation::CreateColorMatrixFilter(matrix4)));
}

TEST(RenderSurfaceFiltersTest, TestOptimize) {
  FilterOperation combines(FilterOperation::CreateBrightnessFilter(1.0f));
  FilterOperation doesnt_combine(
      FilterOperation::CreateBrightnessFilter(1.5f));

  FilterOperations filters;
  FilterOperations optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(0u, optimized.size());

  filters.Append(combines);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(1u, optimized.size());

  filters.Append(combines);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(1u, optimized.size());

  filters.Append(doesnt_combine);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(1u, optimized.size());

  filters.Append(combines);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(2u, optimized.size());

  filters.Append(doesnt_combine);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(2u, optimized.size());

  filters.Append(doesnt_combine);
  optimized = RenderSurfaceFilters::Optimize(filters);
  EXPECT_EQ(3u, optimized.size());
}

}  // namespace
}  // namespace cc
