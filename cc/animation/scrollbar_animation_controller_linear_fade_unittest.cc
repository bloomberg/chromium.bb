// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scrollbar_animation_controller_linear_fade.h"

#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/test/fake_impl_proxy.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class ScrollbarAnimationControllerLinearFadeTest : public testing::Test {
 public:
  ScrollbarAnimationControllerLinearFadeTest()
      : host_impl_(&proxy_, &shared_bitmap_manager_) {}

 protected:
  virtual void SetUp() {
    const int kThumbThickness = 10;
    const int kTrackStart = 0;
    const bool kIsLeftSideVerticalScrollbar = false;
    const bool kIsOverlayScrollbar = true;  // Allow opacity animations.

    scoped_ptr<LayerImpl> scroll_layer =
        LayerImpl::Create(host_impl_.active_tree(), 1);
    scrollbar_layer_ =
        SolidColorScrollbarLayerImpl::Create(host_impl_.active_tree(),
                                             2,
                                             HORIZONTAL,
                                             kThumbThickness,
                                             kTrackStart,
                                             kIsLeftSideVerticalScrollbar,
                                             kIsOverlayScrollbar);
    clip_layer_ = LayerImpl::Create(host_impl_.active_tree(), 3);
    scroll_layer->SetScrollClipLayer(clip_layer_->id());
    LayerImpl* scroll_layer_ptr = scroll_layer.get();
    clip_layer_->AddChild(scroll_layer.Pass());

    scrollbar_layer_->SetClipLayerById(clip_layer_->id());
    scrollbar_layer_->SetScrollLayerById(scroll_layer_ptr->id());
    clip_layer_->SetBounds(gfx::Size(100, 100));
    scroll_layer_ptr->SetBounds(gfx::Size(50, 50));

    scrollbar_controller_ = ScrollbarAnimationControllerLinearFade::Create(
        scroll_layer_ptr,
        base::TimeDelta::FromSeconds(2),
        base::TimeDelta::FromSeconds(3));
  }

  FakeImplProxy proxy_;
  TestSharedBitmapManager shared_bitmap_manager_;
  FakeLayerTreeHostImpl host_impl_;
  scoped_ptr<ScrollbarAnimationControllerLinearFade> scrollbar_controller_;
  scoped_ptr<LayerImpl> clip_layer_;
  scoped_ptr<SolidColorScrollbarLayerImpl> scrollbar_layer_;
};

TEST_F(ScrollbarAnimationControllerLinearFadeTest, HiddenInBegin) {
  scrollbar_controller_->Animate(base::TimeTicks());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest,
       HiddenAfterNonScrollingGesture) {
  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FALSE(scrollbar_controller_->Animate(base::TimeTicks()));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());

  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(100);
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
  scrollbar_controller_->DidScrollGestureEnd(time);

  time += base::TimeDelta::FromSeconds(100);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest, AwakenByScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollGestureBegin();
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());

  EXPECT_FALSE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(100);
  scrollbar_controller_->Animate(time);
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  scrollbar_controller_->DidScrollGestureEnd(time);

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
  EXPECT_FALSE(scrollbar_controller_->DidScrollUpdate(time));
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
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_EQ(2, scrollbar_controller_->DelayBeforeStart(time).InSeconds());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));

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
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
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

TEST_F(ScrollbarAnimationControllerLinearFadeTest,
       AnimationPreservedByNonScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(3);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  scrollbar_controller_->DidScrollGestureEnd(time);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_FALSE(scrollbar_controller_->Animate(time));
  EXPECT_FLOAT_EQ(0.0f, scrollbar_layer_->opacity());
}

TEST_F(ScrollbarAnimationControllerLinearFadeTest,
       AnimationOverriddenByScrollingGesture) {
  base::TimeTicks time;
  time += base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(3);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  scrollbar_controller_->DidScrollGestureBegin();
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(2.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->Animate(time);
  EXPECT_FLOAT_EQ(1.0f / 3.0f, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  EXPECT_FALSE(scrollbar_controller_->DidScrollUpdate(time));
  EXPECT_FALSE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->opacity());

  time += base::TimeDelta::FromSeconds(1);
  scrollbar_controller_->DidScrollGestureEnd(time);
  EXPECT_TRUE(scrollbar_controller_->IsAnimating());
  EXPECT_FLOAT_EQ(1, scrollbar_layer_->opacity());
}

}  // namespace
}  // namespace cc
