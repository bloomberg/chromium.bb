// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_handler.h"

#include "ash/ash_constants.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/lock_state_controller_impl2.h"
#include "ash/wm/property_util.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ash/wm/workspace_controller.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/events/event.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif

namespace ash {
namespace test {

namespace {

// A simple window delegate that returns the specified hit-test code when
// requested and applies a minimum size constraint if there is one.
class TestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  explicit TestWindowDelegate(int hittest_code) {
    set_window_component(hittest_code);
  }
  virtual ~TestWindowDelegate() {}

 private:
  // Overridden from aura::Test::TestWindowDelegate:
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

class ToplevelWindowEventHandlerTest : public AshTestBase {
 public:
  ToplevelWindowEventHandlerTest() {}
  virtual ~ToplevelWindowEventHandlerTest() {}

 protected:
  aura::Window* CreateWindow(int hittest_code) {
    TestWindowDelegate* d1 = new TestWindowDelegate(hittest_code);
    aura::Window* w1 = new aura::Window(d1);
    w1->SetType(aura::client::WINDOW_TYPE_NORMAL);
    w1->set_id(1);
    w1->Init(ui::LAYER_TEXTURED);
    aura::Window* parent =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          internal::kShellWindowId_AlwaysOnTopContainer);
    parent->AddChild(w1);
    w1->SetBounds(gfx::Rect(0, 0, 100, 100));
    w1->Show();
    return w1;
  }

  void DragFromCenterBy(aura::Window* window, int dx, int dy) {
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), window);
    generator.DragMouseBy(dx, dy);
  }

  void TouchDragFromCenterBy(aura::Window* window, int dx, int dy) {
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), window);
    generator.PressMoveAndReleaseTouchBy(dx, dy);
  }

  scoped_ptr<ToplevelWindowEventHandler> handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventHandlerTest);
};

}

TEST_F(ToplevelWindowEventHandlerTest, Caption) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTCAPTION));
  gfx::Size size = w1->bounds().size();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should have been offset by 100,100.
  EXPECT_EQ("100,100", w1->bounds().origin().ToString());
  // Size should not have.
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());

  TouchDragFromCenterBy(w1.get(), 100, 100);
  // Position should have been offset by 100,100.
  EXPECT_EQ("200,200", w1->bounds().origin().ToString());
  // Size should not have.
  EXPECT_EQ(size.ToString(), w1->bounds().size().ToString());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomRight) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, GrowBox) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTGROWBOX));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  gfx::Point position = w1->bounds().origin();
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseToCenterOf(w1.get());
  generator.DragMouseBy(100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200), w1->bounds().size());

  // Shrink the wnidow by (-100, -100).
  generator.DragMouseBy(-100, -100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have decreased by 100,100.
  EXPECT_EQ(gfx::Size(100, 100), w1->bounds().size());

  // Enforce minimum size.
  generator.DragMouseBy(-60, -60);
  EXPECT_EQ(position, w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, Right) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,0.
  EXPECT_EQ(gfx::Size(200, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, Bottom) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOM));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 0,100.
  EXPECT_EQ(gfx::Size(100, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, TopRight) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPRIGHT));
  DragFromCenterBy(w1.get(), -50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, Top) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOP));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 0,50.
  EXPECT_EQ(gfx::Size(100, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, Left) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,0.
  EXPECT_EQ(gfx::Size(50, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomLeft) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMLEFT));
  DragFromCenterBy(w1.get(), 50, -50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, TopLeft) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,50.
  EXPECT_EQ(gfx::Point(50, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, Client) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTCLIENT));
  gfx::Rect bounds = w1->bounds();
  DragFromCenterBy(w1.get(), 100, 100);
  // Neither position nor size should have changed.
  EXPECT_EQ(bounds, w1->bounds());
}

