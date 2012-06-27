// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/toplevel_window_event_filter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ash/wm/workspace_controller.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_window_delegate.h"
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
  explicit TestWindowDelegate(int hittest_code)
      : hittest_code_(hittest_code) {
  }
  virtual ~TestWindowDelegate() {}

  void set_min_size(const gfx::Size& size) {
    min_size_ = size;
  }

 private:
  // Overridden from aura::Test::TestWindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return min_size_;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return hittest_code_;
  }
  virtual void OnWindowDestroyed() OVERRIDE {
    delete this;
  }

  int hittest_code_;
  gfx::Size min_size_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

class ToplevelWindowEventFilterTest : public AshTestBase {
 public:
  ToplevelWindowEventFilterTest() : filter_(NULL), parent_(NULL) {}
  virtual ~ToplevelWindowEventFilterTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    parent_ = new aura::Window(NULL);
    parent_->Init(ui::LAYER_NOT_DRAWN);
    parent_->Show();
    Shell::GetPrimaryRootWindow()->AddChild(parent_);
    parent_->SetBounds(Shell::GetPrimaryRootWindow()->bounds());
    filter_ = new ToplevelWindowEventFilter(parent_);
    parent_->SetEventFilter(filter_);
    SetGridSize(0);
  }

  virtual void TearDown() OVERRIDE {
    filter_ = NULL;
    parent_ = NULL;
    AshTestBase::TearDown();
  }

 protected:
  aura::Window* CreateWindow(int hittest_code) {
    TestWindowDelegate* d1 = new TestWindowDelegate(hittest_code);
    aura::Window* w1 = new aura::Window(d1);
    w1->set_id(1);
    w1->Init(ui::LAYER_TEXTURED);
    w1->SetParent(parent_);
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

  void SetGridSize(int grid_size) {
    Shell::TestApi shell_test(Shell::GetInstance());
    shell_test.workspace_controller()->SetGridSize(grid_size);
  }

  ToplevelWindowEventFilter* filter_;

 private:
  // Window |filter_| is installed on. Owned by RootWindow.
  aura::Window* parent_;

  DISALLOW_COPY_AND_ASSIGN(ToplevelWindowEventFilterTest);
};

}

TEST_F(ToplevelWindowEventFilterTest, Caption) {
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

TEST_F(ToplevelWindowEventFilterTest, BottomRight) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,100.
  EXPECT_EQ(gfx::Size(200, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, GrowBox) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTGROWBOX));
  TestWindowDelegate* window_delegate =
    static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(40, 40));

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

TEST_F(ToplevelWindowEventFilterTest, Right) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTRIGHT));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 100,0.
  EXPECT_EQ(gfx::Size(200, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Bottom) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOM));
  gfx::Point position = w1->bounds().origin();
  DragFromCenterBy(w1.get(), 100, 100);
  // Position should not have changed.
  EXPECT_EQ(position, w1->bounds().origin());
  // Size should have increased by 0,100.
  EXPECT_EQ(gfx::Size(100, 200), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopRight) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPRIGHT));
  DragFromCenterBy(w1.get(), -50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Top) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOP));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 0,50.
  EXPECT_EQ(gfx::Point(0, 50), w1->bounds().origin());
  // Size should have decreased by 0,50.
  EXPECT_EQ(gfx::Size(100, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Left) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,0.
  EXPECT_EQ(gfx::Size(50, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomLeft) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMLEFT));
  DragFromCenterBy(w1.get(), 50, -50);
  // Position should have been offset by 50,0.
  EXPECT_EQ(gfx::Point(50, 0), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopLeft) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPLEFT));
  DragFromCenterBy(w1.get(), 50, 50);
  // Position should have been offset by 50,50.
  EXPECT_EQ(gfx::Point(50, 50), w1->bounds().origin());
  // Size should have decreased by 50,50.
  EXPECT_EQ(gfx::Size(50, 50), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, Client) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTCLIENT));
  gfx::Rect bounds = w1->bounds();
  DragFromCenterBy(w1.get(), 100, 100);
  // Neither position nor size should have changed.
  EXPECT_EQ(bounds, w1->bounds());
}

