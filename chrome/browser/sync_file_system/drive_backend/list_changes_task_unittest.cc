// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kAppID[] = "app_id";
const char kUnregisteredAppID[] = "app_id unregistered";

}  // namespace

class ListChangesTaskTest : public testing::Test,
                            public SyncEngineContext {
 public:
  ListChangesTaskTest() {}
  virtual ~ListChangesTaskTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    fake_drive_service_.reset(new drive::FakeDriveService);
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "gdata/empty_feed.json"));

    drive_uploader_.reset(new drive::DriveUploader(
        fake_drive_service_.get(), base::MessageLoopProxy::current()));

    fake_drive_service_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service_.get(), drive_uploader_.get(),
        kSyncRootFolderTitle));

    SetUpRemoteFolders();
    InitializeMetadataDatabase();
    RegisterApp(kAppID);
  }

  virtual void TearDown() OVERRIDE {
    metadata_database_.reset();
    base::RunLoop().RunUntilIdle();
  }

  virtual drive::DriveServiceInterface* GetDriveService() OVERRIDE {
    return fake_drive_service_.get();
  }

  virtual drive::DriveUploader* GetDriveUploader() OVERRIDE {
    return NULL;
  }

  virtual MetadataDatabase* GetMetadataDatabase() OVERRIDE {
    return metadata_database_.get();
  }

  virtual RemoteChangeProcessor* GetRemoteChangeProcessor() OVERRIDE {
    return NULL;
  }

  virtual base::SequencedTaskRunner* GetBlockingTaskRunner() OVERRIDE {
    return base::MessageLoopProxy::current();
  }

 protected:
  SyncStatusCode RunTask(SequentialSyncTask* sync_task) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_task->RunSequential(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  size_t CountDirtyTracker() {
    return metadata_database_->CountDirtyTracker();
  }

  FakeDriveServiceHelper* fake_drive_service_helper() {
    return fake_drive_service_helper_.get();
  }

  void SetUpChangesInFolder(const std::string& folder_id) {
    std::string new_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "new file", "file contents", &new_file_id));
    std::string same_name_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "new file", "file contents",
                  &same_name_file_id));

    std::string new_folder_id;
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper()->AddFolder(
                  folder_id, "new folder", &new_folder_id));

    std::string modified_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "modified file", "file content",
                  &modified_file_id));
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->UpdateFile(
                  modified_file_id, "modified file content"));


    std::string deleted_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "trashed file", "file content",
                  &deleted_file_id));
    ASSERT_EQ(google_apis::HTTP_NO_CONTENT,
              fake_drive_service_helper()->DeleteResource(deleted_file_id));
  }

  std::string root_resource_id() {
    return fake_drive_service_->GetRootResourceId();
  }

  std::string app_root_folder_id() {
    return app_root_folder_id_;
  }

  std::string unregistered_app_root_folder_id() {
    return unregistered_app_root_folder_id_;
  }

 private:
  void SetUpRemoteFolders() {
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id_));
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddFolder(
                  sync_root_folder_id_, kAppID, &app_root_folder_id_));
    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddFolder(
                  sync_root_folder_id_, kUnregisteredAppID,
                  &unregistered_app_root_folder_id_));
  }

  void InitializeMetadataDatabase() {
    SyncEngineInitializer initializer(this,
                                      base::MessageLoopProxy::current(),
                                      fake_drive_service_.get(),
                                      database_dir_.path(),
                                      in_memory_env_.get());
    EXPECT_EQ(SYNC_STATUS_OK, RunTask(&initializer));
    metadata_database_ = initializer.PassMetadataDatabase();
  }

  void RegisterApp(const std::string& app_id) {
    RegisterAppTask register_app(this, app_id);
    EXPECT_EQ(SYNC_STATUS_OK, RunTask(&register_app));
  }

  scoped_ptr<leveldb::Env> in_memory_env_;

  std::string sync_root_folder_id_;
  std::string app_root_folder_id_;
  std::string unregistered_app_root_folder_id_;

  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir database_dir_;

  scoped_ptr<drive::FakeDriveService> fake_drive_service_;
  scoped_ptr<drive::DriveUploader> drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_service_helper_;

  scoped_ptr<MetadataDatabase> metadata_database_;

  DISALLOW_COPY_AND_ASSIGN(ListChangesTaskTest);
};

TEST_F(ListChangesTaskTest, NoChange) {
  size_t num_dirty_trackers = CountDirtyTracker();

  ListChangesTask list_changes(this);
  EXPECT_EQ(SYNC_STATUS_NO_CHANGE_TO_SYNC, RunTask(&list_changes));

  EXPECT_EQ(num_dirty_trackers, CountDirtyTracker());
}

TEST_F(ListChangesTaskTest, UnrelatedChange) {
  size_t num_dirty_trackers = CountDirtyTracker();

  SetUpChangesInFolder(root_resource_id());
  SetUpChangesInFolder(unregistered_app_root_folder_id());

  ListChangesTask list_changes(this);
  EXPECT_EQ(SYNC_STATUS_OK, RunTask(&list_changes));

  EXPECT_EQ(num_dirty_trackers, CountDirtyTracker());
}

TEST_F(ListChangesTaskTest, UnderTrackedFolder) {
  size_t num_dirty_trackers = CountDirtyTracker();

  SetUpChangesInFolder(app_root_folder_id());

  ListChangesTask list_changes(this);
  EXPECT_EQ(SYNC_STATUS_OK, RunTask(&list_changes));

  EXPECT_EQ(num_dirty_trackers + 4, CountDirtyTracker());
}

}  // namespace drive_backend
}  // namespace sync_file_system
