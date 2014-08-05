// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/leveldb_wrapper.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace sync_file_system {
namespace drive_backend {

namespace {
const int64 kSyncRootTrackerID = 100;
}  // namespace

class RegisterAppTaskTest : public testing::Test {
 public:
  RegisterAppTaskTest()
      : next_file_id_(1000),
        next_tracker_id_(10000) {}
  virtual ~RegisterAppTaskTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
    in_memory_env_.reset(leveldb::NewMemEnv(leveldb::Env::Default()));

    scoped_ptr<drive::FakeDriveService>
        fake_drive_service(new drive::FakeDriveService);
    scoped_ptr<drive::DriveUploaderInterface>
        drive_uploader(new drive::DriveUploader(
            fake_drive_service.get(),
            base::ThreadTaskRunnerHandle::Get()));

    fake_drive_service_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service.get(), drive_uploader.get(),
        kSyncRootFolderTitle));

    context_.reset(
        new SyncEngineContext(
            fake_drive_service.PassAs<drive::DriveServiceInterface>(),
            drive_uploader.Pass(),
            NULL,
            base::ThreadTaskRunnerHandle::Get(),
            base::ThreadTaskRunnerHandle::Get()));

    ASSERT_EQ(google_apis::HTTP_CREATED,
              fake_drive_service_helper_->AddOrphanedFolder(
                  kSyncRootFolderTitle, &sync_root_folder_id_));
  }

  virtual void TearDown() OVERRIDE {
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  scoped_ptr<LevelDBWrapper> OpenLevelDB() {
    leveldb::DB* db = NULL;
    leveldb::Options options;
    options.create_if_missing = true;
    options.env = in_memory_env_.get();
    leveldb::Status status =
        leveldb::DB::Open(options, database_dir_.path().AsUTF8Unsafe(), &db);
    EXPECT_TRUE(status.ok());
    return make_scoped_ptr(new LevelDBWrapper(make_scoped_ptr(db)));
  }

  void SetUpInitialData(LevelDBWrapper* db) {
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

    db->Put(kDatabaseVersionKey,
            base::Int64ToString(kCurrentDatabaseVersion));
    PutServiceMetadataToDB(service_metadata, db);
    PutFileMetadataToDB(sync_root_metadata, db);
    PutFileTrackerToDB(sync_root_tracker, db);
    EXPECT_TRUE(db->Commit().ok());
  }

  void CreateMetadataDatabase(scoped_ptr<LevelDBWrapper> db) {
    ASSERT_TRUE(db);
    ASSERT_FALSE(context_->GetMetadataDatabase());
    scoped_ptr<MetadataDatabase> metadata_db;
    ASSERT_EQ(SYNC_STATUS_OK,
              MetadataDatabase::CreateForTesting(
                  db.Pass(), &metadata_db));
    context_->SetMetadataDatabase(metadata_db.Pass());
  }

  SyncStatusCode RunRegisterAppTask(const std::string& app_id) {
    RegisterAppTask task(context_.get(), app_id);
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    task.RunExclusive(CreateResultReceiver(&status));
    base::RunLoop().RunUntilIdle();
    return status;
  }

  void SetUpRegisteredAppRoot(
      const std::string& app_id,
      LevelDBWrapper* db) {
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

    PutFileMetadataToDB(metadata, db);
    PutFileTrackerToDB(tracker, db);
    EXPECT_TRUE(db->Commit().ok());
  }

  void SetUpUnregisteredAppRoot(const std::string& app_id,
                                LevelDBWrapper* db) {
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

    PutFileMetadataToDB(metadata, db);
    PutFileTrackerToDB(tracker, db);
    EXPECT_TRUE(db->Commit().ok());
  }

  size_t CountRegisteredAppRoot() {
    std::vector<std::string> app_ids;
    context_->GetMetadataDatabase()->GetRegisteredAppIDs(&app_ids);
    return app_ids.size();
  }

  bool IsAppRegistered(const std::string& app_id) {
    TrackerIDSet trackers;
    if (!context_->GetMetadataDatabase()->FindTrackersByParentAndTitle(
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
    TrackerIDSet files;
    if (!context_->GetMetadataDatabase()->FindTrackersByParentAndTitle(
            kSyncRootTrackerID, app_id, &files) ||
        !files.has_active())
      return false;

    FileTracker app_root_tracker;
    EXPECT_TRUE(context_->GetMetadataDatabase()->FindTrackerByTrackerID(
        files.active_tracker(), &app_root_tracker));
    std::string app_root_folder_id = app_root_tracker.file_id();
    scoped_ptr<google_apis::FileResource> entry;
    if (google_apis::HTTP_SUCCESS !=
        fake_drive_service_helper_->GetFileResource(app_root_folder_id, &entry))
      return false;

    return !entry->labels().is_trashed();
  }

 private:
  std::string GenerateFileID() {
    return base::StringPrintf("file_id_%" PRId64, next_file_id_++);
  }

  scoped_ptr<leveldb::Env> in_memory_env_;

  std::string sync_root_folder_id_;

  int64 next_file_id_;
  int64 next_tracker_id_;

  content::TestBrowserThreadBundle browser_threads_;
  base::ScopedTempDir database_dir_;

  scoped_ptr<SyncEngineContext> context_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_service_helper_;

  DISALLOW_COPY_AND_ASSIGN(RegisterAppTaskTest);
};

TEST_F(RegisterAppTaskTest, AlreadyRegistered) {
  scoped_ptr<LevelDBWrapper> db = OpenLevelDB();
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
  scoped_ptr<LevelDBWrapper> db = OpenLevelDB();
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
  scoped_ptr<LevelDBWrapper> db = OpenLevelDB();
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
  scoped_ptr<LevelDBWrapper> db = OpenLevelDB();
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
