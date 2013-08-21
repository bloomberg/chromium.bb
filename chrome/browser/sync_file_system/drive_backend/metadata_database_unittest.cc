// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

#define FPL(a) FILE_PATH_LITERAL(a)

namespace sync_file_system {
namespace drive_backend {

namespace {

typedef MetadataDatabase::FileIDList FileIDList;

const int64 kInitialChangeID = 1234;
const int64 kSyncRootTrackerID = 100;
const char kSyncRootFolderID[] = "sync_root_folder_id";

struct TrackedFile {
  FileMetadata metadata;
  FileTracker tracker;

  // Implies the file should not in the database.
  bool should_be_absent;

  // Implies the file should have a tracker in the database but should have no
  // metadata.
  bool tracker_only;

  TrackedFile() : should_be_absent(false), tracker_only(false) {}
};

void ExpectEquivalent(const ServiceMetadata* left,
                      const ServiceMetadata* right) {
  if (!left) {
    ASSERT_FALSE(right);
    return;
  }
  ASSERT_TRUE(right);

  EXPECT_EQ(left->largest_change_id(), right->largest_change_id());
  EXPECT_EQ(left->sync_root_tracker_id(), right->sync_root_tracker_id());
  EXPECT_EQ(left->next_tracker_id(), right->next_tracker_id());
}

void ExpectEquivalent(const FileDetails* left, const FileDetails* right) {
  if (!left) {
    ASSERT_FALSE(right);
    return;
  }
  ASSERT_TRUE(right);

  std::set<std::string> parents;
  for (int i = 0; i < left->parent_folder_ids_size(); ++i)
    EXPECT_TRUE(parents.insert(left->parent_folder_ids(i)).second);

  for (int i = 0; i < right->parent_folder_ids_size(); ++i)
    EXPECT_EQ(1u, parents.erase(left->parent_folder_ids(i)));
  EXPECT_TRUE(parents.empty());

  EXPECT_EQ(left->title(), right->title());
  EXPECT_EQ(left->file_kind(), right->file_kind());
  EXPECT_EQ(left->md5(), right->md5());
  EXPECT_EQ(left->etag(), right->etag());
  EXPECT_EQ(left->creation_time(), right->creation_time());
  EXPECT_EQ(left->modification_time(), right->modification_time());
  EXPECT_EQ(left->deleted(), right->deleted());
  EXPECT_EQ(left->change_id(), right->change_id());
}

void ExpectEquivalent(const FileMetadata* left, const FileMetadata* right) {
  if (!left) {
    ASSERT_FALSE(right);
    return;
  }
  ASSERT_TRUE(right);

  EXPECT_EQ(left->file_id(), right->file_id());
  ExpectEquivalent(&left->details(), &right->details());
}

void ExpectEquivalent(const FileTracker* left, const FileTracker* right) {
  if (!left) {
    ASSERT_FALSE(right);
    return;
  }
  ASSERT_TRUE(right);

  EXPECT_EQ(left->tracker_id(), right->tracker_id());
  EXPECT_EQ(left->parent_tracker_id(), right->parent_tracker_id());
  EXPECT_EQ(left->file_id(), right->file_id());
  EXPECT_EQ(left->app_id(), right->app_id());
  EXPECT_EQ(left->tracker_kind(), right->tracker_kind());
  ExpectEquivalent(&left->synced_details(), &right->synced_details());
  EXPECT_EQ(left->dirty(), right->dirty());
  EXPECT_EQ(left->active(), right->active());
  EXPECT_EQ(left->needs_folder_listing(), right->needs_folder_listing());
}

template <typename Container>
void ExpectEquivalentMaps(const Container& left, const Container& right);
template <typename Key, typename Value, typename Compare>
void ExpectEquivalent(const std::map<Key, Value, Compare>& left,
                      const std::map<Key, Value, Compare>& right) {
  ExpectEquivalentMaps(left, right);
}

template <typename Container>
void ExpectEquivalentSets(const Container& left, const Container& right);
template <typename Value, typename Compare>
void ExpectEquivalent(const std::set<Value, Compare>& left,
                      const std::set<Value, Compare>& right) {
  return ExpectEquivalentSets(left, right);
}

void ExpectEquivalent(const TrackerSet& left,
                      const TrackerSet& right) {
  {
    SCOPED_TRACE("Expect equivalent active_tracker");
    ExpectEquivalent(left.active_tracker(), right.active_tracker());
  }
  ExpectEquivalent(left.tracker_set(), right.tracker_set());
}

template <typename Container>
void ExpectEquivalentMaps(const Container& left, const Container& right) {
  ASSERT_EQ(left.size(), right.size());

  typedef typename Container::const_iterator const_iterator;
  const_iterator left_itr = left.begin();
  const_iterator right_itr = right.begin();
  while (left_itr != left.end()) {
    EXPECT_EQ(left_itr->first, right_itr->first);
    ExpectEquivalent(left_itr->second, right_itr->second);
    ++left_itr;
    ++right_itr;
  }
}

template <typename Container>
void ExpectEquivalentSets(const Container& left, const Container& right) {
  ASSERT_EQ(left.size(), right.size());

  typedef typename Container::const_iterator const_iterator;
  const_iterator left_itr = left.begin();
  const_iterator right_itr = right.begin();
  while (left_itr != left.end()) {
    ExpectEquivalent(*left_itr, *right_itr);
    ++left_itr;
    ++right_itr;
  }
}

void SyncStatusResultCallback(SyncStatusCode* status_out,
                              SyncStatusCode status) {
  EXPECT_EQ(SYNC_STATUS_UNKNOWN, *status_out);
  *status_out = status;
}

void DatabaseCreateResultCallback(SyncStatusCode* status_out,
                                  scoped_ptr<MetadataDatabase>* database_out,
                                  SyncStatusCode status,
                                  scoped_ptr<MetadataDatabase> database) {
  EXPECT_EQ(SYNC_STATUS_UNKNOWN, *status_out);
  *status_out = status;
  *database_out = database.Pass();
}

}  // namespace

class MetadataDatabaseTest : public testing::Test {
 public:
  MetadataDatabaseTest()
      : next_change_id_(kInitialChangeID + 1),
        next_tracker_id_(kSyncRootTrackerID + 1),
        next_file_id_number_(1),
        next_md5_sequence_number_(1) {}

