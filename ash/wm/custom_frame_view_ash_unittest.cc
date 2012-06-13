// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/custom_frame_view_ash.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

namespace {

class ShellViewsDelegate : public views::TestViewsDelegate {
 public:
  ShellViewsDelegate() {}
  virtual ~ShellViewsDelegate() {}

  // Overridden from views::TestViewsDelegate:
  virtual views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    return ash::Shell::GetInstance()->CreateDefaultNonClientFrameView(widget);
  }
  bool UseTransparentWindows() const OVERRIDE {
    // Ash uses transparent window frames.
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() {}
  virtual ~TestWidgetDelegate() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual bool CanMaximize() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

class CustomFrameViewAshTest : public ash::test::AshTestBase {
 public:
  CustomFrameViewAshTest() {}
  virtual ~CustomFrameViewAshTest() {}

  views::Widget* CreateWidget() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    views::Widget* widget = new views::Widget;
    params.delegate = new TestWidgetDelegate;
    widget->Init(params);
    widget->Show();
    return widget;
  }

  CustomFrameViewAsh* custom_frame_view_ash(views::Widget* widget) const {
    return static_cast<CustomFrameViewAsh*>(widget->non_client_view()->
        frame_view());
  }

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    if (!views::ViewsDelegate::views_delegate) {
      views_delegate_.reset(new ShellViewsDelegate);
      views::ViewsDelegate::views_delegate = views_delegate_.get();
    }
  }

  virtual void TearDown() OVERRIDE {
    if (views_delegate_.get()) {
      views::ViewsDelegate::views_delegate = NULL;
      views_delegate_.reset();
    }
    AshTestBase::TearDown();
  }

 private:
  scoped_ptr<views::ViewsDelegate> views_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CustomFrameViewAshTest);
};

// Tests that clicking on the resize-button toggles between maximize and normal
// state.
TEST_F(CustomFrameViewAshTest, ResizeButtonToggleMaximize) {
  views::Widget* widget = CreateWidget();
  aura::Window* window = widget->GetNativeWindow();
  CustomFrameViewAsh* frame = custom_frame_view_ash(widget);
  CustomFrameViewAsh::TestApi test(frame);
  views::View* view = test.maximize_button();
  gfx::Point center = view->GetScreenBounds().CenterPoint();

  aura::test::EventGenerator generator(window->GetRootWindow(), center);

  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  generator.ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(ash::wm::IsWindowMaximized(window));

  center = view->GetScreenBounds().CenterPoint();
  generator.MoveMouseTo(center);
  generator.ClickLeftButton();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(ash::wm::IsWindowMaximized(window));

  widget->Close();
}

// Tests that click+dragging on the resize-button tiles or minimizes the window.
TEST_F(CustomFrameViewAshTest, ResizeButtonDrag) {
  views::Widget* widget = CreateWidget();
  aura::Window* window = widget->GetNativeWindow();
  CustomFrameViewAsh* frame = custom_frame_view_ash(widget);
  CustomFrameViewAsh::TestApi test(frame);
  views::View* view = test.maximize_button();
  gfx::Point center = view->GetScreenBounds().CenterPoint();
  const int kGridSize = ash::Shell::GetInstance()->GetGridSize();

  aura::test::EventGenerator generator(window->GetRootWindow(), center);

  EXPECT_TRUE(ash::wm::IsWindowNormal(window));

  // Snap right.
  {
    generator.PressLeftButton();
    generator.MoveMouseBy(10, 0);
    generator.ReleaseLeftButton();
    RunAllPendingInMessageLoop();

    EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
    EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
    internal::SnapSizer sizer(window, center,
        internal::SnapSizer::RIGHT_EDGE, kGridSize);
    EXPECT_EQ(sizer.target_bounds().ToString(), window->bounds().ToString());
  }

  // Snap left.
  {
    center = view->GetScreenBounds().CenterPoint();
    generator.MoveMouseTo(center);
    generator.PressLeftButton();
    generator.MoveMouseBy(-10, 0);
    generator.ReleaseLeftButton();
    RunAllPendingInMessageLoop();

    EXPECT_FALSE(ash::wm::IsWindowMaximized(window));
    EXPECT_FALSE(ash::wm::IsWindowMinimized(window));
    internal::SnapSizer sizer(window, center,
        internal::SnapSizer::LEFT_EDGE, kGridSize);
    EXPECT_EQ(sizer.target_bounds().ToString(), window->bounds().ToString());
  }

  // Minimize.
  {
    center = view->GetScreenBounds().CenterPoint();
    generator.MoveMouseTo(center);
    generator.PressLeftButton();
    generator.MoveMouseBy(0, 10);
    generator.ReleaseLeftButton();
    RunAllPendingInMessageLoop();

    EXPECT_TRUE(ash::wm::IsWindowMinimized(window));
  }

  widget->Close();
}

}  // namespace internal
}  // namespace ash
