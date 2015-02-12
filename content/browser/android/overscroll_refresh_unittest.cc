// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/layer.h"
#include "content/browser/android/overscroll_refresh.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/resources/resource_manager.h"

namespace content {

class OverscrollRefreshTest : public OverscrollRefreshClient,
                              public ui::ResourceManager,
                              public testing::Test {
 public:
  OverscrollRefreshTest()
      : refresh_triggered_(false), still_refreshing_(false) {}

  // OverscrollRefreshClient implementation.
  void TriggerRefresh() override {
    refresh_triggered_ = true;
    still_refreshing_ = true;
  }

  bool IsStillRefreshing() const override { return still_refreshing_; }

  // ResourceManager implementation.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override {
    return base::android::ScopedJavaLocalRef<jobject>();
  }

  Resource* GetResource(ui::AndroidResourceType res_type, int res_id) override {
    return nullptr;
  }

  void PreloadResource(ui::AndroidResourceType res_type, int res_id) override {}

  bool GetAndResetRefreshTriggered() {
    bool triggered = refresh_triggered_;
    refresh_triggered_ = false;
    return triggered;
  }

  void PullBeyondActivationThreshold(OverscrollRefresh* effect) {
    for (int i = 0; i < OverscrollRefresh::kMinPullsToActivate; ++i)
      EXPECT_TRUE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 100)));
  }

 protected:

  scoped_ptr<OverscrollRefresh> CreateEffect() {
    const float kDragTargetPixels = 100;
    const bool kMirror = false;
    scoped_ptr<OverscrollRefresh> effect(
        new OverscrollRefresh(this, this, kDragTargetPixels, kMirror));

    const gfx::SizeF kViewportSize(512, 512);
    const gfx::Vector2dF kScrollOffset;
    const bool kOverflowYHidden = false;
    effect->UpdateDisplay(kViewportSize, kScrollOffset, kOverflowYHidden);

    return effect.Pass();
  }

  void SignalRefreshCompleted() { still_refreshing_ = false; }

 private:
  bool refresh_triggered_;
  bool still_refreshing_;
};

TEST_F(OverscrollRefreshTest, Basic) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();

  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());

  effect->OnScrollBegin();
  EXPECT_FALSE(effect->IsActive());
  EXPECT_TRUE(effect->IsAwaitingScrollUpdateAck());

  // The initial scroll should not be consumed, as it should first be offered
  // to content.
  gfx::Vector2dF scroll_up(0, 10);
  EXPECT_FALSE(effect->WillHandleScrollUpdate(scroll_up));
  EXPECT_FALSE(effect->IsActive());
  EXPECT_TRUE(effect->IsAwaitingScrollUpdateAck());

  // The unconsumed, overscrolling scroll will trigger the effect->
  effect->OnScrollUpdateAck(false);
  EXPECT_TRUE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());

  // Further scrolls will be consumed.
  EXPECT_TRUE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 50)));
  EXPECT_TRUE(effect->IsActive());

  // Even scrolls in the down direction should be consumed.
  EXPECT_TRUE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, -50)));
  EXPECT_TRUE(effect->IsActive());

  // Feed enough scrolls to the effect to exceeds the threshold.
  PullBeyondActivationThreshold(effect.get());
  EXPECT_TRUE(effect->IsActive());

  // Ending the scroll while beyond the threshold should trigger a refresh.
  gfx::Vector2dF zero_velocity;
  EXPECT_FALSE(GetAndResetRefreshTriggered());
  effect->OnScrollEnd(zero_velocity);
  EXPECT_TRUE(effect->IsActive());
  EXPECT_TRUE(GetAndResetRefreshTriggered());
  SignalRefreshCompleted();

  // Ensure animation doesn't explode.
  base::TimeTicks initial_time = base::TimeTicks::Now();
  base::TimeTicks current_time = initial_time;
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  while (effect->Animate(current_time, layer.get()))
    current_time += base::TimeDelta::FromMilliseconds(16);

  // The effect should terminate in a timely fashion.
  EXPECT_GT(current_time.ToInternalValue(), initial_time.ToInternalValue());
  EXPECT_LE(
      current_time.ToInternalValue(),
      (initial_time + base::TimeDelta::FromSeconds(10)).ToInternalValue());
  EXPECT_FALSE(effect->IsActive());
}

