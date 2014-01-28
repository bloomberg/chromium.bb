// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_uploader.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_errorcode.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

fileapi::FileSystemURL URL(const GURL& origin,
                           const std::string& path) {
  return CreateSyncableFileSystemURL(
      origin, base::FilePath::FromUTF8Unsafe(path));
}

}  // namespace

class LocalToRemoteSyncerTest : public testing::Test,
                                public SyncEngineContext {
 public:
  LocalToRemoteSyncerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~LocalToRemoteSyncerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    fake_drive_service_.reset(new FakeDriveServiceWrapper);
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "gdata/empty_feed.json"));

    drive_uploader_.reset(new FakeDriveUploader(fake_drive_service_.get()));
    fake_drive_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service_.get(), drive_uploader_.get(),
        kSyncRootFolderTitle));
    fake_remote_change_processor_.reset(new FakeRemoteChangeProcessor);

    RegisterSyncableFileSystem();
  }

  virtual void TearDown() OVERRIDE {
    RevokeSyncableFileSystem();

    fake_remote_change_processor_.reset();
    metadata_database_.reset();
    fake_drive_helper_.reset();
    drive_uploader_.reset();
    fake_drive_service_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void InitializeMetadataDatabase() {
    SyncEngineInitializer initializer(this,
                                      base::MessageLoopProxy::current(),
                                      fake_drive_service_.get(),
                                      database_dir_.path(),
                                      in_memory_env_.get());
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    initializer.Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, status);
    metadata_database_ = initializer.PassMetadataDatabase();
  }

  void RegisterApp(const std::string& app_id,
                   const std::string& app_root_folder_id) {
    SyncStatusCode status = SYNC_STATUS_FAILED;
    metadata_database_->RegisterApp(app_id, app_root_folder_id,
                                    CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  virtual drive::DriveServiceInterface* GetDriveService() OVERRIDE {
    return fake_drive_service_.get();
  }

  virtual drive::DriveUploaderInterface* GetDriveUploader() OVERRIDE {
    return drive_uploader_.get();
  }

  virtual MetadataDatabase* GetMetadataDatabase() OVERRIDE {
    return metadata_database_.get();
  }

  virtual RemoteChangeProcessor* GetRemoteChangeProcessor() OVERRIDE {
    return fake_remote_change_processor_.get();
  }

  virtual base::SequencedTaskRunner* GetBlockingTaskRunner() OVERRIDE {
    return base::MessageLoopProxy::current().get();
  }

 protected:
  std::string CreateSyncRoot() {
    std::string sync_root_folder_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id));
    return sync_root_folder_id;
  }

  std::string CreateRemoteFolder(const std::string& parent_folder_id,
                                 const std::string& title) {
    std::string folder_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddFolder(
                  parent_folder_id, title, &folder_id));
    return folder_id;
  }

  std::string CreateRemoteFile(const std::string& parent_folder_id,
                               const std::string& title,
                               const std::string& content) {
    std::string file_id;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->AddFile(
                  parent_folder_id, title, content, &file_id));
    return file_id;
  }

  void DeleteResource(const std::string& file_id) {
    EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
              fake_drive_helper_->DeleteResource(file_id));
  }

  SyncStatusCode RunLocalToRemoteSyncer(FileChange file_change,
                           const fileapi::FileSystemURL& url) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    base::FilePath local_path = base::FilePath::FromUTF8Unsafe("dummy");
    scoped_ptr<LocalToRemoteSyncer> syncer(new LocalToRemoteSyncer(
        this, SyncFileMetadata(file_change.file_type(), 0, base::Time()),
        file_change, local_path, url));
    syncer->Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  SyncStatusCode ListChanges() {
    ListChangesTask list_changes(this);
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    list_changes.Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  SyncStatusCode RunRemoteToLocalSyncer() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    scoped_ptr<RemoteToLocalSyncer> syncer(new RemoteToLocalSyncer(this));
    syncer->Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  ScopedVector<google_apis::ResourceEntry>
  GetResourceEntriesForParentAndTitle(const std::string& parent_folder_id,
                                      const std::string& title) {
    ScopedVector<google_apis::ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    return entries.Pass();
  }

  std::string GetFileIDForParentAndTitle(const std::string& parent_folder_id,
                                         const std::string& title) {
    ScopedVector<google_apis::ResourceEntry> entries =
        GetResourceEntriesForParentAndTitle(parent_folder_id, title);
    if (entries.size() != 1)
      return std::string();
    return entries[0]->resource_id();
  }

  void VerifyTitleUniqueness(const std::string& parent_folder_id,
                             const std::string& title,
                             google_apis::DriveEntryKind kind) {
    ScopedVector<google_apis::ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(kind, entries[0]->kind());
  }

  void VerifyFileDeletion(const std::string& parent_folder_id,
                          const std::string& title) {
    ScopedVector<google_apis::ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    EXPECT_TRUE(entries.empty());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir database_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;

  scoped_ptr<FakeDriveServiceWrapper> fake_drive_service_;
  scoped_ptr<FakeDriveUploader> drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  scoped_ptr<MetadataDatabase> metadata_database_;
  scoped_ptr<FakeRemoteChangeProcessor> fake_remote_change_processor_;

  DISALLOW_COPY_AND_ASSIGN(LocalToRemoteSyncerTest);
};

TEST_F(LocalToRemoteSyncerTest, CreateFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "file1")));
  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      URL(kOrigin, "folder")));
  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "folder/file2")));

  std::string folder_id = GetFileIDForParentAndTitle(app_root, "folder");
  ASSERT_FALSE(folder_id.empty());

  VerifyTitleUniqueness(app_root, "file1", google_apis::ENTRY_KIND_FILE);
  VerifyTitleUniqueness(app_root, "folder", google_apis::ENTRY_KIND_FOLDER);
  VerifyTitleUniqueness(folder_id, "file2", google_apis::ENTRY_KIND_FILE);
}

