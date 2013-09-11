// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

void SyncStatusResultCallback(SyncStatusCode* status_out,
                              SyncStatusCode status) {
  ASSERT_TRUE(status_out);
  EXPECT_EQ(SYNC_STATUS_UNKNOWN, *status_out);
  *status_out = status;
}

void ResourceEntryResultCallback(
    google_apis::GDataErrorCode* error_out,
    scoped_ptr<google_apis::ResourceEntry>* entry_out,
    google_apis::GDataErrorCode error,
    scoped_ptr<google_apis::ResourceEntry> entry) {
  ASSERT_TRUE(error_out);
  ASSERT_TRUE(entry_out);
  EXPECT_EQ(google_apis::GDATA_OTHER_ERROR, *error_out);
  *error_out = error;
  *entry_out = entry.Pass();
}

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

    initializer_->Run(base::Bind(&SyncStatusResultCallback, &status));
    base::MessageLoop::current()->RunUntilIdle();

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
    fake_drive_service_.GetResourceEntry(
        file->metadata.file_id(),
        base::Bind(&ResourceEntryResultCallback,
                   &error, &entry));
    base::MessageLoop::current()->RunUntilIdle();

    if (entry) {
      file->resource =
          drive::util::ConvertResourceEntryToFileResource(*entry);
    }

    if (file_out)
      *file_out = file.Pass();
    return error;
  }

  google_apis::GDataErrorCode GetSyncRoot(
      scoped_ptr<TrackedFile>* sync_root_out) {
    return FillTrackedFileByTrackerID(
        metadata_database_->GetSyncRootTrackerID(),
        sync_root_out);
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

  scoped_ptr<TrackedFile> sync_root;
  ASSERT_EQ(google_apis::HTTP_SUCCESS, GetSyncRoot(&sync_root));
  EXPECT_EQ("Chrome Syncable FileSystem", sync_root->resource->title());
}

}  // namespace drive_backend
}  // namespace sync_file_system