  virtual ~MetadataDatabaseTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE { DropDatabase(); }

 protected:
  std::string GenerateFileID() {
    return "file_id_" + base::Int64ToString(next_file_id_number_++);
  }

  int64 GetTrackerIDByFileID(const std::string& file_id) {
    TrackerSet trackers;
    if (metadata_database_->FindTrackersByFileID(file_id, &trackers)) {
      EXPECT_FALSE(trackers.empty());
      return (*trackers.begin())->tracker_id();
    }
    return 0;
  }

  SyncStatusCode InitializeMetadataDatabase() {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    MetadataDatabase::Create(base::MessageLoopProxy::current(),
                             database_dir_.path(),
                             base::Bind(&DatabaseCreateResultCallback,
                                        &status, &metadata_database_));
    message_loop_.RunUntilIdle();
    return status;
  }

  void DropDatabase() {
    metadata_database_.reset();
    message_loop_.RunUntilIdle();
  }

  void SetUpDatabaseByTrackedFiles(const TrackedFile** tracked_files,
                                   int size) {
    scoped_ptr<leveldb::DB> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    for (int i = 0; i < size; ++i) {
      const TrackedFile* file = tracked_files[i];
      if (file->should_be_absent)
        continue;
      if (!file->tracker_only)
        EXPECT_TRUE(PutFileToDB(db.get(), file->metadata).ok());
      EXPECT_TRUE(PutTrackerToDB(db.get(), file->tracker).ok());
    }
  }

