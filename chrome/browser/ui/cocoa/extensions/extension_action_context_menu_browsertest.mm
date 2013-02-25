// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

using extensions::Extension;

class ExtensionActionContextMenuTest : public ExtensionBrowserTest {
public:
  ExtensionActionContextMenuTest() : extension_(NULL), action_(NULL) {}

 protected:
  void SetupPageAction() {
    extension_ = InstallExtension(
        test_data_dir_.AppendASCII("browsertest")
                      .AppendASCII("page_action_popup"),
        1);
    EXPECT_TRUE(extension_);
    extensions::ExtensionActionManager* action_manager =
        extensions::ExtensionActionManager::Get(browser()->profile());
    action_ = action_manager->GetPageAction(*extension_);
    EXPECT_TRUE(action_);

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    action_->SetAppearance(ExtensionTabUtil::GetTabId(contents),
                           ExtensionAction::ACTIVE);

    BrowserWindowCocoa* window =
        static_cast<BrowserWindowCocoa*>(browser()->window());
    ToolbarController* toolbarController =
        [window->cocoa_controller() toolbarController];
    LocationBarViewMac* locationBarView =
        [toolbarController locationBarBridge];
    locationBarView->Update(contents, false);
  }

  const Extension* extension_;
  ExtensionAction* action_;
};

IN_PROC_BROWSER_TEST_F(ExtensionActionContextMenuTest, BasicTest) {
  SetupPageAction();
  scoped_nsobject<ExtensionActionContextMenu> menu;
  menu.reset([[ExtensionActionContextMenu alloc] initWithExtension:extension_
                                                           browser:browser()
                                                   extensionAction:action_]);

  NSMenuItem* inspectItem = [menu itemWithTag:
        extension_action_context_menu::kExtensionContextInspect];
  EXPECT_TRUE(inspectItem);

  PrefService* service = browser()->profile()->GetPrefs();
  bool original = service->GetBoolean(prefs::kExtensionsUIDeveloperMode);

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  EXPECT_FALSE([inspectItem isHidden]);

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);
  EXPECT_TRUE([inspectItem isHidden]);

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, original);
}

// Test that browser action context menus work. Browser actions have their
// menus created during browser initialization, when there is no tab. This
// test simulates that and checks the menu is operational.
IN_PROC_BROWSER_TEST_F(ExtensionActionContextMenuTest, BrowserAction) {
  extension_ = InstallExtension(
      test_data_dir_.AppendASCII("browsertest")
                    .AppendASCII("browser_action_popup"),
      1);
  EXPECT_TRUE(extension_);
  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(browser()->profile());
  action_ = action_manager->GetBrowserAction(*extension_);
  EXPECT_TRUE(action_);

  Browser* empty_browser(
       new Browser(Browser::CreateParams(browser()->profile(),
                                         browser()->host_desktop_type())));

  scoped_nsobject<ExtensionActionContextMenu> menu;
  menu.reset([[ExtensionActionContextMenu alloc]
      initWithExtension:extension_
                browser:empty_browser
        extensionAction:action_]);

  NSMenuItem* inspectItem = [menu itemWithTag:
        extension_action_context_menu::kExtensionContextInspect];
  EXPECT_TRUE(inspectItem);

  // Close the empty browser. Can't just free it directly because there are
  // dangling references in the various native controllers that must be
  // cleaned up first.
  NSWindow* window = empty_browser->window()->GetNativeWindow();
  BrowserWindowController* wc =
    [BrowserWindowController browserWindowControllerForWindow:window];
  ASSERT_TRUE(wc != NULL);
  [wc destroyBrowser];
}

IN_PROC_BROWSER_TEST_F(ExtensionActionContextMenuTest, RunInspectPopup) {
  SetupPageAction();
  scoped_nsobject<ExtensionActionContextMenu> menu;
  menu.reset([[ExtensionActionContextMenu alloc] initWithExtension:extension_
                                                           browser:browser()
                                                   extensionAction:action_]);

  NSMenuItem* inspectItem = [menu itemWithTag:
        extension_action_context_menu::kExtensionContextInspect];
  EXPECT_TRUE(inspectItem);

  PrefService* service = browser()->profile()->GetPrefs();
  bool original = service->GetBoolean(prefs::kExtensionsUIDeveloperMode);

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  EXPECT_FALSE([inspectItem isHidden]);

  content::WindowedNotificationObserver devtools_attached_observer(
    content::NOTIFICATION_DEVTOOLS_AGENT_ATTACHED,
    content::NotificationService::AllSources());
  [NSApp sendAction:[inspectItem action]
                 to:[inspectItem target]
               from:inspectItem];
  devtools_attached_observer.Wait();

  // Hide the popup to prevent racy crashes at test cleanup.
  BrowserActionTestUtil test_util(browser());
  test_util.HidePopup();

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, original);
}
