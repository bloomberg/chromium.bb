// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace sync_file_system {
namespace drive_backend {

namespace {
const int64 kSyncRootTrackerID = 100;
}  // namespace

class RegisterAppTaskTest : public testing::Test,
                            public SyncEngineContext {
 public:
  RegisterAppTaskTest()
      : next_file_id_(1000),
        next_tracker_id_(10000) {}
  virtual ~RegisterAppTaskTest() {}

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

    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id_));
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

 protected:
  scoped_ptr<leveldb::DB> OpenLevelDB() {
    leveldb::DB* db = NULL;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status =
        leveldb::DB::Open(options, database_dir_.path().AsUTF8Unsafe(), &db);
    EXPECT_TRUE(status.ok());
    return make_scoped_ptr<leveldb::DB>(db);
  }

  void SetUpInitialData(leveldb::DB* db) {
    ServiceMetadata service_metadata;
    service_metadata.set_largest_change_id(100);
    service_metadata.set_sync_root_tracker_id(kSyncRootTrackerID);
    service_metadata.set_next_tracker_id(next_tracker_id_);

    FileDetails sync_root_details;
    sync_root_details.set_title(kSyncRootFolderTitle);
    sync_root_details.set_file_kind(FILE_KIND_FOLDER);
    sync_root_details.set_change_id(1);

    FileMetadata sync_root_metadata;
    sync_root_metadata.set_file_id(sync_root_folder_id_);
    *sync_root_metadata.mutable_details() = sync_root_details;

    FileTracker sync_root_tracker;
    sync_root_tracker.set_tracker_id(service_metadata.sync_root_tracker_id());
    sync_root_tracker.set_parent_tracker_id(0);
    sync_root_tracker.set_file_id(sync_root_metadata.file_id());
    sync_root_tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
    *sync_root_tracker.mutable_synced_details() = sync_root_details;
    sync_root_tracker.set_active(true);

    leveldb::WriteBatch batch;
    batch.Put(kDatabaseVersionKey,
              base::Int64ToString(kCurrentDatabaseVersion));
    PutServiceMetadataToBatch(service_metadata, &batch);
    PutFileToBatch(sync_root_metadata, &batch);
    PutTrackerToBatch(sync_root_tracker, &batch);
    EXPECT_TRUE(db->Write(leveldb::WriteOptions(), &batch).ok());
  }

  void CreateMetadataDatabase(scoped_ptr<leveldb::DB> db) {
    ASSERT_TRUE(db);
    ASSERT_FALSE(metadata_database_);
    ASSERT_EQ(SYNC_STATUS_OK,
              MetadataDatabase::CreateForTesting(
                  db.Pass(), &metadata_database_));
  }

  SyncStatusCode RunRegisterAppTask(const std::string& app_id) {
    RegisterAppTask task(this, app_id);
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    task.Run(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void SetUpRegisteredAppRoot(
      const std::string& app_id,
      leveldb::DB* db) {
    FileDetails details;
    details.set_title(app_id);
    details.set_file_kind(FILE_KIND_FOLDER);
    details.add_parent_folder_ids(sync_root_folder_id_);

    FileMetadata metadata;
    metadata.set_file_id(GenerateFileID());
    *metadata.mutable_details() = details;

    FileTracker tracker;
    tracker.set_parent_tracker_id(kSyncRootTrackerID);
    tracker.set_tracker_id(next_tracker_id_++);
    tracker.set_file_id(metadata.file_id());
    tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
    tracker.set_app_id(app_id);
    *tracker.mutable_synced_details() = details;
    tracker.set_active(true);

    leveldb::WriteBatch batch;
    PutFileToBatch(metadata, &batch);
    PutTrackerToBatch(tracker, &batch);
    EXPECT_TRUE(db->Write(leveldb::WriteOptions(), &batch).ok());
  }

  void SetUpUnregisteredAppRoot(const std::string& app_id,
                                leveldb::DB* db) {
    FileDetails details;
    details.set_title(app_id);
    details.set_file_kind(FILE_KIND_FOLDER);
    details.add_parent_folder_ids(sync_root_folder_id_);

    FileMetadata metadata;
    metadata.set_file_id(GenerateFileID());
    *metadata.mutable_details() = details;

    FileTracker tracker;
    tracker.set_parent_tracker_id(kSyncRootTrackerID);
    tracker.set_tracker_id(next_tracker_id_++);
    tracker.set_file_id(metadata.file_id());
    tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
    *tracker.mutable_synced_details() = details;
    tracker.set_active(false);

    leveldb::WriteBatch batch;
    PutFileToBatch(metadata, &batch);
    PutTrackerToBatch(tracker, &batch);
    EXPECT_TRUE(db->Write(leveldb::WriteOptions(), &batch).ok());
  }

  size_t CountRegisteredAppRoot() {
    // TODO(tzik): Add function to MetadataDatabase to list trackers by parent.
    typedef MetadataDatabase::TrackersByTitle TrackersByTitle;
    const TrackersByTitle& trackers_by_title =
        metadata_database_->trackers_by_parent_and_title_[kSyncRootTrackerID];

    size_t count = 0;
    for (TrackersByTitle::const_iterator itr = trackers_by_title.begin();
         itr != trackers_by_title.end(); ++itr) {
      if (itr->second.has_active())
        ++count;
    }

    return count;
  }

  bool IsAppRegistered(const std::string& app_id) {
    TrackerSet trackers;
    if (!metadata_database_->FindTrackersByParentAndTitle(
            kSyncRootTrackerID, app_id, &trackers))
      return false;
    return trackers.has_active();
  }

  size_t CountRemoteFileInSyncRoot() {
    ScopedVector<google_apis::ResourceEntry> files;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_service_helper_->ListFilesInFolder(
                  sync_root_folder_id_, &files));
    return files.size();
  }

  bool HasRemoteAppRoot(const std::string& app_id) {
    TrackerSet files;
    if (!metadata_database_->FindTrackersByParentAndTitle(
            kSyncRootTrackerID, app_id, &files) ||
        !files.has_active())
      return false;

    std::string app_root_folder_id = files.active_tracker()->file_id();
    scoped_ptr<google_apis::ResourceEntry> entry;
    if (google_apis::HTTP_SUCCESS !=
        fake_drive_service_helper_->GetResourceEntry(
            app_root_folder_id, &entry))
      return false;

    return !entry->deleted();
  }

 private:
  std::string GenerateFileID() {
    return base::StringPrintf("file_id_%" PRId64, next_file_id_++);
  }

  std::string sync_root_folder_id_;

  int64 next_file_id_;
  int64 next_tracker_id_;

  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir database_dir_;

  scoped_ptr<drive::FakeDriveService> fake_drive_service_;
  scoped_ptr<drive::DriveUploader> drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_service_helper_;

  scoped_ptr<MetadataDatabase> metadata_database_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAppTaskTest);
};

