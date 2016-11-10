// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/test_utils.h"

using AppListControllerDelegateAshTest = extensions::PlatformAppBrowserTest;

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
