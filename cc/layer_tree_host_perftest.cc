// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "cc/content_layer.h"
#include "cc/nine_patch_layer.h"
#include "cc/solid_color_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_test_common.h"

namespace cc {
namespace {

class LayerTreeHostPerfTest : public WebKitTests::ThreadedTest {};

class LayerTreeHostPerfTestSevenTabSwitcher : public LayerTreeHostPerfTest {
 public:
  LayerTreeHostPerfTestSevenTabSwitcher() : num_draws_(0) {
    fake_delegate_.setPaintAllOpaque(true);
  }

  scoped_refptr<Layer> CreateLayer(float x, float y, int width, int height) {
    scoped_refptr<Layer> layer = Layer::create();
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    return layer;
  }

  scoped_refptr<ContentLayer> CreateContentLayer(float x, float y, int width, int height) {
    scoped_refptr<ContentLayer> layer = ContentLayer::create(&fake_delegate_);
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    layer->setIsDrawable(true);
    return layer;
  }

  scoped_refptr<SolidColorLayer> CreateColorLayer(float x, float y, int width, int height) {
    scoped_refptr<SolidColorLayer> layer = SolidColorLayer::create();
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    layer->setIsDrawable(true);
    return layer;
  }

  scoped_refptr<NinePatchLayer> CreateDecorationLayer(float x, float y, int width, int height) {
    scoped_refptr<NinePatchLayer> layer = NinePatchLayer::create();
    layer->setAnchorPoint(gfx::Point());
    layer->setPosition(gfx::PointF(x, y));
    layer->setBounds(gfx::Size(width, height));
    layer->setIsDrawable(true);

    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 1, 1);
    bitmap.allocPixels(NULL, NULL);
    layer->setBitmap(bitmap, gfx::Rect(0, 0, width, height));

    return layer;
  }

  scoped_refptr<Layer> addChild(scoped_refptr<Layer> parent, scoped_refptr<Layer> child) {
    parent->addChild(child);
    return child;
  }