TEST_F(RegisterAppTaskTest, AlreadyRegistered) {
  scoped_ptr<leveldb::DB> db(OpenLevelDB());
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  SetUpRegisteredAppRoot(kAppID, db.get());

  CreateMetadataDatabase(db.Pass());
  EXPECT_EQ(SYNC_STATUS_OK, RunRegisterAppTask(kAppID));

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));
}

TEST_F(RegisterAppTaskTest, CreateAppFolder) {
  scoped_ptr<leveldb::DB> db(OpenLevelDB());
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  CreateMetadataDatabase(db.Pass());
  RunRegisterAppTask(kAppID);

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));

  EXPECT_EQ(1u, CountRemoteFileInSyncRoot());
  EXPECT_TRUE(HasRemoteAppRoot(kAppID));
}

TEST_F(RegisterAppTaskTest, RegisterExistingFolder) {
  scoped_ptr<leveldb::DB> db(OpenLevelDB());
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  SetUpUnregisteredAppRoot(kAppID, db.get());

  CreateMetadataDatabase(db.Pass());
  RunRegisterAppTask(kAppID);

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));
}

TEST_F(RegisterAppTaskTest, RegisterExistingFolder_MultipleCandidate) {
  scoped_ptr<leveldb::DB> db(OpenLevelDB());
  ASSERT_TRUE(db);
  SetUpInitialData(db.get());

  const std::string kAppID = "app_id";
  SetUpUnregisteredAppRoot(kAppID, db.get());
  SetUpUnregisteredAppRoot(kAppID, db.get());

  CreateMetadataDatabase(db.Pass());
  RunRegisterAppTask(kAppID);

  EXPECT_EQ(1u, CountRegisteredAppRoot());
  EXPECT_TRUE(IsAppRegistered(kAppID));
}

}  // namespace drive_backend
}  // namespace sync_file_system
