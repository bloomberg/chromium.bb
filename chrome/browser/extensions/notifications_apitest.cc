// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

#if defined(ENABLE_MESSAGE_CENTER)

#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_center_util.h"

class RichWebkitNotificationTest : public ExtensionApiTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        message_center::switches::kEnableRichNotifications);
  }
};

class DisabledRichWebkitNotificationTest : public ExtensionApiTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        message_center::switches::kDisableRichNotifications);
  }
};

#endif

// TODO(kbr): remove: http://crbug.com/222296
#if defined(OS_MACOSX)
#import "base/mac/mac_util.h"
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NotificationsNoPermission) {
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
  // Notifications not supported on linux/views yet.
#else
  ASSERT_TRUE(RunExtensionTest("notifications/has_not_permission")) << message_;
#endif
}

#if defined(ENABLE_MESSAGE_CENTER)
IN_PROC_BROWSER_TEST_F(RichWebkitNotificationTest, NoHTMLNotifications) {
  ASSERT_TRUE(RunExtensionTest("notifications/no_html")) << message_;
}

#if !defined(OS_CHROMEOS)
// HTML notifications fail on ChromeOS whether or not rich notifications
// are enabled.
IN_PROC_BROWSER_TEST_F(DisabledRichWebkitNotificationTest,
                       HasHTMLNotifications) {
  ASSERT_FALSE(message_center::IsRichNotificationEnabled());
  ASSERT_TRUE(RunExtensionTest("notifications/has_permission_manifest"))
      << message_;
}
#endif

#elif !defined(OS_LINUX) || !defined(TOOLKIT_VIEWS)
// Notifications not supported on linux/views yet.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       NotificationsHasPermissionManifest) {
  ASSERT_TRUE(RunExtensionTest("notifications/has_permission_manifest"))
      << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NotificationsHasPermission) {
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
  // Notifications not supported on linux/views yet.
#else

#if defined(OS_MACOSX)
  // TODO(kbr): re-enable: http://crbug.com/222296
  if (base::mac::IsOSMountainLionOrLater())
    return;
#endif

  DesktopNotificationServiceFactory::GetForProfile(browser()->profile())
      ->GrantPermission(GURL(
          "chrome-extension://peoadpeiejnhkmpaakpnompolbglelel"));
  ASSERT_TRUE(RunExtensionTest("notifications/has_permission_prefs"))
      << message_;
#endif
}