TEST_F(ToplevelWindowEventHandlerTest, LeftPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large left-to-right drag.  Window width should be clamped to
  // minimum and position change should be limited as well.
  DragFromCenterBy(w1.get(), 333, 0);
  EXPECT_EQ(gfx::Point(60, 0), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, RightPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));
  gfx::Point position = w1->bounds().origin();

  // Simulate a large right-to-left drag.  Window width should be clamped to
  // minimum and position should not change.
  DragFromCenterBy(w1.get(), -333, 0);
  EXPECT_EQ(position, w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, TopLeftPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large top-left to bottom-right drag.  Window width should be
  // clamped to minimum and position should be limited.
  DragFromCenterBy(w1.get(), 333, 444);
  EXPECT_EQ(gfx::Point(60, 60), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, TopRightPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large top-right to bottom-left drag.  Window size should be
  // clamped to minimum, x position should not change, and y position should
  // be clamped.
  DragFromCenterBy(w1.get(), -333, 444);
  EXPECT_EQ(gfx::Point(0, 60), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomLeftPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));

  // Simulate a large bottom-left to top-right drag.  Window size should be
  // clamped to minimum, x position should be clamped, and y position should
  // not change.
  DragFromCenterBy(w1.get(), 333, -444);
  EXPECT_EQ(gfx::Point(60, 0), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomRightPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_minimum_size(gfx::Size(40, 40));
  gfx::Point position = w1->bounds().origin();

  // Simulate a large bottom-right to top-left drag.  Window size should be
  // clamped to minimum and position should not change.
  DragFromCenterBy(w1.get(), -333, -444);
  EXPECT_EQ(position, w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomRightWorkArea) {
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOMRIGHT));
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      target.get()).work_area();
  gfx::Point position = target->bounds().origin();
  // Drag further than work_area bottom.
  DragFromCenterBy(target.get(), 100, work_area.height());
  // Position should not have changed.
  EXPECT_EQ(position, target->bounds().origin());
  // Size should have increased by 100, work_area.height() - target->bounds.y()
  EXPECT_EQ(gfx::Size(200, work_area.height() - target->bounds().y()),
            target->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomLeftWorkArea) {
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOMLEFT));
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      target.get()).work_area();
  gfx::Point position = target->bounds().origin();
  // Drag further than work_area bottom.
  DragFromCenterBy(target.get(), -30, work_area.height());
  // origin is now at 70, 100.
  EXPECT_EQ(position.x() - 30, target->bounds().x());
  EXPECT_EQ(position.y(), target->bounds().y());
  // Size should have increased by 30, work_area.height() - target->bounds.y()
  EXPECT_EQ(gfx::Size(130, work_area.height() - target->bounds().y()),
            target->bounds().size());
}

TEST_F(ToplevelWindowEventHandlerTest, BottomWorkArea) {
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOM));
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      target.get()).work_area();
  gfx::Point position = target->bounds().origin();
  // Drag further than work_area bottom.
  DragFromCenterBy(target.get(), 0, work_area.height());
  // Position should not have changed.
  EXPECT_EQ(position, target->bounds().origin());
  // Size should have increased by 0, work_area.height() - target->bounds.y()
  EXPECT_EQ(gfx::Size(100, work_area.height() - target->bounds().y()),
            target->bounds().size());
}

// Verifies we don't let windows drag to a -y location.
TEST_F(ToplevelWindowEventHandlerTest, DontDragToNegativeY) {
  scoped_ptr<aura::Window> target(CreateWindow(HTTOP));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  generator.MoveMouseTo(0, 5);
  generator.DragMouseBy(0, -5);
  // The y location and height should not have changed.
  EXPECT_EQ(0, target->bounds().y());
  EXPECT_EQ(100, target->bounds().height());
}

// Verifies we don't let windows go bigger than the display width.
TEST_F(ToplevelWindowEventHandlerTest, DontGotWiderThanScreen) {
  scoped_ptr<aura::Window> target(CreateWindow(HTRIGHT));
  gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
      target.get()).bounds();
  DragFromCenterBy(target.get(), work_area.width() * 2, 0);
  // The y location and height should not have changed.
  EXPECT_EQ(work_area.width(), target->bounds().width());
}

