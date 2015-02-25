// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_

#include "chrome/browser/extensions/extension_test_notification_observer.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/extension.h"

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

  void AddManagedPolicyProvider();

 private:
  bool WaitForExtensionViewsToLoad();
  const extensions::Extension* InstallUnpackedExtension(
      const base::FilePath& path, const char* id);
  const extensions::Extension* InstallExtension(const base::FilePath& path);

  scoped_ptr<ExtensionTestNotificationObserver> observer_;

  // The default profile to be used.
  Profile* profile_;

  // Used to simulate managed extensions (by being registered as a provider).
  extensions::TestManagementPolicyProvider policy_provider_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsUIBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_SETTINGS_BROWSERTEST_H_
