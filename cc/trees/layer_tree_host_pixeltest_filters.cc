// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostFiltersPixelTest : public LayerTreePixelTest {};

TEST_F(LayerTreeHostFiltersPixelTest, BackgroundFilterBlur) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // The green box is entirely behind a layer with background blur, so it
  // should appear blurred on its edges.
  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(30, 30, 140, 140), SK_ColorTRANSPARENT);
  background->AddChild(green);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));
  blur->SetBackgroundFilters(filters);

#if defined(OS_WIN)
  // Windows has 436 pixels off by 1: crbug.com/259915
  float percentage_pixels_large_error = 1.09f;  // 436px / (200*200)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 1;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL("background_filter_blur.png")));
}

TEST_F(LayerTreeHostFiltersPixelTest, DISABLED_BackgroundFilterBlurOutsets) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // The green border is outside the layer with background blur, but the
  // background blur should use pixels from outside its layer borders, up to the
  // radius of the blur effect. So the border should be blurred underneath the
  // top layer causing the green to bleed under the transparent layer, but not
  // in the 1px region between the transparent layer and the green border.
  scoped_refptr<SolidColorLayer> green_border = CreateSolidColorLayerWithBorder(
      gfx::Rect(1, 1, 198, 198), SK_ColorWHITE, 10, kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(12, 12, 176, 176), SK_ColorTRANSPARENT);
  background->AddChild(green_border);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(5.f));
  blur->SetBackgroundFilters(filters);

#if defined(OS_WIN)
  // Windows has 2250 pixels off by at most 2: crbug.com/259922
  float percentage_pixels_large_error = 5.625f;  // 2250px / (200*200)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 2;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "background_filter_blur_outsets.png")));
}

TEST_F(LayerTreeHostFiltersPixelTest, BackgroundFilterBlurOffAxis) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  // This verifies that the perspective of the clear layer (with black border)
  // does not influence the blending of the green box behind it. Also verifies
  // that the blur is correctly clipped inside the transformed clear layer.
  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(50, 50, 100, 100), kCSSGreen);
  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayerWithBorder(
      gfx::Rect(30, 30, 120, 120), SK_ColorTRANSPARENT, 1, SK_ColorBLACK);
  background->AddChild(green);
  background->AddChild(blur);

  background->SetPreserves3d(true);
  gfx::Transform background_transform;
  background_transform.ApplyPerspectiveDepth(200.0);
  background->SetTransform(background_transform);

  blur->SetPreserves3d(true);
  gfx::Transform blur_transform;
  blur_transform.Translate(55.0, 65.0);
  blur_transform.RotateAboutXAxis(85.0);
  blur_transform.RotateAboutYAxis(180.0);
  blur_transform.RotateAboutZAxis(20.0);
  blur_transform.Translate(-60.0, -60.0);
  blur->SetTransform(blur_transform);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateBlurFilter(2.f));
  blur->SetBackgroundFilters(filters);

#if defined(OS_WIN)
  // Windows has 151 pixels off by at most 2: crbug.com/225027
  float percentage_pixels_large_error = 0.3775f;  // 151px / (200*200)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 1.f;
  int large_error_allowed = 2;
  int small_error_allowed = 0;
  pixel_comparator_.reset(new FuzzyPixelComparator(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed));
#endif

  RunPixelTest(GL_WITH_BITMAP,
               background,
               base::FilePath(FILE_PATH_LITERAL(
                   "background_filter_blur_off_axis.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