// Verifies that touch-gestures drag the window correctly.
TEST_F(ToplevelWindowEventHandlerTest, GestureDrag) {
  scoped_ptr<aura::Window> target(
      CreateTestWindowInShellWithDelegate(
          new TestWindowDelegate(HTCAPTION),
          0,
          gfx::Rect(0, 0, 100, 100)));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);
  target->SetProperty(aura::client::kCanMaximizeKey, true);

  gfx::Point end = location;

  // Snap right;
  {
    // Get the expected snapped bounds before snapping.
    internal::SnapSizer sizer(target.get(), location,
        internal::SnapSizer::RIGHT_EDGE,
        internal::SnapSizer::OTHER_INPUT);
    gfx::Rect snapped_bounds = sizer.GetSnapBounds(target->bounds());

    end.Offset(100, 0);
    generator.GestureScrollSequence(location, end,
        base::TimeDelta::FromMilliseconds(5),
        10);
    RunAllPendingInMessageLoop();

    // Verify that the window has moved after the gesture.
    EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
    EXPECT_EQ(snapped_bounds.ToString(), target->bounds().ToString());
  }

  old_bounds = target->bounds();

  // Snap left.
  {
    // Get the expected snapped bounds before snapping.
    internal::SnapSizer sizer(target.get(), location,
        internal::SnapSizer::LEFT_EDGE,
        internal::SnapSizer::OTHER_INPUT);
    gfx::Rect snapped_bounds = sizer.GetSnapBounds(target->bounds());
    end = location = target->GetBoundsInRootWindow().CenterPoint();
    end.Offset(-100, 0);
    generator.GestureScrollSequence(location, end,
        base::TimeDelta::FromMilliseconds(5),
        10);
    RunAllPendingInMessageLoop();

    EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
    EXPECT_EQ(snapped_bounds.ToString(), target->bounds().ToString());
  }

  gfx::Rect bounds_before_maximization = target->bounds();
  bounds_before_maximization.Offset(0, 100);
  target->SetBounds(bounds_before_maximization);
  old_bounds = target->bounds();

  // Maximize.
  end = location = target->GetBoundsInRootWindow().CenterPoint();
  end.Offset(0, -100);
  generator.GestureScrollSequence(location, end,
      base::TimeDelta::FromMilliseconds(5),
      10);
  RunAllPendingInMessageLoop();
  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  EXPECT_TRUE(wm::IsWindowMaximized(target.get()));
  EXPECT_EQ(old_bounds.ToString(),
            GetRestoreBoundsInScreen(target.get())->ToString());

  wm::RestoreWindow(target.get());
  target->SetBounds(old_bounds);

  // Minimize.
  end = location = target->GetBoundsInRootWindow().CenterPoint();
  end.Offset(0, 100);
  generator.GestureScrollSequence(location, end,
      base::TimeDelta::FromMilliseconds(5),
      10);
  RunAllPendingInMessageLoop();
  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  EXPECT_TRUE(wm::IsWindowMinimized(target.get()));
  EXPECT_TRUE(GetWindowAlwaysRestoresToRestoreBounds(target.get()));
  EXPECT_EQ(old_bounds.ToString(),
            GetRestoreBoundsInScreen(target.get())->ToString());
}

// Tests that a gesture cannot minimize a window in login/lock screen.
TEST_F(ToplevelWindowEventHandlerTest, GestureDragMinimizeLoginScreen) {
  LockStateControllerImpl2* state_controller =
      static_cast<LockStateControllerImpl2*>
          (Shell::GetInstance()->lock_state_controller());
  state_controller->OnLoginStateChanged(user::LOGGED_IN_NONE);
  state_controller->OnLockStateChanged(false);
  SetUserLoggedIn(false);

  scoped_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  aura::Window* lock = internal::RootWindowController::ForWindow(target.get())->
      GetContainer(internal::kShellWindowId_LockSystemModalContainer);
  lock->AddChild(target.get());
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);
  target->SetProperty(aura::client::kCanMaximizeKey, true);

  gfx::Point end = location;
  end.Offset(0, 100);
  generator.GestureScrollSequence(location, end,
      base::TimeDelta::FromMilliseconds(5),
      10);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(wm::IsWindowMinimized(target.get()));
}

