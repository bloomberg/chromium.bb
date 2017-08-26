// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chromeos/ash_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"
#include "chrome/browser/ui/ash/app_list/app_list_service_ash.h"
#include "chrome/browser/ui/ash/app_list/test/app_list_service_ash_test_api.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/views/app_list_item_view.h"
#include "ui/app_list/views/apps_grid_view.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/window_util.h"

using AppListTest = InProcessBrowserTest;
using AppListControllerDelegateAshTest = extensions::PlatformAppBrowserTest;

// Test that clicking on app list context menus doesn't close the app list.
IN_PROC_BROWSER_TEST_F(AppListTest, ClickingContextMenuDoesNotDismiss) {
  // Show the app list on the primary display.
  AppListServiceAsh* service = AppListServiceAsh::GetInstance();
  app_list::AppListPresenterImpl* presenter = service->GetAppListPresenter();
  presenter->Show(display::Screen::GetScreen()->GetPrimaryDisplay().id());
  aura::Window* window = presenter->GetWindow();
  ASSERT_TRUE(window);

  // Show a context menu for the first app list item view.
  AppListServiceAshTestApi test_api;
  app_list::AppsGridView* grid_view = test_api.GetRootGridView();
  app_list::AppListItemView* item_view = grid_view->GetItemViewAt(0);
  item_view->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  // Find the context menu as a transient child of the app list.
  aura::Window* transient_parent =
      chromeos::GetAshConfig() == ash::Config::MASH ? window->parent() : window;
  const std::vector<aura::Window*>& transient_children =
      wm::GetTransientChildren(transient_parent);
  ASSERT_EQ(1u, transient_children.size());
  aura::Window* menu = transient_children[0];

  // Press the left mouse button on the menu window, AppListPresenterDelegateMus
  // should not close the app list nor the context menu on this pointer event.
  ui::test::EventGenerator menu_event_generator(menu);
  menu_event_generator.set_current_location(menu->GetBoundsInScreen().origin());
  menu_event_generator.PressLeftButton();

  // Check that the window and the app list are still open.
  ASSERT_EQ(window, presenter->GetWindow());
  EXPECT_EQ(1u, wm::GetTransientChildren(transient_parent).size());
}

// Test AppListControllerDelegateAsh::IsAppOpen for extension apps.
IN_PROC_BROWSER_TEST_F(AppListControllerDelegateAshTest, IsExtensionAppOpen) {
  AppListControllerDelegateAsh delegate(nullptr);
  EXPECT_FALSE(delegate.IsAppOpen("fake_extension_app_id"));

  base::FilePath extension_path = test_data_dir_.AppendASCII("app");
  const extensions::Extension* extension_app = LoadExtension(extension_path);
  ASSERT_NE(nullptr, extension_app);
  EXPECT_FALSE(delegate.IsAppOpen(extension_app->id()));
  {
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    OpenApplication(AppLaunchParams(
        profile(), extension_app, extensions::LAUNCH_CONTAINER_WINDOW,
        WindowOpenDisposition::NEW_WINDOW, extensions::SOURCE_TEST));
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate.IsAppOpen(extension_app->id()));
}

// Test AppListControllerDelegateAsh::IsAppOpen for platform apps.
IN_PROC_BROWSER_TEST_F(AppListControllerDelegateAshTest, IsPlatformAppOpen) {
  AppListControllerDelegateAsh delegate(nullptr);
  EXPECT_FALSE(delegate.IsAppOpen("fake_platform_app_id"));

  const extensions::Extension* app = InstallPlatformApp("minimal");
  EXPECT_FALSE(delegate.IsAppOpen(app->id()));
  {
    content::WindowedNotificationObserver app_loaded_observer(
        content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::NotificationService::AllSources());
    LaunchPlatformApp(app);
    app_loaded_observer.Wait();
  }
  EXPECT_TRUE(delegate.IsAppOpen(app->id()));
}
