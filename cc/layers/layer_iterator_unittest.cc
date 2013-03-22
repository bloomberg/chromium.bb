// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer_iterator.h"

#include "cc/layers/layer.h"
#include "cc/trees/layer_tree_host_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

namespace cc {
namespace {

class TestLayer : public Layer {
 public:
  static scoped_refptr<TestLayer> Create() {
    return make_scoped_refptr(new TestLayer());
  }

  int count_representing_target_surface_;
  int count_representing_contributing_surface_;
  int count_representing_itself_;

  virtual bool DrawsContent() const OVERRIDE { return draws_content_; }
  void set_draws_content(bool draws_content) { draws_content_ = draws_content; }

 private:
  TestLayer() : Layer(), draws_content_(true) {
    SetBounds(gfx::Size(100, 100));
    SetPosition(gfx::Point());
    SetAnchorPoint(gfx::Point());
  }
  virtual ~TestLayer() {}

  bool draws_content_;
};

#define EXPECT_COUNT(layer, target, contrib, itself)                           \
  EXPECT_EQ(target, layer->count_representing_target_surface_);                \
  EXPECT_EQ(contrib, layer->count_representing_contributing_surface_);         \
  EXPECT_EQ(itself, layer->count_representing_itself_);

typedef LayerIterator<Layer,
                      std::vector<scoped_refptr<Layer> >,
                      RenderSurface,
                      LayerIteratorActions::FrontToBack> FrontToBack;
typedef LayerIterator<Layer,
                      std::vector<scoped_refptr<Layer> >,
                      RenderSurface,
                      LayerIteratorActions::BackToFront> BackToFront;

void ResetCounts(std::vector<scoped_refptr<Layer> >* render_surface_layerList) {
  for (unsigned surface_index = 0;
       surface_index < render_surface_layerList->size();
       ++surface_index) {
    TestLayer* render_surface_layer = static_cast<TestLayer*>(
        (*render_surface_layerList)[surface_index].get());
    RenderSurface* render_surface = render_surface_layer->render_surface();

    render_surface_layer->count_representing_target_surface_ = -1;
    render_surface_layer->count_representing_contributing_surface_ = -1;
    render_surface_layer->count_representing_itself_ = -1;

    for (unsigned layer_index = 0;
         layer_index < render_surface->layer_list().size();
         ++layer_index) {
      TestLayer* layer = static_cast<TestLayer*>(
          render_surface->layer_list()[layer_index].get());

      layer->count_representing_target_surface_ = -1;
      layer->count_representing_contributing_surface_ = -1;
      layer->count_representing_itself_ = -1;
    }
  }
}

void IterateFrontToBack(
    std::vector<scoped_refptr<Layer> >* render_surface_layerList) {
  ResetCounts(render_surface_layerList);
  int count = 0;
  for (FrontToBack it = FrontToBack::Begin(render_surface_layerList);
       it != FrontToBack::End(render_surface_layerList);
       ++it, ++count) {
    TestLayer* layer = static_cast<TestLayer*>(*it);
    if (it.represents_target_render_surface())
      layer->count_representing_target_surface_ = count;
    if (it.represents_contributing_render_surface())
      layer->count_representing_contributing_surface_ = count;
    if (it.represents_itself())
      layer->count_representing_itself_ = count;
  }
}

void IterateBackToFront(
    std::vector<scoped_refptr<Layer> >* render_surface_layerList) {
  ResetCounts(render_surface_layerList);
  int count = 0;
  for (BackToFront it = BackToFront::Begin(render_surface_layerList);
       it != BackToFront::End(render_surface_layerList);
       ++it, ++count) {
    TestLayer* layer = static_cast<TestLayer*>(*it);
    if (it.represents_target_render_surface())
      layer->count_representing_target_surface_ = count;
    if (it.represents_contributing_render_surface())
      layer->count_representing_contributing_surface_ = count;
    if (it.represents_itself())
      layer->count_representing_itself_ = count;
  }
}

TEST(LayerIteratorTest, EmptyTree) {
  std::vector<scoped_refptr<Layer> > render_surface_layerList;

  IterateBackToFront(&render_surface_layerList);
  IterateFrontToBack(&render_surface_layerList);
}

TEST(LayerIteratorTest, SimpleTree) {
  scoped_refptr<TestLayer> root_layer = TestLayer::Create();
  scoped_refptr<TestLayer> first = TestLayer::Create();
  scoped_refptr<TestLayer> second = TestLayer::Create();
  scoped_refptr<TestLayer> third = TestLayer::Create();
  scoped_refptr<TestLayer> fourth = TestLayer::Create();

  root_layer->CreateRenderSurface();

  root_layer->AddChild(first);
  root_layer->AddChild(second);
  root_layer->AddChild(third);
  root_layer->AddChild(fourth);

  std::vector<scoped_refptr<Layer> > render_surface_layerList;
  LayerTreeHostCommon::CalculateDrawProperties(root_layer.get(),
                                               root_layer->bounds(),
                                               1,
                                               1,
                                               256,
                                               false,
                                               &render_surface_layerList);

  IterateBackToFront(&render_surface_layerList);
  EXPECT_COUNT(root_layer, 0, -1, 1);
  EXPECT_COUNT(first, -1, -1, 2);
  EXPECT_COUNT(second, -1, -1, 3);
  EXPECT_COUNT(third, -1, -1, 4);
  EXPECT_COUNT(fourth, -1, -1, 5);

  IterateFrontToBack(&render_surface_layerList);
  EXPECT_COUNT(root_layer, 5, -1, 4);
  EXPECT_COUNT(first, -1, -1, 3);
  EXPECT_COUNT(second, -1, -1, 2);
  EXPECT_COUNT(third, -1, -1, 1);
  EXPECT_COUNT(fourth, -1, -1, 0);

}

TEST(LayerIteratorTest, ComplexTree) {
  scoped_refptr<TestLayer> root_layer = TestLayer::Create();
  scoped_refptr<TestLayer> root1 = TestLayer::Create();
  scoped_refptr<TestLayer> root2 = TestLayer::Create();
  scoped_refptr<TestLayer> root3 = TestLayer::Create();
  scoped_refptr<TestLayer> root21 = TestLayer::Create();
  scoped_refptr<TestLayer> root22 = TestLayer::Create();
  scoped_refptr<TestLayer> root23 = TestLayer::Create();
  scoped_refptr<TestLayer> root221 = TestLayer::Create();
  scoped_refptr<TestLayer> root231 = TestLayer::Create();

  root_layer->CreateRenderSurface();

  root_layer->AddChild(root1);
  root_layer->AddChild(root2);
  root_layer->AddChild(root3);
  root2->AddChild(root21);
  root2->AddChild(root22);
  root2->AddChild(root23);
  root22->AddChild(root221);
  root23->AddChild(root231);

  std::vector<scoped_refptr<Layer> > render_surface_layerList;
  LayerTreeHostCommon::CalculateDrawProperties(root_layer.get(),
                                               root_layer->bounds(),
                                               1,
                                               1,
                                               256,
                                               false,
                                               &render_surface_layerList);

  IterateBackToFront(&render_surface_layerList);
  EXPECT_COUNT(root_layer, 0, -1, 1);
  EXPECT_COUNT(root1, -1, -1, 2);
  EXPECT_COUNT(root2, -1, -1, 3);
  EXPECT_COUNT(root21, -1, -1, 4);
  EXPECT_COUNT(root22, -1, -1, 5);
  EXPECT_COUNT(root221, -1, -1, 6);
  EXPECT_COUNT(root23, -1, -1, 7);
  EXPECT_COUNT(root231, -1, -1, 8);
  EXPECT_COUNT(root3, -1, -1, 9);

  IterateFrontToBack(&render_surface_layerList);
  EXPECT_COUNT(root_layer, 9, -1, 8);
  EXPECT_COUNT(root1, -1, -1, 7);
  EXPECT_COUNT(root2, -1, -1, 6);
  EXPECT_COUNT(root21, -1, -1, 5);
  EXPECT_COUNT(root22, -1, -1, 4);
  EXPECT_COUNT(root221, -1, -1, 3);
  EXPECT_COUNT(root23, -1, -1, 2);
  EXPECT_COUNT(root231, -1, -1, 1);
  EXPECT_COUNT(root3, -1, -1, 0);

}

TEST(LayerIteratorTest, ComplexTreeMultiSurface) {
  scoped_refptr<TestLayer> root_layer = TestLayer::Create();
  scoped_refptr<TestLayer> root1 = TestLayer::Create();
  scoped_refptr<TestLayer> root2 = TestLayer::Create();
  scoped_refptr<TestLayer> root3 = TestLayer::Create();
  scoped_refptr<TestLayer> root21 = TestLayer::Create();
  scoped_refptr<TestLayer> root22 = TestLayer::Create();
  scoped_refptr<TestLayer> root23 = TestLayer::Create();
  scoped_refptr<TestLayer> root221 = TestLayer::Create();
  scoped_refptr<TestLayer> root231 = TestLayer::Create();

  root_layer->CreateRenderSurface();
  root_layer->render_surface()->
      SetContentRect(gfx::Rect(gfx::Point(), root_layer->bounds()));

  root_layer->AddChild(root1);
  root_layer->AddChild(root2);
  root_layer->AddChild(root3);
  root2->set_draws_content(false);
  root2->SetOpacity(0.5f);
  root2->SetForceRenderSurface(true);  // Force the layer to own a new surface.
  root2->AddChild(root21);
  root2->AddChild(root22);
  root2->AddChild(root23);
  root22->SetOpacity(0.5f);
  root22->AddChild(root221);
  root23->SetOpacity(0.5f);
  root23->AddChild(root231);

  std::vector<scoped_refptr<Layer> > render_surface_layerList;
  LayerTreeHostCommon::CalculateDrawProperties(root_layer.get(),
                                               root_layer->bounds(),
                                               1,
                                               1,
                                               256,
                                               false,
                                               &render_surface_layerList);

  IterateBackToFront(&render_surface_layerList);
  EXPECT_COUNT(root_layer, 0, -1, 1);
  EXPECT_COUNT(root1, -1, -1, 2);
  EXPECT_COUNT(root2, 4, 3, -1);
  EXPECT_COUNT(root21, -1, -1, 5);
  EXPECT_COUNT(root22, 7, 6, 8);
  EXPECT_COUNT(root221, -1, -1, 9);
  EXPECT_COUNT(root23, 11, 10, 12);
  EXPECT_COUNT(root231, -1, -1, 13);
  EXPECT_COUNT(root3, -1, -1, 14);

  IterateFrontToBack(&render_surface_layerList);
  EXPECT_COUNT(root_layer, 14, -1, 13);
  EXPECT_COUNT(root1, -1, -1, 12);
  EXPECT_COUNT(root2, 10, 11, -1);
  EXPECT_COUNT(root21, -1, -1, 9);
  EXPECT_COUNT(root22, 7, 8, 6);
  EXPECT_COUNT(root221, -1, -1, 5);
  EXPECT_COUNT(root23, 3, 4, 2);
  EXPECT_COUNT(root231, -1, -1, 1);
  EXPECT_COUNT(root3, -1, -1, 0);
}

}  // namespace
}  // namespace cc
