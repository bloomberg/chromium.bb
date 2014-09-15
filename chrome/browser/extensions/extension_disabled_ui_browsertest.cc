// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_fetcher.h"
#include "sync/protocol/extension_specifics.pb.h"
#include "sync/protocol/sync.pb.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionRegistry;
using extensions::ExtensionPrefs;

class ExtensionDisabledGlobalErrorTest : public ExtensionBrowserTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kAppsGalleryUpdateURL,
                                    "http://localhost/autoupdate/updates.xml");
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    service_ = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    registry_ = ExtensionRegistry::Get(browser()->profile());
    const base::FilePath test_dir =
        test_data_dir_.AppendASCII("permissions_increase");
    const base::FilePath pem_path = test_dir.AppendASCII("permissions.pem");
    path_v1_ = PackExtensionWithOptions(
        test_dir.AppendASCII("v1"),
        scoped_temp_dir_.path().AppendASCII("permissions1.crx"),
        pem_path,
        base::FilePath());
    path_v2_ = PackExtensionWithOptions(
        test_dir.AppendASCII("v2"),
        scoped_temp_dir_.path().AppendASCII("permissions2.crx"),
        pem_path,
        base::FilePath());
    path_v3_ = PackExtensionWithOptions(
        test_dir.AppendASCII("v3"),
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
    size_t size_before = registry_->enabled_extensions().size();
    const Extension* extension = InstallExtension(path_v1_, 1);
    if (!extension)
      return NULL;
    if (registry_->enabled_extensions().size() != size_before + 1)
      return NULL;
    return extension;
  }

  // Upgrade to a version that wants more permissions. We should disable the
  // extension and prompt the user to reenable.
  const Extension* UpdateIncreasingPermissionExtension(
      const Extension* extension,
      const base::FilePath& crx_path,
      int expected_change) {
    size_t size_before = registry_->enabled_extensions().size();
    if (UpdateExtension(extension->id(), crx_path, expected_change))
      return NULL;
    content::RunAllBlockingPoolTasksUntilIdle();
    EXPECT_EQ(size_before + expected_change,
              registry_->enabled_extensions().size());
    if (registry_->disabled_extensions().size() != 1u)
      return NULL;

    return registry_->disabled_extensions().begin()->get();
  }

  // Helper function to install an extension and upgrade it to a version
  // requiring additional permissions. Returns the new disabled Extension.
  const Extension* InstallAndUpdateIncreasingPermissionsExtension() {
    const Extension* extension = InstallIncreasingPermissionExtensionV1();
    extension = UpdateIncreasingPermissionExtension(extension, path_v2_, -1);
    return extension;
  }

  ExtensionService* service_;
  ExtensionRegistry* registry_;
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
  const size_t size_before = registry_->enabled_extensions().size();

  service_->GrantPermissionsAndEnableExtension(extension);
  EXPECT_EQ(size_before + 1, registry_->enabled_extensions().size());
  EXPECT_EQ(0u, registry_->disabled_extensions().size());
  ASSERT_FALSE(GetExtensionDisabledGlobalError());
}

