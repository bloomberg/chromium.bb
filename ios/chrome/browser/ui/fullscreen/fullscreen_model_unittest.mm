// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"

#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_model_observer.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The toolbar height to use for tests.
const CGFloat kToolbarHeight = 50.0;
}  // namespace

// Test fixture for FullscreenModel.
class FullscreenModelTest : public PlatformTest {
 public:
  FullscreenModelTest() : PlatformTest() {
    model_.AddObserver(&observer_);
    // Set the toolbar height to kToolbarHeight, and simulate a page load that
    // finishes with a 0.0 y content offset.
    model_.SetToolbarHeight(kToolbarHeight);
    model_.ResetForNavigation();
    model_.SetYContentOffset(0.0);
  }
  ~FullscreenModelTest() override { model_.RemoveObserver(&observer_); }

  FullscreenModel& model() { return model_; }
  TestFullscreenModelObserver& observer() { return observer_; }

 private:
  FullscreenModel model_;
  TestFullscreenModelObserver observer_;
};

// Tests that incremending and decrementing the disabled counter correctly
// disabled/enables the model.
TEST_F(FullscreenModelTest, EnableDisable) {
  ASSERT_TRUE(model().enabled());
  ASSERT_TRUE(observer().enabled());
  // Increment the disabled counter and check that the model is disabled.
  model().IncrementDisabledCounter();
  EXPECT_FALSE(model().enabled());
  EXPECT_FALSE(observer().enabled());
  // Increment again and check that the model is still disabled.
  model().IncrementDisabledCounter();
  EXPECT_FALSE(model().enabled());
  EXPECT_FALSE(observer().enabled());
  // Decrement the counter and check that the model is still disabled.
  model().DecrementDisabledCounter();
  EXPECT_FALSE(model().enabled());
  EXPECT_FALSE(observer().enabled());
  // Decrement again and check that the model is reenabled.
  model().DecrementDisabledCounter();
  EXPECT_TRUE(model().enabled());
  EXPECT_TRUE(observer().enabled());
}

// Tests that calling ResetForNavigation() resets the model to a fully-visible
// pre-scroll state.
TEST_F(FullscreenModelTest, ResetForNavigation) {
  // Simulate a scroll event and check that progress has been updated.
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  ASSERT_EQ(observer().progress(), 0.5);
  // Call ResetForNavigation() and verify that the base offset is reset and that
  // the toolbar is fully visible.
  model().ResetForNavigation();
  EXPECT_FALSE(model().has_base_offset());
  EXPECT_EQ(observer().progress(), 1.0);
}

// Tests that the end progress value of a scroll adjustment animation is used
// as the model's progress.
TEST_F(FullscreenModelTest, AnimationEnded) {
  const CGFloat kAnimationEndProgress = 0.5;
  ASSERT_EQ(observer().progress(), 1.0);
  model().AnimationEndedWithProgress(kAnimationEndProgress);
  // Check that the resulting progress value was not broadcast.
  EXPECT_EQ(observer().progress(), 1.0);
  // Start dragging to to simulate a touch that occurs while the scroll end
  // animation is in progress.  This would cancel the scroll animation and call
  // AnimationEndedWithProgress().  After this occurs, the base offset should be
  // updated to a value corresponding with a 0.5 progress value.
  model().SetScrollViewIsDragging(true);
  EXPECT_EQ(
      GetFullscreenBaseOffsetForProgress(&model(), kAnimationEndProgress),
      model().GetYContentOffset() - kAnimationEndProgress * kToolbarHeight);
}

// Tests that changing the toolbar height fully shows the new toolbar and
// responds appropriately to content offset changes.
TEST_F(FullscreenModelTest, UpdateToolbarHeight) {
  // Reset the toolbar height and verify that the base offset is reset and that
  // the toolbar is fully visible.
  model().SetToolbarHeight(2.0 * kToolbarHeight);
  EXPECT_FALSE(model().has_base_offset());
  EXPECT_EQ(observer().progress(), 1.0);
  // Simulate a page load to a 0.0 y content offset.
  model().ResetForNavigation();
  model().SetYContentOffset(0.0);
  // Simulate a scroll to -kToolbarHeight.  Since toolbar_height() is twice
  // that, this should produce a progress value of 0.5.
  SimulateFullscreenUserScrollWithDelta(&model(), kToolbarHeight);
  ASSERT_EQ(model().GetYContentOffset(), kToolbarHeight);
  EXPECT_EQ(observer().progress(), 0.5);
}

// Tests that updating the y content offset produces the expected progress
// value.
TEST_F(FullscreenModelTest, UserScroll) {
  const CGFloat kFinalProgress = 0.5;
  SimulateFullscreenUserScrollForProgress(&model(), kFinalProgress);
  EXPECT_EQ(observer().progress(), kFinalProgress);
  EXPECT_EQ(model().GetYContentOffset(), kFinalProgress * kToolbarHeight);
}

// Tests that updating the y content offset programmatically (i.e. while the
// scroll view is not scrolling) does not produce a new progress value for
// observers.
TEST_F(FullscreenModelTest, ProgrammaticScroll) {
  // Perform a programmatic scroll that would result in a progress of 0.5, and
  // verify that the initial progress value of 1.0 is maintained.
  const CGFloat kProgress = 0.5;
  model().SetYContentOffset(kProgress * kToolbarHeight);
  EXPECT_EQ(observer().progress(), 1.0);
}

// Tests that setting scrolling to false sends a scroll end signal to its
// observers.
TEST_F(FullscreenModelTest, ScrollEnded) {
  model().SetScrollViewIsScrolling(true);
  model().SetScrollViewIsScrolling(false);
  EXPECT_TRUE(observer().scroll_end_received());
}

// Tests that the base offset is updated when dragging begins.
TEST_F(FullscreenModelTest, DraggingStarted) {
  model().ResetForNavigation();
  model().SetScrollViewIsDragging(true);
  EXPECT_TRUE(model().has_base_offset());
}
