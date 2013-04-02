// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_metadata_store.h"

#include <utility>

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
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

#define FPL FILE_PATH_LITERAL

using content::BrowserThread;

namespace sync_file_system {

namespace {

const char kOrigin[] = "chrome-extension://example";
const char* const kServiceName = DriveFileSyncService::kServiceName;

typedef DriveMetadataStore::ResourceIdByOrigin ResourceIdByOrigin;
typedef DriveMetadataStore::OriginByResourceId OriginByResourceId;

fileapi::FileSystemURL URL(const base::FilePath& path) {
  return CreateSyncableFileSystemURL(GURL(kOrigin), kServiceName, path);
}

std::string GetResourceID(const ResourceIdByOrigin& sync_origins,
                          const GURL& origin) {
  ResourceIdByOrigin::const_iterator itr = sync_origins.find(origin);
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
    drive_metadata_store_->disabled_origins_.clear();
    EXPECT_TRUE(drive_metadata_store_->batch_sync_origins().empty());
    EXPECT_TRUE(drive_metadata_store_->incremental_sync_origins().empty());
    EXPECT_TRUE(drive_metadata_store_->disabled_origins().empty());
  }

  void RestoreOriginsFromDB() {
    EXPECT_TRUE(ui_task_runner_->RunsTasksOnCurrentThread());
    drive_metadata_store_->RestoreOrigins(
        base::Bind(&DriveMetadataStoreTest::DidRestoreOrigins,
                   base::Unretained(this)));
    message_loop_.Run();
  }

  SyncStatusCode EnableOrigin(const GURL& origin) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    drive_metadata_store_->EnableOrigin(
        origin, base::Bind(&DriveMetadataStoreTest::DidFinishDBTask,
                           base::Unretained(this), &status));
    message_loop_.Run();
    return status;
  }

  SyncStatusCode DisableOrigin(const GURL& origin) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    drive_metadata_store_->DisableOrigin(
        origin, base::Bind(&DriveMetadataStoreTest::DidFinishDBTask,
                           base::Unretained(this), &status));
    message_loop_.Run();
    return status;
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

  void MarkAsCreated() {
    created_ = true;
  }

