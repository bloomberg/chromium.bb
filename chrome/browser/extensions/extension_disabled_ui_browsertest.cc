// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/common/extensions/extension.h"

class ExtensionDisabledGlobalErrorTest : public ExtensionBrowserTest {
 protected:
  void SetUpOnMainThread() {
    service_ = browser()->profile()->GetExtensionService();
  }

  // Returns the ExtensionDisabledGlobalError, if present.
  // Caution: currently only supports one error at a time.
  GlobalError* GetExtensionDisabledGlobalError() {
    return GlobalErrorServiceFactory::GetForProfile(browser()->profile())->
        GetGlobalErrorByMenuItemCommandID(IDC_EXTENSION_DISABLED_FIRST);
  }

  // Helper function to install an extension and upgrade it to a version
  // requiring additional permissions. Returns the new disabled Extension.
  const Extension* InstallAndUpdateIncreasingPermissionsExtension() {
    size_t size_before = service_->extensions()->size();

    // Install the initial version, which should happen just fine.
    const Extension* extension = InstallExtension(
        test_data_dir_.AppendASCII("permissions-low-v1.crx"), 1);
    if (!extension)
      return NULL;
    if (service_->extensions()->size() != size_before + 1)
      return NULL;

    // Upgrade to a version that wants more permissions. We should disable the
    // extension and prompt the user to reenable.
    if (UpdateExtension(
            extension->id(),
            test_data_dir_.AppendASCII("permissions-high-v2.crx"), -1))
      return NULL;
    EXPECT_EQ(size_before, service_->extensions()->size());
    if (service_->disabled_extensions()->size() != 1u)
      return NULL;

    return *service_->disabled_extensions()->begin();
  }

  ExtensionService* service_;
};

// Tests the process of updating an extension to one that requires higher
// permissions, and accepting the permissions.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, AcceptPermissions) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());
  const size_t size_before = service_->extensions()->size();

  service_->GrantPermissionsAndEnableExtension(extension);
  EXPECT_EQ(size_before + 1, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
}

// Tests uninstalling an extension that was disabled due to higher permissions.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, Uninstall) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());
  const size_t size_before = service_->extensions()->size();

  UninstallExtension(extension->id());
  EXPECT_EQ(size_before, service_->extensions()->size());
  EXPECT_EQ(0u, service_->disabled_extensions()->size());
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
}