  void VerifyTrackedFile(const TrackedFile& file) {
    if (!file.should_be_absent) {
      if (file.tracker_only) {
        EXPECT_FALSE(metadata_database()->FindFileByFileID(
            file.metadata.file_id(), NULL));
      } else {
        VerifyFile(file.metadata);
      }
      VerifyTracker(file.tracker);
      return;
    }

    EXPECT_FALSE(metadata_database()->FindFileByFileID(
        file.metadata.file_id(), NULL));
    EXPECT_FALSE(metadata_database()->FindTrackerByTrackerID(
        file.tracker.tracker_id(), NULL));
  }

  void VerifyTrackedFiles(const TrackedFile** tracked_files, int size) {
    for (int i = 0; i < size; ++i)
      VerifyTrackedFile(*tracked_files[i]);
  }

  MetadataDatabase* metadata_database() { return metadata_database_.get(); }

  leveldb::DB* db() {
    if (!metadata_database_)
      return NULL;
    return metadata_database_->db_.get();
  }

  scoped_ptr<leveldb::DB> InitializeLevelDB() {
    leveldb::DB* db = NULL;
    leveldb::Options options;
    options.create_if_missing = true;
    options.max_open_files = 0;  // Use minimum.
    leveldb::Status status =
        leveldb::DB::Open(options, database_dir_.path().AsUTF8Unsafe(), &db);
    EXPECT_TRUE(status.ok());

    db->Put(leveldb::WriteOptions(), "VERSION", base::Int64ToString(3));
    SetUpServiceMetadata(db);

    return make_scoped_ptr(db);
  }

  void SetUpServiceMetadata(leveldb::DB* db) {
    ServiceMetadata service_metadata;
    service_metadata.set_largest_change_id(kInitialChangeID);
    service_metadata.set_sync_root_tracker_id(kSyncRootTrackerID);
    service_metadata.set_next_tracker_id(next_tracker_id_);
    std::string value;
    ASSERT_TRUE(service_metadata.SerializeToString(&value));
    db->Put(leveldb::WriteOptions(), "SERVICE", value);
  }

  FileMetadata CreateSyncRootMetadata() {
    FileMetadata sync_root;
    sync_root.set_file_id(kSyncRootFolderID);
    FileDetails* details = sync_root.mutable_details();
    details->set_title("Chrome Syncable FileSystem");
    details->set_file_kind(FILE_KIND_FOLDER);
    return sync_root;
  }

  FileMetadata CreateFileMetadata(const FileMetadata& parent,
                                  const std::string& title) {
    FileMetadata file;
    file.set_file_id(GenerateFileID());
    FileDetails* details = file.mutable_details();
    details->add_parent_folder_ids(parent.file_id());
    details->set_title(title);
    details->set_file_kind(FILE_KIND_FILE);
    details->set_md5(
        "md5_value_" + base::Int64ToString(next_md5_sequence_number_++));
    return file;
  }

  FileMetadata CreateFolderMetadata(const FileMetadata& parent,
                                    const std::string& title) {
    FileMetadata folder;
    folder.set_file_id(GenerateFileID());
    FileDetails* details = folder.mutable_details();
    details->add_parent_folder_ids(parent.file_id());
    details->set_title(title);
    details->set_file_kind(FILE_KIND_FOLDER);
    return folder;
  }

  FileTracker CreateSyncRootTracker(const FileMetadata& sync_root) {
    FileTracker sync_root_tracker;
    sync_root_tracker.set_tracker_id(kSyncRootTrackerID);
    sync_root_tracker.set_parent_tracker_id(0);
    sync_root_tracker.set_file_id(sync_root.file_id());
    sync_root_tracker.set_dirty(false);
    sync_root_tracker.set_active(true);
    sync_root_tracker.set_needs_folder_listing(false);
    *sync_root_tracker.mutable_synced_details() = sync_root.details();
    return sync_root_tracker;
  }

  FileTracker CreateTracker(const FileTracker& parent_tracker,
                            const FileMetadata& file) {
    FileTracker tracker;
    tracker.set_tracker_id(next_tracker_id_++);
    tracker.set_parent_tracker_id(parent_tracker.tracker_id());
    tracker.set_file_id(file.file_id());
    tracker.set_app_id(parent_tracker.app_id());
    tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
    tracker.set_dirty(false);
    tracker.set_active(true);
    tracker.set_needs_folder_listing(false);
    *tracker.mutable_synced_details() = file.details();
    return tracker;
  }

