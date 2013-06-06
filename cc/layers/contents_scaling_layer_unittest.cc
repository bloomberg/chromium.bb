// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/contents_scaling_layer.h"

#include <vector>

#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MockContentsScalingLayer : public ContentsScalingLayer {
 public:
  MockContentsScalingLayer()
      : ContentsScalingLayer() {}

  virtual void SetNeedsDisplayRect(const gfx::RectF& dirty_rect) OVERRIDE {
    last_needs_display_rect_ = dirty_rect;
    ContentsScalingLayer::SetNeedsDisplayRect(dirty_rect);
  }

  void ResetNeedsDisplay() {
    needs_display_ = false;
  }

  const gfx::RectF& LastNeedsDisplayRect() const {
    return last_needs_display_rect_;
  }

 private:
  virtual ~MockContentsScalingLayer() {}

  gfx::RectF last_needs_display_rect_;
};

void CalcDrawProps(Layer* root, float device_scale) {
  LayerList render_surface_layer_list;
  LayerTreeHostCommon::CalculateDrawProperties(
      root,
      gfx::Size(500, 500),
      gfx::Transform(),
      device_scale,
      1.f,
      NULL,
      1024,
      false,
      false,
      &render_surface_layer_list);
}

TEST(ContentsScalingLayerTest, CheckContentsBounds) {
  scoped_refptr<MockContentsScalingLayer> test_layer =
      make_scoped_refptr(new MockContentsScalingLayer());

  scoped_refptr<Layer> root = Layer::Create();
  root->AddChild(test_layer);

  test_layer->SetBounds(gfx::Size(320, 240));
  CalcDrawProps(root.get(), 1.f);
  EXPECT_FLOAT_EQ(1.f, test_layer->contents_scale_x());
  EXPECT_FLOAT_EQ(1.f, test_layer->contents_scale_y());
  EXPECT_EQ(320, test_layer->content_bounds().width());
  EXPECT_EQ(240, test_layer->content_bounds().height());

  CalcDrawProps(root.get(), 2.f);
  EXPECT_EQ(640, test_layer->content_bounds().width());
  EXPECT_EQ(480, test_layer->content_bounds().height());

  test_layer->SetBounds(gfx::Size(10, 20));
  CalcDrawProps(root.get(), 2.f);
  EXPECT_EQ(20, test_layer->content_bounds().width());
  EXPECT_EQ(40, test_layer->content_bounds().height());

  CalcDrawProps(root.get(), 1.33f);
  EXPECT_EQ(14, test_layer->content_bounds().width());
  EXPECT_EQ(27, test_layer->content_bounds().height());
}

}  // namespace
}  // namespace cc
