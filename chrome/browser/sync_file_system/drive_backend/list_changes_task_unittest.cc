// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

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

    fake_drive_service_.reset(new drive::FakeDriveService);
    ASSERT_TRUE(fake_drive_service_->LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_->LoadResourceListForWapi(
        "gdata/empty_feed.json"));

    drive_uploader_.reset(new drive::DriveUploader(
        fake_drive_service_.get(), base::MessageLoopProxy::current()));

    fake_drive_service_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service_.get(), drive_uploader_.get()));

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

  virtual MetadataDatabase* GetMetadataDatabase() OVERRIDE {
    return metadata_database_.get();
  }

  int64 GetRemoteLargestChangeID() {
    scoped_ptr<google_apis::AboutResource> about_resource;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->GetAboutResource(&about_resource));
    return about_resource->largest_change_id();
  }

 protected:
  SyncStatusCode RunTask(SyncTask* sync_task) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    sync_task->Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  size_t CountDirtyTracker() {
    return metadata_database_->dirty_trackers_.size();
  }

  FakeDriveServiceHelper* fake_drive_service_helper() {
    return fake_drive_service_helper_.get();
  }

  drive::FakeDriveService* fake_drive_service() {
    return fake_drive_service_.get();
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


    std::string trashed_file_id;
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->AddFile(
                  folder_id, "trashed file", "file content",
                  &trashed_file_id));
    ASSERT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper()->RemoveResource(trashed_file_id));
  }

  std::string root_resource_id() {
    return fake_drive_service_->GetRootResourceId();
  }

  std::string sync_root_folder_id() {
    return sync_root_folder_id_;
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
    SyncEngineInitializer initializer(base::MessageLoopProxy::current(),
                                      fake_drive_service_.get(),
                                      database_dir_.path());
    EXPECT_EQ(SYNC_STATUS_OK, RunTask(&initializer));
    metadata_database_ = initializer.PassMetadataDatabase();
  }

  void RegisterApp(const std::string& app_id) {
    RegisterAppTask register_app(this, app_id);
    EXPECT_EQ(SYNC_STATUS_OK, RunTask(&register_app));
  }

  std::string GenerateFileID() {
    return base::StringPrintf("file_id_%" PRId64, next_file_id_++);
  }

  std::string sync_root_folder_id_;
  std::string app_root_folder_id_;
  std::string unregistered_app_root_folder_id_;

  int64 next_file_id_;
  int64 next_tracker_id_;

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
  EXPECT_EQ(SYNC_STATUS_OK, RunTask(&list_changes));

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
