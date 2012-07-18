// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/screen_ash.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {
class ScreenAshTest : public test::AshTestBase {
 public:
  ScreenAshTest() {}
  virtual ~ScreenAshTest() {}

  virtual void SetUp() OVERRIDE {
    internal::DisplayController::SetExtendedDesktopEnabled(true);
    internal::DisplayController::SetVirtualScreenCoordinatesEnabled(true);
    AshTestBase::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    AshTestBase::TearDown();
    internal::DisplayController::SetExtendedDesktopEnabled(false);
    internal::DisplayController::SetVirtualScreenCoordinatesEnabled(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenAshTest);
};

#if !defined(OS_WIN)
TEST_F(ScreenAshTest, Bounds) {
  UpdateDisplay("0+0-600x600,600+0-500x500");

  views::Widget* primary =
      views::Widget::CreateWindowWithBounds(NULL, gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary =
      views::Widget::CreateWindowWithBounds(NULL, gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  // Maximized bounds
  EXPECT_EQ("0,0 600x598",
            ScreenAsh::GetMaximizedWindowParentBounds(
                primary->GetNativeView()).ToString());
  EXPECT_EQ("0,0 500x500",
            ScreenAsh::GetMaximizedWindowParentBounds(
                secondary->GetNativeView()).ToString());

  // Unmaximized work area bounds
  EXPECT_EQ("0,0 600x552",
            ScreenAsh::GetUnmaximizedWorkAreaParentBounds(
                primary->GetNativeView()).ToString());
  EXPECT_EQ("0,0 500x500",
            ScreenAsh::GetUnmaximizedWorkAreaParentBounds(
                secondary->GetNativeView()).ToString());

  // Display bounds
  EXPECT_EQ("0,0 600x600",
            ScreenAsh::GetDisplayParentBounds(
                primary->GetNativeView()).ToString());
  EXPECT_EQ("0,0 500x500",
            ScreenAsh::GetDisplayParentBounds(
                secondary->GetNativeView()).ToString());

  // Work area bounds
  EXPECT_EQ("0,0 600x552",
            ScreenAsh::GetDisplayWorkAreaParentBounds(
                primary->GetNativeView()).ToString());
  EXPECT_EQ("0,0 500x500",
            ScreenAsh::GetDisplayWorkAreaParentBounds(
                secondary->GetNativeView()).ToString());
}
#endif

TEST_F(ScreenAshTest, ConvertRect) {
  UpdateDisplay("0+0-600x600,600+0-500x500");

  views::Widget* primary =
      views::Widget::CreateWindowWithBounds(NULL, gfx::Rect(10, 10, 100, 100));
  primary->Show();
  views::Widget* secondary =
      views::Widget::CreateWindowWithBounds(NULL, gfx::Rect(610, 10, 100, 100));
  secondary->Show();

  EXPECT_EQ(
      "0,0 100x100",
      ScreenAsh::ConvertRectFromScreen(
          primary->GetNativeView(), gfx::Rect(10, 10, 100, 100)).ToString());
  EXPECT_EQ(
      "10,10 100x100",
      ScreenAsh::ConvertRectFromScreen(
          secondary->GetNativeView(), gfx::Rect(620, 20, 100, 100)).ToString());

  EXPECT_EQ(
      "40,40 100x100",
      ScreenAsh::ConvertRectToScreen(
          primary->GetNativeView(), gfx::Rect(30, 30, 100, 100)).ToString());
  EXPECT_EQ(
      "650,50 100x100",
      ScreenAsh::ConvertRectToScreen(
          secondary->GetNativeView(), gfx::Rect(40, 40, 100, 100)).ToString());
}

}  // namespace test
}  // namespace ash
