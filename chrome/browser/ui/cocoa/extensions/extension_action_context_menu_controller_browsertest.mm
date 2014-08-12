// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_action_context_menu_controller.h"

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
#include "chrome/common/pref_names.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

using extensions::Extension;

namespace {

NSMenuItem* GetInspectItem(NSMenu* menu) {
  return [menu itemWithTitle:
      l10n_util::GetNSStringWithFixup(IDS_EXTENSION_ACTION_INSPECT_POPUP)];
}

}  // namespace

class ExtensionActionContextMenuControllerTest : public ExtensionBrowserTest {
 public:
  ExtensionActionContextMenuControllerTest() : extension_(NULL), action_(NULL) {
  }

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
    action_->SetIsVisible(extensions::ExtensionTabUtil::GetTabId(contents),
                          true);

    BrowserWindowCocoa* window =
        static_cast<BrowserWindowCocoa*>(browser()->window());
    ToolbarController* toolbarController =
        [window->cocoa_controller() toolbarController];
    LocationBarViewMac* locationBarView =
        [toolbarController locationBarBridge];
    locationBarView->Update(NULL);
  }

  const Extension* extension_;
  ExtensionAction* action_;
};

IN_PROC_BROWSER_TEST_F(ExtensionActionContextMenuControllerTest, BasicTest) {
  SetupPageAction();
  base::scoped_nsobject<ExtensionActionContextMenuController> controller(
      [[ExtensionActionContextMenuController alloc] initWithExtension:extension_
                                                              browser:browser()
                                                      extensionAction:action_]);


  PrefService* service = browser()->profile()->GetPrefs();
  bool original = service->GetBoolean(prefs::kExtensionsUIDeveloperMode);

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  [controller populateMenu:menu];
  EXPECT_GT([menu numberOfItems], 0);
  EXPECT_TRUE(GetInspectItem(menu));

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, false);
  [menu removeAllItems];
  [controller populateMenu:menu];
  EXPECT_GT([menu numberOfItems], 0);
  EXPECT_FALSE(GetInspectItem(menu));

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, original);
}

// Test that browser action context menus work. Browser actions have their
// menus created during browser initialization, when there is no tab. This
// test simulates that and checks the menu is operational.
IN_PROC_BROWSER_TEST_F(ExtensionActionContextMenuControllerTest,
                       BrowserAction) {
  extension_ =
      InstallExtension(test_data_dir_.AppendASCII("browsertest").AppendASCII(
                           "browser_action_popup"),
                       1);
  EXPECT_TRUE(extension_);
  extensions::ExtensionActionManager* action_manager =
      extensions::ExtensionActionManager::Get(browser()->profile());
  action_ = action_manager->GetBrowserAction(*extension_);
  EXPECT_TRUE(action_);

  Browser* empty_browser(
       new Browser(Browser::CreateParams(browser()->profile(),
                                         browser()->host_desktop_type())));

  base::scoped_nsobject<ExtensionActionContextMenuController> controller(
      [[ExtensionActionContextMenuController alloc]
          initWithExtension:extension_
                    browser:empty_browser
            extensionAction:action_]);
  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  [controller populateMenu:menu];
  EXPECT_GT([menu numberOfItems], 0);

  // Close the empty browser. Can't just free it directly because there are
  // dangling references in the various native controllers that must be
  // cleaned up first.
  NSWindow* window = empty_browser->window()->GetNativeWindow();
  BrowserWindowController* wc =
    [BrowserWindowController browserWindowControllerForWindow:window];
  ASSERT_TRUE(wc != NULL);
  [wc destroyBrowser];
}

namespace {

class DevToolsAttachedObserver {
 public:
  DevToolsAttachedObserver(const base::Closure& callback)
      : callback_(callback),
        devtools_callback_(base::Bind(
            &DevToolsAttachedObserver::OnDevToolsStateChanged,
            base::Unretained(this))) {
    content::DevToolsManager::GetInstance()->AddAgentStateCallback(
        devtools_callback_);
  }

  ~DevToolsAttachedObserver() {
    content::DevToolsManager::GetInstance()->RemoveAgentStateCallback(
        devtools_callback_);
  }

  void OnDevToolsStateChanged(content::DevToolsAgentHost*, bool attached) {
    if (attached)
      callback_.Run();
  }

 private:
  base::Closure callback_;
  base::Callback<void(content::DevToolsAgentHost*, bool)> devtools_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsAttachedObserver);
};

}  // namespace

IN_PROC_BROWSER_TEST_F(
    ExtensionActionContextMenuControllerTest, DISABLED_RunInspectPopup) {
  SetupPageAction();
  base::scoped_nsobject<ExtensionActionContextMenuController> controller(
      [[ExtensionActionContextMenuController alloc] initWithExtension:extension_
                                                              browser:browser()
                                                      extensionAction:action_]);

  PrefService* service = browser()->profile()->GetPrefs();
  bool original = service->GetBoolean(prefs::kExtensionsUIDeveloperMode);

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, true);

  base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@""]);
  [controller populateMenu:menu];
  EXPECT_GT([menu numberOfItems], 0);
  NSMenuItem* inspectItem = GetInspectItem(menu);
  EXPECT_TRUE(inspectItem);

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  DevToolsAttachedObserver observer(loop_runner->QuitClosure());
  [NSApp sendAction:[inspectItem action]
                 to:[inspectItem target]
               from:inspectItem];
  loop_runner->Run();

  service->SetBoolean(prefs::kExtensionsUIDeveloperMode, original);
}