TEST_F(ToplevelWindowEventFilterTest, LeftPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(40, 40));

  // Simulate a large left-to-right drag.  Window width should be clamped to
  // minimum and position change should be limited as well.
  DragFromCenterBy(w1.get(), 333, 0);
  EXPECT_EQ(gfx::Point(60, 0), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, RightPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(40, 40));
  gfx::Point position = w1->bounds().origin();

  // Simulate a large right-to-left drag.  Window width should be clamped to
  // minimum and position should not change.
  DragFromCenterBy(w1.get(), -333, 0);
  EXPECT_EQ(position, w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 100), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopLeftPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(40, 40));

  // Simulate a large top-left to bottom-right drag.  Window width should be
  // clamped to minimum and position should be limited.
  DragFromCenterBy(w1.get(), 333, 444);
  EXPECT_EQ(gfx::Point(60, 60), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, TopRightPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTTOPRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(40, 40));

  // Simulate a large top-right to bottom-left drag.  Window size should be
  // clamped to minimum, x position should not change, and y position should
  // be clamped.
  DragFromCenterBy(w1.get(), -333, 444);
  EXPECT_EQ(gfx::Point(0, 60), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomLeftPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMLEFT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(40, 40));

  // Simulate a large bottom-left to top-right drag.  Window size should be
  // clamped to minimum, x position should be clamped, and y position should
  // not change.
  DragFromCenterBy(w1.get(), 333, -444);
  EXPECT_EQ(gfx::Point(60, 0), w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomRightPastMinimum) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTBOTTOMRIGHT));
  TestWindowDelegate* window_delegate =
      static_cast<TestWindowDelegate*>(w1->delegate());
  window_delegate->set_min_size(gfx::Size(40, 40));
  gfx::Point position = w1->bounds().origin();

  // Simulate a large bottom-right to top-left drag.  Window size should be
  // clamped to minimum and position should not change.
  DragFromCenterBy(w1.get(), -333, -444);
  EXPECT_EQ(position, w1->bounds().origin());
  EXPECT_EQ(gfx::Size(40, 40), w1->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, DoubleClickCaptionTogglesMaximize) {
  scoped_ptr<aura::Window> w1(CreateWindow(HTCAPTION));
  EXPECT_FALSE(wm::IsWindowMaximized(w1.get()));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(), w1.get());
  generator.DoubleClickLeftButton();

  EXPECT_TRUE(wm::IsWindowMaximized(w1.get()));
  generator.DoubleClickLeftButton();

  EXPECT_FALSE(wm::IsWindowMaximized(w1.get()));
}

TEST_F(ToplevelWindowEventFilterTest, BottomRightWorkArea) {
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOMRIGHT));
  gfx::Rect work_area =
      gfx::Screen::GetDisplayNearestWindow(target.get()).work_area();
  gfx::Point position = target->bounds().origin();
  // Drag further than work_area bottom.
  DragFromCenterBy(target.get(), 100, work_area.height());
  // Position should not have changed.
  EXPECT_EQ(position, target->bounds().origin());
  // Size should have increased by 100, work_area.height() - target->bounds.y()
  EXPECT_EQ(gfx::Size(200, work_area.height() - target->bounds().y()),
            target->bounds().size());
}

TEST_F(ToplevelWindowEventFilterTest, BottomLeftWorkArea) {
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOMLEFT));
  gfx::Rect work_area =
      gfx::Screen::GetDisplayNearestWindow(target.get()).work_area();
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

TEST_F(ToplevelWindowEventFilterTest, BottomWorkArea) {
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOM));
  gfx::Rect work_area =
      gfx::Screen::GetDisplayNearestWindow(target.get()).work_area();
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
TEST_F(ToplevelWindowEventFilterTest, DontDragToNegativeY) {
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
TEST_F(ToplevelWindowEventFilterTest, DontGotWiderThanScreen) {
  scoped_ptr<aura::Window> target(CreateWindow(HTRIGHT));
  gfx::Rect work_area =
      gfx::Screen::GetDisplayNearestWindow(target.get()).bounds();
  DragFromCenterBy(target.get(), work_area.width() * 2, 0);
  // The y location and height should not have changed.
  EXPECT_EQ(work_area.width(), target->bounds().width());
}

// Verifies that when a grid size is set resizes snap to the grid.
TEST_F(ToplevelWindowEventFilterTest, ResizeSnaps) {
  SetGridSize(8);
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOMRIGHT));
  DragFromCenterBy(target.get(), 11, 21);
  EXPECT_EQ(112, target->bounds().width());
  EXPECT_EQ(120, target->bounds().height());
  target.reset(CreateWindow(HTTOPLEFT));
  target->SetBounds(gfx::Rect(48, 96, 100, 100));
  DragFromCenterBy(target.get(), -11, -21);
  EXPECT_EQ(40, target->bounds().x());
  EXPECT_EQ(80, target->bounds().y());
  EXPECT_EQ(112, target->bounds().width());
  EXPECT_EQ(120, target->bounds().height());
}

