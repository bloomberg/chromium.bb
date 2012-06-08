// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"

using extensions::Extension;

typedef PlatformAppBrowserTest LauncherPlatformAppBrowserTest;

// Test that we can launch a platform app and get a running item.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchUnpinned) {
  ash::Launcher* launcher = ash::Shell::GetInstance()->launcher();
  int item_count = launcher->model()->item_count();
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window = CreateShellWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, launcher->model()->item_count());
  ash::LauncherItem item =
      launcher->model()->items()[launcher->model()->item_count() - 2];
  ASSERT_EQ(ash::TYPE_PLATFORM_APP, item.type);
  ASSERT_EQ(ash::STATUS_RUNNING, item.status);
  CloseShellWindow(window);
  --item_count;
  ASSERT_EQ(item_count, launcher->model()->item_count());
}

// Test that we can launch a platform app that already has a shortcut.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, LaunchPinned) {
  ash::Launcher* launcher = ash::Shell::GetInstance()->launcher();
  int item_count = launcher->model()->item_count();

  // First get app_id.
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  const std::string app_id = extension->id();

  // Then create a shortcut.
  ChromeLauncherController* controller =
      static_cast<ChromeLauncherController*>(launcher->delegate());
  ash::LauncherID shortcut_id =
      controller->CreateAppLauncherItem(NULL, app_id, ash::STATUS_CLOSED);
  ++item_count;
  ASSERT_EQ(item_count, launcher->model()->item_count());
  ash::LauncherItem item = *launcher->model()->ItemByID(shortcut_id);
  ASSERT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  ASSERT_EQ(ash::STATUS_CLOSED, item.status);

  // Open a window. Confirm the item is now running.
  ShellWindow* window = CreateShellWindow(extension);
  ASSERT_EQ(item_count, launcher->model()->item_count());
  item = *launcher->model()->ItemByID(shortcut_id);
  ASSERT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  ASSERT_EQ(ash::STATUS_RUNNING, item.status);

  // Then close it, make sure there's still an item.
  CloseShellWindow(window);
  ASSERT_EQ(item_count, launcher->model()->item_count());
  item = *launcher->model()->ItemByID(shortcut_id);
  ASSERT_EQ(ash::TYPE_APP_SHORTCUT, item.type);
  ASSERT_EQ(ash::STATUS_CLOSED, item.status);
}

// Test that we can launch a platform app with more than one window.
IN_PROC_BROWSER_TEST_F(LauncherPlatformAppBrowserTest, MultipleWindows) {
  ash::Launcher* launcher = ash::Shell::GetInstance()->launcher();
  int item_count = launcher->model()->item_count();

  // First run app.
  const Extension* extension = LoadAndLaunchPlatformApp("launch");
  ShellWindow* window1 = CreateShellWindow(extension);
  ++item_count;
  ASSERT_EQ(item_count, launcher->model()->item_count());
  ash::LauncherItem item =
      launcher->model()->items()[launcher->model()->item_count() - 2];
  ash::LauncherID item_id = item.id;
  ASSERT_EQ(ash::TYPE_PLATFORM_APP, item.type);
  ASSERT_EQ(ash::STATUS_RUNNING, item.status);

  // Add second window.
  ShellWindow* window2 = CreateShellWindow(extension);
  // Confirm item stays.
  ASSERT_EQ(item_count, launcher->model()->item_count());
  item = *launcher->model()->ItemByID(item_id);
  ASSERT_EQ(ash::STATUS_RUNNING, item.status);

  // Close second window.
  CloseShellWindow(window2);
  // Confirm item stays.
  ASSERT_EQ(item_count, launcher->model()->item_count());
  item = *launcher->model()->ItemByID(item_id);
  ASSERT_EQ(ash::STATUS_RUNNING, item.status);

  // Close first window.
  CloseShellWindow(window1);
  // Confirm item is removed.
  --item_count;
  ASSERT_EQ(item_count, launcher->model()->item_count());
}
