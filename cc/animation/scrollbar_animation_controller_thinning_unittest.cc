// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_thinning.h"

#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ScrollbarAnimationControllerThinningTest : public testing::Test {
 public:
  ScrollbarAnimationControllerThinningTest() : host_impl_(&proxy_) {}

 protected:
  virtual void SetUp() {
    scroll_layer_ = LayerImpl::Create(host_impl_.active_tree(), 1);
    const int kId = 2;
    const int kThumbThickness = 10;
    const bool kIsLeftSideVerticalScrollbar = false;
    scrollbar_layer_ = SolidColorScrollbarLayerImpl::Create(
        host_impl_.active_tree(), kId, HORIZONTAL, kThumbThickness,
        kIsLeftSideVerticalScrollbar);

    scroll_layer_->SetMaxScrollOffset(gfx::Vector2d(50, 50));
    scroll_layer_->SetBounds(gfx::Size(50, 50));
    scroll_layer_->SetHorizontalScrollbarLayer(scrollbar_layer_.get());

    scrollbar_controller_ = ScrollbarAnimationControllerThinning::CreateForTest(
        scroll_layer_.get(),
        base::TimeDelta::FromSeconds(2),
        base::TimeDelta::FromSeconds(3));
  }

  FakeImplProxy proxy_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<ScrollbarAnimationControllerThinning> scrollbar_controller_;
  scoped_ptr<LayerImpl> scroll_layer_;
  scoped_ptr<SolidColorScrollbarLayerImpl> scrollbar_layer_;
};

TEST_F(ScrollbarAnimationControllerThinningTest, Idle) {
  scrollbar_controller_->Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());
}

TEST_F(ScrollbarAnimationControllerThinningTest, AwakenByScrollGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  scrollbar_controller_->DidScrollGestureEnd(time);

  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);

  scrollbar_controller_->DidScrollGestureBegin();
  scrollbar_controller_->DidScrollGestureEnd(time);

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());
}

TEST_F(ScrollbarAnimationControllerThinningTest, AwakenByProgrammaticScroll) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.9f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.8f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.6f, scrollbar_layer_->thumb_thickness_scale_factor());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(0.7f, scrollbar_layer_->opacity());
  EXPECT_FLOAT_EQ(0.4f, scrollbar_layer_->thumb_thickness_scale_factor());
}

}  // namespace
}  // namespace cc
