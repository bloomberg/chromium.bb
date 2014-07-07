// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/drive_metadata_store.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/browser/fileapi/isolated_context.h"

#define FPL FILE_PATH_LITERAL

using content::BrowserThread;

namespace sync_file_system {

namespace {

const char kOrigin[] = "chrome-extension://example";

typedef DriveMetadataStore::ResourceIdByOrigin ResourceIdByOrigin;
typedef DriveMetadataStore::OriginByResourceId OriginByResourceId;

fileapi::FileSystemURL URL(const base::FilePath& path) {
  return CreateSyncableFileSystemURL(GURL(kOrigin), path);
}

std::string GetResourceID(const ResourceIdByOrigin& sync_origins,
                          const GURL& origin) {
  ResourceIdByOrigin::const_iterator itr = sync_origins.find(origin);
  if (itr == sync_origins.end())
    return std::string();
  return itr->second;
}

std::string CreateResourceId(const std::string& resource_id) {
  return IsDriveAPIDisabled() ? resource_id
                              : drive_backend::RemoveWapiIdPrefix(resource_id);
}

DriveMetadata CreateMetadata(const std::string& resource_id,
                             const std::string& md5_checksum,
                             bool conflicted,
                             bool to_be_fetched,
                             DriveMetadata_ResourceType file_type) {
  DriveMetadata metadata;
  metadata.set_resource_id(CreateResourceId(resource_id));
  metadata.set_md5_checksum(md5_checksum);
  metadata.set_conflicted(conflicted);
  metadata.set_to_be_fetched(to_be_fetched);
  metadata.set_type(file_type);
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

    ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    file_task_runner_ = file_thread_->message_loop_proxy();

    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    RegisterSyncableFileSystem();
  }

  virtual void TearDown() OVERRIDE {
    SetDisableDriveAPI(false);
    RevokeSyncableFileSystem();

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
        new DriveMetadataStore(base_dir_.path(), file_task_runner_.get()));
    drive_metadata_store_->Initialize(
        base::Bind(&DriveMetadataStoreTest::DidInitializeDatabase,
                   base::Unretained(this),
                   &done,
                   &status,
                   &created));
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
    EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsOriginDisabled(origin));
  }

  void VerifyIncrementalSyncOrigin(const GURL& origin,
                                   const std::string& resource_id) {
    EXPECT_TRUE(metadata_store()->IsIncrementalSyncOrigin(origin));
    EXPECT_FALSE(metadata_store()->IsOriginDisabled(origin));
    EXPECT_EQ(resource_id,
              GetResourceID(metadata_store()->incremental_sync_origins(),
                            origin));
  }

  void VerifyDisabledOrigin(const GURL& origin,
                            const std::string& resource_id) {
    EXPECT_FALSE(metadata_store()->IsIncrementalSyncOrigin(origin));
    EXPECT_TRUE(metadata_store()->IsOriginDisabled(origin));
    EXPECT_EQ(resource_id,
              GetResourceID(metadata_store()->disabled_origins(), origin));
  }

  DriveMetadataStore* metadata_store() {
    return drive_metadata_store_.get();
  }

  const DriveMetadataStore::MetadataMap& metadata_map() {
    return drive_metadata_store_->metadata_map_;
  }

  void VerifyReverseMap() {
    const ResourceIdByOrigin& incremental_sync_origins =
        drive_metadata_store_->incremental_sync_origins_;
    const ResourceIdByOrigin& disabled_origins =
        drive_metadata_store_->disabled_origins_;
    const OriginByResourceId& origin_by_resource_id =
        drive_metadata_store_->origin_by_resource_id_;

    size_t expected_size = incremental_sync_origins.size() +
                           disabled_origins.size();
    size_t actual_size = origin_by_resource_id.size();
    EXPECT_EQ(expected_size, actual_size);
    EXPECT_TRUE(VerifyReverseMapInclusion(incremental_sync_origins,
                                          origin_by_resource_id));
    EXPECT_TRUE(VerifyReverseMapInclusion(disabled_origins,
                                          origin_by_resource_id));
  }

  void ReadWrite_Body();
  void GetConflictURLs_Body();
  void GetToBeFetchedFiles_Body();
  void StoreSyncRootDirectory_Body();
  void StoreSyncOrigin_Body();
  void DisableOrigin_Body();
  void RemoveOrigin_Body();
  void GetResourceIdForOrigin_Body();
  void ResetOriginRootDirectory_Body();
  void GetFileMetadataMap_Body();

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

  base::MessageLoop message_loop_;
  scoped_ptr<base::Thread> file_thread_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  scoped_ptr<DriveMetadataStore> drive_metadata_store_;

  bool created_;

  DISALLOW_COPY_AND_ASSIGN(DriveMetadataStoreTest);
};

