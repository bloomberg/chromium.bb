// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider_win.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"


class DisplaySettingsProviderWinTest : public testing::Test {
 public:
  class MockDisplaySettingsProviderWin : public DisplaySettingsProviderWin {
   public:
    MockDisplaySettingsProviderWin()
        : DisplaySettingsProviderWin() {
      OnDisplaySettingsChanged();
    }
    virtual ~MockDisplaySettingsProviderWin() { }
    virtual gfx::Rect GetPrimaryWorkArea() const {
      return gfx::Rect(0, 0, 800, 600);
    }

    // Expose the protected methods from base class for testing purpose.
    using DisplaySettingsProviderWin::GetDesktopBarThicknessFromBounds;
    using DisplaySettingsProviderWin::GetDesktopBarVisibilityFromBounds;
  };
};

TEST_F(DisplaySettingsProviderWinTest, GetDesktopBarThicknessFromBounds) {
  scoped_ptr<MockDisplaySettingsProviderWin> provider(
      new MockDisplaySettingsProviderWin());

  int thickness;

  // Bottom bar.
  thickness = provider->GetDesktopBarThicknessFromBounds(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_BOTTOM,
      gfx::Rect(0, 560, 800, 40));
  EXPECT_EQ(40, thickness);

  // Right bar.
  thickness = provider->GetDesktopBarThicknessFromBounds(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT,
      gfx::Rect(760, 0, 30, 600));
  EXPECT_EQ(30, thickness);

  // Left bar.
  thickness = provider->GetDesktopBarThicknessFromBounds(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_LEFT,
      gfx::Rect(760, 0, 35, 600));
  EXPECT_EQ(35, thickness);
}

TEST_F(DisplaySettingsProviderWinTest, GetDesktopBarVisibilityFromBounds) {
  scoped_ptr<MockDisplaySettingsProviderWin> provider(
      new MockDisplaySettingsProviderWin());

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
