// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"

#include "base/command_line.h"
#import "base/mac/foundation_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/apps/app_window_registry_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "ui/message_center/message_center.h"

namespace {

// The param selects whether to use ChromeNativeAppWindowViewsMac, otherwise it
// will use NativeAppWindowCocoa.
class QuitWithAppsControllerInteractiveTest
    : public testing::WithParamInterface<bool>,
      public extensions::PlatformAppBrowserTest {
 protected:
  QuitWithAppsControllerInteractiveTest() : app_(NULL) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAppsKeepChromeAliveInTests);
    command_line->AppendSwitch(
        GetParam() ? switches::kEnableMacViewsNativeAppWindows
                   : switches::kDisableMacViewsNativeAppWindows);
  }

  const extensions::Extension* app_;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuitWithAppsControllerInteractiveTest);
};

}  // namespace

// Test that quitting while apps are open shows a notification instead.
IN_PROC_BROWSER_TEST_P(QuitWithAppsControllerInteractiveTest, QuitBehavior) {
  scoped_refptr<QuitWithAppsController> controller =
      new QuitWithAppsController();
  const Notification* notification;
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  std::vector<Profile*> profiles(profile_manager->GetLoadedProfiles());
  DCHECK(profiles.size());

  // With no app windows open, ShouldQuit returns true.
  EXPECT_TRUE(controller->ShouldQuit());
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID,
      NotificationUIManager::GetProfileID(profiles[0]));
  EXPECT_EQ(NULL, notification);

  // Open an app window.
  ExtensionTestMessageListener listener("Launched", false);
  app_ = InstallAndLaunchPlatformApp("minimal_id");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // One browser and one app window at this point.
  EXPECT_FALSE(BrowserList::GetInstance()->empty());
  EXPECT_TRUE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));

  // On the first quit, show notification.
  EXPECT_FALSE(controller->ShouldQuit());
  EXPECT_TRUE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID,
      NotificationUIManager::GetProfileID(profiles[0]));
  ASSERT_TRUE(notification);

  // If notification was dismissed by click, show again on next quit.
  notification->delegate()->Click();
  message_center->RemoveAllNotifications(false);
  EXPECT_FALSE(controller->ShouldQuit());
  EXPECT_TRUE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID,
      NotificationUIManager::GetProfileID(profiles[0]));
  ASSERT_TRUE(notification);

  EXPECT_FALSE(BrowserList::GetInstance()->empty());
  EXPECT_TRUE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));

  // If notification is closed by user, don't show it next time.
  notification->delegate()->Close(true);
  message_center->RemoveAllNotifications(false);
  EXPECT_FALSE(controller->ShouldQuit());
  EXPECT_TRUE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID,
      NotificationUIManager::GetProfileID(profiles[0]));
  EXPECT_EQ(NULL, notification);

  EXPECT_FALSE(BrowserList::GetInstance()->empty());
  EXPECT_TRUE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));

  // Get a reference to the open app window before the browser closes.
  extensions::AppWindow* app_window = GetFirstAppWindow();

  // Quitting should not quit but close all browsers
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());
  chrome_browser_application_mac::Terminate();
  observer.Wait();

  EXPECT_TRUE(BrowserList::GetInstance()->empty());
  EXPECT_TRUE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));

  // Trying to quit while there are no browsers always shows notification.
  EXPECT_FALSE(controller->ShouldQuit());
  notification = g_browser_process->notification_ui_manager()->FindById(
      QuitWithAppsController::kQuitWithAppsNotificationID,
      NotificationUIManager::GetProfileID(profiles[0]));
  ASSERT_TRUE(notification);

  // Clicking "Quit All Apps." button closes all app windows. With no browsers
  // open, this should also quit Chrome.
  content::WindowedNotificationObserver quit_observer(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  // Since closing app windows may be an async operation, use a watcher.
  content::WebContentsDestroyedWatcher destroyed_watcher(
      app_window->web_contents());
  notification->delegate()->ButtonClick(0);
  destroyed_watcher.Wait();
  message_center->RemoveAllNotifications(false);
  EXPECT_FALSE(AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(0));
  quit_observer.Wait();
}

// Test that, when powering off, Chrome will quit even if there are apps open.
IN_PROC_BROWSER_TEST_P(QuitWithAppsControllerInteractiveTest, QuitOnPowerOff) {
  // Open an app window.
  app_ = LoadAndLaunchPlatformApp("minimal_id", "Launched");

  // First try to terminate with a packaged app running. Chrome should stay
  // running in the background.
  [NSApp terminate:nil];
  EXPECT_FALSE(browser_shutdown::IsTryingToQuit());

  // Simulate a terminate triggered by a power off or log out.
  // Cocoa will send an NSWorkspaceWillPowerOffNotification followed by
  // -[NSApplication terminate:].
  AppController* app_controller =
      base::mac::ObjCCast<AppController>([NSApp delegate]);
  NSNotification* notification =
      [NSNotification notificationWithName:NSWorkspaceWillPowerOffNotification
                                    object:nil];
  [app_controller willPowerOff:notification];
  [NSApp terminate:nil];
  EXPECT_TRUE(browser_shutdown::IsTryingToQuit());
}

INSTANTIATE_TEST_CASE_P(QuitWithAppsControllerInteractiveTestInstance,
                        QuitWithAppsControllerInteractiveTest,
                        ::testing::Bool());
