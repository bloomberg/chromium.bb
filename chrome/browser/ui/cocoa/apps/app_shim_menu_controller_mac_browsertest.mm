// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/apps/app_shim_menu_controller_mac.h"

#import <Cocoa/Cocoa.h>

#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"

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

  void CheckHasAppMenus(const extensions::Extension* app) const {
    const int kExtraTopLevelItems = 4;
    NSArray* item_array = [[NSApp mainMenu] itemArray];
    EXPECT_EQ(initial_menu_item_count_ + kExtraTopLevelItems,
              [item_array count]);
    for (NSUInteger i = 0; i < initial_menu_item_count_; ++i)
      EXPECT_TRUE([[item_array objectAtIndex:i] isHidden]);
    NSMenuItem* app_menu = [item_array objectAtIndex:initial_menu_item_count_];
    EXPECT_EQ(app->id(), base::SysNSStringToUTF8([app_menu title]));
    EXPECT_EQ(app->name(),
              base::SysNSStringToUTF8([[app_menu submenu] title]));
    for (NSUInteger i = initial_menu_item_count_;
         i < initial_menu_item_count_ + kExtraTopLevelItems;
         ++i) {
      NSMenuItem* menu = [item_array objectAtIndex:i];
      EXPECT_GT([[menu submenu] numberOfItems], 0);
      EXPECT_FALSE([menu isHidden]);
    }
  }

  void CheckNoAppMenus() const {
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
  apps::AppWindow* app_1_app_window = apps::AppWindowRegistry::Get(profile())
                                          ->GetAppWindowsForApp(app_1_->id())
                                          .front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_1_app_window->GetNativeWindow()];
  CheckHasAppMenus(app_1_);

  // When another app is focused, the menu item for the app should change.
  apps::AppWindow* app_2_app_window = apps::AppWindowRegistry::Get(profile())
                                          ->GetAppWindowsForApp(app_2_->id())
                                          .front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_2_app_window->GetNativeWindow()];
  CheckHasAppMenus(app_2_);

  // When a browser window is focused, the menu items for the app should be
  // removed.
  BrowserWindow* chrome_window = chrome::BrowserIterator()->window();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:chrome_window->GetNativeWindow()];
  CheckNoAppMenus();

  // When an app window is closed and there are no other app windows, the menu
  // items for the app should be removed.
  app_1_app_window->GetBaseWindow()->Close();
  chrome_window->Close();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowWillCloseNotification
                    object:app_2_app_window->GetNativeWindow()];
  CheckNoAppMenus();
}

IN_PROC_BROWSER_TEST_F(AppShimMenuControllerBrowserTest,
                       ExtensionUninstallUpdatesMenuBar) {
  SetUpApps();

  // This essentially tests that a NSWindowWillCloseNotification gets fired when
  // an app is uninstalled. We need to close the other windows first since the
  // menu only changes on a NSWindowWillCloseNotification if there are no other
  // windows.
  apps::AppWindow* app_2_app_window = apps::AppWindowRegistry::Get(profile())
                                          ->GetAppWindowsForApp(app_2_->id())
                                          .front();
  app_2_app_window->GetBaseWindow()->Close();

  chrome::BrowserIterator()->window()->Close();

  apps::AppWindow* app_1_app_window = apps::AppWindowRegistry::Get(profile())
                                          ->GetAppWindowsForApp(app_1_->id())
                                          .front();
  [[NSNotificationCenter defaultCenter]
      postNotificationName:NSWindowDidBecomeMainNotification
                    object:app_1_app_window->GetNativeWindow()];

  CheckHasAppMenus(app_1_);
  ExtensionService::UninstallExtensionHelper(
      extension_service(),
      app_1_->id(),
      extensions::UNINSTALL_REASON_FOR_TESTING);
  CheckNoAppMenus();
}

}  // namespace
