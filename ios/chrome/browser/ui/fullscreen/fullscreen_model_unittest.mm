// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"

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
    set_toolbar_height(kToolbarHeight);
    model_.ResetForNavigation();
    set_y_content_offset(0.0);
  }
  ~FullscreenModelTest() override { model_.RemoveObserver(&observer_); }

  FullscreenModel& model() { return model_; }
  TestFullscreenModelObserver& observer() { return observer_; }

  // FullscreenModel setters that store values before passing them to the model.
  CGFloat toolbar_height() const { return toolbar_height_; }
  void set_toolbar_height(CGFloat toolbar_height) {
    toolbar_height_ = toolbar_height;
    model_.SetToolbarHeight(toolbar_height_);
  }
  CGFloat y_content_offset() const { return y_content_offset_; }
  void set_y_content_offset(CGFloat y_content_offset) {
    y_content_offset_ = y_content_offset;
    model_.SetYContentOffset(y_content_offset_);
  }

  // The base offset against which fullscreen is being calculated.
  CGFloat GetBaseOffsetForProgress(CGFloat progress) {
    EXPECT_TRUE(model_.has_base_offset());
    return y_content_offset_ - (1.0 - progress) * toolbar_height_;
  }

  // Simulates a user scroll event for a scroll of |offset_delta| points.
  void SimulateUserScrollWithDelta(CGFloat offset_delta) {
    model_.SetScrollViewIsDragging(true);
    model_.SetScrollViewIsScrolling(true);
    set_y_content_offset(y_content_offset() + offset_delta);
    model_.SetScrollViewIsDragging(false);
    model_.SetScrollViewIsScrolling(false);
  }

  // Simulates a user scroll that will result in a progress value of |progress|.
  void SimulateUserScrollForProgress(CGFloat progress) {
    ASSERT_GE(progress, 0.0);
    ASSERT_LE(progress, 1.0);
    CGFloat final_y_content_offset =
        toolbar_height_ * (1.0 - progress) +
        GetBaseOffsetForProgress(observer_.progress());
    SimulateUserScrollWithDelta(final_y_content_offset - y_content_offset());
  }

 private:
  FullscreenModel model_;
  TestFullscreenModelObserver observer_;
  CGFloat toolbar_height_ = 0.0;
  CGFloat y_content_offset_ = 0.0;
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
  SimulateUserScrollForProgress(0.5);
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
  EXPECT_EQ(GetBaseOffsetForProgress(kAnimationEndProgress),
            y_content_offset() - kAnimationEndProgress * kToolbarHeight);
}

// Tests that changing the toolbar height fully shows the new toolbar and
// responds appropriately to content offset changes.
TEST_F(FullscreenModelTest, UpdateToolbarHeight) {
  // Reset the toolbar height and verify that the base offset is reset and that
  // the toolbar is fully visible.
  set_toolbar_height(2.0 * kToolbarHeight);
  EXPECT_FALSE(model().has_base_offset());
  EXPECT_EQ(observer().progress(), 1.0);
  // Simulate a page load to a 0.0 y content offset.
  model().ResetForNavigation();
  set_y_content_offset(0.0);
  // Simulate a scroll to -kToolbarHeight.  Since toolbar_height() is twice
  // that, this should produce a progress value of 0.5.
  SimulateUserScrollWithDelta(kToolbarHeight);
  ASSERT_EQ(y_content_offset(), kToolbarHeight);
  EXPECT_EQ(observer().progress(), 0.5);
}

// Tests that updating the y content offset produces the expected progress
// value.
TEST_F(FullscreenModelTest, UserScroll) {
  const CGFloat final_progress = 0.5;
  SimulateUserScrollForProgress(final_progress);
  EXPECT_EQ(observer().progress(), final_progress);
  EXPECT_EQ(y_content_offset(), final_progress * kToolbarHeight);
}

// Tests that updating the y content offset programmatically (i.e. while the
// scroll view is not scrolling) does not produce a new progress value for
// observers.
TEST_F(FullscreenModelTest, ProgrammaticScroll) {
  // Perform a programmatic scroll that would result in a progress of 0.5, and
  // verify that the initial progress value of 1.0 is maintained.
  const CGFloat kProgress = 0.5;
  set_y_content_offset(kProgress * kToolbarHeight);
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
