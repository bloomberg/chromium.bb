// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_test.h"

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

  WebKit::WebFilterOperations filters;
  filters.append(WebKit::WebFilterOperation::createBlurFilter(2));
  blur->SetBackgroundFilters(filters);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "background_filter_blur.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
