// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_metadata_store.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync_file_system/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#define FPL FILE_PATH_LITERAL

using content::BrowserThread;

namespace sync_file_system {

namespace {

const char kOrigin[] = "chrome-extension://example";
const char* const kServiceName = DriveFileSyncService::kServiceName;

typedef DriveMetadataStore::ResourceIDMap ResourceIDMap;

fileapi::FileSystemURL URL(const base::FilePath& path) {
  return CreateSyncableFileSystemURL(GURL(kOrigin), kServiceName, path);
}

std::string GetResourceID(const ResourceIDMap& sync_origins,
                          const GURL& origin) {
  ResourceIDMap::const_iterator itr = sync_origins.find(origin);
  if (itr == sync_origins.end())
    return std::string();
  return itr->second;
}

DriveMetadata CreateMetadata(const std::string& resource_id,
                             const std::string& md5_checksum,
                             bool conflicted,
                             bool to_be_fetched) {
  DriveMetadata metadata;
  metadata.set_resource_id(resource_id);
  metadata.set_md5_checksum(md5_checksum);
  metadata.set_conflicted(conflicted);
  metadata.set_to_be_fetched(to_be_fetched);
  return metadata;
}

}  // namespace

class DriveMetadataStoreTest : public testing::Test {
 public:
  DriveMetadataStoreTest()
      : created_(false) {}

  virtual ~DriveMetadataStoreTest() {}

  virtual void SetUp() OVERRIDE {
    file_thread_.reset(new base::Thread("Thread_File"));
    file_thread_->Start();

    ui_task_runner_ = base::MessageLoopProxy::current();
    file_task_runner_ = file_thread_->message_loop_proxy();

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(RegisterSyncableFileSystem(kServiceName));
  }

  virtual void TearDown() OVERRIDE {
    EXPECT_TRUE(RevokeSyncableFileSystem(kServiceName));

    DropDatabase();
    file_thread_->Stop();
    message_loop_.RunUntilIdle();
  }

 protected:
  void InitializeDatabase() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());

    bool done = false;
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    bool created = false;

    drive_metadata_store_.reset(
        new DriveMetadataStore(base_dir_.path(), file_task_runner_));
    drive_metadata_store_->Initialize(
        base::Bind(&DriveMetadataStoreTest::DidInitializeDatabase,
                   base::Unretained(this), &done, &status, &created));
    message_loop_.Run();

    EXPECT_TRUE(done);
    EXPECT_EQ(SYNC_STATUS_OK, status);

    if (created) {
      EXPECT_FALSE(created_);
      created_ = created;
      return;
    }
    EXPECT_TRUE(created_);
  }

  void DropDatabase() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_.reset();
  }

  void DropSyncRootDirectoryInStore() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_->sync_root_directory_resource_id_.clear();
  }

  void RestoreSyncRootDirectoryFromDB() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_->RestoreSyncRootDirectory(
        base::Bind(&DriveMetadataStoreTest::DidRestoreSyncRootDirectory,
                   base::Unretained(this)));
    message_loop_.Run();
  }

  void DropSyncOriginsInStore() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_->batch_sync_origins_.clear();
    drive_metadata_store_->incremental_sync_origins_.clear();
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

  SyncStatusCode RemoveOrigin(const GURL& url) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    drive_metadata_store_->RemoveOrigin(
        url, base::Bind(&DriveMetadataStoreTest::DidFinishDBTask,
                        base::Unretained(this), &status));
    message_loop_.Run();
    return status;
  }

  SyncStatusCode UpdateEntry(const fileapi::FileSystemURL& url,
                             const DriveMetadata& metadata) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    drive_metadata_store_->UpdateEntry(
        url, metadata,
        base::Bind(&DriveMetadataStoreTest::DidFinishDBTask,
                   base::Unretained(this), &status));
    message_loop_.Run();
    return status;
  }

  SyncStatusCode DeleteEntry(const fileapi::FileSystemURL& url) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    drive_metadata_store_->DeleteEntry(
        url,
        base::Bind(&DriveMetadataStoreTest::DidFinishDBTask,
                   base::Unretained(this), &status));
    message_loop_.Run();
    return status;
  }

  SyncStatusCode SetLargestChangeStamp(int64 changestamp) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    drive_metadata_store_->SetLargestChangeStamp(
        changestamp, base::Bind(&DriveMetadataStoreTest::DidFinishDBTask,
                                base::Unretained(this), &status));
    message_loop_.Run();
    return status;
  }

  void DidFinishDBTask(SyncStatusCode* status_out,
                       SyncStatusCode status) {
    *status_out = status;
    message_loop_.Quit();
  }

  DriveMetadataStore* metadata_store() {
    return drive_metadata_store_.get();
  }

  const DriveMetadataStore::MetadataMap& metadata_map() {
    return drive_metadata_store_->metadata_map_;
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

  void DidRestoreSyncRootDirectory(SyncStatusCode status) {
    EXPECT_EQ(SYNC_STATUS_OK, status);
    message_loop_.Quit();
  }

  void DidRestoreSyncOrigins(SyncStatusCode status) {
    EXPECT_EQ(SYNC_STATUS_OK, status);
    message_loop_.Quit();
  }

  base::ScopedTempDir base_dir_;

  MessageLoop message_loop_;
  scoped_ptr<base::Thread> file_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<DriveMetadataStore> drive_metadata_store_;

  bool created_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataStoreTest);
};

