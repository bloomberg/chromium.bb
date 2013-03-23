// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/content_layer.h"

#include "cc/layers/content_layer_client.h"
#include "cc/resources/bitmap_content_layer_updater.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect_conversions.h"

using namespace WebKit;

namespace cc {
namespace {

class MockContentLayerClient : public ContentLayerClient {
 public:
  explicit MockContentLayerClient(gfx::Rect opaque_layer_rect)
      : opaque_layer_rect_(opaque_layer_rect) {}

  virtual void PaintContents(SkCanvas* canvas,
                             gfx::Rect clip,
                             gfx::RectF* opaque) OVERRIDE {
    *opaque = gfx::RectF(opaque_layer_rect_);
  }
  virtual void DidChangeLayerCanUseLCDText() OVERRIDE {}

 private:
  gfx::Rect opaque_layer_rect_;
};

TEST(ContentLayerTest, ContentLayerPainterWithDeviceScale) {
  float contents_scale = 2.f;
  gfx::Rect content_rect(10, 10, 100, 100);
  gfx::Rect opaque_rect_in_layer_space(5, 5, 20, 20);
  gfx::RectF opaque_rect_in_content_space = gfx::ScaleRect(
      opaque_rect_in_layer_space, contents_scale, contents_scale);
  MockContentLayerClient client(opaque_rect_in_layer_space);
  scoped_refptr<BitmapContentLayerUpdater> updater =
      BitmapContentLayerUpdater::Create(ContentLayerPainter::Create(&client).
                                            PassAs<LayerPainter>());

  gfx::Rect resulting_opaque_rect;
  updater->PrepareToUpdate(content_rect,
                           gfx::Size(256, 256),
                           contents_scale,
                           contents_scale,
                           &resulting_opaque_rect,
                           NULL);

  EXPECT_RECT_EQ(gfx::ToEnclosingRect(opaque_rect_in_content_space),
                 resulting_opaque_rect);
}

}  // namespace
}  // namespace cc
