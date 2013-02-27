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

  virtual void setNeedsDisplayRect(const gfx::RectF& dirtyRect) OVERRIDE {
    m_lastNeedsDisplayRect = dirtyRect;
    ContentsScalingLayer::setNeedsDisplayRect(dirtyRect);
  }

  void resetNeedsDisplay() {
      m_needsDisplay = false;
  }

  const gfx::RectF& lastNeedsDisplayRect() const {
     return m_lastNeedsDisplayRect;
  }

  void updateContentsScale(float contentsScale) {
      // Simulate calcDrawProperties.
      calculateContentsScale(
          contentsScale,
          false,  // animating_transform_to_screen
          &drawProperties().contents_scale_x,
          &drawProperties().contents_scale_y,
          &drawProperties().content_bounds);
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

  scoped_refptr<Layer> root = Layer::create();
  root->addChild(testLayer);

  testLayer->setBounds(gfx::Size(320, 240));
  calcDrawProps(root, 1.0);
  EXPECT_FLOAT_EQ(1.0, testLayer->contentsScaleX());
  EXPECT_FLOAT_EQ(1.0, testLayer->contentsScaleY());
  EXPECT_EQ(320, testLayer->contentBounds().width());
  EXPECT_EQ(240, testLayer->contentBounds().height());

  calcDrawProps(root, 2.0);
  EXPECT_EQ(640, testLayer->contentBounds().width());
  EXPECT_EQ(480, testLayer->contentBounds().height());

  testLayer->setBounds(gfx::Size(10, 20));
  calcDrawProps(root, 2.0);
  EXPECT_EQ(20, testLayer->contentBounds().width());
  EXPECT_EQ(40, testLayer->contentBounds().height());

  calcDrawProps(root, 1.33f);
  EXPECT_EQ(14, testLayer->contentBounds().width());
  EXPECT_EQ(27, testLayer->contentBounds().height());
}

}  // namespace
}  // namespace cc
