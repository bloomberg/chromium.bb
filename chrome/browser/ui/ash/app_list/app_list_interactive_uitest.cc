// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/app_list/app_list_presenter_service.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

using AppListTest = InProcessBrowserTest;

// An integration test to toggle the app list by pressing the shelf button.
// TODO(jamescook|newcomer): Replace this with a unit test in //ash/app_list
// after app list ownership moves out of the browser process into ash.
// http://crbug.com/733662
IN_PROC_BROWSER_TEST_F(AppListTest, PressAppListButtonToShowAndDismiss) {
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  ash::Shelf* shelf = ash::Shelf::ForWindow(root_window);
  ash::ShelfWidget* shelf_widget = shelf->shelf_widget();
  ash::ShelfView* shelf_view = shelf->GetShelfViewForTesting();
  ash::ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();
  ash::AppListButton* app_list_button = shelf_widget->GetAppListButton();
  // Ensure animations progressed to give the app list button a non-empty size.
  ASSERT_GT(app_list_button->GetBoundsInScreen().height(), 0);

  aura::Window* app_list_container =
      root_window->GetChildById(ash::kShellWindowId_AppListContainer);
  ui::test::EventGenerator generator(shelf_widget->GetNativeWindow());

  // Click the app list button to show the app list.
  ash::Shell* shell = ash::Shell::Get();
  auto* service = AppListServiceAsh::GetInstance();
  auto* presenter = service->GetAppListPresenter();
  EXPECT_FALSE(shell->app_list()->GetTargetVisibility());
  EXPECT_FALSE(presenter->GetTargetVisibility());
  EXPECT_EQ(0u, app_list_container->children().size());
  EXPECT_FALSE(app_list_button->is_showing_app_list());
  generator.set_current_location(
      app_list_button->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  // Flush the mojo message from Ash to Chrome to show the app list.
  shell->app_list()->FlushForTesting();
  EXPECT_TRUE(presenter->GetTargetVisibility());
  // Flush the mojo message from Chrome to Ash reporting the visibility change.
  auto* presenter_service = service->app_list_presenter_service();
  presenter_service->FlushForTesting();
  EXPECT_TRUE(shell->app_list()->GetTargetVisibility());
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_TRUE(app_list_button->is_showing_app_list());

  // Click the button again to dismiss the app list; it will animate to close.
  generator.ClickLeftButton();
  // Flush the mojo message from Ash to Chrome to hide the app list.
  shell->app_list()->FlushForTesting();
  EXPECT_FALSE(presenter->GetTargetVisibility());
  // Flush the mojo message from Chrome to Ash reporting the visibility change.
  presenter_service->FlushForTesting();
  EXPECT_FALSE(shell->app_list()->GetTargetVisibility());
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_FALSE(app_list_button->is_showing_app_list());
}