  virtual void beginTest() OVERRIDE {
    scoped_refptr<Layer> root = CreateLayer(0, 0, 720, 1038); // 1
    scoped_refptr<Layer> layer;

    gfx::Transform down_scale_matrix;
    down_scale_matrix.Scale(0.747, 0.747);

    layer = addChild(root, CreateLayer(0, 0, 0, 0)); // 2

    layer = addChild(root, CreateLayer(628, 15, 0, 0)); // 5
    layer = addChild(root, CreateDecorationLayer(564, -49, 665, 274)); // 13
    layer = addChild(root, CreateDecorationLayer(-16, -16, 569, 807)); // 12
    layer = addChild(root, CreateColorLayer(628, 15, 720, 1038)); // 11
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 16, 720, 1038)); // 34
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 15, 0, 0)); // 6
    layer = addChild(root, CreateDecorationLayer(612, -1, 569, 807)); // 10
    layer = addChild(root, CreateDecorationLayer(827.135986, -1, 354, 96)); // 9
    layer = addChild(root, CreateContentLayer(628, 15, 0, 0)); // 8
    layer = addChild(root, CreateContentLayer(627.418, 15, 0, 0)); // 7

    layer = addChild(root, CreateLayer(628, 161, 0, 0)); // 74
    layer = addChild(root, CreateDecorationLayer(564, 97, 665, 383)); // 82
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 81
    layer = addChild(root, CreateColorLayer(628, 161, 720, 1038)); // 80
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 161, 720, 1038)); // 44
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 161, 0, 0)); // 75
    layer = addChild(root, CreateDecorationLayer(612, 145, 569, 807)); // 79
    layer = addChild(root, CreateDecorationLayer(827.135986, 145, 354, 96)); // 78
    layer = addChild(root, CreateContentLayer(628, 161, 0, 0)); // 77
    layer = addChild(root, CreateContentLayer(627.418, 161, 0, 0)); // 76

    layer = addChild(root, CreateLayer(628, 417, 0, 0)); // 83
    layer = addChild(root, CreateDecorationLayer(564, 353, 665, 445)); // 91
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 90
    layer = addChild(root, CreateColorLayer(628, 417, 720, 1038)); // 89
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 417, 720, 1038)); // 54
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 417, 0, 0)); // 84
    layer = addChild(root, CreateDecorationLayer(612, 401, 569, 807)); // 88
    layer = addChild(root, CreateDecorationLayer(827.135986, 401, 354, 96)); // 87
    layer = addChild(root, CreateContentLayer(628, 417, 0, 0)); // 86
    layer = addChild(root, CreateContentLayer(627.418, 417, 0, 0)); // 85

    layer = addChild(root, CreateLayer(628, 735, 0, 0)); // 92
    layer = addChild(root, CreateDecorationLayer(564, 671, 665, 439)); // 100
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 99
    layer = addChild(root, CreateColorLayer(628, 735, 720, 1038)); // 98
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(628, 735, 720, 1038)); // 64
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(628, 735, 0, 0)); // 93
    layer = addChild(root, CreateDecorationLayer(612, 719, 569, 807)); // 97
    layer = addChild(root, CreateDecorationLayer(827.135986, 719, 354, 96)); // 96
    layer = addChild(root, CreateContentLayer(628, 735, 0, 0)); // 95
    layer = addChild(root, CreateContentLayer(627.418, 735, 0, 0)); // 94

    layer = addChild(root, CreateLayer(30, 15, 0, 0)); // 101
    layer = addChild(root, CreateDecorationLayer(-34, -49, 665, 337)); // 109
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 108
    layer = addChild(root, CreateColorLayer(30, 15, 720, 1038)); // 107
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(30, 15, 0, 0)); // 3
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(30, 15, 0, 0)); // 102
    layer = addChild(root, CreateDecorationLayer(14, -1, 569, 807)); // 106
    layer = addChild(root, CreateDecorationLayer(229.135986, -1, 354, 96)); // 105
    layer = addChild(root, CreateContentLayer(30, 15, 0, 0)); // 104
    layer = addChild(root, CreateContentLayer(30, 15, 0, 0)); // 103

    layer = addChild(root, CreateLayer(30, 227, 0, 0)); // 110
    layer = addChild(root, CreateDecorationLayer(-34, 163, 665, 517)); // 118
    layer = addChild(root, CreateDecorationLayer(0, 0, 569, 807)); // 117
    layer = addChild(root, CreateColorLayer(30, 227, 720, 1038)); // 116
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(30, 227, 720, 1038)); // 4
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(30, 227, 0, 0)); // 111
    layer = addChild(root, CreateDecorationLayer(14, 211, 569, 807)); // 115
    layer = addChild(root, CreateDecorationLayer(229.135986, 211, 354, 96)); // 114
    layer = addChild(root, CreateContentLayer(30, 227, 0, 0)); // 113
    layer = addChild(root, CreateContentLayer(30, 227, 0, 0)); // 112

    layer = addChild(root, CreateLayer(30, 617, 0, 0)); // 119
    layer = addChild(root, CreateDecorationLayer(-34, 553, 665, 559)); // 127
    layer = addChild(root, CreateDecorationLayer(136.349190, 566.524940, 569, 807)); // 126
    layer = addChild(root, CreateColorLayer(30, 617, 720, 1038)); // 125
    layer->setTransform(down_scale_matrix);
    layer = addChild(root, CreateContentLayer(30, 617, 720, 1038)); // 14
    layer->setTransform(down_scale_matrix);

    layer = addChild(root, CreateLayer(30, 617, 0, 0)); // 120
    layer = addChild(root, CreateDecorationLayer(14, 601, 569, 807)); // 124
    layer = addChild(root, CreateDecorationLayer(229.135986, 601, 354, 96)); // 123
    layer = addChild(root, CreateContentLayer(30, 617, 0, 0)); // 122
    layer = addChild(root, CreateContentLayer(30, 617, 0, 0)); // 121

    m_layerTreeHost->setViewportSize(gfx::Size(720, 1038), gfx::Size(720, 1038));
    m_layerTreeHost->setRootLayer(root);
    postSetNeedsCommitToMainThread();
  }

  virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE {
    impl->setNeedsRedraw();
    ++num_draws_;
    if (num_draws_ > 120)
      endTest();
  }

  virtual void afterTest() OVERRIDE {}

 private:
  int num_draws_;
  WebKitTests::FakeContentLayerClient fake_delegate_;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostPerfTestSevenTabSwitcher);

}  // namespace
}  // namespace cc
