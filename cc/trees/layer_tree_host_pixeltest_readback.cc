// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/test/layer_tree_pixel_test.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostReadbackPixelTest : public LayerTreePixelTest {};

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL(
                   "green_with_blue_corner.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small.png")));
}

TEST_F(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayerWithChild) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayer(
      gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue = CreateSolidColorLayer(
      gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(background,
                                 green.get(),
                                 base::FilePath(FILE_PATH_LITERAL(
                                     "green_small_with_blue_corner.png")));
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
