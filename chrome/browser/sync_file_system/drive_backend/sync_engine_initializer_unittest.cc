// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    ASSERT_TRUE(fake_drive_service_.LoadAccountMetadataForWapi(
        "sync_file_system/account_metadata.json"));
    ASSERT_TRUE(fake_drive_service_.LoadResourceListForWapi(
        "gdata/empty_feed.json"));
  }

  virtual void TearDown() OVERRIDE {
    initializer_.reset();
    metadata_database_.reset();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath database_path() {
    return database_dir_.path();
  }

  SyncStatusCode RunInitializer() {
    initializer_.reset(new SyncEngineInitializer(
        base::MessageLoopProxy::current(),
        &fake_drive_service_,
        database_path()));
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;

    initializer_->Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();

    metadata_database_ = initializer_->PassMetadataDatabase();
    return status;
  }

  google_apis::GDataErrorCode FillTrackedFileByTrackerID(
      int64 tracker_id,
      scoped_ptr<TrackedFile>* file_out) {
    scoped_ptr<TrackedFile> file(new TrackedFile);
    if (!metadata_database_->FindTrackerByTrackerID(
            tracker_id, &file->tracker))
      return google_apis::HTTP_NOT_FOUND;
    if (!metadata_database_->FindFileByFileID(
            file->tracker.file_id(), &file->metadata))
      return google_apis::HTTP_NOT_FOUND;

    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceEntry> entry;
    fake_drive_service_.GetResourceEntry(file->metadata.file_id(),
                                         CreateResultReceiver(&error, &entry));
    base::RunLoop().RunUntilIdle();

    if (entry) {
      file->resource =
          drive::util::ConvertResourceEntryToFileResource(*entry);
    }

    if (file_out)
      *file_out = file.Pass();
    return error;
  }

  SyncStatusCode PopulateDatabase(
      const google_apis::FileResource& sync_root,
      const google_apis::FileResource** app_roots,
      size_t app_roots_count) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    scoped_ptr<MetadataDatabase> database;
    MetadataDatabase::Create(
        base::MessageLoopProxy::current(),
        database_path(),
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
    scoped_ptr<google_apis::ResourceEntry> entry;
    fake_drive_service_.AddNewDirectory(
        parent_folder_id, title,
        CreateResultReceiver(&error, &entry));
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(google_apis::HTTP_CREATED, error);
    if (!entry)
      scoped_ptr<google_apis::FileResource>();
    return drive::util::ConvertResourceEntryToFileResource(*entry);
  }

  scoped_ptr<google_apis::FileResource> CreateRemoteSyncRoot() {
    scoped_ptr<google_apis::FileResource> sync_root(
        CreateRemoteFolder(std::string(), kSyncRootFolderTitle));

    for (size_t i = 0; i < sync_root->parents().size(); ++i) {
      google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
      fake_drive_service_.RemoveResourceFromDirectory(
          sync_root->parents()[i]->file_id(),
          sync_root->file_id(),
          CreateResultReceiver(&error));
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
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
    TrackerSet trackers;
    metadata_database_->FindTrackersByFileID(file_id, &trackers);
    return trackers.tracker_set().size();
  }

  bool HasActiveTracker(const std::string& file_id) {
    TrackerSet trackers;
    return metadata_database_->FindTrackersByFileID(file_id, &trackers) &&
        trackers.has_active();
  }

  bool HasNoParent(const std::string& file_id) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    scoped_ptr<google_apis::ResourceEntry> entry;
    fake_drive_service_.GetResourceEntry(
        file_id,
        CreateResultReceiver(&error, &entry));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
    return !entry->GetLinkByType(google_apis::Link::LINK_PARENT);
  }

  size_t NumberOfMetadata() {
    return metadata_database_->file_by_id_.size();
  }

  size_t NumberOfTrackers() {
    return metadata_database_->tracker_by_id_.size();
  }

  google_apis::GDataErrorCode AddParentFolder(
      const std::string& new_parent_folder_id,
      const std::string& file_id) {
    google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    fake_drive_service_.AddResourceToDirectory(
        new_parent_folder_id, file_id,
        CreateResultReceiver(&error));
    base::RunLoop().RunUntilIdle();
    return error;
  }

 private:
  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir database_dir_;
  drive::FakeDriveService fake_drive_service_;

  scoped_ptr<SyncEngineInitializer> initializer_;
  scoped_ptr<MetadataDatabase> metadata_database_;

  DISALLOW_COPY_AND_ASSIGN(SyncEngineInitializerTest);
};

TEST_F(SyncEngineInitializerTest, EmptyDatabase_NoRemoteSyncRoot) {
  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  std::string sync_root_folder_id = GetSyncRootFolderID();
  EXPECT_EQ(1u, CountTrackersForFile(sync_root_folder_id));

  EXPECT_TRUE(HasActiveTracker(sync_root_folder_id));

  EXPECT_EQ(1u, NumberOfMetadata());
  EXPECT_EQ(1u, NumberOfTrackers());
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

  EXPECT_EQ(3u, NumberOfMetadata());
  EXPECT_EQ(3u, NumberOfTrackers());
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

  EXPECT_EQ(3u, NumberOfMetadata());
  EXPECT_EQ(3u, NumberOfTrackers());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_MultiCandidate) {
  scoped_ptr<google_apis::FileResource> sync_root_1(CreateRemoteSyncRoot());
  scoped_ptr<google_apis::FileResource> sync_root_2(CreateRemoteSyncRoot());

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root_1->file_id()));
  EXPECT_EQ(0u, CountTrackersForFile(sync_root_2->file_id()));

  EXPECT_TRUE(HasActiveTracker(sync_root_1->file_id()));
  EXPECT_FALSE(HasActiveTracker(sync_root_2->file_id()));

  EXPECT_EQ(1u, NumberOfMetadata());
  EXPECT_EQ(1u, NumberOfTrackers());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_UndetachedRemoteSyncRoot) {
  scoped_ptr<google_apis::FileResource> sync_root(CreateRemoteFolder(
      std::string(), kSyncRootFolderTitle));
  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(1u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_TRUE(HasActiveTracker(sync_root->file_id()));

  EXPECT_TRUE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, NumberOfMetadata());
  EXPECT_EQ(1u, NumberOfTrackers());
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

  EXPECT_EQ(1u, NumberOfMetadata());
  EXPECT_EQ(1u, NumberOfTrackers());
}

TEST_F(SyncEngineInitializerTest, EmptyDatabase_FakeRemoteSyncRoot) {
  scoped_ptr<google_apis::FileResource> folder(CreateRemoteFolder(
      std::string(), "folder"));
  scoped_ptr<google_apis::FileResource> sync_root(CreateRemoteFolder(
      folder->file_id(), kSyncRootFolderTitle));

  EXPECT_EQ(SYNC_STATUS_OK, RunInitializer());

  EXPECT_EQ(0u, CountTrackersForFile(sync_root->file_id()));
  EXPECT_FALSE(HasNoParent(sync_root->file_id()));

  EXPECT_EQ(1u, NumberOfMetadata());
  EXPECT_EQ(1u, NumberOfTrackers());
}

}  // namespace drive_backend
}  // namespace sync_file_system