  TrackedFile CreateTrackedSyncRoot() {
    TrackedFile sync_root;
    sync_root.metadata = CreateSyncRootMetadata();
    sync_root.tracker = CreateSyncRootTracker(sync_root.metadata);
    return sync_root;
  }

  TrackedFile CreateTrackedAppRoot(const TrackedFile& sync_root,
                                   const std::string& app_id) {
    TrackedFile app_root(CreateTrackedFolder(sync_root, app_id));
    app_root.tracker.set_app_id(app_id);
    app_root.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
    return app_root;
  }

  TrackedFile CreateTrackedFile(const TrackedFile& parent,
                                const std::string& title) {
    TrackedFile file;
    file.metadata = CreateFileMetadata(parent.metadata, title);
    file.tracker = CreateTracker(parent.tracker, file.metadata);
    return file;
  }

  TrackedFile CreateTrackedFolder(const TrackedFile& parent,
                                  const std::string& title) {
    TrackedFile folder;
    folder.metadata = CreateFolderMetadata(parent.metadata, title);
    folder.tracker = CreateTracker(parent.tracker, folder.metadata);
    return folder;
  }

  scoped_ptr<google_apis::ChangeResource> CreateChangeResourceFromMetadata(
      const FileMetadata& file) {
    scoped_ptr<google_apis::ChangeResource> change(
        new google_apis::ChangeResource);
    change->set_change_id(file.details().change_id());
    change->set_file_id(file.file_id());
    change->set_deleted(file.details().deleted());
    if (change->is_deleted())
      return change.Pass();

    scoped_ptr<google_apis::FileResource> file_resource(
        new google_apis::FileResource);
    ScopedVector<google_apis::ParentReference> parents;
    for (int i = 0; i < file.details().parent_folder_ids_size(); ++i) {
      scoped_ptr<google_apis::ParentReference> parent(
          new google_apis::ParentReference);
      parent->set_file_id(file.details().parent_folder_ids(i));
      parents.push_back(parent.release());
    }

    file_resource->set_file_id(file.file_id());
    file_resource->set_parents(&parents);
    file_resource->set_title(file.details().title());
    if (file.details().file_kind() == FILE_KIND_FOLDER)
      file_resource->set_mime_type("application/vnd.google-apps.folder");
    else if (file.details().file_kind() == FILE_KIND_FILE)
      file_resource->set_mime_type("text/plain");
    else
      file_resource->set_mime_type("application/vnd.google-apps.document");
    file_resource->set_md5_checksum(file.details().md5());
    file_resource->set_etag(file.details().etag());
    file_resource->set_created_date(base::Time::FromInternalValue(
        file.details().creation_time()));
    file_resource->set_modified_date(base::Time::FromInternalValue(
        file.details().modification_time()));

    change->set_file(file_resource.Pass());
    return change.Pass();
  }

  void ApplyRenameChangeToMetadata(const std::string& new_title,
                                   FileMetadata* file) {
    FileDetails* details = file->mutable_details();
    details->set_title(new_title);
    details->set_change_id(next_change_id_++);
  }

  void ApplyReorganizeChangeToMetadata(const std::string& new_parent,
                                       FileMetadata* file) {
    FileDetails* details = file->mutable_details();
    details->clear_parent_folder_ids();
    details->add_parent_folder_ids(new_parent);
    details->set_change_id(next_change_id_++);
  }

  void ApplyContentChangeToMetadata(FileMetadata* file) {
    FileDetails* details = file->mutable_details();
    details->set_md5(
        "md5_value_" + base::Int64ToString(next_md5_sequence_number_++));
    details->set_change_id(next_change_id_++);
  }

  void PushToChangeList(scoped_ptr<google_apis::ChangeResource> change,
                        ScopedVector<google_apis::ChangeResource>* changes) {
    changes->push_back(change.release());
  }

  leveldb::Status PutFileToDB(leveldb::DB* db, const FileMetadata& file) {
    std::string key = "FILE: " + file.file_id();
    std::string value;
    file.SerializeToString(&value);
    return db->Put(leveldb::WriteOptions(), key, value);
  }

