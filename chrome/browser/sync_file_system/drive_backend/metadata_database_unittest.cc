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

typedef MetadataDatabase::FileSet FileSet;
typedef MetadataDatabase::FileByFileID FileByFileID;
typedef MetadataDatabase::FilesByParent FilesByParent;
typedef MetadataDatabase::FileByParentAndTitle FileByParentAndTitle;
typedef MetadataDatabase::FileByAppID FileByAppID;

namespace {

const int64 kInitialChangeID = 1234;
const char kSyncRootFolderID[] = "sync_root_folder_id";

void ExpectEquivalent(const DriveFileMetadata::Details* left,
                      const DriveFileMetadata::Details* right) {
  EXPECT_EQ(left->title(), right->title());
  EXPECT_EQ(left->kind(), right->kind());
  EXPECT_EQ(left->md5(), right->md5());
  EXPECT_EQ(left->etag(), right->etag());
  EXPECT_EQ(left->creation_time(), right->creation_time());
  EXPECT_EQ(left->modification_time(), right->modification_time());
  EXPECT_EQ(left->deleted(), right->deleted());
  EXPECT_EQ(left->change_id(), right->change_id());
}

void ExpectEquivalent(const DriveFileMetadata* left,
                      const DriveFileMetadata* right) {
  EXPECT_EQ(left->file_id(), right->file_id());
  EXPECT_EQ(left->parent_folder_id(), right->parent_folder_id());
  EXPECT_EQ(left->app_id(), right->app_id());
  EXPECT_EQ(left->is_app_root(), right->is_app_root());

  {
    SCOPED_TRACE("Expect equivalent synced_details.");
    ExpectEquivalent(&left->synced_details(), &right->synced_details());
  }

  {
    SCOPED_TRACE("Expect equivalent remote_details.");
    ExpectEquivalent(&left->remote_details(), &right->remote_details());
  }

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
    service_metadata.set_sync_root_folder_id(kSyncRootFolderID);
    std::string value;
    ASSERT_TRUE(service_metadata.SerializeToString(&value));
    db->Put(leveldb::WriteOptions(), "SERVICE", value);
  }

  DriveFileMetadata CreateSyncRootMetadata() {
    DriveFileMetadata sync_root;
    sync_root.set_file_id(kSyncRootFolderID);
    sync_root.set_parent_folder_id(std::string());
    sync_root.mutable_synced_details()->set_title("Chrome Syncable FileSystem");
    sync_root.mutable_synced_details()->set_kind(KIND_FOLDER);
    sync_root.set_active(true);
    sync_root.set_dirty(false);
    return sync_root;
  }

  DriveFileMetadata CreateAppRootMetadata(const DriveFileMetadata& sync_root,
                                          const std::string& app_id) {
    DriveFileMetadata app_root(
        CreateFolderMetadata(sync_root, app_id /* title */));
    app_root.set_app_id(app_id);
    app_root.set_is_app_root(true);
    return app_root;
  }

  DriveFileMetadata CreateUnknownFileMetadata(const DriveFileMetadata& parent) {
    DriveFileMetadata file;
    file.set_file_id(GenerateFileID());
    file.set_parent_folder_id(parent.file_id());
    file.set_app_id(parent.app_id());
    file.set_is_app_root(false);
    return file;
  }

  DriveFileMetadata CreateFileMetadata(const DriveFileMetadata& parent,
                                       const std::string& title) {
    DriveFileMetadata file(CreateUnknownFileMetadata(parent));
    file.mutable_synced_details()->add_parent_folder_id(parent.file_id());
    file.mutable_synced_details()->set_title(title);
    file.mutable_synced_details()->set_kind(KIND_FILE);
    file.mutable_synced_details()->set_md5(
        "md5_value_" + base::Int64ToString(next_md5_sequence_number_++));
    file.set_active(true);
    file.set_dirty(false);
    return file;
  }

