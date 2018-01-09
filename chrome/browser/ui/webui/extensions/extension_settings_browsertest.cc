// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_browsertest.h"

#include <string>

#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_system.h"

using extensions::Extension;
using extensions::TestManagementPolicyProvider;

ExtensionSettingsUIBrowserTest::ExtensionSettingsUIBrowserTest()
    : policy_provider_(TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS |
                       TestManagementPolicyProvider::MUST_REMAIN_ENABLED |
                       TestManagementPolicyProvider::MUST_REMAIN_INSTALLED) {
  CHECK(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");
}

ExtensionSettingsUIBrowserTest::~ExtensionSettingsUIBrowserTest() {}

void ExtensionSettingsUIBrowserTest::InstallGoodExtension() {
  EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII("good.crx")));
}

void ExtensionSettingsUIBrowserTest::InstallErrorsExtension() {
  EXPECT_TRUE(
      InstallExtension(test_data_dir_.AppendASCII("error_console")
                           .AppendASCII("runtime_and_manifest_errors")));
}

void ExtensionSettingsUIBrowserTest::InstallSharedModule() {
  base::FilePath shared_module_path =
      test_data_dir_.AppendASCII("api_test").AppendASCII("shared_module");
  EXPECT_TRUE(InstallExtension(shared_module_path.AppendASCII("shared")));
  EXPECT_TRUE(InstallExtension(shared_module_path.AppendASCII("import_pass")));
}

void ExtensionSettingsUIBrowserTest::InstallPackagedApp() {
  EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII("packaged_app")));
}

void ExtensionSettingsUIBrowserTest::InstallHostedApp() {
  EXPECT_TRUE(InstallExtension(test_data_dir_.AppendASCII("hosted_app")));
}

void ExtensionSettingsUIBrowserTest::InstallPlatformApp() {
  EXPECT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("platform_apps").AppendASCII("minimal")));
}

const extensions::Extension*
ExtensionSettingsUIBrowserTest::InstallExtensionWithInPageOptions() {
  const extensions::Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("options_page_in_view"));
  EXPECT_TRUE(extension);
  return extension;
}

void ExtensionSettingsUIBrowserTest::AddManagedPolicyProvider() {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(browser()->profile());
  extension_system->management_policy()->RegisterProvider(&policy_provider_);
}

void ExtensionSettingsUIBrowserTest::SetAutoConfirmUninstall() {
  uninstall_auto_confirm_.reset(new extensions::ScopedTestDialogAutoConfirm(
      extensions::ScopedTestDialogAutoConfirm::ACCEPT));
}

void ExtensionSettingsUIBrowserTest::EnableErrorConsole() {
  error_console_override_.reset(new extensions::FeatureSwitch::ScopedOverride(
      extensions::FeatureSwitch::error_console(), true));
}

void ExtensionSettingsUIBrowserTest::SetDevModeEnabled(bool enabled) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kExtensionsUIDeveloperMode, enabled);
}

void ExtensionSettingsUIBrowserTest::ShrinkWebContentsView() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);
  ResizeWebContents(web_contents, gfx::Rect(0, 0, 400, 400));
}

const Extension* ExtensionSettingsUIBrowserTest::InstallExtension(
    const base::FilePath& path) {
  extensions::ChromeTestExtensionLoader loader(browser()->profile());
  loader.set_ignore_manifest_warnings(true);
  return loader.LoadExtension(path).get();
}

// Tests that viewing a source of the options page works fine.
// This is a regression test for https://crbug.com/796080.
IN_PROC_BROWSER_TEST_F(ExtensionSettingsUIBrowserTest, ViewSource) {
  // Navigate to an in-page (guest-view-based) extension options page
  // and grab the WebContents hosting the options page.
  const extensions::Extension* extension = InstallExtensionWithInPageOptions();
  GURL options_url("chrome://extensions/?options=" + extension->id());
  content::WebContents* options_contents = nullptr;
  {
    content::WebContentsAddedObserver options_contents_added_observer;
    ui_test_utils::NavigateToURL(browser(), options_url);
    options_contents = options_contents_added_observer.GetWebContents();
  }
  ASSERT_TRUE(options_contents);
  content::WaitForLoadStop(options_contents);
  EXPECT_EQ(extension->GetResourceURL("options.html"),
            options_contents->GetLastCommittedURL());

  // Open the view-source of the options page.
  int old_tabs_count = browser()->tab_strip_model()->count();
  content::WebContentsAddedObserver view_source_contents_added_observer;
  options_contents->GetMainFrame()->ViewSource();
  content::WebContents* view_source_contents =
      view_source_contents_added_observer.GetWebContents();
  ASSERT_TRUE(view_source_contents);
  content::WaitForLoadStop(view_source_contents);

  // Verify that the view-source is present in the tab-strip.
  int new_tabs_count = browser()->tab_strip_model()->count();
  EXPECT_EQ(new_tabs_count, old_tabs_count + 1);
  EXPECT_EQ(view_source_contents,
            browser()->tab_strip_model()->GetActiveWebContents());

  // Verify the contents of the view-source tab.
  std::string actual_source_text;
  std::string view_source_extraction_script = R"(
      output = "";
      document.querySelectorAll(".line-content").forEach(function(elem) {
          output += elem.innerText;
      });
      domAutomationController.send(output); )";
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      view_source_contents, view_source_extraction_script,
      &actual_source_text));
  base::FilePath source_path =
      test_data_dir().AppendASCII("options_page_in_view/options.html");
  std::string expected_source_text;
  {
    base::ScopedAllowBlockingForTesting scoped_allow_blocking;
    EXPECT_TRUE(base::ReadFileToString(source_path, &expected_source_text));
  }
  EXPECT_TRUE(
      base::RemoveChars(expected_source_text, "\n", &expected_source_text));
  EXPECT_EQ(expected_source_text, actual_source_text);
}
