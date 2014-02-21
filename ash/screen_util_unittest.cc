// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

typedef test::AshTestBase ScreenUtilTest;

TEST_F(ScreenUtilTest, Bounds) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x600,500x500");
  Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager()->
      SetAutoHideBehavior(ash::SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  views::Widget* primary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  // Maximized bounds
  EXPECT_EQ("0,0 600x597",
            ScreenUtil::GetMaximizedWindowBoundsInParent(
                primary->GetNativeView()).ToString());
  EXPECT_EQ("0,0 500x453",
            ScreenUtil::GetMaximizedWindowBoundsInParent(
                secondary->GetNativeView()).ToString());

  // Display bounds
  EXPECT_EQ("0,0 600x600",
            ScreenUtil::GetDisplayBoundsInParent(
                primary->GetNativeView()).ToString());
  EXPECT_EQ("0,0 500x500",
            ScreenUtil::GetDisplayBoundsInParent(
                secondary->GetNativeView()).ToString());

  // Work area bounds
  EXPECT_EQ("0,0 600x597",
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(
                primary->GetNativeView()).ToString());
  EXPECT_EQ("0,0 500x453",
            ScreenUtil::GetDisplayWorkAreaBoundsInParent(
                secondary->GetNativeView()).ToString());
}

// Test verifies a stable handling of secondary screen widget changes
// (crbug.com/226132).
TEST_F(ScreenUtilTest, StabilityTest) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x600,500x500");
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  EXPECT_EQ(Shell::GetAllRootWindows()[1],
      secondary->GetNativeView()->GetRootWindow());
  secondary->Show();
  secondary->Maximize();
  secondary->Show();
  secondary->SetFullscreen(true);
  secondary->Hide();
  secondary->Close();
}

TEST_F(ScreenUtilTest, ConvertRect) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x600,500x500");

  views::Widget* primary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  EXPECT_EQ(
      "0,0 100x100",
      ScreenUtil::ConvertRectFromScreen(
          primary->GetNativeView(), gfx::Rect(10, 10, 100, 100)).ToString());
  EXPECT_EQ(
      "10,10 100x100",
      ScreenUtil::ConvertRectFromScreen(
          secondary->GetNativeView(), gfx::Rect(620, 20, 100, 100)).ToString());

  EXPECT_EQ(
      "40,40 100x100",
      ScreenUtil::ConvertRectToScreen(
          primary->GetNativeView(), gfx::Rect(30, 30, 100, 100)).ToString());
  EXPECT_EQ(
      "650,50 100x100",
      ScreenUtil::ConvertRectToScreen(
          secondary->GetNativeView(), gfx::Rect(40, 40, 100, 100)).ToString());
}

}  // namespace test
}  // namespace ash