TEST_F(ToplevelWindowEventHandlerTest, GestureDragToRestore) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegate(
          new TestWindowDelegate(HTCAPTION),
          0,
          gfx::Rect(10, 20, 30, 40)));
  window->Show();
  ash::wm::ActivateWindow(window.get());

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());
  gfx::Rect old_bounds = window->bounds();
  gfx::Point location, end;
  end = location = window->GetBoundsInRootWindow().CenterPoint();
  end.Offset(0, 100);
  generator.GestureScrollSequence(location, end,
      base::TimeDelta::FromMilliseconds(5),
      10);
  RunAllPendingInMessageLoop();
  EXPECT_NE(old_bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(wm::IsWindowMinimized(window.get()));
  EXPECT_TRUE(GetWindowAlwaysRestoresToRestoreBounds(window.get()));
  EXPECT_EQ(old_bounds.ToString(),
            GetRestoreBoundsInScreen(window.get())->ToString());
}

// Tests that an unresizable window cannot be dragged or snapped using gestures.
TEST_F(ToplevelWindowEventHandlerTest, GestureDragForUnresizableWindow) {
  scoped_ptr<aura::Window> target(CreateWindow(HTCAPTION));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);

  target->SetProperty(aura::client::kCanResizeKey, false);

  gfx::Point end = location;

  // Try to snap right. The window is not resizable. So it should not snap.
  {
    // Get the expected snapped bounds before the gesture.
    internal::SnapSizer sizer(target.get(), location,
        internal::SnapSizer::RIGHT_EDGE,
        internal::SnapSizer::OTHER_INPUT);
    gfx::Rect snapped_bounds = sizer.GetSnapBounds(target->bounds());

    end.Offset(100, 0);
    generator.GestureScrollSequence(location, end,
        base::TimeDelta::FromMilliseconds(5),
        10);
    RunAllPendingInMessageLoop();

    // Verify that the window has moved after the gesture.
    gfx::Rect expected_bounds(old_bounds);
    expected_bounds.Offset(gfx::Vector2d(100, 0));
    EXPECT_EQ(expected_bounds.ToString(), target->bounds().ToString());

    // Verify that the window did not snap left.
    EXPECT_NE(snapped_bounds.ToString(), target->bounds().ToString());
  }

  old_bounds = target->bounds();

  // Try to snap left. It should not snap.
  {
    // Get the expected snapped bounds before the gesture.
    internal::SnapSizer sizer(target.get(), location,
        internal::SnapSizer::LEFT_EDGE,
        internal::SnapSizer::OTHER_INPUT);
    gfx::Rect snapped_bounds = sizer.GetSnapBounds(target->bounds());
    end = location = target->GetBoundsInRootWindow().CenterPoint();
    end.Offset(-100, 0);
    generator.GestureScrollSequence(location, end,
        base::TimeDelta::FromMilliseconds(5),
        10);
    RunAllPendingInMessageLoop();

    // Verify that the window has moved after the gesture.
    gfx::Rect expected_bounds(old_bounds);
    expected_bounds.Offset(gfx::Vector2d(-100, 0));
    EXPECT_EQ(expected_bounds.ToString(), target->bounds().ToString());

    // Verify that the window did not snap left.
    EXPECT_NE(snapped_bounds.ToString(), target->bounds().ToString());
  }
}

