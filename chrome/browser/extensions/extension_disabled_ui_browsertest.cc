// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/test/net/url_request_prepackaged_interceptor.h"
#include "net/url_request/url_fetcher.h"

using extensions::Extension;

class ExtensionDisabledGlobalErrorTest : public ExtensionBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryUpdateURL,
                                    "http://localhost/autoupdate/updates.xml");
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    service_ = browser()->profile()->GetExtensionService();
    base::FilePath pem_path = test_data_dir_.
        AppendASCII("permissions_increase").AppendASCII("permissions.pem");
    path_v1_ = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v1"),
        scoped_temp_dir_.path().AppendASCII("permissions1.crx"),
        pem_path,
        base::FilePath());
    path_v2_ = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v2"),
        scoped_temp_dir_.path().AppendASCII("permissions2.crx"),
        pem_path,
        base::FilePath());
    path_v3_ = PackExtensionWithOptions(
        test_data_dir_.AppendASCII("permissions_increase").AppendASCII("v3"),
        scoped_temp_dir_.path().AppendASCII("permissions3.crx"),
        pem_path,
        base::FilePath());
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
      const base::FilePath& crx_path,
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
  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath path_v1_;
  base::FilePath path_v2_;
  base::FilePath path_v3_;
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
  service_->extension_prefs()->ClearDisableReasons(extension->id());
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
  service_->extension_prefs()->ClearDisableReasons(extension->id());
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

// Test that an error appears if the extension gets disabled because a
// version with higher permissions was installed by sync.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest,
                       HigherPermissionsFromSync) {
  // Get data for extension v2 (disabled) into sync.
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  std::string extension_id = extension->id();
  // service_->GrantPermissionsAndEnableExtension(extension, false);
  extensions::ExtensionSyncData sync_data =
      service_->GetExtensionSyncData(*extension);
  UninstallExtension(extension_id);
  extension = NULL;

  // Install extension v1.
  InstallIncreasingPermissionExtensionV1();

  // Note: This interceptor gets requests on the IO thread.
  content::URLRequestPrepackagedInterceptor interceptor;
  net::URLFetcher::SetEnableInterceptionForTests(true);
  interceptor.SetResponseIgnoreQuery(
      GURL("http://localhost/autoupdate/updates.xml"),
      test_data_dir_.AppendASCII("permissions_increase")
                    .AppendASCII("updates.xml"));
  interceptor.SetResponseIgnoreQuery(
      GURL("http://localhost/autoupdate/v2.crx"),
      scoped_temp_dir_.path().AppendASCII("permissions2.crx"));

  extensions::ExtensionUpdater::CheckParams params;
  params.check_blacklist = false;
  service_->updater()->set_default_check_params(params);

  // Sync is replacing an older version, so it pends.
  EXPECT_FALSE(service_->ProcessExtensionSyncData(sync_data));

  WaitForExtensionInstall();

  extension = service_->GetExtensionById(extension_id, true);
  ASSERT_TRUE(extension);
  EXPECT_EQ("2", extension->VersionString());
  EXPECT_EQ(1u, service_->disabled_extensions()->size());
  EXPECT_EQ(Extension::DISABLE_PERMISSIONS_INCREASE,
            service_->extension_prefs()->GetDisableReasons(extension_id));
  EXPECT_TRUE(GetExtensionDisabledGlobalError());
}
