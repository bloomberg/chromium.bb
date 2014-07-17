// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database_index_on_disk.h"

#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_test_util.h"
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

}  // namespace

class MetadataDatabaseIndexOnDiskTest : public testing::Test {
 public:
  virtual ~MetadataDatabaseIndexOnDiskTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));
    InitializeLevelDB();
    index_ = MetadataDatabaseIndexOnDisk::Create(db_.get(), NULL);
  }

  virtual void TearDown() OVERRIDE {
    index_.reset();
    db_.reset();
    in_memory_env_.reset();
  }

  bool WriteToDB(scoped_ptr<leveldb::WriteBatch> batch) {
    leveldb::Status status = db_->Write(leveldb::WriteOptions(), batch.get());
    return status.ok();
  }

  void CreateTestDatabase(bool build_index) {
    scoped_ptr<FileMetadata> sync_root_metadata =
        test_util::CreateFolderMetadata("sync_root_folder_id",
                                        "Chrome Syncable FileSystem");
    scoped_ptr<FileTracker> sync_root_tracker =
        test_util::CreateTracker(*sync_root_metadata, kSyncRootTrackerID, NULL);

    scoped_ptr<FileMetadata> app_root_metadata =
        test_util::CreateFolderMetadata("app_root_folder_id", "app_title");
    scoped_ptr<FileTracker> app_root_tracker =
        test_util::CreateTracker(*app_root_metadata, kAppRootTrackerID,
                                 sync_root_tracker.get());
    app_root_tracker->set_app_id("app_id");
    app_root_tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);

    scoped_ptr<FileMetadata> file_metadata =
        test_util::CreateFileMetadata("file_id", "file", "file_md5");
    scoped_ptr<FileTracker> file_tracker =
        test_util::CreateTracker(*file_metadata,
                                 kFileTrackerID,
                                 app_root_tracker.get());

    scoped_ptr<FileTracker> placeholder_tracker =
        test_util::CreatePlaceholderTracker("unsynced_file_id",
                                            kPlaceholderTrackerID,
                                            app_root_tracker.get());

    leveldb::WriteBatch batch;
    if (build_index) {
      DCHECK(index());
      index()->StoreFileMetadata(sync_root_metadata.Pass(), &batch);
      index()->StoreFileTracker(sync_root_tracker.Pass(), &batch);
      index()->StoreFileMetadata(app_root_metadata.Pass(), &batch);
      index()->StoreFileTracker(app_root_tracker.Pass(), &batch);
      index()->StoreFileMetadata(file_metadata.Pass(), &batch);
      index()->StoreFileTracker(file_tracker.Pass(), &batch);
      index()->StoreFileTracker(placeholder_tracker.Pass(), &batch);
    } else {
      PutFileMetadataToBatch(*sync_root_metadata, &batch);
      PutFileTrackerToBatch(*sync_root_tracker, &batch);
      PutFileMetadataToBatch(*app_root_metadata, &batch);
      PutFileTrackerToBatch(*app_root_tracker, &batch);
      PutFileMetadataToBatch(*file_metadata, &batch);
      PutFileTrackerToBatch(*file_tracker, &batch);
      PutFileTrackerToBatch(*placeholder_tracker, &batch);
    }

    leveldb::Status status = db_->Write(leveldb::WriteOptions(), &batch);
    ASSERT_TRUE(status.ok());
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
    ASSERT_TRUE(status.ok());
    db_.reset(db);
  }

  scoped_ptr<MetadataDatabaseIndexOnDisk> index_;

  base::ScopedTempDir database_dir_;
  scoped_ptr<leveldb::Env> in_memory_env_;
  scoped_ptr<leveldb::DB> db_;
};

