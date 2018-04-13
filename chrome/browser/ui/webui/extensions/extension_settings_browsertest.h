// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/feature_switch.h"

namespace extensions {
class Extension;
class ScopedTestDialogAutoConfirm;
}

// C++ test fixture used by extension_settings_browsertest.js.
class ExtensionSettingsUIBrowserTest : public WebUIBrowserTest {
 public:
  ExtensionSettingsUIBrowserTest();
  ~ExtensionSettingsUIBrowserTest() override;

 protected:
  void InstallGoodExtension();

  void InstallErrorsExtension();

  void InstallSharedModule();

  void InstallPackagedApp();

  void InstallHostedApp();

  void InstallPlatformApp();

  // Installs chrome/test/data/extensions/options_page_in_view extension
  // and returns it back to the caller.  Can return null upon failure.
  const extensions::Extension* InstallExtensionWithInPageOptions();

  void AddManagedPolicyProvider();

  void SetAutoConfirmUninstall();

  // Enables the error console so errors are displayed in the extensions page.
  void EnableErrorConsole();

  // Sets the DevMode status for the current profile.
  void SetDevModeEnabled(bool enabled);

  // Shrinks the web contents view in order to ensure vertical overflow.
  void ShrinkWebContentsView();

  const base::FilePath& test_data_dir() { return test_data_dir_; }

 private:
  const extensions::Extension* InstallExtension(const base::FilePath& path);

  // Used to simulate managed extensions (by being registered as a provider).
  extensions::TestManagementPolicyProvider policy_provider_;

  base::FilePath test_data_dir_;

  // Disable extension content verification.
  extensions::ScopedIgnoreContentVerifierForTest ignore_content_verification_;

  // Disable extension install verification.
  extensions::ScopedInstallVerifierBypassForTest ignore_install_verification_;

  // Used to enable the error console.
  std::unique_ptr<extensions::FeatureSwitch::ScopedOverride>
      error_console_override_;

  std::unique_ptr<extensions::ScopedTestDialogAutoConfirm>
      uninstall_auto_confirm_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsUIBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_
