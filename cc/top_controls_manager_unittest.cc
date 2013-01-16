// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/top_controls_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/top_controls_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vector2d_f.h"

namespace cc {
namespace {

static const float kTopControlsHeight = 100;

class MockTopControlsManagerClient : public TopControlsManagerClient {
 public:
  MockTopControlsManagerClient()
      : host_impl_(&proxy_),
        redraw_needed_(false),
        update_draw_properties_needed_(false) {
    active_tree_ = LayerTreeImpl::create(&host_impl_);
    root_scroll_layer_ = LayerImpl::create(active_tree_.get(), 1);
    active_tree_->set_root_scroll_layer(root_scroll_layer_.get());
  }

  virtual ~MockTopControlsManagerClient() {}

  virtual void setNeedsRedraw() OVERRIDE {
    redraw_needed_ = true;
  }

  virtual void setNeedsUpdateDrawProperties() OVERRIDE {
    update_draw_properties_needed_ = true;
  }

  virtual LayerTreeImpl* activeTree() OVERRIDE {
    return active_tree_.get();
  }

  TopControlsManager* manager() {
    if (!manager_)
      manager_ = TopControlsManager::Create(this, kTopControlsHeight);
    return manager_.get();
  }

  LayerImpl* rootScrollLayer() {
    return root_scroll_layer_.get();
  }

 private:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<LayerTreeImpl> active_tree_;
  scoped_ptr<LayerImpl> root_scroll_layer_;
  scoped_ptr<TopControlsManager> manager_;
  bool redraw_needed_;
  bool update_draw_properties_needed_;
};

TEST(TopControlsManagerTest, overlayModeDetection) {
  MockTopControlsManagerClient client;
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollDelta(gfx::Vector2dF(0.f, 0.f));

  gfx::Vector2dF remaining_scroll = manager->ScrollBy(
      gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(70.f, manager->content_top_offset());
  EXPECT_FALSE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, 69.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-99.f, manager->controls_top_offset());
  EXPECT_EQ(1.f, manager->content_top_offset());
  EXPECT_FALSE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -20.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-79.f, manager->controls_top_offset());
  EXPECT_EQ(21.f, manager->content_top_offset());
  EXPECT_FALSE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  // Scroll to the toggle point
  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, 21.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_FALSE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, 1.f));
  EXPECT_EQ(1.f, remaining_scroll.y());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -1.f));
  EXPECT_EQ(-1.f, remaining_scroll.y());
  EXPECT_EQ(-99.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(-50.f, remaining_scroll.y());
  EXPECT_EQ(-49.f, manager->controls_top_offset());
  EXPECT_EQ(50.f, manager->content_top_offset());
  EXPECT_FALSE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(0.f, manager->controls_top_offset());
  EXPECT_EQ(100.f, manager->content_top_offset());
  EXPECT_FALSE(manager->is_overlay_mode());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);
}

TEST(TopControlsManagerTest, partialShownHideAnimation) {
  MockTopControlsManagerClient client;
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 300));
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());

  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 270));
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_EQ(-85.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = base::TimeTicks::Now();
  float previous_offset = manager->controls_top_offset();
  while(manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_LT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());
}

TEST(TopControlsManagerTest, partialShownShowAnimation) {
  MockTopControlsManagerClient client;
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 300));
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());

  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 230));
  manager->ScrollBy(gfx::Vector2dF(0.f, -70.f));
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());

  manager->ScrollEnd();
  EXPECT_TRUE(manager->animation());

  base::TimeTicks time = base::TimeTicks::Now();
  float previous_offset = manager->controls_top_offset();
  while(manager->animation()) {
    time = base::TimeDelta::FromMicroseconds(100) + time;
    manager->Animate(time);
    EXPECT_GT(manager->controls_top_offset(), previous_offset);
    previous_offset = manager->controls_top_offset();
  }
  EXPECT_FALSE(manager->animation());
  EXPECT_EQ(0.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  EXPECT_TRUE(manager->is_overlay_mode());
}

}  // namespace
}  // namespace cc
