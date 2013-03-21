// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_service.h"

#include "base/message_loop.h"
#include "chrome/browser/sync_file_system/drive_metadata_store.h"
#include "chrome/browser/sync_file_system/fake_drive_file_sync_client.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using extensions::Extension;

namespace sync_file_system {

namespace {

const char* kServiceName = DriveFileSyncService::kServiceName;
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

}  // namespace

class DriveFileSyncServiceTest : public testing::Test {
 public:
  DriveFileSyncServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_),
        fake_sync_client_(NULL),
        metadata_store_(NULL),
        sync_service_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(RegisterSyncableFileSystem(kServiceName));
    fake_sync_client_ = new FakeDriveFileSyncClient;

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
        scoped_ptr<DriveFileSyncClientInterface>(fake_sync_client_),
        scoped_ptr<DriveMetadataStore>(metadata_store_)).Pass();
  }

  virtual void TearDown() OVERRIDE {
    metadata_store_ = NULL;
    fake_sync_client_ = NULL;
    sync_service_.reset();
    message_loop_.RunUntilIdle();

    base_dir_ = base::FilePath();
    ASSERT_TRUE(RevokeSyncableFileSystem(kServiceName));
  }

  virtual ~DriveFileSyncServiceTest() {
  }

 protected:
  MessageLoop* message_loop() { return &message_loop_; }
  FakeDriveFileSyncClient* fake_sync_client() { return fake_sync_client_; }
  DriveMetadataStore* metadata_store() { return metadata_store_; }
  DriveFileSyncService* sync_service() { return sync_service_.get(); }

 private:
  base::ScopedTempDir scoped_base_dir_;
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  TestingProfile profile_;
  base::FilePath base_dir_;

  FakeDriveFileSyncClient* fake_sync_client_;  // Owned by |sync_service_|.
  DriveMetadataStore* metadata_store_;  // Owned by |sync_service_|.

  scoped_ptr<DriveFileSyncService> sync_service_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncServiceTest);
};

TEST_F(DriveFileSyncServiceTest, DeleteOriginDirectory) {
  // Add fake app origin directory using fake drive_sync_client.
  std::string origin_dir_resource_id = "uninstalledappresourceid";
  fake_sync_client()->PushRemoteChange(
      "parent_id", "parent_title",
      "uninstall_me_folder", origin_dir_resource_id, "resource_md5", false);

  // Add meta_data entry so GURL->resourse_id mapping is there.
  const GURL origin_gurl("chrome-extension://uninstallme");
  metadata_store()->AddBatchSyncOrigin(origin_gurl,
                                       origin_dir_resource_id);
  // Delete the origin directory.
  bool done = false;
  sync_service()->UninstallOrigin(
      origin_gurl,
      base::Bind(&ExpectEqStatus, &done, SYNC_STATUS_OK));
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(done);

  // Assert the App's origin folder was marked as deleted.
  EXPECT_TRUE(fake_sync_client()->remote_resources()
      .find(origin_dir_resource_id)->second.deleted);
}

}  // namespace sync_file_system
