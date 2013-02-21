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
  MockTopControlsManagerClient(float top_controls_show_threshold,
                               float top_controls_hide_threshold)
      : host_impl_(&proxy_),
        redraw_needed_(false),
        update_draw_properties_needed_(false),
        top_controls_show_threshold_(top_controls_show_threshold),
        top_controls_hide_threshold_(top_controls_hide_threshold) {
    active_tree_ = LayerTreeImpl::create(&host_impl_);
    root_scroll_layer_ = LayerImpl::create(active_tree_.get(), 1);
  }

  virtual ~MockTopControlsManagerClient() {}

  virtual void setNeedsRedraw() OVERRIDE {
    redraw_needed_ = true;
  }

  virtual void setActiveTreeNeedsUpdateDrawProperties() OVERRIDE {
    update_draw_properties_needed_ = true;
  }

  virtual bool haveRootScrollLayer() const OVERRIDE {
    return true;
  }

  virtual float rootScrollLayerTotalScrollY() const OVERRIDE {
    return root_scroll_layer_->scrollOffset().y() +
           root_scroll_layer_->scrollDelta().y();
  }

  LayerImpl* rootScrollLayer() {
    return root_scroll_layer_.get();
  }

  TopControlsManager* manager() {
    if (!manager_) {
      manager_ = TopControlsManager::Create(this,
                                            kTopControlsHeight,
                                            top_controls_show_threshold_,
                                            top_controls_hide_threshold_);
    }
    return manager_.get();
  }

 private:
  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<LayerTreeImpl> active_tree_;
  scoped_ptr<LayerImpl> root_scroll_layer_;
  scoped_ptr<TopControlsManager> manager_;
  bool redraw_needed_;
  bool update_draw_properties_needed_;

  float top_controls_show_threshold_;
  float top_controls_hide_threshold_;
};

TEST(TopControlsManagerTest, overlayModeDetection) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollDelta(gfx::Vector2dF(0.f, 0.f));

  manager->ScrollBegin();

  gfx::Vector2dF remaining_scroll = manager->ScrollBy(
      gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(70.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, 69.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-99.f, manager->controls_top_offset());
  EXPECT_EQ(1.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -20.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-79.f, manager->controls_top_offset());
  EXPECT_EQ(21.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  // Scroll to the toggle point
  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, 21.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, 1.f));
  EXPECT_EQ(1.f, remaining_scroll.y());
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -1.f));
  EXPECT_EQ(-1.f, remaining_scroll.y());
  EXPECT_EQ(-99.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(0.f, remaining_scroll.y());
  EXPECT_EQ(-49.f, manager->controls_top_offset());
  EXPECT_EQ(50.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);

  remaining_scroll = manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(0.f, manager->controls_top_offset());
  EXPECT_EQ(100.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollDelta(
      client.rootScrollLayer()->scrollDelta() + remaining_scroll);
}

TEST(TopControlsManagerTest, ensureScrollThresholdApplied) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 200));

  manager->ScrollBegin();

  // Scroll down to hide the controls entirely.
  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(70.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 230));

  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(-60.f, manager->controls_top_offset());
  EXPECT_EQ(40.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 260));

  manager->ScrollBy(gfx::Vector2dF(0.f, 100.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 360));

  // Scroll back up a bit and ensure the controls don't move until we cross
  // the threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -10.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 350));

  manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 300));

  // After hitting the threshold, further scrolling up should result in the top
  // controls showing.
  manager->ScrollBy(gfx::Vector2dF(0.f, -10.f));
  EXPECT_EQ(-90.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 290));

  manager->ScrollBy(gfx::Vector2dF(0.f, -50.f));
  EXPECT_EQ(-40.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 240));

  // Reset the scroll threshold by going further up the page than the initial
  // threshold.
  manager->ScrollBy(gfx::Vector2dF(0.f, -100.f));
  EXPECT_EQ(0.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 140));

  // See that scrolling down the page now will result in the controls hiding.
  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_EQ(-20.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 160));

  manager->ScrollEnd();
}

TEST(TopControlsManagerTest, partialShownHideAnimation) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 300));
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 270));
  manager->ScrollBy(gfx::Vector2dF(0.f, -15.f));
  EXPECT_EQ(-85.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

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
}

TEST(TopControlsManagerTest, partialShownShowAnimation) {
  MockTopControlsManagerClient client(0.5f, 0.5f);
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 300));
  manager->ScrollBy(gfx::Vector2dF(0.f, 300.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 230));
  manager->ScrollBy(gfx::Vector2dF(0.f, -70.f));
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

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
}

TEST(TopControlsManagerTest, partialHiddenWithAmbiguousThresholdShows) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, 20.f));
  EXPECT_EQ(-20.f, manager->controls_top_offset());
  EXPECT_EQ(80.f, manager->content_top_offset());

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
  EXPECT_EQ(100.f, manager->content_top_offset());
}

TEST(TopControlsManagerTest, partialHiddenWithAmbiguousThresholdHides) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();

  manager->ScrollBegin();

  manager->ScrollBy(gfx::Vector2dF(0.f, 30.f));
  EXPECT_EQ(-30.f, manager->controls_top_offset());
  EXPECT_EQ(70.f, manager->content_top_offset());

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
}

TEST(TopControlsManagerTest, partialShownWithAmbiguousThresholdHides) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 300));

  manager->ScrollBy(gfx::Vector2dF(0.f, 200.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

  manager->ScrollBegin();

  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 180));
  manager->ScrollBy(gfx::Vector2dF(0.f, -20.f));
  EXPECT_EQ(-80.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

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
}

TEST(TopControlsManagerTest, partialShownWithAmbiguousThresholdShows) {
  MockTopControlsManagerClient client(0.25f, 0.25f);
  TopControlsManager* manager = client.manager();
  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 300));

  manager->ScrollBy(gfx::Vector2dF(0.f, 200.f));
  EXPECT_EQ(-100.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

  manager->ScrollBegin();

  client.rootScrollLayer()->setScrollOffset(gfx::Vector2d(0, 70));
  manager->ScrollBy(gfx::Vector2dF(0.f, -30.f));
  EXPECT_EQ(-70.f, manager->controls_top_offset());
  EXPECT_EQ(0.f, manager->content_top_offset());

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
}

}  // namespace
}  // namespace cc
