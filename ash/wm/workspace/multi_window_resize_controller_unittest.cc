// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/multi_window_resize_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace_controller_test_helper.h"
#include "ash/wm/workspace/workspace_event_handler_test_helper.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

class MultiWindowResizeControllerTest : public test::AshTestBase {
 public:
  MultiWindowResizeControllerTest() : resize_controller_(NULL) {}
  virtual ~MultiWindowResizeControllerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    WorkspaceController* wc =
        test::ShellTestApi(Shell::GetInstance()).workspace_controller();
    WorkspaceEventHandler* event_handler =
        WorkspaceControllerTestHelper(wc).GetEventHandler();
    resize_controller_ = WorkspaceEventHandlerTestHelper(event_handler).
        resize_controller();
  }

 protected:
  aura::Window* CreateTestWindow(aura::WindowDelegate* delegate,
                                 const gfx::Rect& bounds) {
    aura::Window* window = new aura::Window(delegate);
    window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
    window->Init(aura::WINDOW_LAYER_TEXTURED);
    ParentWindowInPrimaryRootWindow(window);
    window->SetBounds(bounds);
    window->Show();
    return window;
  }

  void ShowNow() {
    resize_controller_->ShowNow();
  }

  bool IsShowing() {
    return resize_controller_->IsShowing();
  }

  bool HasPendingShow() {
    return resize_controller_->show_timer_.IsRunning();
  }

  bool HasPendingHide() {
    return resize_controller_->hide_timer_.IsRunning();
  }

  void Hide() {
    resize_controller_->Hide();
  }

  bool HasTarget(aura::Window* window) {
    if (!resize_controller_->windows_.is_valid())
      return false;
    if ((resize_controller_->windows_.window1 == window ||
         resize_controller_->windows_.window2 == window))
      return true;
    for (size_t i = 0;
         i < resize_controller_->windows_.other_windows.size(); ++i) {
      if (resize_controller_->windows_.other_windows[i] == window)
        return true;
    }
    return false;
  }

  bool IsOverWindows(const gfx::Point& loc) {
    return resize_controller_->IsOverWindows(loc);
  }

  views::Widget* resize_widget() {
    return resize_controller_->resize_widget_.get();
  }

  MultiWindowResizeController* resize_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MultiWindowResizeControllerTest);
};

// Assertions around moving mouse over 2 windows.
TEST_F(MultiWindowResizeControllerTest, BasicTests) {
  aura::test::TestWindowDelegate delegate1;
  scoped_ptr<aura::Window> w1(
      CreateTestWindow(&delegate1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  scoped_ptr<aura::Window> w2(
      CreateTestWindow(&delegate2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  aura::test::EventGenerator generator(w1->GetRootWindow());
  generator.MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  EXPECT_FALSE(IsOverWindows(gfx::Point(200, 200)));

  // Have to explicitly invoke this as MouseWatcher listens for native events.
  resize_controller_->MouseMovedOutOfHost();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_FALSE(IsShowing());
  EXPECT_FALSE(HasPendingHide());
}

// Makes sure deleting a window hides.
TEST_F(MultiWindowResizeControllerTest, DeleteWindow) {
  aura::test::TestWindowDelegate delegate1;
  scoped_ptr<aura::Window> w1(
      CreateTestWindow(&delegate1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  scoped_ptr<aura::Window> w2(
      CreateTestWindow(&delegate2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  aura::test::EventGenerator generator(w1->GetRootWindow());
  generator.MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator.MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // Move the resize widget
  generator.PressLeftButton();
  generator.MoveMouseTo(bounds.x() + 10, bounds.y() + 10);

  // Delete w2.
  w2.reset();
  EXPECT_TRUE(resize_widget() == NULL);
  EXPECT_FALSE(HasPendingShow());
  EXPECT_FALSE(IsShowing());
  EXPECT_FALSE(HasPendingHide());
  EXPECT_FALSE(HasTarget(w1.get()));
}

// Tests resizing.
TEST_F(MultiWindowResizeControllerTest, Drag) {
  aura::test::TestWindowDelegate delegate1;
  scoped_ptr<aura::Window> w1(
      CreateTestWindow(&delegate1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  scoped_ptr<aura::Window> w2(
      CreateTestWindow(&delegate2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  aura::test::EventGenerator generator(w1->GetRootWindow());
  generator.MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // Force a show now.
  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // Move the mouse over the resize widget.
  ASSERT_TRUE(resize_widget());
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator.MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // Move the resize widget
  generator.PressLeftButton();
  generator.MoveMouseTo(bounds.x() + 11, bounds.y() + 10);
  generator.ReleaseLeftButton();

  EXPECT_TRUE(resize_widget());
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());
  EXPECT_EQ("0,0 110x100", w1->bounds().ToString());
  EXPECT_EQ("110,0 100x100", w2->bounds().ToString());
}

// Makes sure three windows are picked up.
TEST_F(MultiWindowResizeControllerTest, Three) {
  aura::test::TestWindowDelegate delegate1;
  scoped_ptr<aura::Window> w1(
      CreateTestWindow(&delegate1, gfx::Rect(0, 0, 100, 100)));
  delegate1.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate2;
  scoped_ptr<aura::Window> w2(
      CreateTestWindow(&delegate2, gfx::Rect(100, 0, 100, 100)));
  delegate2.set_window_component(HTRIGHT);
  aura::test::TestWindowDelegate delegate3;
  scoped_ptr<aura::Window> w3(
      CreateTestWindow(&delegate3, gfx::Rect(200, 0, 100, 100)));
  delegate3.set_window_component(HTRIGHT);

  aura::test::EventGenerator generator(w1->GetRootWindow());
  generator.MoveMouseTo(w1->bounds().CenterPoint());
  EXPECT_TRUE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());
  EXPECT_FALSE(HasTarget(w3.get()));

  ShowNow();
  EXPECT_FALSE(HasPendingShow());
  EXPECT_TRUE(IsShowing());
  EXPECT_FALSE(HasPendingHide());

  // w3 should be picked up when resize is started.
  gfx::Rect bounds(resize_widget()->GetWindowBoundsInScreen());
  generator.MoveMouseTo(bounds.x() + 1, bounds.y() + 1);
  generator.PressLeftButton();
  generator.MoveMouseTo(bounds.x() + 11, bounds.y() + 10);

  EXPECT_TRUE(HasTarget(w3.get()));

  // Release the mouse. The resizer should still be visible and a subsequent
  // press should not trigger a DCHECK.
  generator.ReleaseLeftButton();
  EXPECT_TRUE(IsShowing());
  generator.PressLeftButton();
}

}  // namespace ash