  void VerifyUntrackedOrigin(const GURL& origin) {
    EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsOriginDisabled(origin));
  }

  void VerifyBatchSyncOrigin(const GURL& origin,
                             const std::string& resource_id) {
    EXPECT_TRUE(metadata_store()->IsBatchSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsOriginDisabled(origin));
    EXPECT_EQ(resource_id,
              GetResourceID(metadata_store()->batch_sync_origins(), origin));
  }

  void VerifyIncrementalSyncOrigin(const GURL& origin,
                                   const std::string& resource_id) {
    EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(origin));
    EXPECT_TRUE(metadata_store()->IsIncrementalSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsOriginDisabled(origin));
    EXPECT_EQ(resource_id,
              GetResourceID(metadata_store()->incremental_sync_origins(),
                            origin));
  }

  void VerifyDisabledOrigin(const GURL& origin,
                            const std::string& resource_id) {
    EXPECT_FALSE(metadata_store()->IsBatchSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(origin));
    EXPECT_TRUE(metadata_store()->IsOriginDisabled(origin));
    EXPECT_EQ(resource_id,
              GetResourceID(metadata_store()->disabled_origins(), origin));
  }

  base::FilePath base_dir() {
    return base_dir_.path();
  }

  DriveMetadataStore* metadata_store() {
    return drive_metadata_store_.get();
  }

  const DriveMetadataStore::MetadataMap& metadata_map() {
    return drive_metadata_store_->metadata_map_;
  }

  void VerifyReverseMap() {
    const ResourceIdByOrigin& batch_sync_origins =
        drive_metadata_store_->batch_sync_origins_;
    const ResourceIdByOrigin& incremental_sync_origins =
        drive_metadata_store_->incremental_sync_origins_;
    const ResourceIdByOrigin& disabled_origins =
        drive_metadata_store_->disabled_origins_;
    const OriginByResourceId& origin_by_resource_id =
        drive_metadata_store_->origin_by_resource_id_;

    size_t expected_size =
        batch_sync_origins.size() + incremental_sync_origins.size() +
        disabled_origins.size();
    size_t actual_size = origin_by_resource_id.size();
    EXPECT_EQ(expected_size, actual_size);
    EXPECT_TRUE(VerifyReverseMapInclusion(batch_sync_origins,
                                          origin_by_resource_id));
    EXPECT_TRUE(VerifyReverseMapInclusion(incremental_sync_origins,
                                          origin_by_resource_id));
    EXPECT_TRUE(VerifyReverseMapInclusion(disabled_origins,
                                          origin_by_resource_id));
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

  void DidRestoreOrigins(SyncStatusCode status) {
    EXPECT_EQ(SYNC_STATUS_OK, status);
    message_loop_.Quit();
  }

  bool VerifyReverseMapInclusion(const ResourceIdByOrigin& left,
                                 const OriginByResourceId& right) {
    for (ResourceIdByOrigin::const_iterator itr = left.begin();
         itr != left.end(); ++itr) {
      OriginByResourceId::const_iterator found = right.find(itr->second);
      if (found == right.end() || found->second != itr->first)
        return false;
    }
    return true;
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

  VerifyReverseMap();
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

  VerifyReverseMap();
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

  VerifyReverseMap();
}

TEST_F(DriveMetadataStoreTest, StoreSyncRootDirectory) {
  const std::string kResourceId("hoge");

  InitializeDatabase();

  EXPECT_TRUE(metadata_store()->sync_root_directory().empty());

  metadata_store()->SetSyncRootDirectory(kResourceId);
  EXPECT_EQ(kResourceId, metadata_store()->sync_root_directory());

  DropSyncRootDirectoryInStore();
  EXPECT_TRUE(metadata_store()->sync_root_directory().empty());

  RestoreSyncRootDirectoryFromDB();
  EXPECT_EQ(kResourceId, metadata_store()->sync_root_directory());

  VerifyReverseMap();
}

TEST_F(DriveMetadataStoreTest, StoreSyncOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const GURL kOrigin3("chrome-extension://example3");
  const std::string kResourceId1("hoge");
  const std::string kResourceId2("fuga");
  const std::string kResourceId3("foo");

  InitializeDatabase();

  // Make sure origins have not been marked yet.
  VerifyUntrackedOrigin(kOrigin1);
  VerifyUntrackedOrigin(kOrigin2);
  VerifyUntrackedOrigin(kOrigin3);

  // Mark origins as batch sync origins.
  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->AddBatchSyncOrigin(kOrigin3, kResourceId3);
  VerifyBatchSyncOrigin(kOrigin1, kResourceId1);
  VerifyBatchSyncOrigin(kOrigin2, kResourceId2);
  VerifyBatchSyncOrigin(kOrigin3, kResourceId3);

  // Mark |kOrigin1| as an incremental sync origin, and disable |kOrigin3| as
  // a disabled sync origin. |kOrigin2| should have still been marked as a
  // batch sync origin.
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin1);
  DisableOrigin(kOrigin3);
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyBatchSyncOrigin(kOrigin2, kResourceId2);
  VerifyDisabledOrigin(kOrigin3, kResourceId3);

  DropSyncOriginsInStore();

  // Make sure origins have been dropped.
  VerifyUntrackedOrigin(kOrigin1);
  VerifyUntrackedOrigin(kOrigin2);
  VerifyUntrackedOrigin(kOrigin3);

  RestoreOriginsFromDB();

  // Make sure origins have been restored.
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyBatchSyncOrigin(kOrigin2, kResourceId2);
  VerifyDisabledOrigin(kOrigin3, kResourceId3);

  VerifyReverseMap();
}

TEST_F(DriveMetadataStoreTest, DisableOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kResourceId1("hoge");
  const std::string kResourceId2("fuga");

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);
  VerifyBatchSyncOrigin(kOrigin1, kResourceId1);
  VerifyIncrementalSyncOrigin(kOrigin2, kResourceId2);

  DisableOrigin(kOrigin1);
  DisableOrigin(kOrigin2);
  VerifyDisabledOrigin(kOrigin1, kResourceId1);
  VerifyDisabledOrigin(kOrigin2, kResourceId2);

  EnableOrigin(kOrigin1);
  EnableOrigin(kOrigin2);
  VerifyBatchSyncOrigin(kOrigin1, kResourceId1);
  VerifyBatchSyncOrigin(kOrigin2, kResourceId2);
}

