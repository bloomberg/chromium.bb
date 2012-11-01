// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_metadata_store.h"

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#define FPL FILE_PATH_LITERAL

using content::BrowserThread;
using fileapi::SyncStatusCode;

namespace sync_file_system {

namespace {

typedef DriveMetadataStore::ResourceIDMap ResourceIDMap;

std::string GetResourceID(const ResourceIDMap& sync_origins,
                          const GURL& origin) {
  ResourceIDMap::const_iterator itr = sync_origins.find(origin);
  if (itr == sync_origins.end())
    return std::string();
  return itr->second;
}

}  // namespace

class DriveMetadataStoreTest : public testing::Test {
 public:
  DriveMetadataStoreTest() {
  }

  virtual ~DriveMetadataStoreTest() {
  }

  virtual void SetUp() OVERRIDE {
    file_thread_.reset(new base::Thread("Thread_File"));
    file_thread_->Start();

    ui_task_runner_ = base::MessageLoopProxy::current();
    file_task_runner_ = file_thread_->message_loop_proxy();

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(fileapi::RegisterSyncableFileSystem("test"));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(fileapi::RevokeSyncableFileSystem("test"));

    DropDatabase();
    file_thread_->Stop();
    message_loop_.RunAllPending();
  }

 protected:
  void InitializeDatabase(bool* done, SyncStatusCode* status, bool* created) {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());

    *done = false;
    drive_metadata_store_.reset(
        new DriveMetadataStore(base_dir_.path(), file_task_runner_));
    drive_metadata_store_->Initialize(
        base::Bind(&DriveMetadataStoreTest::DidInitializeDatabase,
                   base::Unretained(this), done, status, created));
    message_loop_.Run();
  }

  void DropDatabase() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_.reset();
  }

  void DropSyncOriginsInStore() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_->ClearSyncOrigins();
    EXPECT_TRUE(drive_metadata_store_->batch_sync_origins().empty());
    EXPECT_TRUE(drive_metadata_store_->incremental_sync_origins().empty());
  }

  void RestoreSyncOriginsFromDB() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_->RestoreSyncOrigins(
        base::Bind(&DriveMetadataStoreTest::DidRestoreSyncOrigins,
                   base::Unretained(this)));
    message_loop_.Run();
  }

  DriveMetadataStore* drive_metadata_store() {
    return drive_metadata_store_.get();
  }

 private:
  void DidInitializeDatabase(bool* done_out,
                             SyncStatusCode* status_out,
                             bool* created_out,
                             SyncStatusCode status,
                             bool created) {
    *done_out = true;
    *status_out = status;
    *created_out = created;
    message_loop_.Quit();
  }

  void DidRestoreSyncOrigins(fileapi::SyncStatusCode status) {
    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
    message_loop_.Quit();
  }

  ScopedTempDir base_dir_;

  MessageLoop message_loop_;
  scoped_ptr<base::Thread> file_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<DriveMetadataStore> drive_metadata_store_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataStoreTest);
};

TEST_F(DriveMetadataStoreTest, InitializationTest) {
  bool done = false;
  SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
  bool created = false;
  InitializeDatabase(&done, &status, &created);
  EXPECT_TRUE(done);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
  EXPECT_TRUE(created);
}

