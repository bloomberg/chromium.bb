// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

namespace ash {

typedef test::AshTestBase AppListControllerTest;

// Tests that app launcher hides when focus moves to a normal window.
TEST_F(AppListControllerTest, HideOnFocusOut) {
  Shell::GetInstance()->ToggleAppList(NULL);
  EXPECT_TRUE(Shell::GetInstance()->GetAppListTargetVisibility());

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window.get());

  EXPECT_FALSE(Shell::GetInstance()->GetAppListTargetVisibility());
}

// Tests that app launcher remains visible when focus is moved to a different
// window in kShellWindowId_AppListContainer.
TEST_F(AppListControllerTest, RemainVisibleWhenFocusingToApplistContainer) {
  Shell::GetInstance()->ToggleAppList(NULL);
  EXPECT_TRUE(Shell::GetInstance()->GetAppListTargetVisibility());

  aura::Window* applist_container = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(), kShellWindowId_AppListContainer);
  scoped_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(0, applist_container));
  wm::ActivateWindow(window.get());

  EXPECT_TRUE(Shell::GetInstance()->GetAppListTargetVisibility());
}

// Tests that clicking outside the app-list bubble closes it.
TEST_F(AppListControllerTest, ClickOutsideBubbleClosesBubble) {
  Shell* shell = Shell::GetInstance();
  shell->ToggleAppList(NULL);

  aura::Window* app_window = shell->GetAppListWindow();
  ASSERT_TRUE(app_window);
  aura::test::EventGenerator generator(shell->GetPrimaryRootWindow(),
                                       app_window);
  // Click on the bubble itself. The bubble should remain visible.
  generator.ClickLeftButton();
  EXPECT_TRUE(shell->GetAppListTargetVisibility());

  // Click outside the bubble. This should close it.
  gfx::Rect app_window_bounds = app_window->GetBoundsInRootWindow();
  gfx::Point point_outside =
      gfx::Point(app_window_bounds.right(), app_window_bounds.y()) +
      gfx::Vector2d(10, 0);
  EXPECT_TRUE(shell->GetPrimaryRootWindow()->bounds().Contains(point_outside));
  generator.MoveMouseToInHost(point_outside);
  generator.ClickLeftButton();
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
}

// Tests that clicking outside the app-list bubble closes it.
TEST_F(AppListControllerTest, TapOutsideBubbleClosesBubble) {
  Shell* shell = Shell::GetInstance();
  shell->ToggleAppList(NULL);

  aura::Window* app_window = shell->GetAppListWindow();
  ASSERT_TRUE(app_window);
  gfx::Rect app_window_bounds = app_window->GetBoundsInRootWindow();

  aura::test::EventGenerator generator(shell->GetPrimaryRootWindow());
  // Click on the bubble itself. The bubble should remain visible.
  generator.GestureTapAt(app_window_bounds.CenterPoint());
  EXPECT_TRUE(shell->GetAppListTargetVisibility());

  // Click outside the bubble. This should close it.
  gfx::Point point_outside =
      gfx::Point(app_window_bounds.right(), app_window_bounds.y()) +
      gfx::Vector2d(10, 0);
  EXPECT_TRUE(shell->GetPrimaryRootWindow()->bounds().Contains(point_outside));
  generator.GestureTapAt(point_outside);
  EXPECT_FALSE(shell->GetAppListTargetVisibility());
}

}  // namespace ash
