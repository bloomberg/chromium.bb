// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_on_disk.h"

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

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
  int64 parent_id = parent_tracker ?
      parent_tracker->tracker_id() : kInvalidTrackerID;
  tracker->set_parent_tracker_id(parent_id);
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

bool CreateTestDatabase(leveldb::DB* db) {
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

  leveldb::WriteBatch batch;
  PutFileMetadataToBatch(*sync_root_metadata, &batch);
  PutFileTrackerToBatch(*sync_root_tracker, &batch);
  PutFileMetadataToBatch(*app_root_metadata, &batch);
  PutFileTrackerToBatch(*app_root_tracker, &batch);
  PutFileMetadataToBatch(*file_metadata, &batch);
  PutFileTrackerToBatch(*file_tracker, &batch);
  PutFileTrackerToBatch(*placeholder_tracker, &batch);

  leveldb::Status status = db->Write(leveldb::WriteOptions(), &batch);
  return status.ok();
}

}  // namespace

class MetadataDatabaseIndexOnDiskTest : public testing::Test {
 public:
  virtual ~MetadataDatabaseIndexOnDiskTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    InitializeLevelDB();
    index_.reset(new MetadataDatabaseIndexOnDisk(db_.get()));
  }

  virtual void TearDown() OVERRIDE {
    index_.reset();
    db_.reset();
    in_memory_env_.reset();
  }

  MetadataDatabaseIndexOnDisk* index() { return index_.get(); }

 private:
  void InitializeLevelDB() {
    leveldb::DB* db = NULL;
    leveldb::Options options;
    options.create_if_missing = true;
    options.max_open_files = 0;  // Use minimum.
    options.env = in_memory_env_.get();
    leveldb::Status status =
        leveldb::DB::Open(options, database_dir_.path().AsUTF8Unsafe(), &db);
    EXPECT_TRUE(status.ok());
    db_.reset(db);

    ASSERT_TRUE(CreateTestDatabase(db_.get()));
  }

  scoped_ptr<MetadataDatabaseIndexOnDisk> index_;

  base::ScopedTempDir database_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;
  scoped_ptr<leveldb::DB> db_;
};

TEST_F(MetadataDatabaseIndexOnDiskTest, GetEntryTest) {
  FileTracker tracker;
  EXPECT_FALSE(index()->GetFileTracker(kInvalidTrackerID, NULL));
  ASSERT_TRUE(index()->GetFileTracker(kFileTrackerID, &tracker));
  EXPECT_EQ(kFileTrackerID, tracker.tracker_id());
  EXPECT_EQ("file_id", tracker.file_id());

  FileMetadata metadata;
  EXPECT_FALSE(index()->GetFileMetadata(std::string(), NULL));
  ASSERT_TRUE(index()->GetFileMetadata("file_id", &metadata));
  EXPECT_EQ("file_id", metadata.file_id());
}

}  // namespace drive_backend
}  // namespace sync_file_system
