// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider_win.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"


class DisplaySettingsProviderWinTest :
    public testing::Test,
    public DisplaySettingsProvider::Observer {
 protected:
  // Overridden from DisplaySettingsProvider::Observer:
  virtual void OnAutoHidingDesktopBarThicknessChanged() OVERRIDE {
  }

  virtual void OnAutoHidingDesktopBarVisibilityChanged(
      DisplaySettingsProvider::DesktopBarAlignment alignment,
      DisplaySettingsProvider::DesktopBarVisibility visibility) OVERRIDE {
  }

  void TestGetDesktopBarVisibilityFromBounds() {
    scoped_ptr<DisplaySettingsProviderWin> provider(
        new DisplaySettingsProviderWin(this));
    provider->set_work_area(gfx::Rect(0, 0, 800, 600));

    DisplaySettingsProvider::DesktopBarVisibility visibility;

    // Tests for bottom bar.
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        gfx::Rect(0, 560, 800, 40));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_VISIBLE, visibility);
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        gfx::Rect(0, 598, 800, 40));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_HIDDEN, visibility);
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
        gfx::Rect(0, 580, 800, 40));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_ANIMATING, visibility);

    // Tests for right bar.
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
        gfx::Rect(760, 0, 40, 600));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_VISIBLE, visibility);
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
        gfx::Rect(798, 0, 40, 600));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_HIDDEN, visibility);
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
        gfx::Rect(780, 0, 40, 600));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_ANIMATING, visibility);

    // Tests for left bar.
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_LEFT,
        gfx::Rect(0, 0, 40, 600));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_VISIBLE, visibility);
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_LEFT,
        gfx::Rect(-38, 0, 40, 600));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_HIDDEN, visibility);
    visibility = provider->GetDesktopBarVisibilityFromBounds(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_LEFT,
        gfx::Rect(-15, 0, 40, 600));
    EXPECT_EQ(DisplaySettingsProvider::DESKTOP_BAR_ANIMATING, visibility);
  }
};

TEST_F(DisplaySettingsProviderWinTest, GetDesktopBarVisibilityFromBounds) {
  TestGetDesktopBarVisibilityFromBounds();
}
