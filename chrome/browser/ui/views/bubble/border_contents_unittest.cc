// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble/border_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test BorderContentsTest;

class TestBorderContents : public BorderContents {
 public:
  TestBorderContents() {}

  void set_monitor_bounds(const gfx::Rect& bounds) {
    monitor_bounds_ = bounds;
  }

  BubbleBorder* bubble_border() const { return bubble_border_; }

 protected:
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect) {
    return monitor_bounds_;
  }

 private:
  gfx::Rect monitor_bounds_;

  DISALLOW_COPY_AND_ASSIGN(TestBorderContents);
};

// Tests that the arrow is moved appropriately when the info-bubble does not fit
// the screen.
TEST_F(BorderContentsTest, BorderContentsSizeAndGetBounds) {
  TestBorderContents border_contents;
  border_contents.Init();

  // Test that the info bubble displays normally when it fits.
  gfx::Rect contents_bounds;
  gfx::Rect window_bounds;
  border_contents.set_monitor_bounds(gfx::Rect(0, 0, 1000, 1000));
  border_contents.SizeAndGetBounds(
      gfx::Rect(100, 100, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_LEFT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  // The arrow shouldn't have changed from TOP_LEFT.
  BubbleBorder::ArrowLocation arrow_location =
      border_contents.bubble_border()->arrow_location();
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(arrow_location));
  EXPECT_GT(window_bounds.x(), 100);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on left.
  border_contents.SizeAndGetBounds(
      gfx::Rect(100, 100, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_RIGHT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to TOP_LEFT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(arrow_location));
  EXPECT_GT(window_bounds.x(), 100);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on left or top.
  border_contents.SizeAndGetBounds(
      gfx::Rect(100, 100, 50, 50),  // |position_relative_to|
      BubbleBorder::BOTTOM_RIGHT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to TOP_LEFT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(arrow_location));
  EXPECT_GT(window_bounds.x(), 100);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on top.
  border_contents.SizeAndGetBounds(
      gfx::Rect(100, 100, 50, 50),  // |position_relative_to|
      BubbleBorder::BOTTOM_LEFT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to TOP_LEFT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(arrow_location));
  EXPECT_GT(window_bounds.x(), 100);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on top and right.
  border_contents.SizeAndGetBounds(
      gfx::Rect(900, 100, 50, 50),  // |position_relative_to|
      BubbleBorder::BOTTOM_LEFT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to TOP_RIGHT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(arrow_location));
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

 // Test bubble not fitting on right.
  border_contents.SizeAndGetBounds(
      gfx::Rect(900, 100, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_LEFT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to TOP_RIGHT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(arrow_location));
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 100 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.

  // Test bubble not fitting on bottom and right.
  border_contents.SizeAndGetBounds(
      gfx::Rect(900, 900, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_LEFT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to BOTTOM_RIGHT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(arrow_location));
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.

  // Test bubble not fitting at the bottom.
  border_contents.SizeAndGetBounds(
      gfx::Rect(100, 900, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_LEFT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to BOTTOM_LEFT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(arrow_location));
  // The window should be right aligned with the position_relative_to.
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.

  // Test bubble not fitting at the bottom and left.
  border_contents.SizeAndGetBounds(
      gfx::Rect(100, 900, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_RIGHT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);
  arrow_location = border_contents.bubble_border()->arrow_location();
  // The arrow should have changed to BOTTOM_LEFT.
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(arrow_location));
  // The window should be right aligned with the position_relative_to.
  EXPECT_LT(window_bounds.x(), 900 + 50 - 500);
  EXPECT_LT(window_bounds.y(), 900 - 500 - 15);  // -15 to roughly compensate
                                                 // for arrow height.
}

// Tests that the arrow is not moved when the info-bubble does not fit the
// screen but moving it would make matter worse.
TEST_F(BorderContentsTest, BorderContentsSizeAndGetBoundsDontMoveArrow) {
  TestBorderContents border_contents;
  border_contents.Init();
  gfx::Rect contents_bounds;
  gfx::Rect window_bounds;
  border_contents.set_monitor_bounds(gfx::Rect(0, 0, 1000, 1000));
  border_contents.SizeAndGetBounds(
      gfx::Rect(400, 100, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_LEFT,
      false,                        // |allow_bubble_offscreen|
      gfx::Size(500, 700),          // |contents_size|
      &contents_bounds, &window_bounds);

  // The arrow should not have changed, as it would make it the bubble even more
  // offscreen.
  BubbleBorder::ArrowLocation arrow_location =
      border_contents.bubble_border()->arrow_location();
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_left(arrow_location));
}

// Test that the 'allow offscreen' prevents the bubble from moving.
TEST_F(BorderContentsTest, BorderContentsSizeAndGetBoundsAllowOffscreen) {
  TestBorderContents border_contents;
  border_contents.Init();
  gfx::Rect contents_bounds;
  gfx::Rect window_bounds;
  border_contents.set_monitor_bounds(gfx::Rect(0, 0, 1000, 1000));
  border_contents.SizeAndGetBounds(
      gfx::Rect(100, 900, 50, 50),  // |position_relative_to|
      BubbleBorder::TOP_RIGHT,
      true,                         // |allow_bubble_offscreen|
      gfx::Size(500, 500),          // |contents_size|
      &contents_bounds, &window_bounds);

  // The arrow should not have changed (eventhough the bubble does not fit).
  BubbleBorder::ArrowLocation arrow_location =
      border_contents.bubble_border()->arrow_location();
  EXPECT_TRUE(BubbleBorder::has_arrow(arrow_location));
  EXPECT_TRUE(BubbleBorder::is_arrow_on_top(arrow_location));
  EXPECT_FALSE(BubbleBorder::is_arrow_on_left(arrow_location));
  // The coordinates should be pointing to 'positive relative to' from
  // TOP_RIGHT.
  EXPECT_LT(window_bounds.x(), 100 + 50 - 500);
  EXPECT_GT(window_bounds.y(), 900 + 50 - 10);  // -10 to roughly compensate for
                                                // arrow overlap.
}
