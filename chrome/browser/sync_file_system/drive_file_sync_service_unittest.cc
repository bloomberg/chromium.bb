// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include "base/message_loop.h"
#include "chrome/browser/sync_file_system/drive/fake_api_util.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_util.h"

namespace sync_file_system {

namespace {

const char kSyncRootResourceId[] = "folder:sync_root_resource_id";

void DidInitialize(bool* done, SyncStatusCode status, bool created) {
  EXPECT_EQ(SYNC_STATUS_OK, status);
  *done = true;
}

void ExpectEqStatus(bool* done,
                    SyncStatusCode expected,
                    SyncStatusCode actual) {
  EXPECT_FALSE(*done);
  *done = true;
  EXPECT_EQ(expected, actual);
}

void ExpectOkStatus(SyncStatusCode status) {
  EXPECT_EQ(SYNC_STATUS_OK, status);
}

}  // namespace

class DriveFileSyncServiceTest : public testing::Test {
 public:
  DriveFileSyncServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        fake_api_util_(NULL),
        metadata_store_(NULL),
        sync_service_(NULL) {}

  virtual void SetUp() OVERRIDE {
    RegisterSyncableFileSystem();
    fake_api_util_ = new drive::FakeAPIUtil;

    ASSERT_TRUE(scoped_base_dir_.CreateUniqueTempDir());
    base_dir_ = scoped_base_dir_.path();
    metadata_store_ = new DriveMetadataStore(
        base_dir_,
        base::MessageLoopProxy::current());
    bool done = false;
    metadata_store_->Initialize(base::Bind(&DidInitialize, &done));
    message_loop_.RunUntilIdle();
    metadata_store_->SetSyncRootDirectory(kSyncRootResourceId);
    EXPECT_TRUE(done);

    sync_service_ = DriveFileSyncService::CreateForTesting(
        &profile_,
        base_dir_,
        scoped_ptr<drive::APIUtilInterface>(fake_api_util_),
        scoped_ptr<DriveMetadataStore>(metadata_store_)).Pass();
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() OVERRIDE {
    metadata_store_ = NULL;
    fake_api_util_ = NULL;
    sync_service_.reset();
    message_loop_.RunUntilIdle();

    base_dir_ = base::FilePath();
    RevokeSyncableFileSystem();
  }

  virtual ~DriveFileSyncServiceTest() {
  }

 protected:
  base::MessageLoop* message_loop() { return &message_loop_; }
  drive::FakeAPIUtil* fake_api_util() { return fake_api_util_; }
  DriveMetadataStore* metadata_store() { return metadata_store_; }
  DriveFileSyncService* sync_service() { return sync_service_.get(); }
  std::map<GURL, std::string>* pending_batch_sync_origins() {
    return &(sync_service()->pending_batch_sync_origins_);
  }

  bool VerifyOriginStatusCount(size_t expected_pending,
                               size_t expected_enabled,
                               size_t expected_disabled) {
    size_t actual_pending = pending_batch_sync_origins()->size();
    size_t actual_enabled = metadata_store()->incremental_sync_origins().size();
    size_t actual_disabled = metadata_store()->disabled_origins().size();

    // Prints which counts don't match up if any.
    EXPECT_EQ(expected_pending, actual_pending);
    EXPECT_EQ(expected_enabled, actual_enabled);
    EXPECT_EQ(expected_disabled, actual_disabled);

    // If any count doesn't match, the original line number can be printed by
    // simply adding ASSERT_TRUE on the call to this function.
    if (expected_pending == actual_pending &&
        expected_enabled == actual_enabled &&
        expected_disabled == actual_disabled)
      return true;

    return false;
  }

 private:
  base::ScopedTempDir scoped_base_dir_;
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  TestingProfile profile_;
  base::FilePath base_dir_;

  drive::FakeAPIUtil* fake_api_util_;   // Owned by |sync_service_|.
  DriveMetadataStore* metadata_store_;  // Owned by |sync_service_|.

  scoped_ptr<DriveFileSyncService> sync_service_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceTest);
};

TEST_F(DriveFileSyncServiceTest, UninstallOrigin) {
  // Add fake app origin directory using fake drive_sync_client.
  std::string origin_dir_resource_id = "uninstalledappresourceid";
  fake_api_util()->PushRemoteChange("parent_id",
                                    "parent_title",
                                    "uninstall_me_folder",
                                    origin_dir_resource_id,
                                    "resource_md5",
                                    SYNC_FILE_TYPE_FILE,
                                    false);

  // Add meta_data entry so GURL->resourse_id mapping is there.
  const GURL origin_gurl("chrome-extension://uninstallme");
  metadata_store()->AddIncrementalSyncOrigin(origin_gurl,
                                             origin_dir_resource_id);

  // Delete the origin directory.
  bool done = false;
  sync_service()->UninstallOrigin(
      origin_gurl,
      base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  // Assert the App's origin folder was marked as deleted.
  EXPECT_TRUE(fake_api_util()->remote_resources().find(
      origin_dir_resource_id)->second.deleted);
}

TEST_F(DriveFileSyncServiceTest, DisableOriginForTrackingChangesPendingOrigin) {
  // Disable a pending origin after DriveFileSystemService has already started.
  const GURL origin("chrome-extension://app");
  std::string origin_resource_id = "app_resource_id";
  pending_batch_sync_origins()->insert(std::make_pair(origin,
                                                      origin_resource_id));
  ASSERT_TRUE(VerifyOriginStatusCount(1u, 0u, 0u));

  // Pending origins that are disabled are dropped and do not go to disabled.
  sync_service()->DisableOriginForTrackingChanges(origin,
                                                  base::Bind(&ExpectOkStatus));
  message_loop()->RunUntilIdle();
  ASSERT_TRUE(VerifyOriginStatusCount(0u, 0u, 0u));
}

TEST_F(DriveFileSyncServiceTest,
       DisableOriginForTrackingChangesIncrementalOrigin) {
  // Disable a pending origin after DriveFileSystemService has already started.
  const GURL origin("chrome-extension://app");
  std::string origin_resource_id = "app_resource_id";
  metadata_store()->AddIncrementalSyncOrigin(origin, origin_resource_id);
  ASSERT_TRUE(VerifyOriginStatusCount(0u, 1u, 0u));

  sync_service()->DisableOriginForTrackingChanges(origin,
                                                  base::Bind(&ExpectOkStatus));
  message_loop()->RunUntilIdle();
  ASSERT_TRUE(VerifyOriginStatusCount(0u, 0u, 1u));
}

TEST_F(DriveFileSyncServiceTest, EnableOriginForTrackingChanges) {
  const GURL origin("chrome-extension://app");
  std::string origin_resource_id = "app_resource_id";
  metadata_store()->AddIncrementalSyncOrigin(origin, origin_resource_id);
  metadata_store()->DisableOrigin(origin, base::Bind(&ExpectOkStatus));
  ASSERT_TRUE(VerifyOriginStatusCount(0u, 0u, 1u));

  // Re-enable the previously disabled origin. It initially goes to pending
  // status and then to enabled (incremental) again when NotifyTasksDone() in
  // DriveFileSyncTaskManager invokes MaybeStartFetchChanges() and pending
  // origins > 0.
  sync_service()->EnableOriginForTrackingChanges(origin,
                                                 base::Bind(&ExpectOkStatus));
  message_loop()->RunUntilIdle();
  ASSERT_TRUE(VerifyOriginStatusCount(0u, 1u, 0u));
}

}  // namespace sync_file_system
