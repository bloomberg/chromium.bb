// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const int64 kSyncRootTrackerID = 1;
const int64 kAppRootTrackerID = 2;
const int64 kFileTrackerID = 3;
const int64 kPlaceholderTrackerID = 4;

scoped_ptr<DatabaseContents> CreateTestDatabaseContents() {
  scoped_ptr<DatabaseContents> contents(new DatabaseContents);

  scoped_ptr<FileMetadata> sync_root_metadata =
      test_util::CreateFolderMetadata("sync_root_folder_id",
                                      "Chrome Syncable FileSystem");
  scoped_ptr<FileTracker> sync_root_tracker =
      test_util::CreateTracker(*sync_root_metadata, kSyncRootTrackerID, NULL);

  scoped_ptr<FileMetadata> app_root_metadata =
      test_util::CreateFolderMetadata("app_root_folder_id", "app_id");
  scoped_ptr<FileTracker> app_root_tracker =
      test_util::CreateTracker(*app_root_metadata, kAppRootTrackerID,
                               sync_root_tracker.get());
  app_root_tracker->set_app_id("app_id");
  app_root_tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);

  scoped_ptr<FileMetadata> file_metadata =
      test_util::CreateFileMetadata("file_id", "file", "file_md5");
  scoped_ptr<FileTracker> file_tracker =
      test_util::CreateTracker(*file_metadata, kFileTrackerID,
                               app_root_tracker.get());

  scoped_ptr<FileTracker> placeholder_tracker =
      test_util::CreatePlaceholderTracker("unsynced_file_id",
                                          kPlaceholderTrackerID,
                                          app_root_tracker.get());

  contents->file_metadata.push_back(sync_root_metadata.release());
  contents->file_trackers.push_back(sync_root_tracker.release());
  contents->file_metadata.push_back(app_root_metadata.release());
  contents->file_trackers.push_back(app_root_tracker.release());
  contents->file_metadata.push_back(file_metadata.release());
  contents->file_trackers.push_back(file_tracker.release());
  contents->file_trackers.push_back(placeholder_tracker.release());
  return contents.Pass();
}

}  // namespace

TEST(MetadataDatabaseIndexTest, GetEntryTest) {
  scoped_ptr<MetadataDatabaseIndex> index =
      MetadataDatabaseIndex::CreateForTesting(
          CreateTestDatabaseContents().get());

  FileTracker tracker;
  EXPECT_FALSE(index->GetFileTracker(kInvalidTrackerID, NULL));
  ASSERT_TRUE(index->GetFileTracker(kFileTrackerID, &tracker));
  EXPECT_EQ(kFileTrackerID, tracker.tracker_id());
  EXPECT_EQ("file_id", tracker.file_id());

  FileMetadata metadata;
  EXPECT_FALSE(index->GetFileMetadata(std::string(), NULL));
  ASSERT_TRUE(index->GetFileMetadata("file_id", &metadata));
  EXPECT_EQ("file_id", metadata.file_id());
}

TEST(MetadataDatabaseIndexTest, IndexLookUpTest) {
  scoped_ptr<MetadataDatabaseIndex> index =
      MetadataDatabaseIndex::CreateForTesting(
          CreateTestDatabaseContents().get());

  TrackerIDSet trackers = index->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(1u, trackers.size());
  EXPECT_TRUE(trackers.has_active());
  EXPECT_EQ(kFileTrackerID, trackers.active_tracker());

  int64 app_root_tracker_id = index->GetAppRootTracker("app_id");
  EXPECT_EQ(kAppRootTrackerID, app_root_tracker_id);

  trackers = index->GetFileTrackerIDsByParentAndTitle(
      app_root_tracker_id, "file");
  EXPECT_EQ(1u, trackers.size());
  EXPECT_TRUE(trackers.has_active());
  EXPECT_EQ(kFileTrackerID, trackers.active_tracker());

  EXPECT_TRUE(index->PickMultiTrackerFileID().empty());
  EXPECT_EQ(kInvalidTrackerID,
            index->PickMultiBackingFilePath().parent_id);
  EXPECT_EQ(kPlaceholderTrackerID, index->PickDirtyTracker());
}

TEST(MetadataDatabaseIndexTest, UpdateTest) {
  scoped_ptr<MetadataDatabaseIndex> index =
      MetadataDatabaseIndex::CreateForTesting(
          CreateTestDatabaseContents().get());

  index->DemoteDirtyTracker(kPlaceholderTrackerID);
  EXPECT_EQ(kInvalidTrackerID, index->PickDirtyTracker());
  index->PromoteDemotedDirtyTrackers();
  EXPECT_EQ(kPlaceholderTrackerID, index->PickDirtyTracker());

  FileMetadata metadata;
  ASSERT_TRUE(index->GetFileMetadata("file_id", &metadata));
  FileTracker app_root_tracker;
  ASSERT_TRUE(index->GetFileTracker(kAppRootTrackerID, &app_root_tracker));

  int64 new_tracker_id = 100;
  scoped_ptr<FileTracker> new_tracker =
      test_util::CreateTracker(metadata, new_tracker_id, &app_root_tracker);
  new_tracker->set_active(false);
  index->StoreFileTracker(new_tracker.Pass());

  EXPECT_EQ("file_id", index->PickMultiTrackerFileID());
  EXPECT_EQ(ParentIDAndTitle(kAppRootTrackerID, std::string("file")),
            index->PickMultiBackingFilePath());

  index->RemoveFileMetadata("file_id");
  index->RemoveFileTracker(kFileTrackerID);

  EXPECT_FALSE(index->GetFileMetadata("file_id", NULL));
  EXPECT_FALSE(index->GetFileTracker(kFileTrackerID, NULL));
}

}  // namespace drive_backend
}  // namespace sync_file_system