TEST_F(LocalToRemoteSyncerTest, CreateFileOnMissingPath) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  // Run the syncer 3 times to create missing folder1 and folder2.
  EXPECT_EQ(SYNC_STATUS_RETRY, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "folder1/folder2/file")));
  EXPECT_EQ(SYNC_STATUS_RETRY, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "folder1/folder2/file")));
  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "folder1/folder2/file")));

  std::string folder_id1 = GetFileIDForParentAndTitle(app_root, "folder1");
  ASSERT_FALSE(folder_id1.empty());
  std::string folder_id2 = GetFileIDForParentAndTitle(folder_id1, "folder2");
  ASSERT_FALSE(folder_id2.empty());

  VerifyTitleUniqueness(app_root, "folder1", google_apis::ENTRY_KIND_FOLDER);
  VerifyTitleUniqueness(folder_id1, "folder2", google_apis::ENTRY_KIND_FOLDER);
  VerifyTitleUniqueness(folder_id2, "file", google_apis::ENTRY_KIND_FILE);
}

TEST_F(LocalToRemoteSyncerTest, DeleteFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "file")));
  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      URL(kOrigin, "folder")));

  VerifyTitleUniqueness(app_root, "file", google_apis::ENTRY_KIND_FILE);
  VerifyTitleUniqueness(app_root, "folder", google_apis::ENTRY_KIND_FOLDER);

  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_DELETE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "file")));
  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_DELETE,
                 SYNC_FILE_TYPE_DIRECTORY),
      URL(kOrigin, "folder")));

  VerifyFileDeletion(app_root, "file");
  VerifyFileDeletion(app_root, "folder");
}

TEST_F(LocalToRemoteSyncerTest, Conflict_CreateFileOnFolder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateRemoteFolder(app_root, "foo");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "foo")));

  // There should exist both file and folder on remote.
  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, "foo");
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(google_apis::ENTRY_KIND_FOLDER, entries[0]->kind());
  EXPECT_EQ(google_apis::ENTRY_KIND_FILE, entries[1]->kind());
}

TEST_F(LocalToRemoteSyncerTest, Conflict_CreateFolderOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateRemoteFile(app_root, "foo", "data");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());

  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      URL(kOrigin, "foo")));

  // There should exist both file and folder on remote.
  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, "foo");
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(google_apis::ENTRY_KIND_FILE, entries[0]->kind());
  EXPECT_EQ(google_apis::ENTRY_KIND_FOLDER, entries[1]->kind());
}

