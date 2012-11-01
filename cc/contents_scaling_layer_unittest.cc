// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/contents_scaling_layer.h"

#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;

class MockContentsScalingLayer : public ContentsScalingLayer {
 public:
  MockContentsScalingLayer()
      : ContentsScalingLayer() {
  }

  virtual void setNeedsDisplayRect(const FloatRect& dirtyRect) OVERRIDE {
    m_lastNeedsDisplayRect = dirtyRect;
    ContentsScalingLayer::setNeedsDisplayRect(dirtyRect);
  }

  void resetNeedsDisplay() {
      m_needsDisplay = false;
  }

  const FloatRect& lastNeedsDisplayRect() const {
    return m_lastNeedsDisplayRect;
  }

 private:
  virtual ~MockContentsScalingLayer() {
  }

  FloatRect m_lastNeedsDisplayRect;
};

TEST(ContentsScalingLayerTest, checkContentsBounds) {
  scoped_refptr<MockContentsScalingLayer> testLayer =
      make_scoped_refptr(new MockContentsScalingLayer());

  testLayer->setBounds(IntSize(320, 240));
  EXPECT_FLOAT_EQ(1.0, testLayer->contentsScaleX());
  EXPECT_FLOAT_EQ(1.0, testLayer->contentsScaleY());
  EXPECT_EQ(320, testLayer->contentBounds().width());
  EXPECT_EQ(240, testLayer->contentBounds().height());

  testLayer->setContentsScale(2.0f);
  EXPECT_EQ(640, testLayer->contentBounds().width());
  EXPECT_EQ(480, testLayer->contentBounds().height());

  testLayer->setBounds(IntSize(10, 20));
  EXPECT_EQ(20, testLayer->contentBounds().width());
  EXPECT_EQ(40, testLayer->contentBounds().height());

  testLayer->setContentsScale(1.33f);
  EXPECT_EQ(14, testLayer->contentBounds().width());
  EXPECT_EQ(27, testLayer->contentBounds().height());
}

TEST(ContentsScalingLayerTest, checkContentsScaleChangeTriggersNeedsDisplay) {
  scoped_refptr<MockContentsScalingLayer> testLayer =
      make_scoped_refptr(new MockContentsScalingLayer());

  testLayer->setBounds(IntSize(320, 240));

  testLayer->resetNeedsDisplay();
  EXPECT_FALSE(testLayer->needsDisplay());

  testLayer->setContentsScale(testLayer->contentsScaleX() + 1.f);
  EXPECT_TRUE(testLayer->needsDisplay());
  EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 320, 240),
                       testLayer->lastNeedsDisplayRect());
}
