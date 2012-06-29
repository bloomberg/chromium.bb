// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/extensions/extension.h"

using extensions::Extension;

class ExtensionDisabledGlobalErrorTest : public ExtensionBrowserTest {
 protected:
  void SetUpOnMainThread() {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    service_ = browser()->profile()->GetExtensionService();
    FilePath pem_path = test_data_dir_.
        AppendASCII("permissions_increase").AppendASCII("permissions.pem");
    path_v1_ = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v1"),
        scoped_temp_dir_.path().AppendASCII("permissions1.crx"),
        pem_path,
        FilePath());
    path_v2_ = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v2"),
        scoped_temp_dir_.path().AppendASCII("permissions2.crx"),
        pem_path,
        FilePath());
    path_v3_ = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v3"),
        scoped_temp_dir_.path().AppendASCII("permissions3.crx"),
        pem_path,
        FilePath());
  }

  // Returns the ExtensionDisabledGlobalError, if present.
  // Caution: currently only supports one error at a time.
  GlobalError* GetExtensionDisabledGlobalError() {
    return GlobalErrorServiceFactory::GetForProfile(browser()->profile())->
        GetGlobalErrorByMenuItemCommandID(IDC_EXTENSION_DISABLED_FIRST);
  }

  // Install the initial version, which should happen just fine.
  const Extension* InstallIncreasingPermissionExtensionV1() {
    size_t size_before = service_->extensions()->size();
    const Extension* extension = InstallExtension(path_v1_, 1);
    if (!extension)
      return NULL;
    if (service_->extensions()->size() != size_before + 1)
      return NULL;
    return extension;
  }

  // Upgrade to a version that wants more permissions. We should disable the
  // extension and prompt the user to reenable.
  const Extension* UpdateIncreasingPermissionExtension(
      const Extension* extension,
      const FilePath& crx_path,
      int expected_change) {
    size_t size_before = service_->extensions()->size();
    if (UpdateExtension(extension->id(), crx_path, expected_change))
      return NULL;
    EXPECT_EQ(size_before + expected_change, service_->extensions()->size());
    if (service_->disabled_extensions()->size() != 1u)
      return NULL;

    return *service_->disabled_extensions()->begin();
  }

  // Helper function to install an extension and upgrade it to a version
  // requiring additional permissions. Returns the new disabled Extension.
  const Extension* InstallAndUpdateIncreasingPermissionsExtension() {
    const Extension* extension = InstallIncreasingPermissionExtensionV1();
    extension = UpdateIncreasingPermissionExtension(extension, path_v2_, -1);
    return extension;
  }

  ExtensionService* service_;
  ScopedTempDir scoped_temp_dir_;
  FilePath path_v1_;
  FilePath path_v2_;
  FilePath path_v3_;
};

// Tests the process of updating an extension to one that requires higher
// permissions, and accepting the permissions.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, AcceptPermissions) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());
  const size_t size_before = service_->extensions()->size();

  service_->GrantPermissionsAndEnableExtension(extension, false);
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

// Tests that no error appears if the user disabled the extension.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, UserDisabled) {
  const Extension* extension = InstallIncreasingPermissionExtensionV1();
  DisableExtension(extension->id());
  extension = UpdateIncreasingPermissionExtension(extension, path_v2_, 0);
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
}

// Test that no error appears if the disable reason is unknown
// (but probably was by the user).
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest,
                       UnknownReasonSamePermissions) {
  const Extension* extension = InstallIncreasingPermissionExtensionV1();
  DisableExtension(extension->id());
  // Clear disable reason to simulate legacy disables.
  service_->extension_prefs()->RemoveDisableReason(extension->id());
  // Upgrade to version 2. Infer from version 1 having the same permissions
  // granted by the user that it was disabled by the user.
  extension = UpdateIncreasingPermissionExtension(extension, path_v2_, 0);
  ASSERT_TRUE(extension);
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
}

// Test that an error appears if the disable reason is unknown
// (but probably was for increased permissions).
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest,
                       UnknownReasonHigherPermissions) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  // Clear disable reason to simulate legacy disables.
  service_->extension_prefs()->RemoveDisableReason(extension->id());
  // We now have version 2 but only accepted permissions for version 1.
  GlobalError* error = GetExtensionDisabledGlobalError();
  ASSERT_TRUE(error);
  // Also, remove the upgrade error for version 2.
  GlobalErrorServiceFactory::GetForProfile(browser()->profile())->
      RemoveGlobalError(error);
  delete error;
  // Upgrade to version 3, with even higher permissions. Infer from
  // version 2 having higher-than-granted permissions that it was disabled
  // for permissions increase.
  extension = UpdateIncreasingPermissionExtension(extension, path_v3_, 0);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());
}