// Tests that dragging multiple windows at the same time is not allowed.
TEST_F(ToplevelWindowEventHandlerTest, GestureDragMultipleWindows) {
  scoped_ptr<aura::Window> target(
      CreateTestWindowInShellWithDelegate(
          new TestWindowDelegate(HTCAPTION),
          0,
          gfx::Rect(0, 0, 100, 100)));
  scoped_ptr<aura::Window> notmoved(
      CreateTestWindowInShellWithDelegate(
          new TestWindowDelegate(HTCAPTION),
          1, gfx::Rect(100, 0, 100, 100)));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);
  target->SetProperty(aura::client::kCanMaximizeKey, true);

  // Send some touch events to start dragging |target|.
  generator.MoveTouch(location);
  generator.PressTouch();
  location.Offset(40, 5);
  generator.MoveTouch(location);

  // Try to drag |notmoved| window. This should not move the window.
  {
    gfx::Rect bounds = notmoved->bounds();
    aura::test::EventGenerator gen(Shell::GetPrimaryRootWindow(),
                                   notmoved.get());
    gfx::Point start = notmoved->bounds().origin() + gfx::Vector2d(10, 10);
    gfx::Point end = start + gfx::Vector2d(100, 10);
    gen.GestureScrollSequence(start, end,
        base::TimeDelta::FromMilliseconds(10),
        10);
    EXPECT_EQ(bounds.ToString(), notmoved->bounds().ToString());
  }
}

// Verifies pressing escape resets the bounds to the original bounds.
// Disabled crbug.com/166219.
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_EscapeReverts DISABLED_EscapeReverts
#else
#define MAYBE_EscapeReverts EscapeReverts
#endif
TEST_F(ToplevelWindowEventHandlerTest, MAYBE_EscapeReverts) {
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOMRIGHT));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 11);

  // Execute any scheduled draws so that pending mouse events are processed.
  RunAllPendingInMessageLoop();

  EXPECT_EQ("0,0 110x111", target->bounds().ToString());
  generator.PressKey(ui::VKEY_ESCAPE, 0);
  generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_EQ("0,0 100x100", target->bounds().ToString());
}

// Verifies window minimization/maximization completes drag.
// Disabled crbug.com/166219.
#if defined(OS_WIN)
#define MAYBE_MinimizeMaximizeCompletes DISABLED_MinimizeMaximizeCompletes
#else
#define MAYBE_MinimizeMaximizeCompletes MinimizeMaximizeCompletes
#endif
TEST_F(ToplevelWindowEventHandlerTest, MAYBE_MinimizeMaximizeCompletes) {
  // Once window is minimized, window dragging completes.
  {
    scoped_ptr<aura::Window> target(CreateWindow(HTCAPTION));
    target->Focus();
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         target.get());
    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    RunAllPendingInMessageLoop();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());

    wm::MinimizeWindow(target.get());
    wm::RestoreWindow(target.get());

    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    RunAllPendingInMessageLoop();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());
  }

  // Once window is maximized, window dragging completes.
  {
    scoped_ptr<aura::Window> target(CreateWindow(HTCAPTION));
    target->Focus();
    aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                         target.get());
    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    RunAllPendingInMessageLoop();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());

    wm::MaximizeWindow(target.get());
    wm::RestoreWindow(target.get());

    generator.PressLeftButton();
    generator.MoveMouseBy(10, 11);
    RunAllPendingInMessageLoop();
    EXPECT_EQ("10,11 100x100", target->bounds().ToString());
  }
}