TEST_F(DriveMetadataStoreTest, ReadWriteTest) {
  const GURL kOrigin("http://example.com");

  bool done = false;
  SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
  bool created = false;
  InitializeDatabase(&done, &status, &created);
  EXPECT_TRUE(done);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
  EXPECT_TRUE(created);

  fileapi::FileSystemURL url(
      fileapi::CreateSyncableFileSystemURL(kOrigin, "test", FilePath()));
  DriveMetadata metadata;
  EXPECT_EQ(fileapi::SYNC_DATABASE_ERROR_NOT_FOUND,
            drive_metadata_store()->ReadEntry(url, &metadata));

  metadata.set_resource_id("1234567890");
  metadata.set_md5_checksum("9876543210");
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            drive_metadata_store()->UpdateEntry(url, metadata));
  drive_metadata_store()->SetLargestChangeStamp(1);

  DropDatabase();
  done = false;
  status = fileapi::SYNC_STATUS_UNKNOWN;
  created = true;
  InitializeDatabase(&done, &status, &created);
  EXPECT_TRUE(done);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
  EXPECT_FALSE(created);

  EXPECT_EQ(1, drive_metadata_store()->GetLargestChangeStamp());

  DriveMetadata metadata2;
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            drive_metadata_store()->ReadEntry(url, &metadata2));
  EXPECT_EQ(metadata.resource_id(), metadata2.resource_id());
  EXPECT_EQ(metadata.md5_checksum(), metadata2.md5_checksum());

  EXPECT_EQ(fileapi::SYNC_STATUS_OK, drive_metadata_store()->DeleteEntry(url));
  EXPECT_EQ(fileapi::SYNC_DATABASE_ERROR_NOT_FOUND,
            drive_metadata_store()->ReadEntry(url, &metadata));
  EXPECT_EQ(fileapi::SYNC_DATABASE_ERROR_NOT_FOUND,
            drive_metadata_store()->DeleteEntry(url));
}

TEST_F(DriveMetadataStoreTest, StoreSyncOrigin) {
  const GURL kOrigin1("http://www1.example.com");
  const GURL kOrigin2("http://www2.example.com");
  const std::string kResourceID1("hoge");
  const std::string kResourceID2("fuga");

  bool done = false;
  SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
  bool created = false;
  InitializeDatabase(&done, &status, &created);
  EXPECT_TRUE(done);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
  EXPECT_TRUE(created);

  DriveMetadataStore* store = drive_metadata_store();

  // Make sure origins have not been marked yet.
  EXPECT_FALSE(store->IsBatchSyncOrigin(kOrigin1));
  EXPECT_FALSE(store->IsBatchSyncOrigin(kOrigin2));
  EXPECT_FALSE(store->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(store->IsIncrementalSyncOrigin(kOrigin2));

  // Mark origins as batch sync origins.
  store->AddBatchSyncOrigin(kOrigin1, kResourceID1);
  store->AddBatchSyncOrigin(kOrigin2, kResourceID2);
  EXPECT_TRUE(store->IsBatchSyncOrigin(kOrigin1));
  EXPECT_TRUE(store->IsBatchSyncOrigin(kOrigin2));
  EXPECT_EQ(kResourceID1, GetResourceID(store->batch_sync_origins(), kOrigin1));
  EXPECT_EQ(kResourceID2, GetResourceID(store->batch_sync_origins(), kOrigin2));

  // Mark |kOrigin1| as an incremental sync origin. |kOrigin2| should have still
  // been marked as a batch sync origin.
  store->MoveBatchSyncOriginToIncremental(kOrigin1);
  EXPECT_FALSE(store->IsBatchSyncOrigin(kOrigin1));
  EXPECT_TRUE(store->IsBatchSyncOrigin(kOrigin2));
  EXPECT_TRUE(store->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(store->IsIncrementalSyncOrigin(kOrigin2));
  EXPECT_EQ(kResourceID1,
            GetResourceID(store->incremental_sync_origins(), kOrigin1));
  EXPECT_EQ(kResourceID2, GetResourceID(store->batch_sync_origins(), kOrigin2));

  DropSyncOriginsInStore();

  // Make sure origins have been dropped.
  EXPECT_FALSE(store->IsBatchSyncOrigin(kOrigin1));
  EXPECT_FALSE(store->IsBatchSyncOrigin(kOrigin2));
  EXPECT_FALSE(store->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(store->IsIncrementalSyncOrigin(kOrigin2));

  RestoreSyncOriginsFromDB();

  // Make sure origins have been restored.
  EXPECT_FALSE(store->IsBatchSyncOrigin(kOrigin1));
  EXPECT_TRUE(store->IsBatchSyncOrigin(kOrigin2));
  EXPECT_TRUE(store->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(store->IsIncrementalSyncOrigin(kOrigin2));
  EXPECT_EQ(kResourceID1,
            GetResourceID(store->incremental_sync_origins(), kOrigin1));
  EXPECT_EQ(kResourceID2, GetResourceID(store->batch_sync_origins(), kOrigin2));
}

}  // namespace sync_file_system
