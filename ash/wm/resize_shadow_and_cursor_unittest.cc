// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_constants.h"
#include "ash/frame/custom_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/cursor_manager_test_api.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_state.h"
#include "base/bind.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/hit_test.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace test {

namespace {

// views::WidgetDelegate which uses ash::CustomFrameViewAsh.
class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() {}
  virtual ~TestWidgetDelegate() {}

  // views::WidgetDelegateView overrides:
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual bool CanMaximize() const OVERRIDE {
    return true;
  }
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    return new ash::CustomFrameViewAsh(widget);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWidgetDelegate);
};

}  // namespace

// The test tests that the mouse cursor is changed and that the resize shadows
// are shown when the mouse is hovered over the window edge.
class ResizeShadowAndCursorTest : public AshTestBase {
 public:
  ResizeShadowAndCursorTest() {}
  virtual ~ResizeShadowAndCursorTest() {}

  // AshTestBase override:
  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    views::Widget* widget(views::Widget::CreateWindowWithContextAndBounds(
        new TestWidgetDelegate(), CurrentContext(), gfx::Rect(0, 0, 200, 100)));
    widget->Show();
    window_ = widget->GetNativeView();

    // Add a child window to |window_| in order to properly test that the resize
    // handles and the resize shadows are shown when the mouse is
    // ash::kResizeInsideBoundsSize inside of |window_|'s edges.
    aura::Window* child = CreateTestWindowInShell(
        SK_ColorWHITE, 0, gfx::Rect(0, 10, 200, 90));
    window_->AddChild(child);
  }

  // Returns the hit test code if there is a resize shadow. Returns HTNOWHERE if
  // there is no resize shadow.
  int ResizeShadowHitTest() const {
    ash::ResizeShadow* resize_shadow = ash::Shell::GetInstance()
                                           ->resize_shadow_controller()
                                           ->GetShadowForWindowForTest(window_);
    return resize_shadow ? resize_shadow->GetLastHitTestForTest() : HTNOWHERE;
  }

  // Returns true if there is a resize shadow.
  bool HasResizeShadow() const {
    return ResizeShadowHitTest() != HTNOWHERE;
  }

  // Returns the current cursor type.
  int GetCurrentCursorType() const {
    CursorManagerTestApi test_api(ash::Shell::GetInstance()->cursor_manager());
    return test_api.GetCurrentCursor().native_type();
  }

  // Called for each step of a scroll sequence initiated at the bottom right
  // corner of |window_|. Tests whether the resize shadow is shown.
  void ProcessBottomRightResizeGesture(ui::EventType type,
                                       const gfx::Vector2dF& delta) {
    if (type == ui::ET_GESTURE_SCROLL_END) {
      // After gesture scroll ends, there should be no resize shadow.
      EXPECT_FALSE(HasResizeShadow());
    } else {
      EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowHitTest());
    }
  }

  aura::Window* window() {
    return window_;
  }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ResizeShadowAndCursorTest);
};

// Test whether the resize shadows are visible and the cursor type based on the
// mouse's position.
TEST_F(ResizeShadowAndCursorTest, MouseHover) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(ash::wm::GetWindowState(window())->IsNormalStateType());

  generator.MoveMouseTo(50, 50);
  EXPECT_FALSE(HasResizeShadow());
  EXPECT_EQ(ui::kCursorNull, GetCurrentCursorType());

  generator.MoveMouseTo(gfx::Point(50, 0));
  EXPECT_EQ(HTTOP, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorNorthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 50);
  EXPECT_FALSE(HasResizeShadow());
  EXPECT_EQ(ui::kCursorNull, GetCurrentCursorType());

  generator.MoveMouseTo(200, 100);
  EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorSouthEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + ash::kResizeOutsideBoundsSize - 1);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 + ash::kResizeOutsideBoundsSize + 10);
  EXPECT_FALSE(HasResizeShadow());
  EXPECT_EQ(ui::kCursorNull, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - ash::kResizeInsideBoundsSize);
  EXPECT_EQ(HTBOTTOM, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorSouthResize, GetCurrentCursorType());

  generator.MoveMouseTo(50, 100 - ash::kResizeInsideBoundsSize - 10);
  EXPECT_FALSE(HasResizeShadow());
  EXPECT_EQ(ui::kCursorNull, GetCurrentCursorType());
}

// Test that the resize shadows stay visible and that the cursor stays the same
// as long as a user is resizing a window.
TEST_F(ResizeShadowAndCursorTest, MouseDrag) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(ash::wm::GetWindowState(window())->IsNormalStateType());
  gfx::Size initial_size(window()->bounds().size());

  generator.MoveMouseTo(200, 50);
  generator.PressLeftButton();
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorEastResize, GetCurrentCursorType());

  generator.MoveMouseTo(210, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorEastResize, GetCurrentCursorType());

  generator.ReleaseLeftButton();
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorEastResize, GetCurrentCursorType());

  gfx::Size new_size(window()->bounds().size());
  EXPECT_NE(new_size.ToString(), initial_size.ToString());
}

// Test that the resize shadows stay visible while resizing a window via touch.
TEST_F(ResizeShadowAndCursorTest, Touch) {
  ASSERT_TRUE(ash::wm::GetWindowState(window())->IsNormalStateType());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  int start_x = 200 + ash::kResizeOutsideBoundsSize - 1;
  int start_y = 100 + ash::kResizeOutsideBoundsSize - 1;
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(start_x, start_y),
      gfx::Point(start_x + 50, start_y + 50),
      base::TimeDelta::FromMilliseconds(200),
      3,
      base::Bind(&ResizeShadowAndCursorTest::ProcessBottomRightResizeGesture,
                 base::Unretained(this)));
}

// Test that the resize shadows are not visible and that the default cursor is
// used when the window is maximized.
TEST_F(ResizeShadowAndCursorTest, MaximizeRestore) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(ash::wm::GetWindowState(window())->IsNormalStateType());

  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - ash::kResizeInsideBoundsSize, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorEastResize, GetCurrentCursorType());

  ash::wm::GetWindowState(window())->Maximize();
  gfx::Rect bounds(window()->GetBoundsInRootWindow());
  gfx::Point right_center(bounds.right() - 1,
                          (bounds.y() + bounds.bottom()) / 2);
  generator.MoveMouseTo(right_center);
  EXPECT_FALSE(HasResizeShadow());
  EXPECT_EQ(ui::kCursorNull, GetCurrentCursorType());

  ash::wm::GetWindowState(window())->Restore();
  generator.MoveMouseTo(200, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorEastResize, GetCurrentCursorType());
  generator.MoveMouseTo(200 - ash::kResizeInsideBoundsSize, 50);
  EXPECT_EQ(HTRIGHT, ResizeShadowHitTest());
  EXPECT_EQ(ui::kCursorEastResize, GetCurrentCursorType());
}

}  // namespace test
}  // namespace ash
