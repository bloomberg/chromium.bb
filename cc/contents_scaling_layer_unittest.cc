// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/contents_scaling_layer.h"

#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MockContentsScalingLayer : public ContentsScalingLayer {
 public:
  MockContentsScalingLayer()
      : ContentsScalingLayer() {
  }

  virtual void SetNeedsDisplayRect(const gfx::RectF& dirtyRect) OVERRIDE {
    m_lastNeedsDisplayRect = dirtyRect;
    ContentsScalingLayer::SetNeedsDisplayRect(dirtyRect);
  }

  void resetNeedsDisplay() {
      needs_display_ = false;
  }

  const gfx::RectF& lastNeedsDisplayRect() const {
     return m_lastNeedsDisplayRect;
  }

  void updateContentsScale(float contentsScale) {
      // Simulate calcDrawProperties.
      CalculateContentsScale(
          contentsScale,
          false,  // animating_transform_to_screen
          &draw_properties().contents_scale_x,
          &draw_properties().contents_scale_y,
          &draw_properties().content_bounds);
  }

 private:
  virtual ~MockContentsScalingLayer() {
  }

  gfx::RectF m_lastNeedsDisplayRect;
};

void calcDrawProps(Layer* root, float deviceScale)
{
    std::vector<scoped_refptr<Layer> > renderSurfaceLayerList;
    LayerTreeHostCommon::calculateDrawProperties(
        root,
        gfx::Size(500, 500),
        deviceScale,
        1,
        1024,
        false,
        renderSurfaceLayerList);
}

TEST(ContentsScalingLayerTest, checkContentsBounds) {
  scoped_refptr<MockContentsScalingLayer> testLayer =
      make_scoped_refptr(new MockContentsScalingLayer());

  scoped_refptr<Layer> root = Layer::Create();
  root->AddChild(testLayer);

  testLayer->SetBounds(gfx::Size(320, 240));
  calcDrawProps(root, 1.0);
  EXPECT_FLOAT_EQ(1.0, testLayer->contents_scale_x());
  EXPECT_FLOAT_EQ(1.0, testLayer->contents_scale_y());
  EXPECT_EQ(320, testLayer->content_bounds().width());
  EXPECT_EQ(240, testLayer->content_bounds().height());

  calcDrawProps(root, 2.0);
  EXPECT_EQ(640, testLayer->content_bounds().width());
  EXPECT_EQ(480, testLayer->content_bounds().height());

  testLayer->SetBounds(gfx::Size(10, 20));
  calcDrawProps(root, 2.0);
  EXPECT_EQ(20, testLayer->content_bounds().width());
  EXPECT_EQ(40, testLayer->content_bounds().height());

  calcDrawProps(root, 1.33f);
  EXPECT_EQ(14, testLayer->content_bounds().width());
  EXPECT_EQ(27, testLayer->content_bounds().height());
}

}  // namespace
}  // namespace cc
