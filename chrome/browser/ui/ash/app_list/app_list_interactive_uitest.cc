// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/events/test/event_generator.h"

using AppListTest = InProcessBrowserTest;

// A helper class that waits for the app list window to show or hide.
class AppListWaiter : public aura::WindowObserver {
 public:
  AppListWaiter() {
    app_list_container_ =
        ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                 ash::kShellWindowId_AppListContainer);
    app_list_container_->AddObserver(this);
  }
  ~AppListWaiter() override { app_list_container_->RemoveObserver(this); }

  void WaitForOpen() {
    if (app_list_container_->children().size() == 0)
      run_loop_.Run();
  }

  void WaitForClose() {
    if (app_list_container_->children().size() > 0)
      run_loop_.Run();
  }

 private:
  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override { run_loop_.Quit(); }
  void OnWillRemoveWindow(aura::Window* window) override { run_loop_.Quit(); }

  aura::Window* app_list_container_ = nullptr;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(AppListWaiter);
};

// An integration test to toggle the app list by pressing the shelf button.
// TODO(jamescook|newcomer): Replace this with a unit test in //ash/app_list
// after app list ownership moves out of the browser process into ash.
// http://crbug.com/733662
//
// Disabled due to timeouts on ChromeOS: https://crbug.com/770138
IN_PROC_BROWSER_TEST_F(AppListTest,
                       DISABLED_PressAppListButtonToShowAndDismiss) {
  aura::Window* root_window = ash::Shell::GetPrimaryRootWindow();
  ash::Shelf* shelf = ash::Shelf::ForWindow(root_window);
  ash::ShelfWidget* shelf_widget = shelf->shelf_widget();
  ash::AppListButton* app_list_button = shelf_widget->GetAppListButton();

  aura::Window* app_list_container =
      root_window->GetChildById(ash::kShellWindowId_AppListContainer);
  ui::test::EventGenerator generator(shelf_widget->GetNativeWindow());

  // Click the app list button to show the app list.
  ash::Shell* shell = ash::Shell::Get();
  auto* presenter = AppListServiceAsh::GetInstance()->GetAppListPresenter();
  EXPECT_FALSE(shell->app_list()->GetTargetVisibility());
  EXPECT_FALSE(presenter->GetTargetVisibility());
  EXPECT_EQ(0u, app_list_container->children().size());
  EXPECT_FALSE(app_list_button->is_showing_app_list());
  generator.set_current_location(
      app_list_button->GetBoundsInScreen().CenterPoint());
  generator.ClickLeftButton();
  AppListWaiter().WaitForOpen();
  EXPECT_TRUE(presenter->GetTargetVisibility());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(shell->app_list()->GetTargetVisibility());
  EXPECT_EQ(1u, app_list_container->children().size());
  EXPECT_TRUE(app_list_button->is_showing_app_list());

  // Click the button again to dismiss the app list.
  generator.ClickLeftButton();
  AppListWaiter().WaitForClose();
  EXPECT_FALSE(presenter->GetTargetVisibility());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(shell->app_list()->GetTargetVisibility());
  EXPECT_EQ(0u, app_list_container->children().size());
  EXPECT_FALSE(app_list_button->is_showing_app_list());
}
