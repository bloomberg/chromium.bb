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
#include "chrome/browser/sync_file_system/fake_remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

class RemoteToLocalSyncerTest : public testing::Test,
                                public SyncEngineContext {
 public:
  typedef FakeRemoteChangeProcessor::URLToFileChangesMap URLToFileChangesMap;

  RemoteToLocalSyncerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~RemoteToLocalSyncerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());

    fake_drive_service_.reset(new drive::FakeDriveService);
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "gdata/empty_feed.json"));

    drive_uploader_.reset(new drive::DriveUploader(
        fake_drive_service_.get(), base::MessageLoopProxy::current().get()));
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

  fileapi::FileSystemURL BuildURLForOriginAndFileID(
      const GURL& origin,
      const std::string& file_id) {
    TrackerSet trackers;
    if (!metadata_database_->FindTrackersByFileID(file_id, &trackers) ||
        !trackers.has_active()) {
      return fileapi::FileSystemURL();
    }

    FileTracker* tracker = trackers.active_tracker();
    base::FilePath path;
    if (!metadata_database_->BuildPathForTracker(
            tracker->tracker_id(), &path)) {
      return fileapi::FileSystemURL();
    }
    return CreateSyncableFileSystemURL(origin, path);
  }

  SyncStatusCode MakeTrackerDirtyByFileID(const std::string& file_id) {
    TrackerSet trackers;
    bool result = metadata_database_->FindTrackersByFileID(file_id, &trackers);
    if (!result || !trackers.has_active())
      return SYNC_FILE_ERROR_NOT_FOUND;

    FileTracker* tracker = trackers.active_tracker();
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_->MarkTrackerDirty(tracker->tracker_id(),
                                         CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  fileapi::FileSystemURL CreateURL(const GURL& origin,
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

  std::string CreateFile(const std::string& parent_folder_id,
                         const std::string& title,
                         const std::string& content) {
    std::string file_id;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->AddFile(
                  parent_folder_id, title, content, &file_id));
    return file_id;
  }

  void DeleteFile(const std::string& file_id) {
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->RemoveResource(file_id));
  }

  SyncStatusCode RunSyncer() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    scoped_ptr<RemoteToLocalSyncer> syncer(
        new RemoteToLocalSyncer(this, RemoteToLocalSyncer::PRIORITY_NORMAL));
    syncer->Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void RunSyncerUntilIdle() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    while (status != SYNC_STATUS_NO_CHANGE_TO_SYNC)
      status = RunSyncer();
  }

  SyncStatusCode ListChanges() {
    ListChangesTask list_changes(this);
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    list_changes.Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void VerifyConsistency(const URLToFileChangesMap& expected_changes) {
    URLToFileChangesMap applied_changes =
        fake_remote_change_processor_->GetAppliedRemoteChanges();
    EXPECT_EQ(expected_changes.size(), applied_changes.size());

    for (URLToFileChangesMap::const_iterator itr = applied_changes.begin();
         itr != applied_changes.end(); ++itr) {
      const fileapi::FileSystemURL& url = itr->first;
      URLToFileChangesMap::const_iterator found = expected_changes.find(url);
      if (found == expected_changes.end()) {
        EXPECT_TRUE(found != expected_changes.end());
        continue;
      }

      if (itr->second.empty() || found->second.empty()) {
        EXPECT_TRUE(!itr->second.empty());
        EXPECT_TRUE(!found->second.empty());
        continue;
      }

      EXPECT_EQ(found->second.back().change(),
                itr->second.back().change()) << url.DebugString();
      EXPECT_EQ(found->second.back().file_type(),
                itr->second.back().file_type()) << url.DebugString();
    }
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir database_dir_;

  scoped_ptr<drive::FakeDriveService> fake_drive_service_;
  scoped_ptr<drive::DriveUploader> drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_helper_;
  scoped_ptr<MetadataDatabase> metadata_database_;
  scoped_ptr<FakeRemoteChangeProcessor> fake_remote_change_processor_;

  DISALLOW_COPY_AND_ASSIGN(RemoteToLocalSyncerTest);
};

TEST_F(RemoteToLocalSyncerTest, AddNewFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder1 = CreateFolder(app_root, "folder1");
  const std::string file1 = CreateFile(app_root, "file1", "data1");
  const std::string folder2 = CreateFolder(folder1, "folder2");
  const std::string file2 = CreateFile(folder1, "file2", "data2");

  RunSyncerUntilIdle();

  URLToFileChangesMap expected_changes;

  // Create expected changes.
  // TODO(nhiroki): Clean up creating URL part.
  expected_changes[CreateURL(kOrigin, "/")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/folder1")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/file1")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE));
  expected_changes[CreateURL(kOrigin, "/folder1/folder2")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/folder1/file2")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE));

  VerifyConsistency(expected_changes);
}

TEST_F(RemoteToLocalSyncerTest, DeleteFile) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder = CreateFolder(app_root, "folder");
  const std::string file = CreateFile(app_root, "file", "data");

  URLToFileChangesMap expected_changes;
  expected_changes[CreateURL(kOrigin, "/")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/folder")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/file")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE));

  RunSyncerUntilIdle();
  VerifyConsistency(expected_changes);

  DeleteFile(folder);
  DeleteFile(file);

  expected_changes[CreateURL(kOrigin, "/folder")].push_back(
      FileChange(FileChange::FILE_CHANGE_DELETE,
                 SYNC_FILE_TYPE_UNKNOWN));
  expected_changes[CreateURL(kOrigin, "/file")].push_back(
      FileChange(FileChange::FILE_CHANGE_DELETE,
                 SYNC_FILE_TYPE_UNKNOWN));

  ListChanges();
  RunSyncerUntilIdle();
  VerifyConsistency(expected_changes);
}

TEST_F(RemoteToLocalSyncerTest, DeleteNestedFiles) {
  const GURL kOrigin("chrome-extension://example");
  const std::string sync_root = CreateSyncRoot();
  const std::string app_root = CreateFolder(sync_root, kOrigin.host());
  InitializeMetadataDatabase();
  RegisterApp(kOrigin.host(), app_root);

  const std::string folder1 = CreateFolder(app_root, "folder1");
  const std::string file1 = CreateFile(app_root, "file1", "data1");
  const std::string folder2 = CreateFolder(folder1, "folder2");
  const std::string file2 = CreateFile(folder1, "file2", "data2");

  URLToFileChangesMap expected_changes;
  expected_changes[CreateURL(kOrigin, "/")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/folder1")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/file1")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE));
  expected_changes[CreateURL(kOrigin, "/folder1/folder2")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY));
  expected_changes[CreateURL(kOrigin, "/folder1/file2")].push_back(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_FILE));

  RunSyncerUntilIdle();
  VerifyConsistency(expected_changes);

  DeleteFile(folder1);

  expected_changes[CreateURL(kOrigin, "/folder1")].push_back(
      FileChange(FileChange::FILE_CHANGE_DELETE,
                 SYNC_FILE_TYPE_UNKNOWN));
  // Changes for descendant files ("folder2" and "file2") should be ignored.

  ListChanges();
  RunSyncerUntilIdle();
  VerifyConsistency(expected_changes);
}

}  // namespace drive_backend
}  // namespace sync_file_system
