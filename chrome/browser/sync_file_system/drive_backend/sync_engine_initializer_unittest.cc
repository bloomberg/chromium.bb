// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const int64 kInitialLargestChangeID = 1234;

}  // namespace

class SyncEngineInitializerTest : public testing::Test {
 public:
  struct TrackedFile {
    scoped_ptr<google_apis::FileResource> resource;
    FileMetadata metadata;
    FileTracker tracker;
  };

  SyncEngineInitializerTest() {}
  virtual ~SyncEngineInitializerTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    scoped_ptr<drive::DriveServiceInterface>
        fake_drive_service(new drive::FakeDriveService);

    sync_context_.reset(new SyncEngineContext(
        fake_drive_service.Pass(),
        scoped_ptr<drive::DriveUploaderInterface>(),
        NULL,
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get()));

    sync_task_manager_.reset(new SyncTaskManager(
        base::WeakPtr<SyncTaskManager::Client>(),
        1 /* maximum_parallel_task */,
        base::ThreadTaskRunnerHandle::Get()));
    sync_task_manager_->Initialize(SYNC_STATUS_OK);
  }

  virtual void TearDown() OVERRIDE {
    sync_task_manager_.reset();
    metadata_database_.reset();
    sync_context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath database_path() {
    return database_dir_.path();
  }

  SyncStatusCode RunInitializer() {
    SyncEngineInitializer* initializer =
        new SyncEngineInitializer(sync_context_.get(),
                                  database_path(),
                                  in_memory_env_.get());
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;

    sync_task_manager_->ScheduleSyncTask(
        FROM_HERE,
        scoped_ptr<SyncTask>(initializer),
        SyncTaskManager::PRIORITY_MED,
        base::Bind(&SyncEngineInitializerTest::DidRunInitializer,
                   base::Unretained(this), initializer, &status));

    base::RunLoop().RunUntilIdle();
    return status;
  }

  void DidRunInitializer(SyncEngineInitializer* initializer,
                         SyncStatusCode* status_out,
                         SyncStatusCode status) {
    *status_out = status;
    metadata_database_ = initializer->PassMetadataDatabase();
  }

  SyncStatusCode PopulateDatabase(
      const google_apis::FileResource& sync_root,
      const google_apis::FileResource** app_roots,
      size_t app_roots_count) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    scoped_ptr<MetadataDatabase> database;
    MetadataDatabase::Create(
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        database_path(),
        in_memory_env_.get(),
        CreateResultReceiver(&status, &database));
    base::RunLoop().RunUntilIdle();
    if (status != SYNC_STATUS_OK)
      return status;

    // |app_root_list| must not own the resources here. Be sure to call
    // weak_clear later.
    ScopedVector<google_apis::FileResource> app_root_list;
    for (size_t i = 0; i < app_roots_count; ++i) {
      app_root_list.push_back(
          const_cast<google_apis::FileResource*>(app_roots[i]));
    }

    status = SYNC_STATUS_UNKNOWN;
    database->PopulateInitialData(kInitialLargestChangeID,
                                  sync_root,
                                  app_root_list,
                                  CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();

    app_root_list.weak_clear();

    return SYNC_STATUS_OK;
  }

  scoped_ptr<google_apis::FileResource> CreateRemoteFolder(
      const std::string& parent_folder_id,
      const std::string& title) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::FileResource> entry;
    sync_context_->GetDriveService()->AddNewDirectory(
        parent_folder_id, title,
        drive::DriveServiceInterface::AddNewDirectoryOptions(),
        CreateResultReceiver(&error, &entry));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(google_apis::HTTP_CREATED, error);
    return entry.Pass();
  }

  scoped_ptr<google_apis::FileResource> CreateRemoteSyncRoot() {
    scoped_ptr<google_apis::FileResource> sync_root(
        CreateRemoteFolder(std::string(), kSyncRootFolderTitle));

    for (size_t i = 0; i < sync_root->parents().size(); ++i) {
      google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
      sync_context_->GetDriveService()->RemoveResourceFromDirectory(
          sync_root->parents()[i].file_id(),
          sync_root->file_id(),
          CreateResultReceiver(&error));
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(google_apis::HTTP_NO_CONTENT, error);
    }

    return sync_root.Pass();
  }

  std::string GetSyncRootFolderID() {
    int64 sync_root_tracker_id = metadata_database_->GetSyncRootTrackerID();
    FileTracker sync_root_tracker;
    EXPECT_TRUE(metadata_database_->FindTrackerByTrackerID(
        sync_root_tracker_id, &sync_root_tracker));
    return sync_root_tracker.file_id();
  }

  size_t CountTrackersForFile(const std::string& file_id) {
    TrackerIDSet trackers;
    metadata_database_->FindTrackersByFileID(file_id, &trackers);
    return trackers.size();
  }

  bool HasActiveTracker(const std::string& file_id) {
    TrackerIDSet trackers;
    return metadata_database_->FindTrackersByFileID(file_id, &trackers) &&
        trackers.has_active();
  }

  bool HasNoParent(const std::string& file_id) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::FileResource> entry;
    sync_context_->GetDriveService()->GetFileResource(
        file_id,
        CreateResultReceiver(&error, &entry));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
    return entry->parents().empty();
  }

  size_t CountFileMetadata() {
    return metadata_database_->CountFileMetadata();
  }

  size_t CountFileTracker() {
    return metadata_database_->CountFileTracker();
  }

  google_apis::GDataErrorCode AddParentFolder(
      const std::string& new_parent_folder_id,
      const std::string& file_id) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    sync_context_->GetDriveService()->AddResourceToDirectory(
        new_parent_folder_id, file_id,
        CreateResultReceiver(&error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

 private:
  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir database_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;

  scoped_ptr<MetadataDatabase> metadata_database_;
  scoped_ptr<SyncTaskManager> sync_task_manager_;
  scoped_ptr<SyncEngineContext> sync_context_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineInitializerTest);
};