  DriveFileMetadata CreateFolderMetadata(const DriveFileMetadata& parent,
                                         const std::string& title) {
    DriveFileMetadata folder(CreateUnknownFileMetadata(parent));
    folder.mutable_synced_details()->add_parent_folder_id(parent.file_id());
    folder.mutable_synced_details()->set_title(title);
    folder.mutable_synced_details()->set_kind(KIND_FOLDER);
    folder.set_active(true);
    folder.set_dirty(false);
    return folder;
  }

  scoped_ptr<google_apis::ChangeResource> CreateChangeResourceFromMetadata(
      const DriveFileMetadata& file) {
    scoped_ptr<google_apis::ChangeResource> change(
        new google_apis::ChangeResource);
    change->set_change_id(file.remote_details().change_id());
    change->set_file_id(file.file_id());
    change->set_deleted(file.remote_details().deleted());
    if (change->is_deleted())
      return change.Pass();

    scoped_ptr<google_apis::FileResource> file_resource(
        new google_apis::FileResource);
    ScopedVector<google_apis::ParentReference> parents;
    for (int i = 0; i < file.remote_details().parent_folder_id_size(); ++i) {
      scoped_ptr<google_apis::ParentReference> parent(
          new google_apis::ParentReference);
      parent->set_file_id(file.remote_details().parent_folder_id(i));
      parents.push_back(parent.release());
    }

    file_resource->set_file_id(file.file_id());
    file_resource->set_parents(&parents);
    file_resource->set_title(file.remote_details().title());
    if (file.remote_details().kind() == KIND_FOLDER)
      file_resource->set_mime_type("application/vnd.google-apps.folder");
    else if (file.remote_details().kind() == KIND_FILE)
      file_resource->set_mime_type("text/plain");
    else
      file_resource->set_mime_type("application/vnd.google-apps.document");
    file_resource->set_md5_checksum(file.remote_details().md5());
    file_resource->set_etag(file.remote_details().etag());
    file_resource->set_created_date(base::Time::FromInternalValue(
        file.remote_details().creation_time()));
    file_resource->set_modified_date(base::Time::FromInternalValue(
        file.remote_details().modification_time()));

    change->set_file(file_resource.Pass());
    return change.Pass();
  }

  void ApplyRenameChangeToMetadata(const std::string& new_title,
                                   DriveFileMetadata* file) {
    DriveFileMetadata::Details* details = file->mutable_remote_details();
    *details = file->synced_details();
    details->set_title(new_title);
    details->set_change_id(next_change_id_++);
    file->set_dirty(true);
  }

  void ApplyReorganizeChangeToMetadata(const std::string& new_parent,
                                       DriveFileMetadata* file) {
    DriveFileMetadata::Details* details = file->mutable_remote_details();
    *details = file->synced_details();
    details->clear_parent_folder_id();
    details->add_parent_folder_id(new_parent);
    details->set_change_id(next_change_id_++);
    file->set_dirty(true);
  }

  void ApplyContentChangeToMetadata(DriveFileMetadata* file) {
    DriveFileMetadata::Details* details = file->mutable_remote_details();
    *details = file->synced_details();
    details->set_md5(
        "md5_value_" + base::Int64ToString(next_md5_sequence_number_++));
    details->set_change_id(next_change_id_++);
    file->set_dirty(true);
  }

  void ApplyNoopChangeToMetadata(DriveFileMetadata* file) {
    *file->mutable_remote_details() = file->synced_details();
    file->mutable_remote_details()->set_change_id(next_change_id_++);
    file->set_dirty(true);
  }

  void ApplyNewFileChangeToMetadata(DriveFileMetadata* file) {
    *file->mutable_remote_details() = file->synced_details();
    file->clear_synced_details();
    file->mutable_remote_details()->set_change_id(next_change_id_++);
    file->set_active(false);
    file->set_dirty(true);
  }

  void PushToChangeList(scoped_ptr<google_apis::ChangeResource> change,
                        ScopedVector<google_apis::ChangeResource>* changes) {
    changes->push_back(change.release());
  }

