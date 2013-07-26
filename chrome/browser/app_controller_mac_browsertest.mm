// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/platform_app_browsertest_util.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#import "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"

namespace {

class AppControllerPlatformAppBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  AppControllerPlatformAppBrowserTest()
      : active_browser_list_(BrowserList::GetInstance(
                                chrome::GetActiveDesktop())) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppId,
                                    "1234");
  }

  const BrowserList* active_browser_list_;
};

// Test that if only a platform app window is open and no browser windows are
// open then a reopen event does nothing.
IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       PlatformAppReopenWithWindows) {
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  NSUInteger old_window_count = [[NSApp windows] count];
  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:YES];

  EXPECT_TRUE(result);
  EXPECT_EQ(old_window_count, [[NSApp windows] count]);
  EXPECT_EQ(1u, active_browser_list_->size());
}

// Test that focusing an app window changes the menu bar.
IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       PlatformAppFocusUpdatesMenuBar) {
  base::scoped_nsobject<AppController> app_controller(
      [[AppController alloc] init]);

  // Start two apps and wait for them to be launched.
  ExtensionTestMessageListener listener_1("Launched", false);
  const extensions::Extension* app_1 =
      InstallAndLaunchPlatformApp("minimal_id");
  ASSERT_TRUE(listener_1.WaitUntilSatisfied());
  ExtensionTestMessageListener listener_2("Launched", false);
  const extensions::Extension* app_2 =
      InstallAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(listener_2.WaitUntilSatisfied());

  NSMenu* main_menu = [NSApp mainMenu];
  NSUInteger initial_menu_item_count = [[main_menu itemArray] count];

  // When an app is focused, all Chrome menu items should be hidden, and a menu
  // item for the app should be added.
  apps::ShellWindow* app_1_shell_window =
      extensions::ShellWindowRegistry::Get(profile())->
          GetShellWindowsForApp(app_1->id()).front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_1_shell_window->GetNativeWindow()];
  NSArray* item_array = [main_menu itemArray];
  EXPECT_EQ(initial_menu_item_count + 1, [item_array count]);
  for (NSUInteger i = 0; i < initial_menu_item_count; ++i)
    EXPECT_TRUE([[item_array objectAtIndex:i] isHidden]);
  NSMenuItem* last_item = [item_array lastObject];
  EXPECT_EQ(app_1->id(), base::SysNSStringToUTF8([last_item title]));
  EXPECT_EQ(app_1->name(),
            base::SysNSStringToUTF8([[last_item submenu] title]));
  EXPECT_FALSE([last_item isHidden]);

  // When another app is focused, the menu item for the app should change.
  apps::ShellWindow* app_2_shell_window =
      extensions::ShellWindowRegistry::Get(profile())->
          GetShellWindowsForApp(app_2->id()).front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_2_shell_window->GetNativeWindow()];
  item_array = [main_menu itemArray];
  EXPECT_EQ(initial_menu_item_count + 1, [item_array count]);
  for (NSUInteger i = 0; i < initial_menu_item_count; ++i)
    EXPECT_TRUE([[item_array objectAtIndex:i] isHidden]);
  last_item = [item_array lastObject];
  EXPECT_EQ(app_2->id(), base::SysNSStringToUTF8([last_item title]));
  EXPECT_EQ(app_2->name(),
            base::SysNSStringToUTF8([[last_item submenu] title]));
  EXPECT_FALSE([last_item isHidden]);

  // When Chrome is focused, the menu item for the app should be removed.
  NSWindow* browser_native_window =
      active_browser_list_->get(0)->window()->GetNativeWindow();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:browser_native_window];
  item_array = [main_menu itemArray];
  EXPECT_EQ(initial_menu_item_count, [item_array count]);
  for (NSUInteger i = 0; i < initial_menu_item_count; ++i)
    EXPECT_FALSE([[item_array objectAtIndex:i] isHidden]);
}

IN_PROC_BROWSER_TEST_F(AppControllerPlatformAppBrowserTest,
                       ActivationFocusesBrowserWindow) {
  base::scoped_nsobject<AppController> app_controller(
      [[AppController alloc] init]);

  ExtensionTestMessageListener listener("Launched", false);
  const extensions::Extension* app =
      InstallAndLaunchPlatformApp("minimal");
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  NSWindow* app_window = extensions::ShellWindowRegistry::Get(profile())->
      GetShellWindowsForApp(app->id()).front()->GetNativeWindow();
  NSWindow* browser_window = browser()->window()->GetNativeWindow();

  EXPECT_LE([[NSApp orderedWindows] indexOfObject:app_window],
            [[NSApp orderedWindows] indexOfObject:browser_window]);
  [app_controller applicationShouldHandleReopen:NSApp
                              hasVisibleWindows:YES];
  EXPECT_LE([[NSApp orderedWindows] indexOfObject:browser_window],
            [[NSApp orderedWindows] indexOfObject:app_window]);
}

class AppControllerWebAppBrowserTest : public InProcessBrowserTest {
 protected:
  AppControllerWebAppBrowserTest()
      : active_browser_list_(BrowserList::GetInstance(
                                chrome::GetActiveDesktop())) {
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kApp, GetAppURL());
  }

  std::string GetAppURL() const {
    return "http://example.com/";
  }

  const BrowserList* active_browser_list_;
};

// Test that in web app mode a reopen event opens the app URL.
IN_PROC_BROWSER_TEST_F(AppControllerWebAppBrowserTest,
                       WebAppReopenWithNoWindows) {
  base::scoped_nsobject<AppController> ac([[AppController alloc] init]);
  EXPECT_EQ(1u, active_browser_list_->size());
  BOOL result = [ac applicationShouldHandleReopen:NSApp hasVisibleWindows:NO];

  EXPECT_FALSE(result);
  EXPECT_EQ(2u, active_browser_list_->size());

  Browser* browser = active_browser_list_->get(0);
  GURL current_url =
      browser->tab_strip_model()->GetActiveWebContents()->GetURL();
  EXPECT_EQ(GetAppURL(), current_url.spec());
}

}  // namespace
