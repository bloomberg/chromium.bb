// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_NotificationsNoPermission) {
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
  // Notifications not supported on linux/views yet.
#else
  ASSERT_TRUE(RunExtensionTest("notifications/has_not_permission")) << message_;
#endif
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       DISABLED_NotificationsHasPermissionManifest) {
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
  // Notifications not supported on linux/views yet.
#else
  ASSERT_TRUE(RunExtensionTest("notifications/has_permission_manifest"))
      << message_;
#endif
}

// http://crbug.com/98061
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_NotificationsHasPermission) {
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
  // Notifications not supported on linux/views yet.
#else
  DesktopNotificationServiceFactory::GetForProfile(browser()->profile())
      ->GrantPermission(GURL(
          "chrome-extension://peoadpeiejnhkmpaakpnompolbglelel"));
  ASSERT_TRUE(RunExtensionTest("notifications/has_permission_prefs"))
      << message_;
#endif
}