  leveldb::Status PutFileToDB(leveldb::DB* db, const DriveFileMetadata& file) {
    std::string key = "FILE: " + file.file_id();
    std::string value;
    file.SerializeToString(&value);
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
      SCOPED_TRACE("Expect equivalent file_by_file_id_ contents.");
      ExpectEquivalent(metadata_database_->file_by_file_id_,
                       metadata_database_2->file_by_file_id_);
    }

    {
      SCOPED_TRACE("Expect equivalent files_by_parent_ contents.");
      ExpectEquivalent(metadata_database_->files_by_parent_,
                       metadata_database_2->files_by_parent_);
    }

    {
      SCOPED_TRACE("Expect equivalent app_root_by_app_id_ contents.");
      ExpectEquivalent(metadata_database_->app_root_by_app_id_,
                       metadata_database_2->app_root_by_app_id_);
    }

    {
      SCOPED_TRACE("Expect equivalent"
                   " active_file_by_parent_and_title_ contents.");
      ExpectEquivalent(metadata_database_->active_file_by_parent_and_title_,
                       metadata_database_2->active_file_by_parent_and_title_);
    }

    {
      SCOPED_TRACE("Expect equivalent dirty_files_ contents.");
      ExpectEquivalent(metadata_database_->dirty_files_,
                       metadata_database_2->dirty_files_);
    }
  }

  void VerifyFile(const DriveFileMetadata& file) {
    DriveFileMetadata file_in_metadata_db;
    ASSERT_TRUE(metadata_database()->FindFileByFileID(
        file.file_id(), &file_in_metadata_db));

    SCOPED_TRACE("Expect equivalent " + file.file_id());
    ExpectEquivalent(&file, &file_in_metadata_db);
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

 private:

  base::ScopedTempDir database_dir_;
  base::MessageLoop message_loop_;

  scoped_ptr<MetadataDatabase> metadata_database_;

  int64 next_change_id_;
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
  std::string app_id = "app_id";
  DriveFileMetadata sync_root(CreateSyncRootMetadata());
  DriveFileMetadata app_root(CreateAppRootMetadata(sync_root, "app_id"));
  DriveFileMetadata file(CreateFileMetadata(app_root, "file"));
  DriveFileMetadata folder(CreateFolderMetadata(app_root, "folder"));
  DriveFileMetadata file_in_folder(
      CreateFileMetadata(folder, "file_in_folder"));
  DriveFileMetadata orphaned(CreateUnknownFileMetadata(sync_root));
  orphaned.set_parent_folder_id(std::string());

  {
    scoped_ptr<leveldb::DB> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file_in_folder).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), orphaned).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  VerifyFile(sync_root);
  VerifyFile(app_root);
  VerifyFile(file);
  VerifyFile(folder);
  VerifyFile(file_in_folder);
  EXPECT_FALSE(metadata_database()->FindFileByFileID(orphaned.file_id(), NULL));
}

TEST_F(MetadataDatabaseTest, AppManagementTest) {
  DriveFileMetadata sync_root(CreateSyncRootMetadata());
  DriveFileMetadata app_root(CreateAppRootMetadata(sync_root, "app_id"));
  DriveFileMetadata folder(CreateFolderMetadata(sync_root, "folder"));
  folder.set_active(false);

  {
    scoped_ptr<leveldb::DB> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());
  VerifyFile(sync_root);
  VerifyFile(app_root);
  VerifyFile(folder);

  folder.set_app_id("foo");
  EXPECT_EQ(SYNC_STATUS_OK, RegisterApp(folder.app_id(), folder.file_id()));

  folder.set_is_app_root(true);
  folder.set_active(true);
  folder.set_dirty(true);
  folder.set_needs_folder_listing(true);
  VerifyFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, DisableApp(folder.app_id()));
  folder.set_active(false);
  VerifyFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, EnableApp(folder.app_id()));
  folder.set_active(true);
  VerifyFile(folder);
  VerifyReloadConsistency();

  EXPECT_EQ(SYNC_STATUS_OK, UnregisterApp(folder.app_id()));
  folder.set_app_id(std::string());
  folder.set_is_app_root(false);
  folder.set_active(false);
  VerifyFile(folder);
  VerifyReloadConsistency();
}

