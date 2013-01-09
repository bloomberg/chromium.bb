// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/features/feature.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/quota/quota_manager.h"

using ::testing::_;
using ::testing::Return;
using sync_file_system::MockRemoteFileSyncService;
using sync_file_system::RemoteFileSyncService;
using sync_file_system::SyncFileSystemServiceFactory;

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

ACTION_P(NotifyOkStateAndCallback, mock_remote_service) {
  mock_remote_service->NotifyRemoteServiceStateUpdated(
      sync_file_system::REMOTE_SERVICE_OK, "Test event description.");
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg1, fileapi::SYNC_STATUS_OK));
}

ACTION_P2(UpdateRemoteChangeQueue, origin, mock_remote_service) {
  *origin = arg0;
  mock_remote_service->NotifyRemoteChangeQueueUpdated(1);
}

ACTION_P2(ReturnWithFakeFileAddedStatus, origin, mock_remote_service) {
  fileapi::FileSystemURL mock_url(*origin,
                                  fileapi::kFileSystemTypeTest,
                                  FilePath(FILE_PATH_LITERAL("foo")));
  mock_remote_service->NotifyRemoteChangeQueueUpdated(0);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(arg1,
                            fileapi::SYNC_STATUS_OK,
                            mock_url,
                            fileapi::SYNC_OPERATION_ADDED));
}

}  // namespace

// TODO(calvinlo): Add Chrome OS support for syncable file system
// (http://crbug.com/160693)
#if !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, DeleteFileSystem) {
  ASSERT_TRUE(RunPlatformAppTest("sync_file_system/delete_file_system"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, GetFileSyncStatus) {
  EXPECT_CALL(*mock_remote_service(), IsConflicting(_)).WillOnce(Return(true));
  ASSERT_TRUE(RunPlatformAppTest("sync_file_system/get_file_sync_status"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, GetUsageAndQuota) {
  ASSERT_TRUE(RunExtensionTest("sync_file_system/get_usage_and_quota"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, OnFileSynced) {
  // Mock a pending remote change to be synced.
  GURL origin;
  EXPECT_CALL(*mock_remote_service(), RegisterOriginForTrackingChanges(_, _))
      .WillOnce(UpdateRemoteChangeQueue(&origin, mock_remote_service()));
  EXPECT_CALL(*mock_remote_service(), ProcessRemoteChange(_, _))
      .WillOnce(ReturnWithFakeFileAddedStatus(&origin,
                                               mock_remote_service()));
  ASSERT_TRUE(RunPlatformAppTest("sync_file_system/on_file_synced"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(SyncFileSystemApiTest, OnSyncStateChanged) {
  EXPECT_CALL(*mock_remote_service(), RegisterOriginForTrackingChanges(_, _))
      .WillOnce(NotifyOkStateAndCallback(mock_remote_service()));
  ASSERT_TRUE(RunPlatformAppTest("sync_file_system/on_sync_state_changed"))
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

}  // namespace chrome