  leveldb::Status PutTrackerToDB(leveldb::DB* db,
                                 const FileTracker& tracker) {
    std::string key = "TRACKER: " + base::Int64ToString(tracker.tracker_id());
    std::string value;
    tracker.SerializeToString(&value);
    return db->Put(leveldb::WriteOptions(), key, value);
  }

  void VerifyReloadConsistency() {
    scoped_ptr<MetadataDatabase> metadata_database_2;
    ASSERT_EQ(SYNC_STATUS_OK,
              MetadataDatabase::CreateForTesting(
                  metadata_database_->db_.Pass(),
                  &metadata_database_2));
    metadata_database_->db_ = metadata_database_2->db_.Pass();

    {
      SCOPED_TRACE("Expect equivalent service_metadata");
      ExpectEquivalent(metadata_database_->service_metadata_.get(),
                       metadata_database_2->service_metadata_.get());
    }

    {
      SCOPED_TRACE("Expect equivalent file_by_id_ contents.");
      ExpectEquivalent(metadata_database_->file_by_id_,
                       metadata_database_2->file_by_id_);
    }

    {
      SCOPED_TRACE("Expect equivalent tracker_by_id_ contents.");
      ExpectEquivalent(metadata_database_->tracker_by_id_,
                       metadata_database_2->tracker_by_id_);
    }

    {
      SCOPED_TRACE("Expect equivalent trackers_by_file_id_ contents.");
      ExpectEquivalent(metadata_database_->trackers_by_file_id_,
                       metadata_database_2->trackers_by_file_id_);
    }

    {
      SCOPED_TRACE("Expect equivalent app_root_by_app_id_ contents.");
      ExpectEquivalent(metadata_database_->app_root_by_app_id_,
                       metadata_database_2->app_root_by_app_id_);
    }

    {
      SCOPED_TRACE("Expect equivalent trackers_by_parent_and_title_ contents.");
      ExpectEquivalent(metadata_database_->trackers_by_parent_and_title_,
                       metadata_database_2->trackers_by_parent_and_title_);
    }

    {
      SCOPED_TRACE("Expect equivalent dirty_trackers_ contents.");
      ExpectEquivalent(metadata_database_->dirty_trackers_,
                       metadata_database_2->dirty_trackers_);
    }
  }

  void VerifyFile(const FileMetadata& file) {
    FileMetadata file_in_metadata_database;
    ASSERT_TRUE(metadata_database()->FindFileByFileID(
        file.file_id(), &file_in_metadata_database));

    SCOPED_TRACE("Expect equivalent " + file.file_id());
    ExpectEquivalent(&file, &file_in_metadata_database);
  }

  void VerifyTracker(const FileTracker& tracker) {
    FileTracker tracker_in_metadata_database;
    ASSERT_TRUE(metadata_database()->FindTrackerByTrackerID(
        tracker.tracker_id(), &tracker_in_metadata_database));

    SCOPED_TRACE("Expect equivalent tracker[" +
                 base::Int64ToString(tracker.tracker_id()) + "]");
    ExpectEquivalent(&tracker, &tracker_in_metadata_database);
  }

  SyncStatusCode RegisterApp(const std::string& app_id,
                             const std::string& folder_id) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_->RegisterApp(
        app_id, folder_id,
        base::Bind(&SyncStatusResultCallback, &status));
    message_loop_.RunUntilIdle();
    return status;
  }

  SyncStatusCode DisableApp(const std::string& app_id) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_->DisableApp(
        app_id, base::Bind(&SyncStatusResultCallback, &status));
    message_loop_.RunUntilIdle();
    return status;
  }