void DriveMetadataStoreTest::ReadWrite_Body() {
  InitializeDatabase();

  const fileapi::FileSystemURL url = URL(base::FilePath());
  DriveMetadata metadata;
  EXPECT_EQ(SYNC_DATABASE_ERROR_NOT_FOUND,
            metadata_store()->ReadEntry(url, &metadata));

  metadata = CreateMetadata(
      "file:1234567890", "09876543210", true, false,
      DriveMetadata_ResourceType_RESOURCE_TYPE_FILE);
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

void DriveMetadataStoreTest::GetConflictURLs_Body() {
  InitializeDatabase();

  fileapi::FileSystemURLSet urls;
  EXPECT_EQ(SYNC_STATUS_OK, metadata_store()->GetConflictURLs(&urls));
  EXPECT_EQ(0U, urls.size());

  const base::FilePath path1(FPL("file1"));
  const base::FilePath path2(FPL("file2"));
  const base::FilePath path3(FPL("file3"));

  // Populate metadata in DriveMetadataStore. The metadata identified by "file2"
  // and "file3" are marked as conflicted.
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      URL(path1),
      CreateMetadata("file:1", "1", false, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      URL(path2),
      CreateMetadata("file:2", "2", true, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      URL(path3),
      CreateMetadata("file:3", "3", true, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));

  EXPECT_EQ(SYNC_STATUS_OK, metadata_store()->GetConflictURLs(&urls));
  EXPECT_EQ(2U, urls.size());
  EXPECT_FALSE(ContainsKey(urls, URL(path1)));
  EXPECT_TRUE(ContainsKey(urls, URL(path2)));
  EXPECT_TRUE(ContainsKey(urls, URL(path3)));

  VerifyReverseMap();
}

void DriveMetadataStoreTest::GetToBeFetchedFiles_Body() {
  InitializeDatabase();

  DriveMetadataStore::URLAndDriveMetadataList list;
  EXPECT_EQ(SYNC_STATUS_OK, metadata_store()->GetToBeFetchedFiles(&list));
  EXPECT_TRUE(list.empty());

  const base::FilePath path1(FPL("file1"));
  const base::FilePath path2(FPL("file2"));
  const base::FilePath path3(FPL("file3"));

  // Populate metadata in DriveMetadataStore. The metadata identified by "file2"
  // and "file3" are marked to be fetched.
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      URL(path1),
      CreateMetadata("file:1", "1", false, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      URL(path2),
      CreateMetadata("file:2", "2", false, true,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      URL(path3),
      CreateMetadata("file:3", "3", false, true,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));

  EXPECT_EQ(SYNC_STATUS_OK,
            metadata_store()->GetToBeFetchedFiles(&list));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(list[0].first, URL(path2));
  EXPECT_EQ(list[1].first, URL(path3));

  VerifyReverseMap();
}

void DriveMetadataStoreTest::StoreSyncRootDirectory_Body() {
  const std::string kResourceId(CreateResourceId("folder:hoge"));

  InitializeDatabase();
  EXPECT_TRUE(metadata_store()->sync_root_directory().empty());

  metadata_store()->SetSyncRootDirectory(kResourceId);
  EXPECT_EQ(kResourceId, metadata_store()->sync_root_directory());

  DropDatabase();
  InitializeDatabase();
  EXPECT_EQ(kResourceId, metadata_store()->sync_root_directory());
}

void DriveMetadataStoreTest::StoreSyncOrigin_Body() {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const std::string kResourceId1(CreateResourceId("folder:hoge"));
  const std::string kResourceId2(CreateResourceId("folder:fuga"));

  InitializeDatabase();

  // Make sure origins have not been marked yet.
  VerifyUntrackedOrigin(kOrigin1);
  VerifyUntrackedOrigin(kOrigin2);

  // Mark origins as incremental sync origins.
  metadata_store()->AddIncrementalSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddIncrementalSyncOrigin(kOrigin2, kResourceId2);
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyIncrementalSyncOrigin(kOrigin2, kResourceId2);

  // Disabled origin 2, origin 1 should still be incremental.
  DisableOrigin(kOrigin2);
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyDisabledOrigin(kOrigin2, kResourceId2);

  DropDatabase();
  InitializeDatabase();

  // Make sure origins have been restored.
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyDisabledOrigin(kOrigin2, kResourceId2);

  VerifyReverseMap();
}

void DriveMetadataStoreTest::DisableOrigin_Body() {
  const GURL kOrigin1("chrome-extension://example1");
  const std::string kResourceId1(CreateResourceId("folder:hoge"));

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId1);

  DisableOrigin(kOrigin1);
  VerifyDisabledOrigin(kOrigin1, kResourceId1);

  // Re-enabled origins go back to DriveFileSyncService and are not tracked
  // in DriveMetadataStore.
  EnableOrigin(kOrigin1);
  VerifyUntrackedOrigin(kOrigin1);
}

void DriveMetadataStoreTest::RemoveOrigin_Body() {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const GURL kOrigin3("chrome-extension://example3");
  const std::string kResourceId1(CreateResourceId("folder:hogera"));
  const std::string kResourceId2(CreateResourceId("folder:fugaga"));
  const std::string kResourceId3(CreateResourceId("folder:piyopiyo"));

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddIncrementalSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddIncrementalSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->AddIncrementalSyncOrigin(kOrigin3, kResourceId3);
  DisableOrigin(kOrigin3);
  EXPECT_EQ(2u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(1u, metadata_store()->disabled_origins().size());

  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      CreateSyncableFileSystemURL(kOrigin1, base::FilePath(FPL("guf"))),
      CreateMetadata("file:foo", "spam", false, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      CreateSyncableFileSystemURL(kOrigin2, base::FilePath(FPL("mof"))),
      CreateMetadata("file:bar", "ham", false, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      CreateSyncableFileSystemURL(kOrigin3, base::FilePath(FPL("waf"))),
      CreateMetadata("folder:baz", "egg", false, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER)));

  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin2));
  EXPECT_EQ(SYNC_STATUS_OK, RemoveOrigin(kOrigin3));

  DropDatabase();
  InitializeDatabase();

  // kOrigin1 should be the only one left.
  EXPECT_EQ(1u, metadata_store()->incremental_sync_origins().size());
  EXPECT_EQ(0u, metadata_store()->disabled_origins().size());
  EXPECT_TRUE(metadata_store()->IsIncrementalSyncOrigin(kOrigin1));
  EXPECT_EQ(1u, metadata_map().size());

  DriveMetadataStore::MetadataMap::const_iterator found =
      metadata_map().find(kOrigin1);
  EXPECT_TRUE(found != metadata_map().end() && found->second.size() == 1u);

  VerifyReverseMap();
}

void DriveMetadataStoreTest::GetResourceIdForOrigin_Body() {
  const GURL kOrigin1("chrome-extension://example1");
  const GURL kOrigin2("chrome-extension://example2");
  const GURL kOrigin3("chrome-extension://example3");
  const std::string kResourceId1(CreateResourceId("folder:hogera"));
  const std::string kResourceId2(CreateResourceId("folder:fugaga"));
  const std::string kResourceId3(CreateResourceId("folder:piyopiyo"));

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));
  metadata_store()->SetSyncRootDirectory(CreateResourceId("folder:root"));

  metadata_store()->AddIncrementalSyncOrigin(kOrigin1, kResourceId1);
  metadata_store()->AddIncrementalSyncOrigin(kOrigin2, kResourceId2);
  metadata_store()->AddIncrementalSyncOrigin(kOrigin3, kResourceId3);
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

