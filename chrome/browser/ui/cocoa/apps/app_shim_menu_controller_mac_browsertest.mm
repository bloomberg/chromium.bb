// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"

#import <Cocoa/Cocoa.h>

#include "apps/shell_window_registry.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"

namespace {

class AppShimMenuControllerBrowserTest
    : public extensions::PlatformAppBrowserTest {
 protected:
  AppShimMenuControllerBrowserTest()
      : app_1_(NULL),
        app_2_(NULL),
        initial_menu_item_count_(0) {}

  virtual ~AppShimMenuControllerBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PlatformAppBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableAppShims);
  }

  // Start two apps and wait for them to be launched.
  void SetUpApps() {
    ExtensionTestMessageListener listener_1("Launched", false);
    app_1_ = InstallAndLaunchPlatformApp("minimal_id");
    ASSERT_TRUE(listener_1.WaitUntilSatisfied());
    ExtensionTestMessageListener listener_2("Launched", false);
    app_2_ = InstallAndLaunchPlatformApp("minimal");
    ASSERT_TRUE(listener_2.WaitUntilSatisfied());

    initial_menu_item_count_ = [[[NSApp mainMenu] itemArray] count];
  }

  void CheckHasAppMenu(const extensions::Extension* app) const {
    NSArray* item_array = [[NSApp mainMenu] itemArray];
    EXPECT_EQ(initial_menu_item_count_ + 1, [item_array count]);
    for (NSUInteger i = 0; i < initial_menu_item_count_; ++i)
      EXPECT_TRUE([[item_array objectAtIndex:i] isHidden]);
    NSMenuItem* last_item = [item_array lastObject];
    EXPECT_EQ(app->id(), base::SysNSStringToUTF8([last_item title]));
    EXPECT_EQ(app->name(),
              base::SysNSStringToUTF8([[last_item submenu] title]));
    EXPECT_FALSE([last_item isHidden]);
  }

  void CheckNoAppMenu() const {
    NSArray* item_array = [[NSApp mainMenu] itemArray];
    EXPECT_EQ(initial_menu_item_count_, [item_array count]);
    for (NSUInteger i = 0; i < initial_menu_item_count_; ++i)
      EXPECT_FALSE([[item_array objectAtIndex:i] isHidden]);
  }

  const extensions::Extension* app_1_;
  const extensions::Extension* app_2_;
  NSUInteger initial_menu_item_count_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppShimMenuControllerBrowserTest);
};

// Test that focusing an app window changes the menu bar.
IN_PROC_BROWSER_TEST_F(AppShimMenuControllerBrowserTest,
                       PlatformAppFocusUpdatesMenuBar) {
  SetUpApps();
  // When an app is focused, all Chrome menu items should be hidden, and a menu
  // item for the app should be added.
  apps::ShellWindow* app_1_shell_window =
      apps::ShellWindowRegistry::Get(profile())->
          GetShellWindowsForApp(app_1_->id()).front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_1_shell_window->GetNativeWindow()];
  CheckHasAppMenu(app_1_);

  // When another app is focused, the menu item for the app should change.
  apps::ShellWindow* app_2_shell_window =
      apps::ShellWindowRegistry::Get(profile())->
          GetShellWindowsForApp(app_2_->id()).front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_2_shell_window->GetNativeWindow()];
  CheckHasAppMenu(app_2_);

  // When the app window loses focus, the menu items for the app should be
  // removed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidResignMainNotification
                    object:app_2_shell_window->GetNativeWindow()];
  CheckNoAppMenu();

  // When an app window is closed, the menu items for the app should be removed.
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_2_shell_window->GetNativeWindow()];
  CheckHasAppMenu(app_2_);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowWillCloseNotification
                    object:app_2_shell_window->GetNativeWindow()];
  CheckNoAppMenu();
}

IN_PROC_BROWSER_TEST_F(AppShimMenuControllerBrowserTest,
                       ExtensionUninstallUpdatesMenuBar) {
  SetUpApps();

  apps::ShellWindow* app_1_shell_window =
      apps::ShellWindowRegistry::Get(profile())->
          GetShellWindowsForApp(app_1_->id()).front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_1_shell_window->GetNativeWindow()];

  CheckHasAppMenu(app_1_);
  ExtensionService::UninstallExtensionHelper(extension_service(), app_1_->id());
  CheckNoAppMenu();
}

}  // namespace