  SyncStatusCode EnableApp(const std::string& app_id) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_->EnableApp(
        app_id, base::Bind(&SyncStatusResultCallback, &status));
    message_loop_.RunUntilIdle();
    return status;
  }

  SyncStatusCode UnregisterApp(const std::string& app_id) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_->UnregisterApp(
        app_id, base::Bind(&SyncStatusResultCallback, &status));
    message_loop_.RunUntilIdle();
    return status;
  }

  SyncStatusCode UpdateByChangeList(
      ScopedVector<google_apis::ChangeResource> changes) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_->UpdateByChangeList(
        changes.Pass(), base::Bind(&SyncStatusResultCallback, &status));
    message_loop_.RunUntilIdle();
    return status;
  }

  SyncStatusCode PopulateFolder(const std::string& folder_id,
                                const FileIDList& listed_children) {
    SyncStatusCode status = SYNC_STATUS_UNKNOWN;
    metadata_database_->PopulateFolder(
        folder_id, listed_children,
        base::Bind(&SyncStatusResultCallback, &status));
    message_loop_.RunUntilIdle();
    return status;
  }

 private:
  base::ScopedTempDir database_dir_;
  base::MessageLoop message_loop_;

  scoped_ptr<MetadataDatabase> metadata_database_;

  int64 next_change_id_;
  int64 next_tracker_id_;
  int64 next_file_id_number_;
  int64 next_md5_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseTest);
};

TEST_F(MetadataDatabaseTest, InitializationTest_Empty) {
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  DropDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
}

TEST_F(MetadataDatabaseTest, InitializationTest_SimpleTree) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedFolder(sync_root, "app_id"));
  app_root.tracker.set_app_id(app_root.metadata.details().title());
  app_root.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);

  TrackedFile file(CreateTrackedFile(app_root, "file"));
  TrackedFile folder(CreateTrackedFolder(app_root, "folder"));
  TrackedFile file_in_folder(CreateTrackedFile(folder, "file_in_folder"));
  TrackedFile orphaned_file(CreateTrackedFile(sync_root, "orphaned_file"));
  orphaned_file.metadata.mutable_details()->clear_parent_folder_ids();
  orphaned_file.tracker.set_parent_tracker_id(0);

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &file, &folder, &file_in_folder, &orphaned_file
  };

  SetUpDatabaseByTrackedFiles(tracked_files, arraysize(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  orphaned_file.should_be_absent = true;
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));
}

TEST_F(MetadataDatabaseTest, AppManagementTest) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedFolder(sync_root, "app_id"));
  app_root.tracker.set_app_id(app_root.metadata.details().title());
  app_root.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);

  TrackedFile file(CreateTrackedFile(app_root, "file"));
  TrackedFile folder(CreateTrackedFolder(sync_root, "folder"));
  folder.tracker.set_active(false);

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &file, &folder,
  };
  SetUpDatabaseByTrackedFiles(tracked_files, arraysize(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));

  folder.tracker.set_app_id("foo");
  EXPECT_EQ(SYNC_STATUS_OK, RegisterApp(
      folder.tracker.app_id(), folder.metadata.file_id()));
  folder.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
  folder.tracker.set_active(true);
  folder.tracker.set_dirty(true);
  folder.tracker.set_needs_folder_listing(true);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, DisableApp(folder.tracker.app_id()));
  folder.tracker.set_tracker_kind(TRACKER_KIND_DISABLED_APP_ROOT);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, EnableApp(folder.tracker.app_id()));
  folder.tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, UnregisterApp(folder.tracker.app_id()));
  folder.tracker.set_app_id(std::string());
  folder.tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
  folder.tracker.set_active(false);
  VerifyTrackedFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, UnregisterApp(app_root.tracker.app_id()));
  app_root.tracker.set_app_id(std::string());
  app_root.tracker.set_tracker_kind(TRACKER_KIND_REGULAR);
  app_root.tracker.set_active(false);
  app_root.tracker.set_dirty(true);
  file.should_be_absent = true;
  VerifyTrackedFile(app_root);
  VerifyTrackedFile(file);
  VerifyReloadConsistency();
}

