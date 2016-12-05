// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extension_settings_browsertest.h"

#include "base/path_service.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/chrome_paths.h"
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