TEST_F(SyncEngineInitializerTest, EmptyDatabase_NoRemoteSyncRoot) {
  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  std::string sync_root_folder_id = GetSyncRootFolderID();
  EXPECT_EQ(1u, CountTrackersForFile(sync_root_folder_id));

  EXPECT_TRUE(HasActiveTracker(sync_root_folder_id));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_RemoteSyncRootExists) {
  scoped_ptr<google_apis::FileResource> sync_root(
      CreateRemoteSyncRoot());
  scoped_ptr<google_apis::FileResource> app_root_1(
      CreateRemoteFolder(sync_root->file_id(), "app-root 1"));
  scoped_ptr<google_apis::FileResource> app_root_2(
      CreateRemoteFolder(sync_root->file_id(), "app-root 2"));

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_1->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_2->file_id()));

  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_1->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_2->file_id()));

  EXPECT_EQ(3u, CountFileMetadata());
  EXPECT_EQ(3u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, DatabaseAlreadyInitialized) {
  scoped_ptr<google_apis::FileResource> sync_root(CreateRemoteSyncRoot());
  scoped_ptr<google_apis::FileResource> app_root_1(
      CreateRemoteFolder(sync_root->file_id(), "app-root 1"));
  scoped_ptr<google_apis::FileResource> app_root_2(
      CreateRemoteFolder(sync_root->file_id(), "app-root 2"));

  const google_apis::FileResource* app_roots[] = {
    app_root_1.get(), app_root_2.get()
  };
  EXPECT_EQ(SYNC_STATUS_OK,
            PopulateDatabase(*sync_root, app_roots, arraysize(app_roots)));

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_1->file_id()));
  EXPECT_EQ(1u, CountTrackersForFile(app_root_2->file_id()));

  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_1->file_id()));
  EXPECT_FALSE(HasActiveTracker(app_root_2->file_id()));

  EXPECT_EQ(3u, CountFileMetadata());
  EXPECT_EQ(3u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_MultiCandidate) {
  scoped_ptr<google_apis::FileResource> sync_root_1(CreateRemoteSyncRoot());
  scoped_ptr<google_apis::FileResource> sync_root_2(CreateRemoteSyncRoot());

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root_1->file_id()));
  EXPECT_EQ(0u, CountTrackersForFile(sync_root_2->file_id()));

  EXPECT_TRUE(HasActiveTracker(sync_root_1->file_id()));
  EXPECT_FALSE(HasActiveTracker(sync_root_2->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_UndetachedRemoteSyncRoot) {
  scoped_ptr<google_apis::FileResource> sync_root(CreateRemoteFolder(
      std::string(), kSyncRootFolderTitle));
  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));

  EXPECT_TRUE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_MultiparentSyncRoot) {
  scoped_ptr<google_apis::FileResource> folder(CreateRemoteFolder(
      std::string(), "folder"));
  scoped_ptr<google_apis::FileResource> sync_root(CreateRemoteFolder(
      std::string(), kSyncRootFolderTitle));
  AddParentFolder(sync_root->file_id(), folder->file_id());

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));

  EXPECT_TRUE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_FakeRemoteSyncRoot) {
  scoped_ptr<google_apis::FileResource> folder(CreateRemoteFolder(
      std::string(), "folder"));
  scoped_ptr<google_apis::FileResource> sync_root(CreateRemoteFolder(
      folder->file_id(), kSyncRootFolderTitle));

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(0u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_FALSE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, CountFileMetadata());
  EXPECT_EQ(1u, CountFileTracker());
}

}  // namespace drive_backend
}  // namespace sync_file_system
