// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/system_tray_client.h"

#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/update/tray_update.h"
#include "ash/common/wm_shell.h"
#include "ash/root_window_controller.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

ash::TrayUpdate* GetTrayUpdate() {
  return ash::WmShell::Get()
      ->GetPrimaryRootWindowController()
      ->GetSystemTray()
      ->tray_update();
}

using SystemTrayClientTest = InProcessBrowserTest;

// Test that a chrome update shows the update icon in the system menu.
IN_PROC_BROWSER_TEST_F(SystemTrayClientTest, UpdateTrayIcon) {
  ash::TrayUpdate* tray_update = GetTrayUpdate();

  // When no update is pending, the icon isn't visible.
  EXPECT_FALSE(tray_update->tray_view()->visible());

  // Simulate an upgrade. This sends a mojo message to ash.
  UpgradeDetector::GetInstance()->NotifyUpgradeRecommended();
  content::RunAllPendingInMessageLoop();

  // Tray icon is now visible.
  EXPECT_TRUE(tray_update->tray_view()->visible());
}

// Test that a flash update causes the update UI to show in the system menu.
IN_PROC_BROWSER_TEST_F(SystemTrayClientTest, FlashUpdateTrayIcon) {
  ash::TrayUpdate* tray_update = GetTrayUpdate();

  // When no update is pending, the icon isn't visible.
  EXPECT_FALSE(tray_update->tray_view()->visible());

  // Simulate a Flash update. This sends a mojo message to ash.
  SystemTrayClient::Get()->SetFlashUpdateAvailable();
  content::RunAllPendingInMessageLoop();

  // Tray icon is now visible.
  EXPECT_TRUE(tray_update->tray_view()->visible());
}