// Test class for mouse and touch resize shadow tests.
class ToplevelWindowEventHandlerResizeTest
    : public ToplevelWindowEventHandlerTest {
 public:
  ToplevelWindowEventHandlerResizeTest() : delegate_(NULL) {}
  virtual ~ToplevelWindowEventHandlerResizeTest() {}

  virtual void SetUp() OVERRIDE {
    ToplevelWindowEventHandlerTest::SetUp();

    delegate_ = new TestWindowDelegate(HTNOWHERE);
    target_.reset(CreateTestWindowInShellWithDelegate(
        delegate_, 0, gfx::Rect(0, 0, 100, 100)));

    gfx::Insets mouse_insets = gfx::Insets(-ash::kResizeOutsideBoundsSize,
                                           -ash::kResizeOutsideBoundsSize,
                                           -ash::kResizeOutsideBoundsSize,
                                           -ash::kResizeOutsideBoundsSize);
    gfx::Insets touch_insets =
        mouse_insets.Scale(ash::kResizeOutsideBoundsScaleForTouch);
    target_->SetHitTestBoundsOverrideOuter(mouse_insets, touch_insets);
    target_->set_hit_test_bounds_override_inner(mouse_insets);
  }

  virtual void TearDown() OVERRIDE {
    target_.reset();
    ToplevelWindowEventHandlerTest::TearDown();
  }

  // Called on each scroll event. Checks if the correct resize shadow is shown.
  void ProcessEvent(ui::EventType type, const gfx::Vector2dF& delta) {
    if (type == ui::ET_GESTURE_SCROLL_END) {
      // After gesture scroll ends, there should be no resize shadow.
      EXPECT_FALSE(HasResizeShadow());
    } else {
      // Check if there is a resize shadow under the correct border.
      ASSERT_TRUE(HasResizeShadow());
      EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowLastHitTest());
    }
  }

 protected:
  void SetHittestCode(int hittest_code) {
    delegate_->set_window_component(hittest_code);
  }

  aura::Window* target() { return target_.get(); }

  bool HasResizeShadow() const {
    // There is no shadow if no ResizeShadow object is associated with the
    // window or there is one but its hit test is set to HTNOWHERE. Since we
    // don't want to tie tests to that implementation detail, both cases are
    // considered here.
    return ResizeShadow() && ResizeShadowLastHitTest() != HTNOWHERE;
  }

  int ResizeShadowLastHitTest() const {
    return ResizeShadow()->GetLastHitTestForTest();
  }

 private:
  internal::ResizeShadow* ResizeShadow() const {
    return Shell::GetInstance()->resize_shadow_controller()->
        GetShadowForWindowForTest(target_.get());
  }

  TestWindowDelegate* delegate_;
  scoped_ptr<aura::Window> target_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventHandlerResizeTest);
};

// Tests resize shadows for touch resizing.
TEST_F(ToplevelWindowEventHandlerResizeTest, TouchResizeShadows) {
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), target());

  // Drag bottom right border of the window and check for the resize shadows.
  // Shadows are checked in the callback function.
  SetHittestCode(HTBOTTOMRIGHT);
  generator.GestureScrollSequenceWithCallback(
      gfx::Point(105, 105),
      gfx::Point(150, 150),
      base::TimeDelta::FromMilliseconds(100),
      3,
      base::Bind(&ToplevelWindowEventHandlerResizeTest::ProcessEvent,
                 base::Unretained(this)));
  RunAllPendingInMessageLoop();
}

// Tests resize shadows for mouse resizing.
TEST_F(ToplevelWindowEventHandlerResizeTest, MouseResizeShadows) {
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), target());

  // There should be no shadow at the beginning.
  EXPECT_FALSE(HasResizeShadow());

  // Move mouse over the right border. Shadows should appear.
  SetHittestCode(HTRIGHT);
  generator.MoveMouseTo(gfx::Point(100, 50));
  ASSERT_TRUE(HasResizeShadow());
  EXPECT_EQ(HTRIGHT, ResizeShadowLastHitTest());

  // Move mouse over the bottom right border. Shadows should stay.
  SetHittestCode(HTBOTTOMRIGHT);
  generator.MoveMouseTo(100, 100);
  ASSERT_TRUE(HasResizeShadow());
  EXPECT_EQ(HTBOTTOMRIGHT, ResizeShadowLastHitTest());

  // Move mouse into the window. Shadows should disappear.
  SetHittestCode(HTCLIENT);
  generator.MoveMouseTo(50, 50);
  EXPECT_FALSE(HasResizeShadow());

  // Move mouse over the bottom order. Shadows should reappear.
  SetHittestCode(HTBOTTOM);
  generator.MoveMouseTo(50, 100);
  ASSERT_TRUE(HasResizeShadow());
  EXPECT_EQ(HTBOTTOM, ResizeShadowLastHitTest());

  // Move mouse out of the window. Shadows should disappear.
  generator.MoveMouseTo(150, 150);
  EXPECT_FALSE(HasResizeShadow());

  RunAllPendingInMessageLoop();
}

}  // namespace test
}  // namespace ash