TEST_F(MetadataDatabaseTest, BuildPathTest) {
  FileMetadata sync_root(CreateSyncRootMetadata());
  FileTracker sync_root_tracker(CreateSyncRootTracker(sync_root));

  FileMetadata app_root(CreateFolderMetadata(sync_root, "app_id"));
  FileTracker app_root_tracker(
      CreateTracker(sync_root_tracker, app_root));
  app_root_tracker.set_app_id(app_root.details().title());
  app_root_tracker.set_tracker_kind(TRACKER_KIND_APP_ROOT);

  FileMetadata folder(CreateFolderMetadata(app_root, "folder"));
  FileTracker folder_tracker(CreateTracker(app_root_tracker, folder));

  FileMetadata file(CreateFolderMetadata(folder, "file"));
  FileTracker file_tracker(CreateTracker(folder_tracker, file));

  FileMetadata inactive_folder(CreateFolderMetadata(app_root, "folder"));
  FileTracker inactive_folder_tracker(CreateTracker(app_root_tracker,
                                                          inactive_folder));
  inactive_folder_tracker.set_active(false);

  {
    scoped_ptr<leveldb::DB> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), sync_root_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), app_root_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), folder_tracker).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file).ok());
    EXPECT_TRUE(PutTrackerToDB(db.get(), file_tracker).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  base::FilePath path;
  EXPECT_FALSE(metadata_database()->BuildPathForTracker(
      sync_root_tracker.tracker_id(), &path));
  EXPECT_TRUE(metadata_database()->BuildPathForTracker(
      app_root_tracker.tracker_id(), &path));
  EXPECT_EQ(base::FilePath(FPL("/")).NormalizePathSeparators(), path);
  EXPECT_TRUE(metadata_database()->BuildPathForTracker(
      file_tracker.tracker_id(), &path));
  EXPECT_EQ(base::FilePath(FPL("/folder/file")).NormalizePathSeparators(),
            path);
}

TEST_F(MetadataDatabaseTest, UpdateByChangeListTest) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedFolder(sync_root, "app_id"));
  TrackedFile disabled_app_root(CreateTrackedFolder(sync_root, "disabled_app"));
  TrackedFile file(CreateTrackedFile(app_root, "file"));
  TrackedFile renamed_file(CreateTrackedFile(app_root, "to be renamed"));
  TrackedFile folder(CreateTrackedFolder(app_root, "folder"));
  TrackedFile reorganized_file(
      CreateTrackedFile(app_root, "to be reorganized"));
  TrackedFile updated_file(
      CreateTrackedFile(app_root, "to be updated"));
  TrackedFile noop_file(CreateTrackedFile(app_root, "has noop change"));
  TrackedFile new_file(CreateTrackedFile(app_root, "to be added later"));
  new_file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &disabled_app_root,
    &file, &renamed_file, &folder, &reorganized_file, &updated_file, &noop_file,
    &new_file,
  };

  SetUpDatabaseByTrackedFiles(tracked_files, arraysize(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  ApplyRenameChangeToMetadata("renamed", &renamed_file.metadata);
  ApplyReorganizeChangeToMetadata(folder.metadata.file_id(),
                                  &reorganized_file.metadata);
  ApplyContentChangeToMetadata(&updated_file.metadata);

  ScopedVector<google_apis::ChangeResource> changes;
  PushToChangeList(
      CreateChangeResourceFromMetadata(renamed_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(reorganized_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(updated_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(noop_file.metadata), &changes);
  PushToChangeList(
      CreateChangeResourceFromMetadata(new_file.metadata), &changes);
  EXPECT_EQ(SYNC_STATUS_OK, UpdateByChangeList(changes.Pass()));

  renamed_file.tracker.set_dirty(true);
  reorganized_file.tracker.set_dirty(true);
  updated_file.tracker.set_dirty(true);
  noop_file.tracker.set_dirty(true);
  new_file.tracker.clear_synced_details();
  new_file.tracker.set_active(false);
  new_file.tracker.set_dirty(true);
  new_file.tracker.set_tracker_id(
      GetTrackerIDByFileID(new_file.metadata.file_id()));
  EXPECT_NE(0, new_file.tracker.tracker_id());

  new_file.should_be_absent = false;

  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));
  VerifyReloadConsistency();
}

TEST_F(MetadataDatabaseTest, PopulateFolderTest_RegularFolder) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedAppRoot(sync_root, "app_id"));
  app_root.tracker.set_app_id(app_root.metadata.details().title());

  TrackedFile folder_to_populate(
      CreateTrackedFolder(app_root, "folder_to_populate"));
  folder_to_populate.tracker.set_needs_folder_listing(true);
  folder_to_populate.tracker.set_dirty(true);

  TrackedFile known_file(CreateTrackedFile(folder_to_populate, "known_file"));
  TrackedFile new_file(CreateTrackedFile(folder_to_populate, "new_file"));
  new_file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &folder_to_populate, &known_file, &new_file
  };

  SetUpDatabaseByTrackedFiles(tracked_files, arraysize(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));

  FileIDList listed_children;
  listed_children.push_back(known_file.metadata.file_id());
  listed_children.push_back(new_file.metadata.file_id());

  EXPECT_EQ(SYNC_STATUS_OK,
            PopulateFolder(folder_to_populate.metadata.file_id(),
                           listed_children));

  folder_to_populate.tracker.set_dirty(false);
  folder_to_populate.tracker.set_needs_folder_listing(false);
  new_file.tracker.set_tracker_id(
      GetTrackerIDByFileID(new_file.metadata.file_id()));
  new_file.tracker.set_dirty(true);
  new_file.tracker.set_active(false);
  new_file.tracker.clear_synced_details();
  new_file.should_be_absent = false;
  new_file.tracker_only = true;
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));
  VerifyReloadConsistency();
}

