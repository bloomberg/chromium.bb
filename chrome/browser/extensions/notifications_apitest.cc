// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/extensions/lazy_background_page_test_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_center_util.h"

class NotificationIdleTest : public ExtensionApiTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        extensions::switches::kEventPageIdleTime, "1");
    command_line->AppendSwitchASCII(
        extensions::switches::kEventPageSuspendingTime, "1");
  }

  const extensions::Extension* LoadExtensionAndWait(
      const std::string& test_name) {
    LazyBackgroundObserver page_complete;
    base::FilePath extdir = test_data_dir_.AppendASCII(test_name);
    const extensions::Extension* extension = LoadExtension(extdir);
    if (extension)
      page_complete.Wait();
    return extension;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NotificationsNoPermission) {
  ASSERT_TRUE(RunExtensionTest("notifications/has_not_permission")) << message_;
}

// This test verifies that on RichNotification-enabled platforms HTML
// notificaitons are disabled.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_NoHTMLNotifications DISABLED_NoHTMLNotifications
#elif defined(RUN_MESSAGE_CENTER_TESTS)
#define MAYBE_NoHTMLNotifications NoHTMLNotifications
#else
#define MAYBE_NoHTMLNotifications DISABLED_NoHTMLNotifications
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_NoHTMLNotifications) {
  ASSERT_TRUE(message_center::IsRichNotificationEnabled());
  ASSERT_TRUE(RunExtensionTest("notifications/no_html")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, NotificationsHasPermission) {
  DesktopNotificationServiceFactory::GetForProfile(browser()->profile())
      ->GrantPermission(GURL(
          "chrome-extension://peoadpeiejnhkmpaakpnompolbglelel"));
  ASSERT_TRUE(RunExtensionTest("notifications/has_permission_prefs"))
      << message_;
}

  // MessaceCenter-specific test.
#if defined(RUN_MESSAGE_CENTER_TESTS)
#define MAYBE_NotificationsAllowUnload NotificationsAllowUnload
#else
#define MAYBE_NotificationsAllowUnload DISABLED_NotificationsAllowUnload
#endif

IN_PROC_BROWSER_TEST_F(NotificationIdleTest, MAYBE_NotificationsAllowUnload) {
  const extensions::Extension* extension =
      LoadExtensionAndWait("notifications/api/unload");
  ASSERT_TRUE(extension) << message_;

  // Lazy Background Page has been shut down.
  extensions::ProcessManager* pm =
      extensions::ExtensionSystem::Get(profile())->process_manager();
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(last_loaded_extension_id()));
}