TEST_F(DriveMetadataStoreTest, InitializationTest) {
  InitializeDatabase();
}

TEST_F(DriveMetadataStoreTest, ReadWriteTest) {
  InitializeDatabase();

  const fileapi::FileSystemURL url = URL(base::FilePath());
  DriveMetadata metadata;
  EXPECT_EQ(SYNC_DATABASE_ERROR_NOT_FOUND,
            metadata_store()->ReadEntry(url, &metadata));

  metadata = CreateMetadata("1234567890", "09876543210", true, false);
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(url, metadata));
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  DropDatabase();
  InitializeDatabase();

  EXPECT_EQ(1, metadata_store()->GetLargestChangeStamp());

  DriveMetadata metadata2;
  EXPECT_EQ(SYNC_STATUS_OK,
            metadata_store()->ReadEntry(url, &metadata2));
  EXPECT_EQ(metadata.resource_id(), metadata2.resource_id());
  EXPECT_EQ(metadata.md5_checksum(), metadata2.md5_checksum());
  EXPECT_EQ(metadata.conflicted(), metadata2.conflicted());

  EXPECT_EQ(SYNC_STATUS_OK, DeleteEntry(url));
  EXPECT_EQ(SYNC_DATABASE_ERROR_NOT_FOUND,
            metadata_store()->ReadEntry(url, &metadata));
  EXPECT_EQ(SYNC_DATABASE_ERROR_NOT_FOUND, DeleteEntry(url));
}

TEST_F(DriveMetadataStoreTest, GetConflictURLsTest) {
  InitializeDatabase();

  fileapi::FileSystemURLSet urls;
  EXPECT_EQ(SYNC_STATUS_OK, metadata_store()->GetConflictURLs(&urls));
  EXPECT_EQ(0U, urls.size());

  const base::FilePath path1(FPL("file1"));
  const base::FilePath path2(FPL("file2"));
  const base::FilePath path3(FPL("file3"));

  // Populate metadata in DriveMetadataStore. The metadata identified by "file2"
  // and "file3" are marked as conflicted.
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(URL(path1), CreateMetadata("1", "1", false, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(URL(path2), CreateMetadata("2", "2", true, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(URL(path3), CreateMetadata("3", "3", true, false)));

  EXPECT_EQ(SYNC_STATUS_OK, metadata_store()->GetConflictURLs(&urls));
  EXPECT_EQ(2U, urls.size());
  EXPECT_FALSE(ContainsKey(urls, URL(path1)));
  EXPECT_TRUE(ContainsKey(urls, URL(path2)));
  EXPECT_TRUE(ContainsKey(urls, URL(path3)));
}

TEST_F(DriveMetadataStoreTest, GetToBeFetchedFilessTest) {
  InitializeDatabase();

  DriveMetadataStore::URLAndResourceIdList list;
  EXPECT_EQ(SYNC_STATUS_OK, metadata_store()->GetToBeFetchedFiles(&list));
  EXPECT_TRUE(list.empty());

  const base::FilePath path1(FPL("file1"));
  const base::FilePath path2(FPL("file2"));
  const base::FilePath path3(FPL("file3"));

  // Populate metadata in DriveMetadataStore. The metadata identified by "file2"
  // and "file3" are marked to be fetched.
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(URL(path1), CreateMetadata("1", "1", false, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(URL(path2), CreateMetadata("2", "2", false, true)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(URL(path3), CreateMetadata("3", "3", false, true)));

  EXPECT_EQ(SYNC_STATUS_OK,
            metadata_store()->GetToBeFetchedFiles(&list));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(list[0].first, URL(path2));
  EXPECT_EQ(list[1].first, URL(path3));
}

TEST_F(DriveMetadataStoreTest, StoreSyncRootDirectory) {
  const std::string kResourceID("hoge");

  InitializeDatabase();

  EXPECT_TRUE(metadata_store()->sync_root_directory().empty());

  metadata_store()->SetSyncRootDirectory(kResourceID);
  EXPECT_EQ(kResourceID, metadata_store()->sync_root_directory());

  DropSyncRootDirectoryInStore();
  EXPECT_TRUE(metadata_store()->sync_root_directory().empty());

  RestoreSyncRootDirectoryFromDB();
  EXPECT_EQ(kResourceID, metadata_store()->sync_root_directory());
}

TEST_F(DriveMetadataStoreTest, StoreSyncOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kResourceID1("hoge");
  const std::string kResourceID2("fuga");

  InitializeDatabase();

  // Make sure origins have not been marked yet.
  EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(kOrigin1));
  EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(kOrigin2));
  EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(kOrigin2));

  // Mark origins as batch sync origins.
  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceID1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceID2);
  EXPECT_TRUE(metadata_store()->IsBatchSyncOrigin(kOrigin1));
  EXPECT_TRUE(metadata_store()->IsBatchSyncOrigin(kOrigin2));
  EXPECT_EQ(kResourceID1,
            GetResourceID(metadata_store()->batch_sync_origins(), kOrigin1));
  EXPECT_EQ(kResourceID2,
            GetResourceID(metadata_store()->batch_sync_origins(), kOrigin2));

  // Mark |kOrigin1| as an incremental sync origin. |kOrigin2| should have still
  // been marked as a batch sync origin.
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin1);
  EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(kOrigin1));
  EXPECT_TRUE(metadata_store()->IsBatchSyncOrigin(kOrigin2));
  EXPECT_TRUE(metadata_store()->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(kOrigin2));
  EXPECT_EQ(kResourceID1,
            GetResourceID(metadata_store()->incremental_sync_origins(),
                          kOrigin1));
  EXPECT_EQ(kResourceID2,
            GetResourceID(metadata_store()->batch_sync_origins(), kOrigin2));

  DropSyncOriginsInStore();

  // Make sure origins have been dropped.
  EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(kOrigin1));
  EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(kOrigin2));
  EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(kOrigin2));

  RestoreSyncOriginsFromDB();

  // Make sure origins have been restored.
  EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(kOrigin1));
  EXPECT_TRUE(metadata_store()->IsBatchSyncOrigin(kOrigin2));
  EXPECT_TRUE(metadata_store()->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(kOrigin2));
  EXPECT_EQ(kResourceID1,
            GetResourceID(metadata_store()->incremental_sync_origins(),
                          kOrigin1));
  EXPECT_EQ(kResourceID2,
            GetResourceID(metadata_store()->batch_sync_origins(), kOrigin2));
}

