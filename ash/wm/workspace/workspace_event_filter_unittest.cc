// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_filter.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace_controller.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

class WorkspaceEventFilterTest : public test::AshTestBase {
 public:
  WorkspaceEventFilterTest() {}
  virtual ~WorkspaceEventFilterTest() {}

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
  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventFilterTest);
};

TEST_F(WorkspaceEventFilterTest, DoubleClickSingleAxisResizeEdge) {
  Shell::TestApi shell_test(Shell::GetInstance());
  WorkspaceManager* manager =
      shell_test.workspace_controller()->workspace_manager();
  manager->set_grid_size(0);

  // Double clicking the vertical resize edge of a window should maximize it
  // vertically.
  gfx::Rect restored_bounds(10, 10, 50, 50);
  aura::test::TestWindowDelegate wd;
  scoped_ptr<aura::Window> window(CreateTestWindow(&wd, restored_bounds));

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

  // Double-click the top resize edge again to maximize vertically, then double
  // click the caption to maximize in all directions.
  generator.DoubleClickLeftButton();
  wd.set_window_component(HTCAPTION);
  generator.DoubleClickLeftButton();
  EXPECT_TRUE(wm::IsWindowMaximized(window.get()));

  // Double clicking the caption again should completely restore the window.
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

}  // namespace internal
}  // namespace ash