TEST_F(MetadataDatabaseTest, PopulateFolderTest_InactiveFolder) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile app_root(CreateTrackedAppRoot(sync_root, "app_id"));

  TrackedFile inactive_folder(CreateTrackedFolder(app_root, "inactive_folder"));
  inactive_folder.tracker.set_active(false);
  inactive_folder.tracker.set_dirty(true);

  TrackedFile new_file(
      CreateTrackedFile(inactive_folder, "file_in_inactive_folder"));
  new_file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &app_root, &inactive_folder, &new_file,
  };

  SetUpDatabaseByTrackedFiles(tracked_files, arraysize(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));

  FileIDList listed_children;
  listed_children.push_back(new_file.metadata.file_id());

  EXPECT_EQ(SYNC_STATUS_OK,
            PopulateFolder(inactive_folder.metadata.file_id(),
                           listed_children));
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));
  VerifyReloadConsistency();
}

TEST_F(MetadataDatabaseTest, PopulateFolderTest_DisabledAppRoot) {
  TrackedFile sync_root(CreateTrackedSyncRoot());
  TrackedFile disabled_app_root(
      CreateTrackedAppRoot(sync_root, "disabled_app"));
  disabled_app_root.tracker.set_dirty(true);
  disabled_app_root.tracker.set_needs_folder_listing(true);

  TrackedFile known_file(CreateTrackedFile(disabled_app_root, "known_file"));
  TrackedFile file(CreateTrackedFile(disabled_app_root, "file"));
  file.should_be_absent = true;

  const TrackedFile* tracked_files[] = {
    &sync_root, &disabled_app_root, &disabled_app_root, &known_file, &file,
  };

  SetUpDatabaseByTrackedFiles(tracked_files, arraysize(tracked_files));
  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));

  FileIDList disabled_app_children;
  disabled_app_children.push_back(file.metadata.file_id());
  EXPECT_EQ(SYNC_STATUS_OK, PopulateFolder(
      disabled_app_root.metadata.file_id(), disabled_app_children));
  file.tracker.set_tracker_id(GetTrackerIDByFileID(file.metadata.file_id()));
  file.tracker.clear_synced_details();
  file.tracker.set_dirty(true);
  file.tracker.set_active(false);
  file.should_be_absent = false;
  file.tracker_only = true;

  disabled_app_root.tracker.set_dirty(false);
  disabled_app_root.tracker.set_needs_folder_listing(false);
  VerifyTrackedFiles(tracked_files, arraysize(tracked_files));
  VerifyReloadConsistency();
}

}  // namespace drive_backend
}  // namespace sync_file_system
