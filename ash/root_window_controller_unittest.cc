// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"
#include "ash/display/multi_display_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace {

class TestDelegate : public views::WidgetDelegateView {
 public:
  explicit TestDelegate(bool system_modal) : system_modal_(system_modal) {}
  virtual ~TestDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }

  virtual ui::ModalType GetModalType() const OVERRIDE {
    return system_modal_ ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_NONE;
  }

 private:
  bool system_modal_;
  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

views::Widget* CreateTestWidget(const gfx::Rect& bounds) {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(NULL, bounds);
  widget->Show();
  return widget;
}

views::Widget* CreateModalWidget(const gfx::Rect& bounds) {
  views::Widget* widget =
      views::Widget::CreateWindowWithBounds(new TestDelegate(true), bounds);
  widget->Show();
  return widget;
}

aura::Window* GetModalContainer(aura::RootWindow* root_window) {
  return Shell::GetContainer(
      root_window,
      ash::internal::kShellWindowId_SystemModalContainer);
}

}  // namespace

namespace test {
class RootWindowControllerTest : public test::AshTestBase {
 public:
  RootWindowControllerTest() {}
  virtual ~RootWindowControllerTest() {}

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
  DISALLOW_COPY_AND_ASSIGN(RootWindowControllerTest);
};

TEST_F(RootWindowControllerTest, MoveWindows_Basic) {
  UpdateDisplay("0+0-600x600,600+0-500x500");
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  // Emulate virtual screen coordinate system.
  root_windows[0]->SetBounds(gfx::Rect(0, 0, 600, 600));
  root_windows[1]->SetBounds(gfx::Rect(600, 0, 500, 500));

  views::Widget* normal = CreateTestWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("100x100", normal->GetWindowScreenBounds().size().ToString());

  views::Widget* maximized = CreateTestWidget(gfx::Rect(700, 10, 100, 100));
  maximized->Maximize();
  EXPECT_EQ(root_windows[1], maximized->GetNativeView()->GetRootWindow());
#if !defined(OS_WIN)
  // TODO(oshima): Window reports smaller screen size. Investigate why.
  EXPECT_EQ("500x500", maximized->GetWindowScreenBounds().size().ToString());
#endif

  views::Widget* minimized = CreateTestWidget(gfx::Rect(800, 10, 100, 100));
  minimized->Minimize();
  EXPECT_EQ(root_windows[1], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("100x100", minimized->GetWindowScreenBounds().size().ToString());

  views::Widget* fullscreen = CreateTestWidget(gfx::Rect(900, 10, 100, 100));
  fullscreen->SetFullscreen(true);
  EXPECT_EQ(root_windows[1], fullscreen->GetNativeView()->GetRootWindow());
#if !defined(OS_WIN)
  // TODO(oshima): Window reports smaller screen size. Investigate why.
  EXPECT_EQ("500x500", fullscreen->GetWindowScreenBounds().size().ToString());
#endif

  UpdateDisplay("0+0-600x600");

  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_EQ("100x100", normal->GetWindowScreenBounds().size().ToString());

  // Maximized area on primary display has 2px (given as
  // kAutoHideSize in shelf_layout_manager.cc) inset at the bottom.
  EXPECT_EQ(root_windows[0], maximized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("600x598", maximized->GetWindowScreenBounds().size().ToString());

  EXPECT_EQ(root_windows[0], minimized->GetNativeView()->GetRootWindow());
  EXPECT_EQ("100x100", minimized->GetWindowScreenBounds().size().ToString());

  EXPECT_EQ(root_windows[0], fullscreen->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(fullscreen->IsFullscreen());
  EXPECT_EQ("600x600", fullscreen->GetWindowScreenBounds().size().ToString());
}

TEST_F(RootWindowControllerTest, MoveWindows_Modal) {
  UpdateDisplay("0+0-500x500,500+0-500x500");

  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  // Emulate virtual screen coordinate system.
  root_windows[0]->SetBounds(gfx::Rect(0, 0, 500, 500));
  root_windows[1]->SetBounds(gfx::Rect(500, 0, 500, 500));

  views::Widget* normal = CreateTestWidget(gfx::Rect(300, 10, 100, 100));
  EXPECT_EQ(root_windows[0], normal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(normal->GetNativeView()));

  views::Widget* modal = CreateModalWidget(gfx::Rect(650, 10, 100, 100));
  EXPECT_EQ(root_windows[1], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(GetModalContainer(root_windows[1])->Contains(
      modal->GetNativeView()));
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  aura::test::EventGenerator generator_1st(root_windows[0]);
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));

  UpdateDisplay("0+0-500x500");
  EXPECT_EQ(root_windows[0], modal->GetNativeView()->GetRootWindow());
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
  generator_1st.ClickLeftButton();
  EXPECT_TRUE(wm::IsActiveWindow(modal->GetNativeView()));
}

}  // namespace test
}  // namespace ash
