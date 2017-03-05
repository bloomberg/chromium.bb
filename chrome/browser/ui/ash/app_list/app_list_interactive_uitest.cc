// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/common/shelf/app_list_button.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/run_loop.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/events/test/event_generator.h"

using AppListTest = InProcessBrowserTest;

// An integration test to toggle the app list by pressing the shelf button.
IN_PROC_BROWSER_TEST_F(AppListTest, PressAppListButtonToShowAndDismiss) {
  ash::WmShell* shell = ash::WmShell::Get();
  ash::WmShelf* shelf = ash::WmShelf::ForWindow(shell->GetActiveWindow());
  ash::ShelfWidget* shelf_widget = shelf->shelf_widget();
  ash::AppListButton* app_list_button = shelf_widget->GetAppListButton();

  ash::WmWindow* root_window = shell->GetActiveWindow()->GetRootWindow();
  ash::WmWindow* app_list_container = root_window->GetChildByShellWindowId(
      ash::kShellWindowId_AppListContainer);
  ui::test::EventGenerator generator(shelf_widget->GetNativeWindow());

  // Click the app list button to show the app list.
  EXPECT_FALSE(shell->app_list()->GetTargetVisibility());
  EXPECT_EQ(0u, app_list_container->GetChildren().size());
  EXPECT_FALSE(app_list_button->is_showing_app_list());
  generator.set_current_location(
      app_list_button->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(shell->app_list()->GetTargetVisibility());
  EXPECT_EQ(1u, app_list_container->GetChildren().size());
  EXPECT_TRUE(app_list_button->is_showing_app_list());

  // Click the button again to dismiss the app list; the window animates closed.
  generator.ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(shell->app_list()->GetTargetVisibility());
  EXPECT_EQ(1u, app_list_container->GetChildren().size());
  EXPECT_FALSE(app_list_button->is_showing_app_list());
}
