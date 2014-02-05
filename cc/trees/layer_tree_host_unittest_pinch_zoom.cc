// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_host.h"

#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using gfx::Vector2d;

namespace cc {
namespace {

class LayerTreeHostPinchZoomTest : public LayerTreeTest {};

// Ensure that the DistributeScrollOffsetToViewports() method of LayerTreeHost
// does not set values outside the scroll offset max.
class LayerTreeHostPinchZoomTestDistributeScrollOffsetToViewports
    : public LayerTreeHostPinchZoomTest {
 public:
  // Sets up a 2X scale resulting in the following viewport configuration:
  //    +------------------------------------------------+
  //    |              |              |                  |
  //    |              |              |                  |
  //    |              |              |                  |
  //    |  InnerClip   |5px           |                  |
  //    |              |              |                  |
  //    |              |              |10px              |
  //    |--------------+              |                  |
  //    |      5px                    |                  |
  //    |                             |                  | 20px
  //    | Inner Scroll/Outer Clip     |                  |
  //    |                             |                  |
  //    |-----------------------------+                  |
  //    |             10px                               |
  //    |                                                |
  //    |                                                |
  //    |                 Outer Scroll Layer             |
  //    |                                                |
  //    |                                                |
  //    +------------------------------------------------+
  //                         20px
  //
  virtual void SetupTree() OVERRIDE {
    scoped_refptr<Layer> inner_viewport_clip_layer = Layer::Create();
    scoped_refptr<Layer> page_scale_layer = Layer::Create();
    scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
    scoped_refptr<Layer> outer_viewport_clip_layer = Layer::Create();
    scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();

    inner_viewport_scroll_layer->SetScrollClipLayerId(
        inner_viewport_clip_layer->id());
    inner_viewport_scroll_layer->SetUserScrollable(true, true);

    inner_viewport_clip_layer->AddChild(page_scale_layer);
    page_scale_layer->AddChild(inner_viewport_scroll_layer);

    inner_viewport_scroll_layer->AddChild(outer_viewport_clip_layer);
    outer_viewport_clip_layer->AddChild(outer_viewport_scroll_layer);

    outer_viewport_scroll_layer->SetScrollClipLayerId(
        outer_viewport_clip_layer->id());
    outer_viewport_scroll_layer->SetUserScrollable(true, true);

    layer_tree_host()->RegisterViewportLayers(page_scale_layer,
        inner_viewport_scroll_layer,
        outer_viewport_scroll_layer);

    inner_viewport_clip_layer->SetBounds(gfx::Size(10, 10));
    outer_viewport_clip_layer->SetAnchorPoint(gfx::PointF());
    outer_viewport_clip_layer->SetBounds(gfx::Size(10, 10));

    layer_tree_host()->SetRootLayer(inner_viewport_clip_layer);

    inner_viewport_scroll_layer->SetBounds(gfx::Size(10, 10));
    outer_viewport_scroll_layer->SetBounds(gfx::Size(20, 20));
    layer_tree_host()->SetPageScaleFactorAndLimits(2.0f, 1.0f, 5.0f);

    // Sanity check
    EXPECT_VECTOR_EQ(Vector2d(5, 5),
        inner_viewport_scroll_layer->MaxScrollOffset());
    EXPECT_VECTOR_EQ(Vector2d(10, 10),
        outer_viewport_scroll_layer->MaxScrollOffset());
    EXPECT_VECTOR_EQ(Vector2d(0, 0),
        inner_viewport_scroll_layer->scroll_offset());
    EXPECT_VECTOR_EQ(Vector2d(0, 0),
        outer_viewport_scroll_layer->scroll_offset());
  }

  virtual void BeginTest() OVERRIDE {
    Layer *inner = layer_tree_host()->inner_viewport_scroll_layer();
    Layer *outer = layer_tree_host()->outer_viewport_scroll_layer();

    // Try to scroll beyond offset minium
    gfx::Vector2d offset(-20, -20);
    outer->SetScrollOffset(offset);
    EXPECT_VECTOR_EQ(Vector2d(0, 0), inner->scroll_offset());
    EXPECT_VECTOR_EQ(Vector2d(0, 0), outer->scroll_offset());

    // Try to scroll beyond offset maximum
    offset = gfx::Vector2d(20, 20);
    outer->SetScrollOffset(offset);
    EXPECT_VECTOR_EQ(Vector2d(5, 5), inner->scroll_offset());
    EXPECT_VECTOR_EQ(Vector2d(10, 10), outer->scroll_offset());

    EndTest();
  }

  virtual void AfterTest() OVERRIDE {}
};

TEST_F(LayerTreeHostPinchZoomTestDistributeScrollOffsetToViewports, Test) {
    RunTest(true, false, false);
}

}  // namespace
}  // namespace cc
