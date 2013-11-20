// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioner.h"

#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

typedef test::AshTestBase WindowPositionerTest;

TEST_F(WindowPositionerTest, OpenMaximizedWindowOnSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("400x400,500x500");
  Shell::GetInstance()->set_target_root_window(
      Shell::GetAllRootWindows()[1]);
  shell::ToplevelWindow::CreateParams params;
  params.can_resize = true;
  params.can_maximize = true;
  views::Widget* widget =
      shell::ToplevelWindow::CreateToplevelWindow(params);
  EXPECT_EQ("400,0 500x453", widget->GetWindowBoundsInScreen().ToString());
}

TEST_F(WindowPositionerTest, OpenDefaultWindowOnSecondDisplay) {
  if (!SupportsMultipleDisplays())
    return;
#if defined(OS_WIN)
  ash::WindowPositioner::SetMaximizeFirstWindow(true);
#endif
  UpdateDisplay("400x400,1400x900");
  aura::Window* second_root_window = Shell::GetAllRootWindows()[1];
  Shell::GetInstance()->set_target_root_window(
      second_root_window);
  shell::ToplevelWindow::CreateParams params;
  params.can_resize = true;
  params.can_maximize = true;
  views::Widget* widget =
      shell::ToplevelWindow::CreateToplevelWindow(params);
  gfx::Rect bounds = widget->GetWindowBoundsInScreen();
#if defined(OS_WIN)
  EXPECT_TRUE(widget->IsMaximized());
#else
  // The window should be in the 2nd display with the default size.
  EXPECT_EQ("300x300", bounds.size().ToString());
#endif
  EXPECT_TRUE(Shell::GetScreen()->GetDisplayNearestWindow(
      second_root_window).bounds().Contains(bounds));
}

namespace {

// A WidgetDelegate that returns the out of display saved bounds.
class OutOfDisplayDelegate : public views::WidgetDelegate {
 public:
  explicit OutOfDisplayDelegate(views::Widget* widget) : widget_(widget) {}
  virtual ~OutOfDisplayDelegate() {}

  // Overridden from WidgetDelegate:
  virtual void DeleteDelegate() OVERRIDE {
    delete this;
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return widget_;
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return widget_;
  }
  virtual bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE {
    bounds->SetRect(450, 10, 100, 100);
    *show_state = ui::SHOW_STATE_NORMAL;
    return true;
  }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(OutOfDisplayDelegate);
};

}  // namespace

TEST_F(WindowPositionerTest, EnsureMinimumVisibility) {
  if (!SupportsHostWindowResize())
    return;

  UpdateDisplay("400x400");
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = new OutOfDisplayDelegate(widget);
  params.context = Shell::GetPrimaryRootWindow();
  widget->Init(params);
  widget->SetBounds(gfx::Rect(450,10, 100, 100));
  wm::GetWindowState(widget->GetNativeView())->set_minimum_visibility(true);
  widget->Show();
  // Make sure the bounds is adjusted to be inside the work area.
  EXPECT_EQ("390,10 100x100", widget->GetWindowBoundsInScreen().ToString());
  widget->CloseNow();
}

}  // namespace
