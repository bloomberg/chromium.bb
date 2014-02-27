// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index.h"

#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const int64 kSyncRootTrackerID = 1;
const int64 kAppRootTrackerID = 2;
const int64 kFileTrackerID = 3;
const int64 kPlaceholderTrackerID = 4;

scoped_ptr<FileMetadata> CreateFolderMetadata(const std::string& file_id,
                                              const std::string& title) {
  FileDetails details;
  details.set_title(title);
  details.set_file_kind(FILE_KIND_FOLDER);
  details.set_missing(false);

  scoped_ptr<FileMetadata> metadata(new FileMetadata);
  metadata->set_file_id(file_id);
  *metadata->mutable_details() = details;

  return metadata.Pass();
}

scoped_ptr<FileMetadata> CreateFileMetadata(const std::string& file_id,
                                            const std::string& title,
                                            const std::string& md5) {
  FileDetails details;
  details.set_title(title);
  details.set_file_kind(FILE_KIND_FILE);
  details.set_missing(false);
  details.set_md5(md5);

  scoped_ptr<FileMetadata> metadata(new FileMetadata);
  metadata->set_file_id(file_id);
  *metadata->mutable_details() = details;

  return metadata.Pass();
}

scoped_ptr<FileTracker> CreateTracker(const FileMetadata& metadata,
                                      int64 tracker_id,
                                      const FileTracker* parent_tracker) {
  scoped_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  if (parent_tracker)
    tracker->set_parent_tracker_id(parent_tracker->tracker_id());
  tracker->set_file_id(metadata.file_id());
  if (parent_tracker)
    tracker->set_app_id(parent_tracker->app_id());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  *tracker->mutable_synced_details() = metadata.details();
  tracker->set_dirty(false);
  tracker->set_active(true);
  tracker->set_needs_folder_listing(false);
  return tracker.Pass();
}

scoped_ptr<FileTracker> CreatePlaceholderTracker(
    const std::string& file_id,
    int64 tracker_id,
    const FileTracker* parent_tracker) {
  scoped_ptr<FileTracker> tracker(new FileTracker);
  tracker->set_tracker_id(tracker_id);
  if (parent_tracker)
    tracker->set_parent_tracker_id(parent_tracker->tracker_id());
  tracker->set_file_id(file_id);
  if (parent_tracker)
    tracker->set_app_id(parent_tracker->app_id());
  tracker->set_tracker_kind(TRACKER_KIND_REGULAR);
  tracker->set_dirty(true);
  tracker->set_active(false);
  tracker->set_needs_folder_listing(false);
  return tracker.Pass();
}

scoped_ptr<DatabaseContents> CreateTestDatabaseContents() {
  scoped_ptr<DatabaseContents> contents(new DatabaseContents);

  scoped_ptr<FileMetadata> sync_root_metadata =
      CreateFolderMetadata("sync_root_folder_id",
                           "Chrome Syncable FileSystem");
  scoped_ptr<FileTracker> sync_root_tracker =
      CreateTracker(*sync_root_metadata, kSyncRootTrackerID, NULL);

  scoped_ptr<FileMetadata> app_root_metadata =
      CreateFolderMetadata("app_root_folder_id", "app_id");
  scoped_ptr<FileTracker> app_root_tracker =
      CreateTracker(*app_root_metadata, kAppRootTrackerID,
                    sync_root_tracker.get());
  app_root_tracker->set_app_id("app_id");
  app_root_tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);

  scoped_ptr<FileMetadata> file_metadata =
      CreateFileMetadata("file_id", "file", "file_md5");
  scoped_ptr<FileTracker> file_tracker =
      CreateTracker(*file_metadata, kFileTrackerID, app_root_tracker.get());

  scoped_ptr<FileTracker> placeholder_tracker =
      CreatePlaceholderTracker("unsynced_file_id", kPlaceholderTrackerID,
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
  MetadataDatabaseIndex index(CreateTestDatabaseContents().get());

  EXPECT_FALSE(index.GetFileMetadata(std::string()));
  EXPECT_FALSE(index.GetFileTracker(kInvalidTrackerID));

  const FileTracker* tracker = index.GetFileTracker(kFileTrackerID);
  ASSERT_TRUE(tracker);
  EXPECT_EQ(kFileTrackerID, tracker->tracker_id());
  EXPECT_EQ("file_id", tracker->file_id());

  const FileMetadata* metadata = index.GetFileMetadata("file_id");
  ASSERT_TRUE(metadata);
  EXPECT_EQ("file_id", metadata->file_id());
}

TEST(MetadataDatabaseIndexTest, IndexLookUpTest) {
  MetadataDatabaseIndex index(CreateTestDatabaseContents().get());

  TrackerIDSet trackers = index.GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(1u, trackers.size());
  EXPECT_TRUE(trackers.has_active());
  EXPECT_EQ(kFileTrackerID, trackers.active_tracker());

  int64 app_root_tracker_id = index.GetAppRootTracker("app_id");
  EXPECT_EQ(kAppRootTrackerID, app_root_tracker_id);

  trackers = index.GetFileTrackerIDsByParentAndTitle(
      app_root_tracker_id, "file");
  EXPECT_EQ(1u, trackers.size());
  EXPECT_TRUE(trackers.has_active());
  EXPECT_EQ(kFileTrackerID, trackers.active_tracker());

  EXPECT_TRUE(index.PickMultiTrackerFileID().empty());
  EXPECT_EQ(kInvalidTrackerID,
            index.PickMultiBackingFilePath().parent_id);
  EXPECT_EQ(kPlaceholderTrackerID, index.PickDirtyTracker());
}

TEST(MetadataDatabaseIndexTest, UpdateTest) {
  MetadataDatabaseIndex index(CreateTestDatabaseContents().get());

  index.DemoteDirtyTracker(kPlaceholderTrackerID);
  EXPECT_EQ(kInvalidTrackerID, index.PickDirtyTracker());
  index.PromoteDemotedDirtyTrackers();
  EXPECT_EQ(kPlaceholderTrackerID, index.PickDirtyTracker());

  int64 new_tracker_id = 100;
  scoped_ptr<FileTracker> new_tracker =
      CreateTracker(*index.GetFileMetadata("file_id"),
                    new_tracker_id,
                    index.GetFileTracker(kAppRootTrackerID));
  new_tracker->set_active(false);
  index.StoreFileTracker(new_tracker.Pass());

  EXPECT_EQ("file_id", index.PickMultiTrackerFileID());
  EXPECT_EQ(ParentIDAndTitle(kAppRootTrackerID, std::string("file")),
            index.PickMultiBackingFilePath());

  index.RemoveFileMetadata("file_id");
  index.RemoveFileTracker(kFileTrackerID);

  EXPECT_FALSE(index.GetFileMetadata("file_id"));
  EXPECT_FALSE(index.GetFileTracker(kFileTrackerID));
}

}  // namespace drive_backend
}  // namespace sync_file_system
