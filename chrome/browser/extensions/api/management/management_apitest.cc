// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/test_management_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest.h"
#include "content/public/test/test_utils.h"

using extensions::Extension;
using extensions::Manifest;

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
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }

  virtual void LoadExtensions() {
    FilePath basedir = test_data_dir_.AppendASCII("management");

    // Load 4 enabled items.
    LoadNamedExtension(basedir, "enabled_extension");
    LoadNamedExtension(basedir, "enabled_app");
    LoadNamedExtension(basedir, "description");
    LoadNamedExtension(basedir, "permissions");

    // Load 2 disabled items.
    LoadNamedExtension(basedir, "disabled_extension");
    DisableExtension(extension_ids_["disabled_extension"]);
    LoadNamedExtension(basedir, "disabled_app");
    DisableExtension(extension_ids_["disabled_app"]);
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

 protected:
  void LoadNamedExtension(const FilePath& path,
                          const std::string& name) {
    const Extension* extension = LoadExtension(path.AppendASCII(name));
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  void InstallNamedExtension(const FilePath& path,
                             const std::string& name,
                             Extension::Location install_source) {
    const Extension* extension = InstallExtension(path.AppendASCII(name), 1,
                                                  install_source);
    ASSERT_TRUE(extension);
    extension_ids_[name] = extension->id();
  }

  // Maps installed extension names to their IDs.
  std::map<std::string, std::string> extension_ids_;
};

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Basics) {
  LoadExtensions();

  FilePath basedir = test_data_dir_.AppendASCII("management");
  InstallNamedExtension(basedir, "internal_extension", Extension::INTERNAL);
  InstallNamedExtension(basedir, "external_extension",
                        Extension::EXTERNAL_PREF);
  InstallNamedExtension(basedir, "admin_extension",
                        Extension::EXTERNAL_POLICY_DOWNLOAD);

  ASSERT_TRUE(RunExtensionSubtest("management/test", "basics.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, Uninstall) {
  LoadExtensions();
  ASSERT_TRUE(RunExtensionSubtest("management/test", "uninstall.html"));
}

// Tests actions on extensions when no management policy is in place.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, ManagementPolicyAllowed) {
  LoadExtensions();
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  EXPECT_TRUE(service->GetExtensionById(extension_ids_["enabled_extension"],
                                        false));

  // Ensure that all actions are allowed.
  extensions::ExtensionSystem::Get(
      browser()->profile())->management_policy()->UnregisterAllProviders();

  ASSERT_TRUE(RunExtensionSubtest("management/management_policy",
                                  "allowed.html"));
  // The last thing the test does is uninstall the "enabled_extension".
  EXPECT_FALSE(service->GetExtensionById(extension_ids_["enabled_extension"],
                                         true));
}

// Tests actions on extensions when management policy prohibits those actions.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, ManagementPolicyProhibited) {
  LoadExtensions();
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  EXPECT_TRUE(service->GetExtensionById(extension_ids_["enabled_extension"],
                                        false));

  // Prohibit status changes.
  extensions::ManagementPolicy* policy = extensions::ExtensionSystem::Get(
      browser()->profile())->management_policy();
  policy->UnregisterAllProviders();
  extensions::TestManagementPolicyProvider provider(
    extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  policy->RegisterProvider(&provider);
  ASSERT_TRUE(RunExtensionSubtest("management/management_policy",
                                  "prohibited.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, LaunchPanelApp) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();

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

  // Find the app's browser.  Check that it is a popup.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_popup());
  ASSERT_TRUE(app_browser->is_app());

  // Close the app panel.
  content::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::Source<Browser>(app_browser));

  chrome::CloseWindow(app_browser);
  signal.Wait();

  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(service->GetExtensionById(app_id, true));

  // Set a pref indicating that the user wants to launch in a regular tab.
  // This should be ignored, because panel apps always load in a popup.
  service->extension_prefs()->SetLaunchType(
      app_id, extensions::ExtensionPrefs::LAUNCH_REGULAR);

  // Load the extension again.
  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_panel", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

  // Find the app's browser.  Apps that should load in a panel ignore
  // prefs, so we should still see the launch in a popup.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_type_popup());
  ASSERT_TRUE(app_browser->is_app());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiTest, LaunchTabApp) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();

  // Load an extension that calls launchApp() on any app that gets
  // installed.
  ExtensionTestMessageListener launcher_loaded("launcher loaded", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_on_install")));
  ASSERT_TRUE(launcher_loaded.WaitUntilSatisfied());

  // Code below assumes that the test starts with a single browser window
  // hosting one tab.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Load an app with app.launch.container = "tab".
  std::string app_id;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id);
  ASSERT_FALSE(HasFatalFailure());

  // Check that the app opened in a new tab of the existing browser.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Unload the extension.
  UninstallExtension(app_id);
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_FALSE(service->GetExtensionById(app_id, true));

  // Set a pref indicating that the user wants to launch in a window.
  service->extension_prefs()->SetLaunchType(
      app_id, extensions::ExtensionPrefs::LAUNCH_WINDOW);

  std::string app_id_new;
  LoadAndWaitForLaunch("management/launch_app_tab", &app_id_new);
  ASSERT_FALSE(HasFatalFailure());

  // If the ID changed, then the pref will not apply to the app.
  ASSERT_EQ(app_id, app_id_new);

#if defined(OS_MACOSX)
  // App windows are not yet implemented on mac os.  We should fall back
  // to a normal tab.
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
#else
  // Find the app's browser.  Opening in a new window will create
  // a new browser.
  ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));
  Browser* app_browser = FindOtherBrowser(browser());
  ASSERT_TRUE(app_browser->is_app());
  ASSERT_FALSE(app_browser->is_type_panel());
#endif
}