void DriveMetadataStoreTest::ResetOriginRootDirectory_Body() {
  const GURL kOrigin1("chrome-extension://example1");
  const std::string kResourceId1(CreateResourceId("folder:hoge"));
  const std::string kResourceId2(CreateResourceId("folder:fuga"));

  InitializeDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, SetLargestChangeStamp(1));

  metadata_store()->AddIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId1);
  VerifyReverseMap();

  metadata_store()->SetOriginRootDirectory(kOrigin1, kResourceId2);
  VerifyIncrementalSyncOrigin(kOrigin1, kResourceId2);
  VerifyReverseMap();
}

void DriveMetadataStoreTest::GetFileMetadataMap_Body() {
  InitializeDatabase();

  // Save one file and folder to the origin.
  const base::FilePath file_path = base::FilePath(FPL("file_0"));
  const base::FilePath folder_path = base::FilePath(FPL("folder_0"));

  const GURL origin = GURL("chrome-extension://app_a");
  const fileapi::FileSystemURL url_0 = CreateSyncableFileSystemURL(
      origin, file_path);
  const fileapi::FileSystemURL url_1 = CreateSyncableFileSystemURL(
      origin, folder_path);

  // Insert DrivaMetadata objects.
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      url_0,
      CreateMetadata("file:0", "1", false, false,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)));
  EXPECT_EQ(SYNC_STATUS_OK, UpdateEntry(
      url_1,
      CreateMetadata("folder:0", "2", false, true,
                     DriveMetadata_ResourceType_RESOURCE_TYPE_FOLDER)));

  // Check that DriveMetadata objects get mapped back to generalized
  // FileMetadata objects.
  scoped_ptr<base::ListValue> files = metadata_store()->DumpFiles(origin);
  ASSERT_EQ(2u, files->GetSize());

  base::DictionaryValue* file = NULL;
  std::string str;

  ASSERT_TRUE(files->GetDictionary(0, &file));
  EXPECT_TRUE(file->GetString("title", &str) && str == "file_0");
  EXPECT_TRUE(file->GetString("type", &str) && str == "file");
  EXPECT_TRUE(file->HasKey("details"));

  ASSERT_TRUE(files->GetDictionary(1, &file));
  EXPECT_TRUE(file->GetString("title", &str) && str == "folder_0");
  EXPECT_TRUE(file->GetString("type", &str) && str == "folder");
  EXPECT_TRUE(file->HasKey("details"));
}

