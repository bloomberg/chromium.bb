// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service.h"
#include "apps/app_restore_service_factory.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/test/test_utils.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionSystem;
// TODO(benwells): Move PlatformAppBrowserTest to apps namespace in apps
// component.
using extensions::PlatformAppBrowserTest;

namespace apps {

// Tests that a running app is recorded in the preferences as such.
IN_PROC_BROWSER_TEST_F(PlatformAppBrowserTest, RunningAppsAreRecorded) {
  content::WindowedNotificationObserver extension_suspended(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::NotificationService::AllSources());

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("platform_apps/restart_test"));
  ASSERT_TRUE(extension);
  ExtensionService* extension_service =
      ExtensionSystem::Get(browser()->profile())->extension_service();
  ExtensionPrefs* extension_prefs = extension_service->extension_prefs();

  // App is running.
  ASSERT_TRUE(extension_prefs->IsExtensionRunning(extension->id()));

  // Wait for the extension to get suspended.
  extension_suspended.Wait();

  // App isn't running because it got suspended.
  ASSERT_FALSE(extension_prefs->IsExtensionRunning(extension->id()));

  // Pretend that the app is supposed to be running.
  extension_prefs->SetExtensionRunning(extension->id(), true);

  ExtensionTestMessageListener restart_listener("onRestarted", false);
  apps::AppRestoreServiceFactory::GetForProfile(browser()->profile())->
      HandleStartup(true);
  restart_listener.WaitUntilSatisfied();
}

}  // namespace apps
