// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/enterprise/tray_enterprise.h"
#include "ash/system/tray/label_tray_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/system/update/tray_update.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

namespace {

// TODO(jamescook): Add a test-only mojo API to get system tray details.
ash::SystemTray* GetSystemTray() {
  return ash::Shell::GetPrimaryRootWindowController()->GetSystemTray();
}

}  // namespace

using SystemTrayClientTest = InProcessBrowserTest;

// Test that a chrome update shows the update icon in the system menu.
IN_PROC_BROWSER_TEST_F(SystemTrayClientTest, UpdateTrayIcon) {
  ash::TrayUpdate* tray_update = GetSystemTray()->tray_update();

  // When no update is pending, the icon isn't visible.
  EXPECT_FALSE(tray_update->tray_view()->visible());

  // Simulate an upgrade. This sends a mojo message to ash.
  UpgradeDetector::GetInstance()->NotifyUpgrade();
  content::RunAllPendingInMessageLoop();

  // Tray icon is now visible.
  EXPECT_TRUE(tray_update->tray_view()->visible());
}

// Test that a flash update causes the update UI to show in the system menu.
IN_PROC_BROWSER_TEST_F(SystemTrayClientTest, FlashUpdateTrayIcon) {
  ash::TrayUpdate* tray_update = GetSystemTray()->tray_update();

  // When no update is pending, the icon isn't visible.
  EXPECT_FALSE(tray_update->tray_view()->visible());

  // Simulate a Flash update. This sends a mojo message to ash.
  SystemTrayClient::Get()->SetFlashUpdateAvailable();
  content::RunAllPendingInMessageLoop();

  // Tray icon is now visible.
  EXPECT_TRUE(tray_update->tray_view()->visible());
}

using SystemTrayClientEnterpriseTest = policy::DevicePolicyCrosBrowserTest;

IN_PROC_BROWSER_TEST_F(SystemTrayClientEnterpriseTest, TrayEnterprise) {
  // Mark the device as enterprise managed.
  policy::DevicePolicyCrosTestHelper::MarkAsEnterpriseOwnedBy("example.com");
  content::RunAllPendingInMessageLoop();

  // Open the system tray menu.
  ash::SystemTray* system_tray = GetSystemTray();
  system_tray->ShowDefaultView(ash::BUBBLE_CREATE_NEW);

  // Managed devices show an item in the menu.
  ash::TrayEnterprise* tray_enterprise =
      ash::SystemTrayTestApi(system_tray).tray_enterprise();
  ASSERT_TRUE(tray_enterprise->tray_view());
  EXPECT_TRUE(tray_enterprise->tray_view()->visible());

  system_tray->CloseBubble();
}
