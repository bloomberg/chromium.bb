// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller_test_helper.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

class WorkspaceEventHandlerTest : public test::AshTestBase {
 public:
  WorkspaceEventHandlerTest() {}
  virtual ~WorkspaceEventHandlerTest() {}

 protected:
  aura::Window* CreateTestWindow(aura::WindowDelegate* delegate,
                                 const gfx::Rect& bounds) {
    aura::Window* window = new aura::Window(delegate);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->SetParent(NULL);
    window->SetBounds(bounds);
    window->Show();
    return window;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventHandlerTest);
};

TEST_F(WorkspaceEventHandlerTest, DoubleClickSingleAxisResizeEdge) {
  // Double clicking the vertical resize edge of a window should maximize it
  // vertically.
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, restored_bounds));

  wm::ActivateWindow(window.get());

  gfx::Rect work_area =
      gfx::Screen::GetDisplayNearestWindow(window.get()).work_area();

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());

  // Double-click the top resize edge.
  wd.set_window_component(HTTOP);
  // On X a double click actually generates a drag between each press/release.
  // Explicitly trigger this path since we had bugs in dealing with it
  // correctly.
  generator.PressLeftButton();
  generator.ReleaseLeftButton();
  generator.set_flags(ui::EF_IS_DOUBLE_CLICK);
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_location(), 1);
  generator.ReleaseLeftButton();
  EXPECT_EQ(work_area.y(), window->bounds().y());
  EXPECT_EQ(work_area.height(), window->bounds().height());
  // Single-axis maximization is not considered real maximization.
  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));

  // Restore.
  generator.DoubleClickLeftButton();
  EXPECT_EQ(restored_bounds.y(), window->bounds().y());
  EXPECT_EQ(restored_bounds.height(), window->bounds().height());
  // Note that it should not even be restored at this point, it should have
  // also cleared the restore rectangle.
  EXPECT_EQ(NULL, GetRestoreBoundsInScreen(window.get()));

  // Double-click the top resize edge again to maximize vertically, then double
  // click again to restore.
  generator.DoubleClickLeftButton();
  wd.set_window_component(HTCAPTION);
  generator.DoubleClickLeftButton();
  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));

  EXPECT_EQ(restored_bounds.y(), window->bounds().y());
  EXPECT_EQ(restored_bounds.height(), window->bounds().height());

  // Double clicking the left resize edge should maximize horizontally.
  wd.set_window_component(HTLEFT);
  generator.DoubleClickLeftButton();
  EXPECT_EQ(work_area.x(), window->bounds().x());
  EXPECT_EQ(work_area.width(), window->bounds().width());
  // Single-axis maximization is not considered real maximization.
  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
}

TEST_F(WorkspaceEventHandlerTest, DoubleClickCaptionTogglesMaximize) {
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, gfx::Rect(1, 2, 3, 4)));
  wd.set_window_component(HTCAPTION);
  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());
  generator.DoubleClickLeftButton();
  EXPECT_NE("1,2 3x4", window->bounds().ToString());

  EXPECT_TRUE(wm::IsWindowMaximized(window.get()));
  generator.DoubleClickLeftButton();

  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
  EXPECT_EQ("1,2 3x4", window->bounds().ToString());
}

TEST_F(WorkspaceEventHandlerTest, DoubleTapCaptionTogglesMaximize) {
  aura::test::TestWindowDelegate wd;
  gfx::Rect bounds(10, 20, 30, 40);
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, bounds));
  wd.set_window_component(HTCAPTION);
  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       window.get());
  generator.GestureTapAt(gfx::Point(25, 25));
  generator.GestureTapAt(gfx::Point(25, 25));
  RunAllPendingInMessageLoop();
  EXPECT_NE(bounds.ToString(), window->bounds().ToString());
  EXPECT_TRUE(wm::IsWindowMaximized(window.get()));

  generator.GestureTapAt(gfx::Point(5, 5));
  generator.GestureTapAt(gfx::Point(10, 10));

  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Verifies deleting the window while dragging doesn't crash.
TEST_F(WorkspaceEventHandlerTest, DeleteWhenDragging) {
  // Create a large window in the background. This is necessary so that when we
  // delete |window| WorkspaceEventHandler is still the active event handler.
  aura::test::TestWindowDelegate wd2;
  scoped_ptr<aura::Window> window2(
      CreateTestWindow(&wd2, gfx::Rect(0, 0, 500, 500)));

  aura::test::TestWindowDelegate wd;
  const gfx::Rect bounds(10, 20, 30, 40);
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, bounds));
  wd.set_window_component(HTCAPTION);
  aura::test::EventGenerator generator(window->GetRootWindow());
  generator.MoveMouseToCenterOf(window.get());
  generator.PressLeftButton();
  generator.MoveMouseTo(generator.current_location().Add(gfx::Point(50, 50)));
  DCHECK_NE(bounds.origin().ToString(), window->bounds().origin().ToString());
  window.reset();
  generator.MoveMouseTo(generator.current_location().Add(gfx::Point(50, 50)));
}

// Verifies deleting the window while in a run loop doesn't crash.
TEST_F(WorkspaceEventHandlerTest, DeleteWhileInRunLoop) {
  aura::test::TestWindowDelegate wd;
  const gfx::Rect bounds(10, 20, 30, 40);
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, bounds));
  wd.set_window_component(HTCAPTION);

  ASSERT_TRUE(aura::client::GetWindowMoveClient(window->parent()));
  MessageLoop::current()->DeleteSoon(FROM_HERE, window.get());
  aura::client::GetWindowMoveClient(window->parent())->RunMoveLoop(
      window.release(), gfx::Point());
}

}  // namespace internal
}  // namespace ash