TEST_F(MetadataDatabaseTest, BuildPathTest) {
  DriveFileMetadata sync_root(CreateSyncRootMetadata());
  DriveFileMetadata app_root(
      CreateAppRootMetadata(sync_root, "app_id"));
  DriveFileMetadata folder(CreateFolderMetadata(app_root, "folder"));
  DriveFileMetadata file(CreateFolderMetadata(folder, "file"));
  DriveFileMetadata inactive_folder(CreateFolderMetadata(app_root, "folder"));

  {
    scoped_ptr<leveldb::DB> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  base::FilePath path;
  EXPECT_FALSE(metadata_database()->BuildPathForFile(sync_root.file_id(),
                                                     &path));
  EXPECT_TRUE(metadata_database()->BuildPathForFile(app_root.file_id(), &path));
  EXPECT_EQ(base::FilePath(FPL("/")).NormalizePathSeparators(),
            path);
  EXPECT_TRUE(metadata_database()->BuildPathForFile(file.file_id(), &path));
  EXPECT_EQ(base::FilePath(FPL("/folder/file")).NormalizePathSeparators(),
            path);
}

TEST_F(MetadataDatabaseTest, UpdateByChangeListTest) {
  DriveFileMetadata sync_root(CreateSyncRootMetadata());
  DriveFileMetadata app_root(CreateAppRootMetadata(sync_root, "app_id"));
  DriveFileMetadata disabled_app_root(
      CreateAppRootMetadata(sync_root, "disabled_app"));
  DriveFileMetadata file(CreateFileMetadata(app_root, "file"));
  DriveFileMetadata renamed_file(CreateFileMetadata(app_root, "to be renamed"));
  DriveFileMetadata folder(CreateFolderMetadata(app_root, "folder"));
  DriveFileMetadata reorganized_file(
      CreateFileMetadata(app_root, "to be reorganized"));
  DriveFileMetadata updated_file(CreateFileMetadata(app_root, "to be updated"));
  DriveFileMetadata noop_file(CreateFileMetadata(app_root, "have noop change"));
  DriveFileMetadata new_file(CreateFileMetadata(app_root, "to be added later"));

  {
    scoped_ptr<leveldb::DB> db = InitializeLevelDB();
    ASSERT_TRUE(db);

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), disabled_app_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), renamed_file).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), reorganized_file).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), updated_file).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), noop_file).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeMetadataDatabase());

  ApplyRenameChangeToMetadata("renamed", &renamed_file);
  ApplyReorganizeChangeToMetadata(folder.file_id(), &reorganized_file);
  ApplyContentChangeToMetadata(&updated_file);
  ApplyNoopChangeToMetadata(&noop_file);
  ApplyNewFileChangeToMetadata(&new_file);

  ScopedVector<google_apis::ChangeResource> changes;
  PushToChangeList(CreateChangeResourceFromMetadata(renamed_file), &changes);
  PushToChangeList(CreateChangeResourceFromMetadata(
      reorganized_file), &changes);
  PushToChangeList(CreateChangeResourceFromMetadata(updated_file), &changes);
  PushToChangeList(CreateChangeResourceFromMetadata(noop_file), &changes);
  PushToChangeList(CreateChangeResourceFromMetadata(new_file), &changes);
  EXPECT_EQ(SYNC_STATUS_OK, UpdateByChangeList(changes.Pass()));

  VerifyFile(sync_root);
  VerifyFile(app_root);
  VerifyFile(disabled_app_root);
  VerifyFile(file);
  VerifyFile(renamed_file);
  VerifyFile(folder);
  VerifyFile(reorganized_file);
  VerifyFile(updated_file);
  VerifyFile(noop_file);
  VerifyFile(new_file);
  VerifyReloadConsistency();
}

}  // namespace drive_backend
}  // namespace sync_file_system
