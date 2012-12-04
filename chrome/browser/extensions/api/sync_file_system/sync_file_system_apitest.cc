// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"
#include "webkit/quota/quota_manager.h"

using sync_file_system::MockRemoteFileSyncService;
using sync_file_system::RemoteFileSyncService;
using sync_file_system::SyncFileSystemServiceFactory;
using ::testing::_;

namespace chrome {

namespace {

class SyncFileSystemApiTest : public ExtensionApiTest {
 public:
  // Override the current channel to "trunk" as syncFileSystem is currently
  // available only on trunk channel.
  SyncFileSystemApiTest()
      : current_channel_(VersionInfo::CHANNEL_UNKNOWN) {
  }

  void SetUpInProcessBrowserTestFixture() OVERRIDE {
    mock_remote_service_ = new ::testing::NiceMock<MockRemoteFileSyncService>;
    SyncFileSystemServiceFactory::GetInstance()->set_mock_remote_file_service(
        scoped_ptr<RemoteFileSyncService>(mock_remote_service_));

    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    // TODO(calvinlo): Update test code after default quota is made const
    // (http://crbug.com/155488).
    real_default_quota_ = quota::QuotaManager::kSyncableStorageDefaultHostQuota;
    quota::QuotaManager::kSyncableStorageDefaultHostQuota = 123456789;
  }

  void TearDownInProcessBrowserTestFixture() {
    quota::QuotaManager::kSyncableStorageDefaultHostQuota = real_default_quota_;
  }

  ::testing::NiceMock<MockRemoteFileSyncService>* mock_remote_service() {
    return mock_remote_service_;
  }

 private:
  extensions::Feature::ScopedCurrentChannel current_channel_;
  ::testing::NiceMock<MockRemoteFileSyncService>* mock_remote_service_;
  int64 real_default_quota_;
};

}  // namespace

// TODO(calvinlo): Add Chrome OS support for syncable file system
#if !defined(OS_CHROMEOS)

// TODO(calvinlo): Add Chrome OS support for syncable file system
// (http://crbug.com/160693)
#if !defined(OS_CHROMEOS)

// TODO(benwells): Re-enable this test.
IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, DISABLED_DeleteFileSystem) {
  ASSERT_TRUE(RunPlatformAppTest("sync_file_system/delete_file_system"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, GetUsageAndQuota) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_usage_and_quota"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, RequestFileSystem) {
  EXPECT_CALL(*mock_remote_service(),
              RegisterOriginForTrackingChanges(_, _)).Times(1);
  ASSERT_TRUE(RunPlatformAppTest("sync_file_system/request_file_system"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, WriteFileThenGetUsage) {
  ASSERT_TRUE(RunPlatformAppTest("sync_file_system/write_file_then_get_usage"))
      << message_;
}

#endif  // !defined(OS_CHROMEOS)

#endif  // !defined(OS_CHROMEOS)

}  // namespace chrome

