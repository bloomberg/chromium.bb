// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_util.h"

#include "ash/public/cpp/config.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/unified_desktop_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

using ScreenUtilTest = AshTestBase;

TEST_F(ScreenUtilTest, Bounds) {
  UpdateDisplay("600x600,500x500");
  views::Widget* primary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  // Maximized bounds. By default the shelf is 47px tall (ash::kShelfSize).
  EXPECT_EQ(
      gfx::Rect(0, 0, 600, 552).ToString(),
      ScreenUtil::GetMaximizedWindowBoundsInParent(primary->GetNativeView())
          .ToString());
  EXPECT_EQ(
      gfx::Rect(0, 0, 500, 452).ToString(),
      ScreenUtil::GetMaximizedWindowBoundsInParent(secondary->GetNativeView())
          .ToString());

  // Display bounds
  EXPECT_EQ("0,0 600x600",
            ScreenUtil::GetDisplayBoundsInParent(primary->GetNativeView())
                .ToString());
  EXPECT_EQ("0,0 500x500",
            ScreenUtil::GetDisplayBoundsInParent(secondary->GetNativeView())
                .ToString());

  // Work area bounds
  EXPECT_EQ(
      gfx::Rect(0, 0, 600, 552).ToString(),
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(primary->GetNativeView())
          .ToString());
  EXPECT_EQ(
      gfx::Rect(0, 0, 500, 452).ToString(),
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(secondary->GetNativeView())
          .ToString());
}

// Test verifies a stable handling of secondary screen widget changes
// (crbug.com/226132).
TEST_F(ScreenUtilTest, StabilityTest) {
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
  UpdateDisplay("600x600,500x500");

  views::Widget* primary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  gfx::Rect r1(10, 10, 100, 100);
  ::wm::ConvertRectFromScreen(primary->GetNativeView(), &r1);
  EXPECT_EQ("0,0 100x100", r1.ToString());

  gfx::Rect r2(620, 20, 100, 100);
  ::wm::ConvertRectFromScreen(secondary->GetNativeView(), &r2);
  EXPECT_EQ("10,10 100x100", r2.ToString());

  gfx::Rect r3(30, 30, 100, 100);
  ::wm::ConvertRectToScreen(primary->GetNativeView(), &r3);
  EXPECT_EQ("40,40 100x100", r3.ToString());

  gfx::Rect r4(40, 40, 100, 100);
  ::wm::ConvertRectToScreen(secondary->GetNativeView(), &r4);
  EXPECT_EQ("650,50 100x100", r4.ToString());
}

TEST_F(ScreenUtilTest, ShelfDisplayBoundsInUnifiedDesktop) {
  display_manager()->SetUnifiedDesktopEnabled(true);

  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  aura::Window* window = widget->GetNativeWindow();

  UpdateDisplay("500x400");
  EXPECT_EQ("0,0 500x400",
            ScreenUtil::GetDisplayBoundsWithShelf(window).ToString());

  UpdateDisplay("500x400,600x400");
  EXPECT_EQ("0,0 500x400",
            ScreenUtil::GetDisplayBoundsWithShelf(window).ToString());

  // Move to the 2nd physical display. Shelf's display still should be
  // the first.
  widget->SetBounds(gfx::Rect(800, 0, 100, 100));
  ASSERT_EQ("800,0 100x100", widget->GetWindowBoundsInScreen().ToString());

  EXPECT_EQ("0,0 500x400",
            ScreenUtil::GetDisplayBoundsWithShelf(window).ToString());

  UpdateDisplay("600x500");
  EXPECT_EQ("0,0 600x500",
            ScreenUtil::GetDisplayBoundsWithShelf(window).ToString());
}

TEST_F(ScreenUtilTest, ShelfDisplayBoundsInUnifiedDesktopGrid) {
  // TODO: requires unified desktop mode. http://crbug.com/581462.
  if (Shell::GetAshConfig() == Config::MASH)
    return;

  UpdateDisplay("500x400,400x600,300x600,200x300,600x200,350x400");
  display_manager()->SetUnifiedDesktopEnabled(true);

  views::Widget* widget = views::Widget::CreateWindowWithContextAndBounds(
      NULL, CurrentContext(), gfx::Rect(10, 10, 100, 100));
  aura::Window* window = widget->GetNativeWindow();

  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  ASSERT_EQ(6u, list.size());
  // Create a 3 x 2 vertical layout matrix and set it.
  // [500 x 400] [400 x 600]
  // [300 x 600] [200 x 300]
  // [600 x 200] [350 x 400]
  display::UnifiedDesktopLayoutMatrix matrix;
  matrix.resize(3u);
  matrix[0].emplace_back(list[0]);
  matrix[0].emplace_back(list[1]);
  matrix[1].emplace_back(list[2]);
  matrix[1].emplace_back(list[3]);
  matrix[2].emplace_back(list[4]);
  matrix[2].emplace_back(list[5]);
  display_manager()->SetUnifiedDesktopMatrix(matrix);
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(gfx::Size(766, 1254), screen->GetPrimaryDisplay().size());

  // Regardless of where the window is, the shelf is always in the top left
  // display in the matrix.
  EXPECT_EQ(gfx::Rect(0, 0, 499, 400),
            ScreenUtil::GetDisplayBoundsWithShelf(window));

  // Move to the bottom right display.
  widget->SetBounds(gfx::Rect(620, 940, 100, 100));
  EXPECT_EQ(gfx::Rect(0, 0, 499, 400),
            ScreenUtil::GetDisplayBoundsWithShelf(window));
}

}  // namespace ash
