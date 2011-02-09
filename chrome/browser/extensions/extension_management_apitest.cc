// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/ui_test_utils.h"

namespace {

// Find a browser other than |browser|.
Browser* FindOtherBrowser(Browser* browser) {
  Browser* found = NULL;
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end(); ++it) {
    if (*it == browser)
      continue;
    found = *it;
  }

  return found;
}

}  // namespace

class ExtensionManagementApiTest : public ExtensionApiTest {
 public:
  virtual void InstallExtensions() {
    FilePath basedir = test_data_dir_.AppendASCII("management");

    // Load 4 enabled items.
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("enabled_extension")));
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("enabled_app")));
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("description")));
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("permissions")));

    // Load 2 disabled items.
    ExtensionService* service = browser()->profile()->GetExtensionService();
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("disabled_extension")));
    service->DisableExtension(last_loaded_extension_id_);
    ASSERT_TRUE(LoadExtension(basedir.AppendASCII("disabled_app")));
    service->DisableExtension(last_loaded_extension_id_);
  }

  // Load an app, and wait for a message from app "management/launch_on_install"
  // indicating that the new app has been launched.
  void LoadAndWaitForLaunch(const std::string& app_path,
                            std::string* out_app_id) {
    ExtensionTestMessageListener launched_app("launched app", false);
    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(app_path)));

    if (out_app_id)
      *out_app_id = last_loaded_extension_id_;

    ASSERT_TRUE(launched_app.WaitUntilSatisfied());
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Basics) {
  InstallExtensions();
  ASSERT_TRUE(RunExtensionSubtest("management/test", "basics.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Uninstall) {
  InstallExtensions();
  ASSERT_TRUE(RunExtensionSubtest("management/test", "uninstall.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, LaunchPanelApp) {
  ExtensionService* service = browser()->profile()->GetExtensionService();

  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Load an app with app.launch.container = "panel".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id);
  ASSERT_FALSE(HasFatalFailure());  // Stop the test if any ASSERT failed.

  // Find the app's browser.  Check that it is a panel.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_EQ(Browser::TYPE_APP_POPUP, app_browser->type());

  // Close the app panel.
  app_browser->CloseWindow();
  ui_test_utils::WaitForNotificationFrom(NotificationType::BROWSER_CLOSED,
                                           Source<Browser>(app_browser));

  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(service->GetExtensionById(app_id, true));

  // Set a pref indicating that the user wants to launch in a regular tab.
  // This should be ignored, because panel apps always load in a panel.
  service->extension_prefs()->SetLaunchType(
      app_id, ExtensionPrefs::LAUNCH_REGULAR);

  // Load the extension again.
  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

  // Find the app's browser.  Apps that should load in a panel ignore
  // prefs, so we should still see the launch in a panel.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));
  app_browser = FindOtherBrowser(browser());
  ASSERT_EQ(Browser::TYPE_APP_POPUP, app_browser->type());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, LaunchTabApp) {
  ExtensionService* service = browser()->profile()->GetExtensionService();

  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Code below assumes that the test starts with a single browser window
  // hosting one tab.
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_count());

  // Load an app with app.launch.container = "tab".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id);
  ASSERT_FALSE(HasFatalFailure());

  // Check that the app opened in a new tab of the existing browser.
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_count());

  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(service->GetExtensionById(app_id, true));

  // Set a pref indicating that the user wants to launch in a window.
  service->extension_prefs()->SetLaunchType(
      app_id, ExtensionPrefs::LAUNCH_WINDOW);

  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

#if defined(OS_MACOSX)
  // App windows are not yet implemented on mac os.  We should fall back
  // to a normal tab.
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_count());
#else
  // Find the app's browser.  Opening in a new window will create
  // a new browser.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_EQ(Browser::TYPE_APP, app_browser->type());
#endif
}