TEST_F(DriveMetadataStoreTest, RemoveOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const GURL kOrigin3("chrome-extension://example3");
  const GURL kOrigin4("chrome-extension://example4");
  const std::string kResourceId1("hogera");
  const std::string kResourceId2("fugaga");
  const std::string kResourceId3("piyopiyo");

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);
  metadata_store()->AddBatchSyncOrigin(kOrigin3, kResourceId3);

  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(
                CreateSyncableFileSystemURL(
                    kOrigin1, kServiceName, base::FilePath(FPL("guf"))),
                CreateMetadata("foo", "spam", false, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(
                CreateSyncableFileSystemURL(
                    kOrigin2, kServiceName, base::FilePath(FPL("mof"))),
                CreateMetadata("bar", "ham", false, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(
                CreateSyncableFileSystemURL(
                    kOrigin3, kServiceName, base::FilePath(FPL("waf"))),
                CreateMetadata("baz", "egg", false, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(
                CreateSyncableFileSystemURL(
                    kOrigin4, kServiceName, base::FilePath(FPL("cue"))),
                CreateMetadata("lat", "fork", false, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(
                CreateSyncableFileSystemURL(
                    kOrigin1, kServiceName, base::FilePath(FPL("tic"))),
                CreateMetadata("zav", "sause", false, false)));

  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin1));
  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin2));
  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin4));

  DropDatabase();
  InitializeDatabase();

  // kOrigin3 should be only remaining batch sync origin.
  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_TRUE(metadata_store()->IsBatchSyncOrigin(kOrigin3));
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());
  EXPECT_EQ(1u, metadata_map().size());

  DriveMetadataStore::MetadataMap::const_iterator found =
      metadata_map().find(kOrigin3);
  EXPECT_TRUE(found != metadata_map().end() && found->second.size() == 1u);
}

TEST_F(DriveMetadataStoreTest, GetResourceIdForOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kResourceId1("hogera");
  const std::string kResourceId2("fugaga");

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);

  EXPECT_EQ(kResourceId1, metadata_store()->GetResourceIdForOrigin(kOrigin1));
  EXPECT_EQ(kResourceId2, metadata_store()->GetResourceIdForOrigin(kOrigin2));

  DropDatabase();
  InitializeDatabase();

  EXPECT_EQ(kResourceId1, metadata_store()->GetResourceIdForOrigin(kOrigin1));
  EXPECT_EQ(kResourceId2, metadata_store()->GetResourceIdForOrigin(kOrigin2));
}

}  // namespace sync_file_system