TEST_F(MetadataDatabaseIndexOnDiskTest, GetEntryTest) {
  CreateTestDatabase(false);

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

TEST_F(MetadataDatabaseIndexOnDiskTest, SetEntryTest) {
  // This test does not check updates of indexes.
  CreateTestDatabase(false);

  const int64 tracker_id = 10;
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  scoped_ptr<FileMetadata> metadata =
      test_util::CreateFileMetadata("test_file_id", "test_title", "test_md5");
  FileTracker root_tracker;
  EXPECT_TRUE(index()->GetFileTracker(kSyncRootTrackerID, &root_tracker));
  scoped_ptr<FileTracker> tracker =
      test_util::CreateTracker(*metadata, tracker_id, &root_tracker);

  index()->StoreFileMetadata(metadata.Pass(), batch.get());
  index()->StoreFileTracker(tracker.Pass(), batch.get());

  EXPECT_FALSE(index()->GetFileMetadata("test_file_id", NULL));
  EXPECT_FALSE(index()->GetFileTracker(tracker_id, NULL));

  WriteToDB(batch.Pass());

  metadata.reset(new FileMetadata);
  ASSERT_TRUE(index()->GetFileMetadata("test_file_id", metadata.get()));
  EXPECT_TRUE(metadata->has_details());
  EXPECT_EQ("test_title", metadata->details().title());

  tracker.reset(new FileTracker);
  ASSERT_TRUE(index()->GetFileTracker(tracker_id, tracker.get()));
  EXPECT_EQ("test_file_id", tracker->file_id());

  // Test if removers work.
  batch.reset(new leveldb::WriteBatch);

  index()->RemoveFileMetadata("test_file_id", batch.get());
  index()->RemoveFileTracker(tracker_id, batch.get());

  EXPECT_TRUE(index()->GetFileMetadata("test_file_id", NULL));
  EXPECT_TRUE(index()->GetFileTracker(tracker_id, NULL));

  WriteToDB(batch.Pass());

  EXPECT_FALSE(index()->GetFileMetadata("test_file_id", NULL));
  EXPECT_FALSE(index()->GetFileTracker(tracker_id, NULL));
}

TEST_F(MetadataDatabaseIndexOnDiskTest, BuildIndexTest) {
  CreateTestDatabase(false);

  TrackerIDSet tracker_ids;
  // Before building indexes, no references exist.
  EXPECT_EQ(kInvalidTrackerID, index()->GetAppRootTracker("app_id"));
  tracker_ids = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_TRUE(tracker_ids.empty());
  tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file");
  EXPECT_TRUE(tracker_ids.empty());
  EXPECT_EQ(0U, index()->CountDirtyTracker());

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  index()->BuildTrackerIndexes(batch.get());
  WriteToDB(batch.Pass());

  // After building indexes, we should have correct indexes.
  EXPECT_EQ(kAppRootTrackerID, index()->GetAppRootTracker("app_id"));
  tracker_ids = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(1U, tracker_ids.size());
  EXPECT_EQ(kFileTrackerID, tracker_ids.active_tracker());
  tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file");
  EXPECT_EQ(1U, tracker_ids.size());
  EXPECT_EQ(kFileTrackerID, tracker_ids.active_tracker());
  EXPECT_EQ(1U, index()->CountDirtyTracker());
}

TEST_F(MetadataDatabaseIndexOnDiskTest, AllEntriesTest) {
  CreateTestDatabase(true);

  EXPECT_EQ(3U, index()->CountFileMetadata());
  std::vector<std::string> file_ids(index()->GetAllMetadataIDs());
  ASSERT_EQ(3U, file_ids.size());
  std::sort(file_ids.begin(), file_ids.end());
  EXPECT_EQ("app_root_folder_id", file_ids[0]);
  EXPECT_EQ("file_id", file_ids[1]);
  EXPECT_EQ("sync_root_folder_id", file_ids[2]);

  EXPECT_EQ(4U, index()->CountFileTracker());
  std::vector<int64> tracker_ids = index()->GetAllTrackerIDs();
  ASSERT_EQ(4U, tracker_ids.size());
  std::sort(tracker_ids.begin(), tracker_ids.end());
  EXPECT_EQ(kSyncRootTrackerID, tracker_ids[0]);
  EXPECT_EQ(kAppRootTrackerID, tracker_ids[1]);
  EXPECT_EQ(kFileTrackerID, tracker_ids[2]);
  EXPECT_EQ(kPlaceholderTrackerID, tracker_ids[3]);
}

TEST_F(MetadataDatabaseIndexOnDiskTest, IndexAppRootIDByAppIDTest) {
  CreateTestDatabase(true);

  std::vector<std::string> app_ids = index()->GetRegisteredAppIDs();
  ASSERT_EQ(1U, app_ids.size());
  EXPECT_EQ("app_id", app_ids[0]);

  EXPECT_EQ(kInvalidTrackerID, index()->GetAppRootTracker(""));
  EXPECT_EQ(kAppRootTrackerID, index()->GetAppRootTracker("app_id"));

  const int64 kAppRootTrackerID2 = 12;
  FileTracker sync_root_tracker;
  index()->GetFileTracker(kSyncRootTrackerID, &sync_root_tracker);
  scoped_ptr<FileMetadata> app_root_metadata =
      test_util::CreateFolderMetadata("app_root_folder_id_2", "app_title_2");

  // Testing AddToAppIDIndex
  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  scoped_ptr<FileTracker> app_root_tracker =
      test_util::CreateTracker(*app_root_metadata, kAppRootTrackerID2,
                               &sync_root_tracker);
  app_root_tracker->set_app_id("app_id_2");
  app_root_tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);

  index()->StoreFileTracker(app_root_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(kAppRootTrackerID, index()->GetAppRootTracker("app_id"));
  EXPECT_EQ(kAppRootTrackerID2, index()->GetAppRootTracker("app_id_2"));

  // Testing UpdateInAppIDIndex
  batch.reset(new leveldb::WriteBatch);
  app_root_tracker = test_util::CreateTracker(*app_root_metadata,
                                              kAppRootTrackerID2,
                                              &sync_root_tracker);
  app_root_tracker->set_app_id("app_id_3");
  app_root_tracker->set_active(false);

  index()->StoreFileTracker(app_root_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(kAppRootTrackerID, index()->GetAppRootTracker("app_id"));
  EXPECT_EQ(kInvalidTrackerID, index()->GetAppRootTracker("app_id_2"));
  EXPECT_EQ(kInvalidTrackerID, index()->GetAppRootTracker("app_id_3"));

  batch.reset(new leveldb::WriteBatch);
  app_root_tracker = test_util::CreateTracker(*app_root_metadata,
                                              kAppRootTrackerID2,
                                              &sync_root_tracker);
  app_root_tracker->set_app_id("app_id_3");
  app_root_tracker->set_tracker_kind(TRACKER_KIND_APP_ROOT);

  index()->StoreFileTracker(app_root_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(kAppRootTrackerID, index()->GetAppRootTracker("app_id"));
  EXPECT_EQ(kInvalidTrackerID, index()->GetAppRootTracker("app_id_2"));
  EXPECT_EQ(kAppRootTrackerID2, index()->GetAppRootTracker("app_id_3"));

  // Testing RemoveFromAppIDIndex
  batch.reset(new leveldb::WriteBatch);
  index()->RemoveFileTracker(kAppRootTrackerID2, batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(kAppRootTrackerID, index()->GetAppRootTracker("app_id"));
  EXPECT_EQ(kInvalidTrackerID, index()->GetAppRootTracker("app_id_3"));
}

TEST_F(MetadataDatabaseIndexOnDiskTest, TrackerIDSetByFileIDTest) {
  CreateTestDatabase(true);

  FileTracker app_root_tracker;
  EXPECT_TRUE(index()->GetFileTracker(kAppRootTrackerID, &app_root_tracker));
  FileMetadata metadata;
  EXPECT_TRUE(index()->GetFileMetadata("file_id", &metadata));

  // Testing GetFileTrackerIDsByFileID
  TrackerIDSet tracker_ids = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(1U, tracker_ids.size());
  EXPECT_EQ(kFileTrackerID, tracker_ids.active_tracker());

  const int64 tracker_id = 21;
  // Testing AddToFileIDIndexes
  scoped_ptr<FileTracker> file_tracker =
      test_util::CreateTracker(metadata, tracker_id, &app_root_tracker);

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  index()->StoreFileTracker(file_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(2U, tracker_ids.size());
  EXPECT_EQ(tracker_id, tracker_ids.active_tracker());

  std::string multi_file_id = index()->PickMultiTrackerFileID();
  EXPECT_EQ("file_id", multi_file_id);

  // Testing UpdateInFileIDIndexes
  batch.reset(new leveldb::WriteBatch);
  file_tracker =
      test_util::CreateTracker(metadata, tracker_id, &app_root_tracker);
  file_tracker->set_active(false);

  index()->StoreFileTracker(file_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(2U, tracker_ids.size());
  EXPECT_EQ(kInvalidTrackerID, tracker_ids.active_tracker());

  multi_file_id = index()->PickMultiTrackerFileID();
  EXPECT_EQ("file_id", multi_file_id);

  batch.reset(new leveldb::WriteBatch);
  file_tracker =
      test_util::CreateTracker(metadata, tracker_id, &app_root_tracker);

  index()->StoreFileTracker(file_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(2U, tracker_ids.size());
  EXPECT_EQ(tracker_id, tracker_ids.active_tracker());

  multi_file_id = index()->PickMultiTrackerFileID();
  EXPECT_EQ("file_id", multi_file_id);

  // Testing RemoveFromFileIDIndexes
  batch.reset(new leveldb::WriteBatch);
  index()->RemoveFileTracker(tracker_id, batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByFileID("file_id");
  EXPECT_EQ(1U, tracker_ids.size());
  EXPECT_EQ(kInvalidTrackerID, tracker_ids.active_tracker());

  multi_file_id = index()->PickMultiTrackerFileID();
  EXPECT_TRUE(multi_file_id.empty()) << multi_file_id;
}

TEST_F(MetadataDatabaseIndexOnDiskTest, TrackerIDSetByParentIDAndTitleTest) {
  CreateTestDatabase(true);

  FileTracker app_root_tracker;
  EXPECT_TRUE(index()->GetFileTracker(kAppRootTrackerID, &app_root_tracker));
  FileMetadata metadata;
  EXPECT_TRUE(index()->GetFileMetadata("file_id", &metadata));

  // Testing GetFileTrackerIDsByFileID
  TrackerIDSet tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file");
  EXPECT_EQ(1U, tracker_ids.size());
  EXPECT_EQ(kFileTrackerID, tracker_ids.active_tracker());

  tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file2");
  EXPECT_TRUE(tracker_ids.empty());

  const int64 tracker_id = 72;
  // Testing AddToFileIDIndexes
  scoped_ptr<FileTracker> file_tracker =
      test_util::CreateTracker(metadata, tracker_id, &app_root_tracker);

  scoped_ptr<leveldb::WriteBatch> batch(new leveldb::WriteBatch);
  index()->StoreFileTracker(file_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file");
  EXPECT_EQ(2U, tracker_ids.size());
  EXPECT_EQ(tracker_id, tracker_ids.active_tracker());

  ParentIDAndTitle multi_backing = index()->PickMultiBackingFilePath();
  EXPECT_EQ(kAppRootTrackerID, multi_backing.parent_id);
  EXPECT_EQ("file", multi_backing.title);

  // Testing UpdateInFileIDIndexes
  batch.reset(new leveldb::WriteBatch);
  file_tracker =
      test_util::CreateTracker(metadata, tracker_id, &app_root_tracker);
  file_tracker->set_active(false);

  index()->StoreFileTracker(file_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file");
  EXPECT_EQ(2U, tracker_ids.size());
  EXPECT_EQ(kInvalidTrackerID, tracker_ids.active_tracker());

  multi_backing = index()->PickMultiBackingFilePath();
  EXPECT_EQ(kAppRootTrackerID, multi_backing.parent_id);
  EXPECT_EQ("file", multi_backing.title);

  batch.reset(new leveldb::WriteBatch);
  file_tracker =
      test_util::CreateTracker(metadata, tracker_id, &app_root_tracker);

  index()->StoreFileTracker(file_tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file");
  EXPECT_EQ(2U, tracker_ids.size());
  EXPECT_EQ(tracker_id, tracker_ids.active_tracker());

  multi_backing = index()->PickMultiBackingFilePath();
  EXPECT_EQ(kAppRootTrackerID, multi_backing.parent_id);
  EXPECT_EQ("file", multi_backing.title);

  // Testing RemoveFromFileIDIndexes
  batch.reset(new leveldb::WriteBatch);
  index()->RemoveFileTracker(tracker_id, batch.get());
  WriteToDB(batch.Pass());
  tracker_ids = index()->GetFileTrackerIDsByParentAndTitle(
      kAppRootTrackerID, "file");
  EXPECT_EQ(1U, tracker_ids.size());
  EXPECT_EQ(kInvalidTrackerID, tracker_ids.active_tracker());

  multi_backing = index()->PickMultiBackingFilePath();
  EXPECT_EQ(kInvalidTrackerID, multi_backing.parent_id);
  EXPECT_TRUE(multi_backing.title.empty()) << multi_backing.title;
}

TEST_F(MetadataDatabaseIndexOnDiskTest, DirtyTrackersTest) {
  CreateTestDatabase(true);

  scoped_ptr<leveldb::WriteBatch> batch;
  // Testing public methods
  EXPECT_EQ(1U, index()->CountDirtyTracker());
  EXPECT_FALSE(index()->HasDemotedDirtyTracker());
  EXPECT_EQ(kPlaceholderTrackerID, index()->PickDirtyTracker());
  batch.reset(new leveldb::WriteBatch);
  index()->DemoteDirtyTracker(kPlaceholderTrackerID, batch.get());
  WriteToDB(batch.Pass());
  EXPECT_TRUE(index()->HasDemotedDirtyTracker());
  EXPECT_EQ(1U, index()->CountDirtyTracker());

  const int64 tracker_id = 13;
  scoped_ptr<FileTracker> app_root_tracker(new FileTracker);
  index()->GetFileTracker(kAppRootTrackerID, app_root_tracker.get());

  // Testing AddDirtyTrackerIndexes
  scoped_ptr<FileTracker> tracker =
      test_util::CreatePlaceholderTracker("placeholder",
                                          tracker_id,
                                          app_root_tracker.get());
  batch.reset(new leveldb::WriteBatch);
  index()->StoreFileTracker(tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(2U, index()->CountDirtyTracker());
  EXPECT_EQ(tracker_id, index()->PickDirtyTracker());

  // Testing UpdateDirtyTrackerIndexes
  tracker = test_util::CreatePlaceholderTracker("placeholder",
                                                tracker_id,
                                                app_root_tracker.get());
  tracker->set_dirty(false);
  batch.reset(new leveldb::WriteBatch);
  index()->StoreFileTracker(tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(1U, index()->CountDirtyTracker());
  EXPECT_EQ(kInvalidTrackerID, index()->PickDirtyTracker());

  tracker = test_util::CreatePlaceholderTracker("placeholder",
                                                tracker_id,
                                                app_root_tracker.get());
  batch.reset(new leveldb::WriteBatch);
  index()->StoreFileTracker(tracker.Pass(), batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(2U, index()->CountDirtyTracker());
  EXPECT_EQ(tracker_id, index()->PickDirtyTracker());

  // Testing RemoveFromDirtyTrackerIndexes
  batch.reset(new leveldb::WriteBatch);
  index()->RemoveFileTracker(tracker_id, batch.get());
  WriteToDB(batch.Pass());
  EXPECT_EQ(1U, index()->CountDirtyTracker());
  EXPECT_EQ(kInvalidTrackerID, index()->PickDirtyTracker());
}

}  // namespace drive_backend
}  // namespace sync_file_system