TEST_F(OverscrollRefreshTest, AnimationTerminatesEvenIfRefreshNeverTerminates) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();
  effect->OnScrollBegin();
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect->IsAwaitingScrollUpdateAck());
  effect->OnScrollUpdateAck(false);
  ASSERT_TRUE(effect->IsActive());
  PullBeyondActivationThreshold(effect.get());
  ASSERT_TRUE(effect->IsActive());
  effect->OnScrollEnd(gfx::Vector2dF(0, 0));
  ASSERT_TRUE(GetAndResetRefreshTriggered());

  // Verify that the animation terminates even if the triggered refresh
  // action never terminates (i.e., |still_refreshing_| is always true).
  base::TimeTicks initial_time = base::TimeTicks::Now();
  base::TimeTicks current_time = initial_time;
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  while (effect->Animate(current_time, layer.get()))
    current_time += base::TimeDelta::FromMilliseconds(16);

  EXPECT_GT(current_time.ToInternalValue(), initial_time.ToInternalValue());
  EXPECT_LE(
      current_time.ToInternalValue(),
      (initial_time + base::TimeDelta::FromSeconds(10)).ToInternalValue());
  EXPECT_FALSE(effect->IsActive());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfBelowThreshold) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();
  effect->OnScrollBegin();
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect->IsAwaitingScrollUpdateAck());
  effect->OnScrollUpdateAck(false);
  ASSERT_TRUE(effect->IsActive());

  // Terminating the pull before it exceeds the threshold will prevent refresh.
  EXPECT_TRUE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  effect->OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialYOffsetIsNotZero) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();

  // A positive y scroll offset at the start of scroll will prevent activation,
  // even if the subsequent scroll overscrolls upward.
  gfx::SizeF viewport_size(512, 512);
  gfx::Vector2dF nonzero_offset(0, 10);
  bool overflow_y_hidden = false;
  effect->UpdateDisplay(viewport_size, nonzero_offset, overflow_y_hidden);
  effect->OnScrollBegin();

  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());
  effect->OnScrollUpdateAck(false);
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect->OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfOverflowYHidden) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();

  // "overflow-y: hidden" on the root layer will prevent activation,
  // even if the subsequent scroll overscrolls upward.
  gfx::SizeF viewport_size(512, 512);
  gfx::Vector2dF zero_offset;
  bool overflow_y_hidden = true;
  effect->UpdateDisplay(viewport_size, zero_offset, overflow_y_hidden);
  effect->OnScrollBegin();
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());
  effect->OnScrollUpdateAck(false);
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect->OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialScrollDownward) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();
  effect->OnScrollBegin();

  // A downward initial scroll will prevent activation, even if the subsequent
  // scroll overscrolls upward.
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, -10)));
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());

  effect->OnScrollUpdateAck(false);
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect->OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialScrollOrTouchConsumed) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();
  effect->OnScrollBegin();
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect->IsAwaitingScrollUpdateAck());

  // Consumption of the initial touchmove or scroll should prevent future
  // activation.
  effect->OnScrollUpdateAck(true);
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect->OnScrollUpdateAck(false);
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(effect->IsAwaitingScrollUpdateAck());
  EXPECT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect->OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfInitialScrollsJanked) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();
  effect->OnScrollBegin();
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect->IsAwaitingScrollUpdateAck());
  effect->OnScrollUpdateAck(false);
  ASSERT_TRUE(effect->IsActive());

  // It should take more than just one or two large scrolls to trigger,
  // mitigating likelihood of jank triggering the effect->
  EXPECT_TRUE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  EXPECT_TRUE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 500)));
  effect->OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfFlungDownward) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();
  effect->OnScrollBegin();
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect->IsAwaitingScrollUpdateAck());
  effect->OnScrollUpdateAck(false);
  ASSERT_TRUE(effect->IsActive());

  // Ensure the pull exceeds the necessary threshold.
  PullBeyondActivationThreshold(effect.get());
  ASSERT_TRUE(effect->IsActive());

  // Terminating the pull with a down-directed fling should prevent triggering.
  effect->OnScrollEnd(gfx::Vector2dF(0, -1000));
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

TEST_F(OverscrollRefreshTest, NotTriggeredIfReleasedWithoutActivation) {
  scoped_ptr<OverscrollRefresh> effect = CreateEffect();
  effect->OnScrollBegin();
  ASSERT_FALSE(effect->WillHandleScrollUpdate(gfx::Vector2dF(0, 10)));
  ASSERT_TRUE(effect->IsAwaitingScrollUpdateAck());
  effect->OnScrollUpdateAck(false);
  ASSERT_TRUE(effect->IsActive());

  // Ensure the pull exceeds the necessary threshold.
  PullBeyondActivationThreshold(effect.get());
  ASSERT_TRUE(effect->IsActive());

  // An early release should prevent the refresh action from firing.
  effect->ReleaseWithoutActivation();
  effect->OnScrollEnd(gfx::Vector2dF());
  EXPECT_FALSE(GetAndResetRefreshTriggered());

  // The early release should trigger a dismissal animation.
  EXPECT_TRUE(effect->IsActive());
  base::TimeTicks initial_time = base::TimeTicks::Now();
  base::TimeTicks current_time = initial_time;
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  while (effect->Animate(current_time, layer.get()))
    current_time += base::TimeDelta::FromMilliseconds(16);

  EXPECT_GT(current_time.ToInternalValue(), initial_time.ToInternalValue());
  EXPECT_FALSE(effect->IsActive());
  EXPECT_FALSE(GetAndResetRefreshTriggered());
}

}  // namespace content