// Verifies that when a grid size is set dragging snaps to the grid.
TEST_F(ToplevelWindowEventFilterTest, DragSnaps) {
  SetGridSize(8);
  scoped_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_location().Add(gfx::Point(11, 21)));

  // Execute any scheduled draws so that pending mouse events are processed.
  RunAllPendingInMessageLoop();

  EXPECT_EQ(11, target->bounds().x());
  EXPECT_EQ(21, target->bounds().y());
  // We only snap moves to the grid on release.
  generator.ReleaseLeftButton();
  EXPECT_EQ(8, target->bounds().x());
  EXPECT_EQ(24, target->bounds().y());
}

// Verifies that touch-gestures drag the window correctly.
TEST_F(ToplevelWindowEventFilterTest, GestureDrag) {
  const int kGridSize = 8;
  SetGridSize(kGridSize);
  scoped_ptr<aura::Window> target(CreateWindow(HTCAPTION));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  gfx::Rect old_bounds = target->bounds();
  gfx::Point location(5, 5);

  // Snap right;
  gfx::Point end = location;
  end.Offset(100, 0);
  generator.GestureScrollSequence(location, end,
      base::TimeDelta::FromMilliseconds(5),
      10);
  RunAllPendingInMessageLoop();

  // Verify that the window has moved after the gesture.
  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  {
    internal::SnapSizer sizer(target.get(), location,
        internal::SnapSizer::RIGHT_EDGE, kGridSize);
    EXPECT_EQ(sizer.target_bounds().ToString(), target->bounds().ToString());
  }

  old_bounds = target->bounds();

  // Snap left.
  end = location = target->GetBoundsInRootWindow().CenterPoint();
  end.Offset(-100, 0);
  generator.GestureScrollSequence(location, end,
      base::TimeDelta::FromMilliseconds(5),
      10);
  RunAllPendingInMessageLoop();

  EXPECT_NE(old_bounds.ToString(), target->bounds().ToString());
  {
    internal::SnapSizer sizer(target.get(), location,
        internal::SnapSizer::LEFT_EDGE, kGridSize);
    EXPECT_EQ(sizer.target_bounds().ToString(), target->bounds().ToString());
  }

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
}

// Verifies pressing escape resets the bounds to the original bounds.
#if defined(OS_MACOSX)
#define MAYBE_EscapeReverts FAILS_EscapeReverts
#else
#define MAYBE_EscapeReverts EscapeReverts
#endif
TEST_F(ToplevelWindowEventFilterTest, MAYBE_EscapeReverts) {
  aura::RootWindow* root = Shell::GetPrimaryRootWindow();
  aura::client::ActivationClient* original_client =
      aura::client::GetActivationClient(root);
  aura::test::TestActivationClient activation_client(root);
  scoped_ptr<aura::Window> target(CreateWindow(HTBOTTOMRIGHT));
  target->Focus();
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       target.get());
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_location().Add(gfx::Point(10, 11)));

  // Execute any scheduled draws so that pending mouse events are processed.
  RunAllPendingInMessageLoop();

  EXPECT_EQ("0,0 110x111", target->bounds().ToString());
  generator.PressKey(ui::VKEY_ESCAPE, 0);
  generator.ReleaseKey(ui::VKEY_ESCAPE, 0);
  EXPECT_EQ("0,0 100x100", target->bounds().ToString());
  aura::client::SetActivationClient(root, original_client);
}

}  // namespace test
}  // namespace ash
