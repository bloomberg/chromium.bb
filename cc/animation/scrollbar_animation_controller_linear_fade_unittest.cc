// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_web_scrollbar_theme_geometry.h"
#include "cc/trees/single_thread_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ScrollbarAnimationControllerLinearFadeTest : public testing::Test {
 public:
  ScrollbarAnimationControllerLinearFadeTest() : host_impl_(&proxy_) {}

 protected:
  virtual void SetUp() {
    scroll_layer_ = LayerImpl::Create(host_impl_.active_tree(), 1);
    scoped_ptr<ScrollbarGeometryFixedThumb> geometry(
        ScrollbarGeometryFixedThumb::Create(
            FakeWebScrollbarThemeGeometry::create(false)));
    scrollbar_layer_ = ScrollbarLayerImpl::Create(
        host_impl_.active_tree(), 2, geometry.Pass());

    scroll_layer_->SetMaxScrollOffset(gfx::Vector2d(50, 50));
    scroll_layer_->SetBounds(gfx::Size(50, 50));
    scroll_layer_->SetHorizontalScrollbarLayer(scrollbar_layer_.get());

    scrollbar_controller_ = ScrollbarAnimationControllerLinearFade::Create(
        scroll_layer_.get(),
        base::TimeDelta::FromSeconds(2),
        base::TimeDelta::FromSeconds(3));
  }

  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<ScrollbarAnimationControllerLinearFade> scrollbar_controller_;
  scoped_ptr<LayerImpl> scroll_layer_;
  scoped_ptr<ScrollbarLayerImpl> scrollbar_layer_;
};

TEST_F(ScrollbarAnimationControllerLinearFadeTest, HiddenInBegin) {
  scrollbar_controller_->Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, AwakenByScrollGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_TRUE(scrollbar_controller_->IsScrollGestureInProgress());
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  scrollbar_controller_->DidScrollGestureEnd(time);

  EXPECT_FALSE(scrollbar_controller_->IsScrollGestureInProgress());
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollGestureBegin();
  scrollbar_controller_->DidScrollGestureEnd(time);

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, AwakenByProgrammaticScroll) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidProgrammaticallyUpdateScroll(time);
  EXPECT_FALSE(scrollbar_controller_->IsScrollGestureInProgress());
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  scrollbar_controller_->DidProgrammaticallyUpdateScroll(time);

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidProgrammaticallyUpdateScroll(time);
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

}  // namespace
}  // namespace cc
