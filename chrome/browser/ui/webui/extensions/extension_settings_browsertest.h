// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_

#include "base/macros.h"
#include "chrome/browser/extensions/chrome_extension_test_notification_observer.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/features/feature_channel.h"

class Profile;

// C++ test fixture used by extension_settings_browsertest.js.
class ExtensionSettingsUIBrowserTest : public WebUIBrowserTest {
 public:
  ExtensionSettingsUIBrowserTest();
  ~ExtensionSettingsUIBrowserTest() override;

 protected:
  // Get the profile to use.
  Profile* GetProfile();

  const std::string& last_loaded_extension_id() {
    return observer_->last_loaded_extension_id();
  }

  void SetUpOnMainThread() override;

  void InstallGoodExtension();

  void InstallErrorsExtension();

  void InstallSharedModule();

  void InstallPackagedApp();

  void InstallHostedApp();

  void InstallPlatformApp();

  void AddManagedPolicyProvider();

  void SetAutoConfirmUninstall();

  // Enables the error console so errors are displayed in the extensions page.
  void EnableErrorConsole();

  // Shrinks the web contents view in order to ensure vertical overflow.
  void ShrinkWebContentsView();

 private:
  bool WaitForExtensionViewsToLoad();
  const extensions::Extension* InstallUnpackedExtension(
      const base::FilePath& path);
  const extensions::Extension* InstallExtension(const base::FilePath& path);

  std::unique_ptr<extensions::ChromeExtensionTestNotificationObserver>
      observer_;

  // The default profile to be used.
  Profile* profile_;

  // Used to simulate managed extensions (by being registered as a provider).
  extensions::TestManagementPolicyProvider policy_provider_;

  base::FilePath test_data_dir_;

  // Used to enable the error console.
  std::unique_ptr<extensions::FeatureSwitch::ScopedOverride>
      error_console_override_;

  std::unique_ptr<extensions::ScopedTestDialogAutoConfirm>
      uninstall_auto_confirm_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsUIBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_