// Tests uninstalling an extension that was disabled due to higher permissions.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, Uninstall) {
  const Extension* extension = InstallAndUpdateIncreasingPermissionsExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(GetExtensionDisabledGlobalError());
  const size_t size_before = registry_->enabled_extensions().size();

  UninstallExtension(extension->id());
  EXPECT_EQ(size_before, registry_->enabled_extensions().size());
  EXPECT_EQ(0u, registry_->disabled_extensions().size());
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
  ExtensionPrefs::Get(browser()->profile())
      ->ClearDisableReasons(extension->id());
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
  ExtensionPrefs::Get(service_->profile())
      ->ClearDisableReasons(extension->id());
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
  ExtensionSyncService* sync_service = ExtensionSyncService::Get(
      browser()->profile());
  extensions::ExtensionSyncData sync_data =
      sync_service->GetExtensionSyncData(*extension);
  UninstallExtension(extension_id);
  extension = NULL;

  // Install extension v1.
  InstallIncreasingPermissionExtensionV1();

  // Note: This interceptor gets requests on the IO thread.
  net::LocalHostTestURLRequestInterceptor interceptor(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  net::URLFetcher::SetEnableInterceptionForTests(true);
  interceptor.SetResponseIgnoreQuery(
      GURL("http://localhost/autoupdate/updates.xml"),
      test_data_dir_.AppendASCII("permissions_increase")
                    .AppendASCII("updates.xml"));
  interceptor.SetResponseIgnoreQuery(
      GURL("http://localhost/autoupdate/v2.crx"),
      scoped_temp_dir_.path().AppendASCII("permissions2.crx"));

  extensions::ExtensionUpdater::CheckParams params;
  service_->updater()->set_default_check_params(params);

  // Sync is replacing an older version, so it pends.
  EXPECT_FALSE(sync_service->ProcessExtensionSyncData(sync_data));

  WaitForExtensionInstall();
  content::RunAllBlockingPoolTasksUntilIdle();

  extension = service_->GetExtensionById(extension_id, true);
  ASSERT_TRUE(extension);
  EXPECT_EQ("2", extension->VersionString());
  EXPECT_EQ(1u, registry_->disabled_extensions().size());
  EXPECT_EQ(Extension::DISABLE_PERMISSIONS_INCREASE,
            ExtensionPrefs::Get(service_->profile())
                ->GetDisableReasons(extension_id));
  EXPECT_TRUE(GetExtensionDisabledGlobalError());
}

// Test that an error appears if an extension gets installed server side.
IN_PROC_BROWSER_TEST_F(ExtensionDisabledGlobalErrorTest, RemoteInstall) {
  static const char* extension_id = "pgdpcfcocojkjfbgpiianjngphoopgmo";
  ExtensionSyncService* sync_service =
      ExtensionSyncService::Get(browser()->profile());

  // Note: This interceptor gets requests on the IO thread.
  net::LocalHostTestURLRequestInterceptor interceptor(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));
  net::URLFetcher::SetEnableInterceptionForTests(true);
  interceptor.SetResponseIgnoreQuery(
      GURL("http://localhost/autoupdate/updates.xml"),
      test_data_dir_.AppendASCII("permissions_increase")
          .AppendASCII("updates.xml"));
  interceptor.SetResponseIgnoreQuery(
      GURL("http://localhost/autoupdate/v2.crx"),
      scoped_temp_dir_.path().AppendASCII("permissions2.crx"));

  extensions::ExtensionUpdater::CheckParams params;
  service_->updater()->set_default_check_params(params);

  sync_pb::EntitySpecifics specifics;
  specifics.mutable_extension()->set_id(extension_id);
  specifics.mutable_extension()->set_enabled(false);
  specifics.mutable_extension()->set_remote_install(true);
  specifics.mutable_extension()->set_update_url(
      "http://localhost/autoupdate/updates.xml");
  specifics.mutable_extension()->set_version("2");
  syncer::SyncData sync_data =
      syncer::SyncData::CreateRemoteData(1234567,
                                         specifics,
                                         base::Time::Now(),
                                         syncer::AttachmentIdList(),
                                         syncer::AttachmentServiceProxy());
  // Sync is installing a new extension, so it pends.
  EXPECT_FALSE(sync_service->ProcessExtensionSyncData(
      extensions::ExtensionSyncData(sync_data)));

  WaitForExtensionInstall();
  content::RunAllBlockingPoolTasksUntilIdle();

  const Extension* extension = service_->GetExtensionById(extension_id, true);
  ASSERT_TRUE(extension);
  EXPECT_EQ("2", extension->VersionString());
  EXPECT_EQ(1u, registry_->disabled_extensions().size());
  EXPECT_EQ(Extension::DISABLE_REMOTE_INSTALL,
            ExtensionPrefs::Get(service_->profile())
                ->GetDisableReasons(extension_id));
  EXPECT_TRUE(GetExtensionDisabledGlobalError());
}