TEST_F(DriveMetadataStoreTest, Initialization) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  InitializeDatabase();
}

TEST_F(DriveMetadataStoreTest, Initialization_WAPI) {
  SetDisableDriveAPI(true);
  InitializeDatabase();
}

TEST_F(DriveMetadataStoreTest, ReadWrite) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  ReadWrite_Body();
}

TEST_F(DriveMetadataStoreTest, ReadWrite_WAPI) {
  SetDisableDriveAPI(true);
  ReadWrite_Body();
}

TEST_F(DriveMetadataStoreTest, GetConflictURLs) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  GetConflictURLs_Body();
}

TEST_F(DriveMetadataStoreTest, GetConflictURLs_WAPI) {
  SetDisableDriveAPI(true);
  GetConflictURLs_Body();
}

TEST_F(DriveMetadataStoreTest, GetToBeFetchedFiles) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  GetToBeFetchedFiles_Body();
}

TEST_F(DriveMetadataStoreTest, GetToBeFetchedFiles_WAPI) {
  SetDisableDriveAPI(true);
  GetToBeFetchedFiles_Body();
}

TEST_F(DriveMetadataStoreTest, StoreSyncRootDirectory) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  StoreSyncRootDirectory_Body();
}

TEST_F(DriveMetadataStoreTest, StoreSyncRootDirectory_WAPI) {
  SetDisableDriveAPI(true);
  StoreSyncRootDirectory_Body();
}

TEST_F(DriveMetadataStoreTest, StoreSyncOrigin) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  StoreSyncOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, StoreSyncOrigin_WAPI) {
  SetDisableDriveAPI(true);
  StoreSyncOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, DisableOrigin) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  DisableOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, DisableOrigin_WAPI) {
  SetDisableDriveAPI(true);
  DisableOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, RemoveOrigin) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  RemoveOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, RemoveOrigin_WAPI) {
  SetDisableDriveAPI(true);
  RemoveOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, GetResourceIdForOrigin) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  GetResourceIdForOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, GetResourceIdForOrigin_WAPI) {
  SetDisableDriveAPI(true);
  GetResourceIdForOrigin_Body();
}

TEST_F(DriveMetadataStoreTest, ResetOriginRootDirectory) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  ResetOriginRootDirectory_Body();
}

TEST_F(DriveMetadataStoreTest, ResetOriginRootDirectory_WAPI) {
  SetDisableDriveAPI(true);
  ResetOriginRootDirectory_Body();
}

TEST_F(DriveMetadataStoreTest, GetFileMetadataMap) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  GetFileMetadataMap_Body();
}

TEST_F(DriveMetadataStoreTest, GetFileMetadataMap_WAPI) {
  SetDisableDriveAPI(true);
  GetFileMetadataMap_Body();
}

}  // namespace sync_file_system
