// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/fake_drive_uploader.h"
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

class LocalToRemoteSyncerTest : public testing::Test,
                                public SyncEngineContext {
 public:
  LocalToRemoteSyncerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~LocalToRemoteSyncerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());

    fake_drive_service_.reset(new FakeDriveServiceWrapper);
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "gdata/empty_feed.json"));

    drive_uploader_.reset(new FakeDriveUploader(fake_drive_service_.get()));
    fake_drive_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service_.get(), drive_uploader_.get()));
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
    SyncEngineInitializer initializer(base::MessageLoopProxy::current(),
                                      fake_drive_service_.get(),
                                      database_dir_.path());
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

  fileapi::FileSystemURL URL(const GURL& origin,
                             const std::string& path) {
    return CreateSyncableFileSystemURL(
        origin, base::FilePath::FromUTF8Unsafe(path));
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

  std::string CreateFolder(const std::string& parent_folder_id,
                           const std::string& app_id) {
    std::string folder_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddFolder(
                  parent_folder_id, app_id, &folder_id));
    return folder_id;
  }

  SyncStatusCode RunSyncer(FileChange file_change,
                           const fileapi::FileSystemURL& url) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    base::FilePath local_path = base::FilePath::FromUTF8Unsafe("dummy");
    scoped_ptr<LocalToRemoteSyncer> syncer(new LocalToRemoteSyncer(
        this, file_change, local_path, url));
    syncer->Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void VerifyRemoteFile(const std::string& parent_folder_id,
                        const std::string& title,
                        google_apis::DriveEntryKind kind) {
    ScopedVector<google_apis::ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_folder_id, title, &entries));
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(kind, entries[0]->kind());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir database_dir_;

  scoped_ptr<FakeDriveServiceWrapper> fake_drive_service_;
  scoped_ptr<FakeDriveUploader> drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  scoped_ptr<MetadataDatabase> metadata_database_;
  scoped_ptr<FakeRemoteChangeProcessor> fake_remote_change_processor_;

  DISALLOW_COPY_AND_ASSIGN(LocalToRemoteSyncerTest);
};

TEST_F(LocalToRemoteSyncerTest, CreateLocalFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  EXPECT_EQ(SYNC_STATUS_OK, RunSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE),
      URL(kOrigin, "file")));
  EXPECT_EQ(SYNC_STATUS_OK, RunSyncer(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      URL(kOrigin, "folder")));

  VerifyRemoteFile(app_root, "file", google_apis::ENTRY_KIND_FILE);
  VerifyRemoteFile(app_root, "folder", google_apis::ENTRY_KIND_FOLDER);

  // TODO(nhiroki): Test nested files case.
}

}  // namespace drive_backend
}  // namespace sync_file_system
