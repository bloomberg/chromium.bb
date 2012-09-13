// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/insets.h"

namespace ash {

namespace {

class WorkspaceLayoutManager2Test : public test::AshTestBase {
 public:
  WorkspaceLayoutManager2Test() {}
  virtual ~WorkspaceLayoutManager2Test() {}

  aura::Window* CreateTestWindow(const gfx::Rect& bounds) {
    return aura::test::CreateTestWindowWithBounds(bounds, NULL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManager2Test);
};

// Verifies that a window containing a restore coordinate will be restored to
// to the size prior to minimize, keeping the restore rectangle in tact (if
// there is one).
TEST_F(WorkspaceLayoutManager2Test, RestoreFromMinimizeKeepsRestore) {
  scoped_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);
  SetRestoreBoundsInScreen(window.get(), gfx::Rect(0, 0, 100, 100));
  wm::MinimizeWindow(window.get());
  wm::RestoreWindow(window.get());
  EXPECT_EQ("0,0 100x100", GetRestoreBoundsInScreen(window.get())->ToString());
  EXPECT_EQ("10,15 25x35", window.get()->bounds().ToString());
}

// Verifies when a window is maximized all descendant windows have a size.
TEST_F(WorkspaceLayoutManager2Test, ChildBoundsResetOnMaximize) {
  scoped_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 30, 40)));
  window->Show();
  ash::wm::ActivateWindow(window.get());
  scoped_ptr<aura::Window> child_window(
      aura::test::CreateTestWindowWithBounds(gfx::Rect(5, 6, 7, 8),
                                             window.get()));
  child_window->Show();
  ash::wm::MaximizeWindow(window.get());
  EXPECT_EQ("5,6 7x8", child_window->bounds().ToString());
}

}  // namespace

}  // namespace ash
