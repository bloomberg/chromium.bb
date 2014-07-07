// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
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

class RemoteToLocalSyncerTest : public testing::Test {
 public:
  typedef FakeRemoteChangeProcessor::URLToFileChangesMap URLToFileChangesMap;

  RemoteToLocalSyncerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~RemoteToLocalSyncerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    scoped_ptr<drive::FakeDriveService>
        fake_drive_service(new drive::FakeDriveService);

    scoped_ptr<drive::DriveUploaderInterface>
        drive_uploader(new drive::DriveUploader(
            fake_drive_service.get(),
            base::ThreadTaskRunnerHandle::Get().get()));
    fake_drive_helper_.reset(
        new FakeDriveServiceHelper(fake_drive_service.get(),
                                   drive_uploader.get(),
                                   kSyncRootFolderTitle));
    remote_change_processor_.reset(new FakeRemoteChangeProcessor);

    context_.reset(new SyncEngineContext(
        fake_drive_service.PassAs<drive::DriveServiceInterface>(),
        drive_uploader.Pass(),
        NULL,
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get()));
    context_->SetRemoteChangeProcessor(remote_change_processor_.get());

    RegisterSyncableFileSystem();

    sync_task_manager_.reset(new SyncTaskManager(
        base::WeakPtr<SyncTaskManager::Client>(),
        10 /* max_parallel_task */,
        base::ThreadTaskRunnerHandle::Get()));
    sync_task_manager_->Initialize(SYNC_STATUS_OK);
  }

  virtual void TearDown() OVERRIDE {
    sync_task_manager_.reset();
    RevokeSyncableFileSystem();
    fake_drive_helper_.reset();
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void InitializeMetadataDatabase() {
    SyncEngineInitializer* initializer =
        new SyncEngineInitializer(context_.get(),
                                  database_dir_.path(),
                                  in_memory_env_.get());
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_task_manager_->ScheduleSyncTask(
        FROM_HERE,
        scoped_ptr<SyncTask>(initializer),
        SyncTaskManager::PRIORITY_MED,
        base::Bind(&RemoteToLocalSyncerTest::DidInitializeMetadataDatabase,
                   base::Unretained(this),
                   initializer, &status));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  void DidInitializeMetadataDatabase(SyncEngineInitializer* initializer,
                                     SyncStatusCode* status_out,
                                     SyncStatusCode status) {
    *status_out = status;
    context_->SetMetadataDatabase(initializer->PassMetadataDatabase());
  }


  void RegisterApp(const std::string& app_id,
                   const std::string& app_root_folder_id) {
    SyncStatusCode status = SYNC_STATUS_FAILED;
    context_->GetMetadataDatabase()->RegisterApp(app_id, app_root_folder_id,
                                                 CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(SYNC_STATUS_OK, status);
  }

  MetadataDatabase* GetMetadataDatabase() {
    return context_->GetMetadataDatabase();
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

  void DeleteRemoteFile(const std::string& file_id) {
    EXPECT_EQ(google_apis::HTTP_NO_CONTENT,
              fake_drive_helper_->DeleteResource(file_id));
  }

  void CreateLocalFolder(const fileapi::FileSystemURL& url) {
    remote_change_processor_->UpdateLocalFileMetadata(
        url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                        SYNC_FILE_TYPE_DIRECTORY));
  }

  void CreateLocalFile(const fileapi::FileSystemURL& url) {
    remote_change_processor_->UpdateLocalFileMetadata(
        url, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                        SYNC_FILE_TYPE_FILE));
  }

  SyncStatusCode RunSyncer() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    scoped_ptr<RemoteToLocalSyncer>
        syncer(new RemoteToLocalSyncer(context_.get()));
    syncer->RunPreflight(SyncTaskToken::CreateForTesting(
        CreateResultReceiver(&status)));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void RunSyncerUntilIdle() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    while (status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
      status = RunSyncer();
  }

  SyncStatusCode ListChanges() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_task_manager_->ScheduleSyncTask(
        FROM_HERE,
        scoped_ptr<SyncTask>(new ListChangesTask(context_.get())),
        SyncTaskManager::PRIORITY_MED,
        CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void AppendExpectedChange(const fileapi::FileSystemURL& url,
                            FileChange::ChangeType change_type,
                            SyncFileType file_type) {
    expected_changes_[url].push_back(FileChange(change_type, file_type));
  }

  void VerifyConsistency() {
    remote_change_processor_->VerifyConsistency(expected_changes_);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir database_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;

  scoped_ptr<SyncEngineContext> context_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  scoped_ptr<FakeRemoteChangeProcessor> remote_change_processor_;

  scoped_ptr<SyncTaskManager> sync_task_manager_;

  URLToFileChangesMap expected_changes_;

  DISALLOW_COPY_AND_ASSIGN(RemoteToLocalSyncerTest);
};

TEST_F(RemoteToLocalSyncerTest, AddNewFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder1 = CreateRemoteFolder(app_root, "folder1");
  const std::string file1 = CreateRemoteFile(app_root, "file1", "data1");
  const std::string folder2 = CreateRemoteFolder(folder1, "folder2");
  const std::string file2 = CreateRemoteFile(folder1, "file2", "data2");

  RunSyncerUntilIdle();

  // Create expected changes.
  // TODO(nhiroki): Clean up creating URL part.
  AppendExpectedChange(URL(kOrigin, "folder1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "file1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);
  AppendExpectedChange(URL(kOrigin, "folder1/folder2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "folder1/file2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);

  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, DeleteFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder = CreateRemoteFolder(app_root, "folder");
  const std::string file = CreateRemoteFile(app_root, "file", "data");

  AppendExpectedChange(URL(kOrigin, "folder"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "file"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);

  RunSyncerUntilIdle();
  VerifyConsistency();

  DeleteRemoteFile(folder);
  DeleteRemoteFile(file);

  AppendExpectedChange(URL(kOrigin, "folder"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);
  AppendExpectedChange(URL(kOrigin, "file"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, DeleteNestedFiles) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder1 = CreateRemoteFolder(app_root, "folder1");
  const std::string file1 = CreateRemoteFile(app_root, "file1", "data1");
  const std::string folder2 = CreateRemoteFolder(folder1, "folder2");
  const std::string file2 = CreateRemoteFile(folder1, "file2", "data2");

  AppendExpectedChange(URL(kOrigin, "folder1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "file1"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);
  AppendExpectedChange(URL(kOrigin, "folder1/folder2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);
  AppendExpectedChange(URL(kOrigin, "folder1/file2"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_FILE);

  RunSyncerUntilIdle();
  VerifyConsistency();

  DeleteRemoteFile(folder1);

  AppendExpectedChange(URL(kOrigin, "folder1"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);
  // Changes for descendant files ("folder2" and "file2") should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFileOnFolder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateLocalFolder(URL(kOrigin, "folder"));
  CreateRemoteFile(app_root, "folder", "data");

  // Folder-File conflict happens. File creation should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();

  // Tracker for the remote file should be lowered.
  EXPECT_FALSE(GetMetadataDatabase()->GetNormalPriorityDirtyTracker(NULL));
  EXPECT_TRUE(GetMetadataDatabase()->HasLowPriorityDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFolderOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  RunSyncerUntilIdle();
  VerifyConsistency();

  CreateLocalFile(URL(kOrigin, "file"));
  CreateRemoteFolder(app_root, "file");

  // File-Folder conflict happens. Folder should override the existing file.
  AppendExpectedChange(URL(kOrigin, "file"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFolderOnFolder) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateLocalFolder(URL(kOrigin, "folder"));
  CreateRemoteFolder(app_root, "folder");

  // Folder-Folder conflict happens. Folder creation should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();

  EXPECT_FALSE(GetMetadataDatabase()->HasDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateFileOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  CreateLocalFile(URL(kOrigin, "file"));
  CreateRemoteFile(app_root, "file", "data");

  // File-File conflict happens. File creation should be ignored.

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();

  // Tracker for the remote file should be lowered.
  EXPECT_FALSE(GetMetadataDatabase()->GetNormalPriorityDirtyTracker(NULL));
  EXPECT_TRUE(GetMetadataDatabase()->HasLowPriorityDirtyTracker());
}

TEST_F(RemoteToLocalSyncerTest, Conflict_CreateNestedFolderOnFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  RunSyncerUntilIdle();
  VerifyConsistency();

  const std::string folder = CreateRemoteFolder(app_root, "folder");
  CreateLocalFile(URL(kOrigin, "/folder"));
  CreateRemoteFile(folder, "file", "data");

  // File-Folder conflict happens. Folder should override the existing file.
  AppendExpectedChange(URL(kOrigin, "/folder"),
                       FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                       SYNC_FILE_TYPE_DIRECTORY);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();
}

TEST_F(RemoteToLocalSyncerTest, AppRootDeletion) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateRemoteFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  RunSyncerUntilIdle();
  VerifyConsistency();

  DeleteRemoteFile(app_root);

  AppendExpectedChange(URL(kOrigin, "/"),
                       FileChange::FILE_CHANGE_DELETE,
                       SYNC_FILE_TYPE_UNKNOWN);

  EXPECT_EQ(SYNC_STATUS_OK, ListChanges());
  RunSyncerUntilIdle();
  VerifyConsistency();

  // SyncEngine will re-register the app and resurrect the app root later.
}

}  // namespace drive_backend
}  // namespace sync_file_system
