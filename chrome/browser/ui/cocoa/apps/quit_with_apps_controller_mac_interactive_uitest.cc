// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"

#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "ui/message_center/message_center.h"

typedef apps::AppWindowRegistry::AppWindowList AppWindowList;

namespace {

class QuitWithAppsControllerInteractiveTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  QuitWithAppsControllerInteractiveTest() : app_(NULL) {}

  virtual ~QuitWithAppsControllerInteractiveTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAppsKeepChromeAliveInTests);
  }

  const extensions::Extension* app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitWithAppsControllerInteractiveTest);
};

}  // namespace

// Test that quitting while apps are open shows a notification instead.
IN_PROC_BROWSER_TEST_F(QuitWithAppsControllerInteractiveTest, QuitBehavior) {
  scoped_refptr<QuitWithAppsController> controller =
      new QuitWithAppsController();
  const Notification* notification;
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // With no app windows open, ShouldQuit returns true.
  EXPECT_TRUE(controller->ShouldQuit());
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID);
  EXPECT_EQ(NULL, notification);

  // Open an app window.
  ExtensionTestMessageListener listener("Launched", false);
  app_ = InstallAndLaunchPlatformApp("minimal_id");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // One browser and one app window at this point.
  EXPECT_FALSE(chrome::BrowserIterator().done());
  EXPECT_TRUE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));

  // On the first quit, show notification.
  EXPECT_FALSE(controller->ShouldQuit());
  EXPECT_TRUE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID);
  ASSERT_TRUE(notification);

  // If notification was dismissed by click, show again on next quit.
  notification->delegate()->Click();
  message_center->RemoveAllNotifications(false);
  EXPECT_FALSE(controller->ShouldQuit());
  EXPECT_TRUE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID);
  ASSERT_TRUE(notification);

  EXPECT_FALSE(chrome::BrowserIterator().done());
  EXPECT_TRUE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));

  // If notification is closed by user, don't show it next time.
  notification->delegate()->Close(true);
  message_center->RemoveAllNotifications(false);
  EXPECT_FALSE(controller->ShouldQuit());
  EXPECT_TRUE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID);
  EXPECT_EQ(NULL, notification);

  EXPECT_FALSE(chrome::BrowserIterator().done());
  EXPECT_TRUE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));

  // Quitting should not quit but close all browsers
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());
  chrome_browser_application_mac::Terminate();
  observer.Wait();

  EXPECT_TRUE(chrome::BrowserIterator().done());
  EXPECT_TRUE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));

  // Trying to quit while there are no browsers always shows notification.
  EXPECT_FALSE(controller->ShouldQuit());
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID);
  ASSERT_TRUE(notification);

  // Clicking "Quit All Apps." button closes all app windows. With no browsers
  // open, this should also quit Chrome.
  content::WindowedNotificationObserver quit_observer(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());
  notification->delegate()->ButtonClick(0);
  message_center->RemoveAllNotifications(false);
  EXPECT_FALSE(apps::AppWindowRegistry::IsAppWindowRegisteredInAnyProfile(0));
  quit_observer.Wait();
}