TEST_F(DriveMetadataStoreTest, RemoveOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const GURL kOrigin3("chrome-extension://example3");
  const GURL kOrigin4("chrome-extension://example4");
  const GURL kOrigin5("chrome-extension://example5");
  const std::string kResourceId1("hogera");
  const std::string kResourceId2("fugaga");
  const std::string kResourceId3("piyopiyo");
  const std::string kResourceId5("hogehoge");

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);
  metadata_store()->AddBatchSyncOrigin(kOrigin3, kResourceId3);
  metadata_store()->AddBatchSyncOrigin(kOrigin5, kResourceId5);

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
                    kOrigin5, kServiceName, base::FilePath(FPL("tac"))),
                CreateMetadata("pov", "spoon", false, false)));
  EXPECT_EQ(SYNC_STATUS_OK,
            UpdateEntry(
                CreateSyncableFileSystemURL(
                    kOrigin1, kServiceName, base::FilePath(FPL("tic"))),
                CreateMetadata("zav", "sause", false, false)));

  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin1));
  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin2));
  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin4));
  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin5));

  DropDatabase();
  InitializeDatabase();

  // kOrigin3 should be only remaining batch sync origin.
  EXPECT_EQ(1u, metadata_store()->batch_sync_origins().size());
  EXPECT_TRUE(metadata_store()->IsBatchSyncOrigin(kOrigin3));
  EXPECT_TRUE(metadata_store()->incremental_sync_origins().empty());
  EXPECT_TRUE(metadata_store()->disabled_origins().empty());
  EXPECT_EQ(1u, metadata_map().size());

  DriveMetadataStore::MetadataMap::const_iterator found =
      metadata_map().find(kOrigin3);
  EXPECT_TRUE(found != metadata_map().end() && found->second.size() == 1u);

  VerifyReverseMap();
}

TEST_F(DriveMetadataStoreTest, GetResourceIdForOrigin) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const GURL kOrigin3("chrome-extension://example3");
  const std::string kResourceId1("hogera");
  const std::string kResourceId2("fugaga");
  const std::string kResourceId3("piyopiyo");

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));
  metadata_store()->SetSyncRootDirectory("root");

  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);
  metadata_store()->AddBatchSyncOrigin(kOrigin3, kResourceId3);
  DisableOrigin(kOrigin3);

  EXPECT_EQ(kResourceId1, metadata_store()->GetResourceIdForOrigin(kOrigin1));
  EXPECT_EQ(kResourceId2, metadata_store()->GetResourceIdForOrigin(kOrigin2));
  EXPECT_EQ(kResourceId3, metadata_store()->GetResourceIdForOrigin(kOrigin3));

  DropDatabase();
  InitializeDatabase();

  EXPECT_EQ(kResourceId1, metadata_store()->GetResourceIdForOrigin(kOrigin1));
  EXPECT_EQ(kResourceId2, metadata_store()->GetResourceIdForOrigin(kOrigin2));
  EXPECT_EQ(kResourceId3, metadata_store()->GetResourceIdForOrigin(kOrigin3));

  // Resetting the root directory resource ID to empty makes any
  // GetResourceIdForOrigin return an empty resource ID too, regardless of
  // whether they are known origin or not.
  metadata_store()->SetSyncRootDirectory(std::string());
  EXPECT_TRUE(metadata_store()->GetResourceIdForOrigin(kOrigin1).empty());
  EXPECT_TRUE(metadata_store()->GetResourceIdForOrigin(kOrigin2).empty());
  EXPECT_TRUE(metadata_store()->GetResourceIdForOrigin(kOrigin3).empty());

  // Make sure they're still known origins.
  EXPECT_TRUE(metadata_store()->IsKnownOrigin(kOrigin1));
  EXPECT_TRUE(metadata_store()->IsKnownOrigin(kOrigin2));
  EXPECT_TRUE(metadata_store()->IsKnownOrigin(kOrigin3));

  VerifyReverseMap();
}

