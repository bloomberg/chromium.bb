// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/switch_access_panel.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

class SwitchAccessPanelTest : public testing::Test {
 public:
  SwitchAccessPanelTest() {}
  ~SwitchAccessPanelTest() override = default;

  const gfx::Rect CalculatePanelBounds(
      const gfx::Rect& element_bounds,
      const gfx::Rect& screen_bounds = gfx::Rect(0, 0, 1000, 1000)) const {
    return SwitchAccessPanel::CalculatePanelBounds(
        element_bounds, screen_bounds, kMenuWidth, kMenuHeight);
  }

  int GetFocusRingBuffer() const {
    return SwitchAccessPanel::GetFocusRingBuffer();
  }

  const int kMenuWidth = 300;
  const int kMenuHeight = 200;

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessPanelTest);
};

TEST_F(SwitchAccessPanelTest, ElementInUpperLeftCorner) {
  const gfx::Rect element_bounds(0, 0, 100, 100);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // Because there is space to put the panel off the lower right corner of the
  // element, we expect it to be there.
  EXPECT_EQ(100 + GetFocusRingBuffer(), panel_bounds.x());
  EXPECT_EQ(100 + GetFocusRingBuffer(), panel_bounds.y());
}

TEST_F(SwitchAccessPanelTest, ElementInUpperRightCorner) {
  const gfx::Rect element_bounds(900, 0, 100, 100);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // Because the element is right-justified, but there is space below, we
  // expect the panel to be off the lower left corner of the element.
  EXPECT_EQ(900 - GetFocusRingBuffer(), panel_bounds.right());
  EXPECT_EQ(100 + GetFocusRingBuffer(), panel_bounds.y());
}

TEST_F(SwitchAccessPanelTest, ElementInLowerLeftCorner) {
  const gfx::Rect element_bounds(0, 900, 100, 100);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // Because the element is at the bottom left of the screen, we expect the
  // panel to be off the upper right corner of the element.
  EXPECT_EQ(100 + GetFocusRingBuffer(), panel_bounds.x());
  EXPECT_EQ(900 - GetFocusRingBuffer(), panel_bounds.bottom());
}

TEST_F(SwitchAccessPanelTest, ElementInLowerRightCorner) {
  const gfx::Rect element_bounds(900, 900, 100, 100);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // Because the element is at the bottom right of the screen, we expect the
  // panel to be off the upper left corner of the element.
  EXPECT_EQ(900 - GetFocusRingBuffer(), panel_bounds.right());
  EXPECT_EQ(900 - GetFocusRingBuffer(), panel_bounds.bottom());
}

TEST_F(SwitchAccessPanelTest, ElementFullHeightLeftJustified) {
  const gfx::Rect element_bounds(0, 0, 100, 1000);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // The element is to the left of the screen, and the full height of the
  // screen. So we expect the panel to be directly to the right of the element,
  // and along the bottom edge of the screen.
  EXPECT_EQ(100 + GetFocusRingBuffer(), panel_bounds.x());
  EXPECT_EQ(1000, panel_bounds.bottom());
}

TEST_F(SwitchAccessPanelTest, ElementFullHeightRightJustified) {
  const gfx::Rect element_bounds(900, 0, 100, 1000);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // The element is to the right of the screen, and the full height of the
  // screen. So we expect the panel to be directly to the left of the element,
  // and along the bottom edge of the screen.
  EXPECT_EQ(900 - GetFocusRingBuffer(), panel_bounds.right());
  EXPECT_EQ(1000, panel_bounds.bottom());
}

TEST_F(SwitchAccessPanelTest, ElementFullWidthTopOfScreen) {
  const gfx::Rect element_bounds(0, 0, 1000, 100);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // The element is at the top of the screen, and the full width of the screen.
  // So we expect the panel to be directly below the element, and along the
  // right side of the screen.
  EXPECT_EQ(1000, panel_bounds.right());
  EXPECT_EQ(100 + GetFocusRingBuffer(), panel_bounds.y());
}

TEST_F(SwitchAccessPanelTest, ElementFullWidthBottomOfScreen) {
  const gfx::Rect element_bounds(0, 900, 1000, 100);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // The element is at the bottom of the screen, and the full width of the
  // screen. So we expect the panel to be directly above the element, and along
  // the right side of the screen.
  EXPECT_EQ(1000, panel_bounds.right());
  EXPECT_EQ(900 - GetFocusRingBuffer(), panel_bounds.bottom());
}

TEST_F(SwitchAccessPanelTest, ElementFullscreen) {
  const gfx::Rect element_bounds(0, 0, 1000, 1000);
  const gfx::Rect panel_bounds = CalculatePanelBounds(element_bounds);

  // The element is fullscreen, so we expect the menu in the bottom right
  // corner.
  EXPECT_EQ(1000, panel_bounds.right());
  EXPECT_EQ(1000, panel_bounds.bottom());
}