TEST_F(LocalToRemoteSyncerTest, Conflict_CreateFileOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateRemoteFile(app_root, "foo", "data");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());

  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "foo")));

  // There should exist both files on remote.
  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, "foo");
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(google_apis::ENTRY_KIND_FILE, entries[0]->kind());
  EXPECT_EQ(google_apis::ENTRY_KIND_FILE, entries[1]->kind());
}

TEST_F(LocalToRemoteSyncerTest, Conflict_UpdateDeleteOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string file_id = CreateRemoteFile(app_root, "foo", "data");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());

  SyncStatusCode status;
  do {
    status = RunRemoteToLocalSyncer();
    EXPECT_TRUE(status == SYNC_STATUS_OK ||
                status == SYNC_STATUS_RETRY ||
                status == SYNC_STATUS_NO_CHANGE_TO_SYNC);
  } while (status != SYNC_STATUS_NO_CHANGE_TO_SYNC);

  DeleteResource(file_id);

  EXPECT_EQ(SYNC_STATUS_FILE_BUSY, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "foo")));
  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "foo")));

  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, "foo");
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(google_apis::ENTRY_KIND_FILE, entries[0]->kind());
  EXPECT_TRUE(!entries[0]->deleted());
  EXPECT_NE(file_id, entries[0]->resource_id());
}

TEST_F(LocalToRemoteSyncerTest, Conflict_CreateDeleteOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string file_id = CreateRemoteFile(app_root, "foo", "data");
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  SyncStatusCode status;
  do {
    status = RunRemoteToLocalSyncer();
    EXPECT_TRUE(status == SYNC_STATUS_OK ||
                status == SYNC_STATUS_RETRY ||
                status == SYNC_STATUS_NO_CHANGE_TO_SYNC);
  } while (status != SYNC_STATUS_NO_CHANGE_TO_SYNC);

  DeleteResource(file_id);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());

  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "foo")));

  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, "foo");
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(google_apis::ENTRY_KIND_FILE, entries[0]->kind());
  EXPECT_TRUE(!entries[0]->deleted());
  EXPECT_NE(file_id, entries[0]->resource_id());
}

TEST_F(LocalToRemoteSyncerTest, Conflict_CreateFolderOnFolder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder_id = CreateRemoteFolder(app_root, "foo");

  EXPECT_EQ(SYNC_STATUS_OK, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      URL(kOrigin, "foo")));

  ScopedVector<google_apis::ResourceEntry> entries =
      GetResourceEntriesForParentAndTitle(app_root, "foo");
  ASSERT_EQ(2u, entries.size());
  EXPECT_EQ(google_apis::ENTRY_KIND_FOLDER, entries[0]->kind());
  EXPECT_EQ(google_apis::ENTRY_KIND_FOLDER, entries[1]->kind());
  EXPECT_TRUE(!entries[0]->deleted());
  EXPECT_TRUE(!entries[1]->deleted());
  EXPECT_TRUE(folder_id == entries[0]->resource_id() ||
              folder_id == entries[1]->resource_id());

  TrackerSet trackers;
  EXPECT_TRUE(GetMetadataDatabase()->FindTrackersByFileID(
      folder_id, &trackers));
  EXPECT_EQ(1u, trackers.size());
  ASSERT_TRUE(trackers.has_active());
}

TEST_F(LocalToRemoteSyncerTest, AppRootDeletion) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  DeleteResource(app_root);
  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  SyncStatusCode status;
  do {
    status = RunRemoteToLocalSyncer();
    EXPECT_TRUE(status == SYNC_STATUS_OK ||
                status == SYNC_STATUS_RETRY ||
                status == SYNC_STATUS_NO_CHANGE_TO_SYNC);
  } while (status != SYNC_STATUS_NO_CHANGE_TO_SYNC);

  EXPECT_EQ(SYNC_STATUS_UNKNOWN_ORIGIN, RunLocalToRemoteSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      URL(kOrigin, "foo")));

  // SyncEngine will re-register the app and resurrect the app root later.
}

}  // namespace drive_backend
}  // namespace sync_file_system