TEST_F(DriveMetadataStoreTest, MigrationFromV0) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kSyncRootResourceId("sync_root_resource_id");
  const std::string kResourceId1("hoge");
  const std::string kResourceId2("fuga");
  const std::string kFileResourceId("piyo");
  const base::FilePath kFile(FPL("foo bar"));
  const std::string kFileMD5("file_md5");

  {
    const char kChangeStampKey[] = "CHANGE_STAMP";
    const char kSyncRootDirectoryKey[] = "SYNC_ROOT_DIR";
    const char kDriveMetadataKeyPrefix[] = "METADATA: ";
    const char kDriveBatchSyncOriginKeyPrefix[] = "BSYNC_ORIGIN: ";
    const char kDriveIncrementalSyncOriginKeyPrefix[] = "ISYNC_ORIGIN: ";

    const char kV0ServiceName[] = "drive";
    ASSERT_TRUE(RegisterSyncableFileSystem(kV0ServiceName));
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::DB* db_ptr = NULL;
    std::string db_dir = fileapi::FilePathToString(
        base_dir().Append(DriveMetadataStore::kDatabaseName));
    leveldb::Status status = leveldb::DB::Open(options, db_dir, &db_ptr);

    scoped_ptr<leveldb::DB> db(db_ptr);
    ASSERT_TRUE(status.ok());

    leveldb::WriteOptions write_options;
    db->Put(write_options, kChangeStampKey, "1");
    db->Put(write_options, kSyncRootDirectoryKey, kSyncRootResourceId);

    DriveMetadata drive_metadata;
    drive_metadata.set_resource_id(kFileResourceId);
    drive_metadata.set_md5_checksum(kFileMD5);
    drive_metadata.set_conflicted(false);
    drive_metadata.set_to_be_fetched(false);

    fileapi::FileSystemURL url = CreateSyncableFileSystemURL(
        kOrigin1, kV0ServiceName, kFile);
    std::string serialized_url;
    SerializeSyncableFileSystemURL(url, &serialized_url);
    std::string metadata_string;
    drive_metadata.SerializeToString(&metadata_string);

    db->Put(write_options,
            kDriveMetadataKeyPrefix + serialized_url, metadata_string);
    db->Put(write_options,
            kDriveBatchSyncOriginKeyPrefix + kOrigin1.spec(), kResourceId1);
    db->Put(write_options,
            kDriveIncrementalSyncOriginKeyPrefix + kOrigin2.spec(),
            kResourceId2);
    EXPECT_TRUE(RevokeSyncableFileSystem(kV0ServiceName));
    MarkAsCreated();
  }

  InitializeDatabase();

  EXPECT_EQ(1, metadata_store()->GetLargestChangeStamp());
  EXPECT_EQ(kSyncRootResourceId, metadata_store()->sync_root_directory());
  EXPECT_EQ(kResourceId1, metadata_store()->GetResourceIdForOrigin(kOrigin1));
  EXPECT_EQ(kResourceId2, metadata_store()->GetResourceIdForOrigin(kOrigin2));

  DriveMetadata metadata;
  EXPECT_EQ(SYNC_STATUS_OK,
            metadata_store()->ReadEntry(
                CreateSyncableFileSystemURL(kOrigin1, kServiceName, kFile),
                &metadata));
  EXPECT_EQ(kFileResourceId, metadata.resource_id());
  EXPECT_EQ(kFileMD5, metadata.md5_checksum());
  EXPECT_FALSE(metadata.conflicted());
  EXPECT_FALSE(metadata.to_be_fetched());

  VerifyReverseMap();
}

TEST_F(DriveMetadataStoreTest, ResetOriginRootDirectory) {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kResourceId1("hoge");
  const std::string kResourceId2("fuga");
  const std::string kResourceId3("piyo");

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddBatchSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddBatchSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->MoveBatchSyncOriginToIncremental(kOrigin2);
  VerifyBatchSyncOrigin(kOrigin1, kResourceId1);
  VerifyIncrementalSyncOrigin(kOrigin2, kResourceId2);
  VerifyReverseMap();

  metadata_store()->SetOriginRootDirectory(kOrigin2, kResourceId3);
  VerifyIncrementalSyncOrigin(kOrigin2, kResourceId3);
  VerifyReverseMap();
}

}  // namespace sync_file_system
