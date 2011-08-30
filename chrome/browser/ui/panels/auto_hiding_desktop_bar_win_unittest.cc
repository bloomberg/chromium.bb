// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/auto_hiding_desktop_bar_win.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"


class AutoHidingDesktopBarWinTest : public testing::Test,
                                    public AutoHidingDesktopBar::Observer {
 protected:
  // Overridden from AutoHidingDesktopBar::Observer:
  virtual void OnAutoHidingDesktopBarThicknessChanged() OVERRIDE {
  }

  virtual void OnAutoHidingDesktopBarVisibilityChanged(
      AutoHidingDesktopBar::Alignment alignment,
      AutoHidingDesktopBar::Visibility visibility) OVERRIDE {
  }

  void TestGetVisibilityFromBounds() {
    scoped_refptr<AutoHidingDesktopBarWin> bar =
        new AutoHidingDesktopBarWin(this);
    bar->set_work_area(gfx::Rect(0, 0, 800, 600));

    AutoHidingDesktopBar::Visibility visibility;

    // Tests for bottom bar.
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_BOTTOM, gfx::Rect(0, 560, 800, 40));
    EXPECT_EQ(AutoHidingDesktopBar::VISIBLE, visibility);
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_BOTTOM, gfx::Rect(0, 598, 800, 40));
    EXPECT_EQ(AutoHidingDesktopBar::HIDDEN, visibility);
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_BOTTOM, gfx::Rect(0, 580, 800, 40));
    EXPECT_EQ(AutoHidingDesktopBar::ANIMATING, visibility);

    // Tests for right bar.
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_RIGHT, gfx::Rect(760, 0, 40, 600));
    EXPECT_EQ(AutoHidingDesktopBar::VISIBLE, visibility);
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_RIGHT, gfx::Rect(798, 0, 40, 600));
    EXPECT_EQ(AutoHidingDesktopBar::HIDDEN, visibility);
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_RIGHT, gfx::Rect(780, 0, 40, 600));
    EXPECT_EQ(AutoHidingDesktopBar::ANIMATING, visibility);

    // Tests for left bar.
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_LEFT, gfx::Rect(0, 0, 40, 600));
    EXPECT_EQ(AutoHidingDesktopBar::VISIBLE, visibility);
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_LEFT, gfx::Rect(-38, 0, 40, 600));
    EXPECT_EQ(AutoHidingDesktopBar::HIDDEN, visibility);
    visibility = bar->GetVisibilityFromBounds(
        AutoHidingDesktopBar::ALIGN_LEFT, gfx::Rect(-15, 0, 40, 600));
    EXPECT_EQ(AutoHidingDesktopBar::ANIMATING, visibility);
  }
};

TEST_F(AutoHidingDesktopBarWinTest, GetVisibilityFromBounds) {
  TestGetVisibilityFromBounds();
}
