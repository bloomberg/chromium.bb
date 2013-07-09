// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

const int64 kInitialChangeID = 1234;
const char kSyncRootFolderID[] = "sync_root_folder_id";

template <typename Value>
bool AreEquivalentProtobufs(const Value& left, const Value& right) {
  std::string serialized_left;
  std::string serialized_right;
  left.SerializeToString(&serialized_left);
  right.SerializeToString(&serialized_right);
  return serialized_left == serialized_right;
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
  MetadataDatabaseTest() : next_file_id_number_(1) {}

  virtual ~MetadataDatabaseTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(database_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE { DropDatabase(); }

 protected:
  std::string GenerateFileID() {
    return "file_id_" + base::Int64ToString(next_file_id_number_++);
  }

  SyncStatusCode InitializeDatabase() {
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

  scoped_ptr<leveldb::DB> OpenDB() {
    leveldb::DB* db = NULL;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status =
        leveldb::DB::Open(options, database_dir_.path().AsUTF8Unsafe(), &db);
    EXPECT_TRUE(status.ok());
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

  DriveFileMetadata CreateSyncRoot() {
    DriveFileMetadata metadata;
    metadata.set_file_id(kSyncRootFolderID);
    metadata.set_parent_folder_id(std::string());
    metadata.mutable_synced_details()->set_title("Chrome Syncable FileSystem");
    metadata.mutable_synced_details()->set_kind(KIND_FOLDER);
    metadata.set_active(true);
    metadata.set_dirty(false);
    return metadata;
  }

  DriveFileMetadata CreateUnknownFile(const std::string& app_id,
                                      const std::string& parent_folder_id) {
    DriveFileMetadata metadata;
    metadata.set_file_id(GenerateFileID());
    metadata.set_parent_folder_id(parent_folder_id);
    metadata.set_app_id(app_id);
    metadata.set_is_app_root(parent_folder_id == kSyncRootFolderID);
    return metadata;
  }

  DriveFileMetadata CreateFile(const std::string& app_id,
                               const std::string& parent_folder_id,
                               const std::string& title) {
    DriveFileMetadata file(CreateUnknownFile(app_id, parent_folder_id));
    file.mutable_synced_details()->add_parent_folder_id(parent_folder_id);
    file.mutable_synced_details()->set_title(title);
    file.mutable_synced_details()->set_kind(KIND_FILE);
    file.set_active(true);
    file.set_dirty(false);
    return file;
  }

  DriveFileMetadata CreateFolder(const std::string& app_id,
                                 const std::string& parent_folder_id,
                                 const std::string& title) {
    DriveFileMetadata folder(CreateUnknownFile(app_id, parent_folder_id));
    folder.mutable_synced_details()->add_parent_folder_id(parent_folder_id);
    folder.mutable_synced_details()->set_title(title);
    folder.mutable_synced_details()->set_kind(KIND_FOLDER);
    folder.set_active(true);
    folder.set_dirty(false);
    return folder;
  }

  leveldb::Status PutFileToDB(leveldb::DB* db, const DriveFileMetadata& file) {
    std::string key = "FILE: " + file.file_id();
    std::string value;
    file.SerializeToString(&value);
    return db->Put(leveldb::WriteOptions(), key, value);
  }

  void VerifyMetadataExists(const DriveFileMetadata& file) {
    DriveFileMetadata file_in_metadata_db;
    ASSERT_TRUE(metadata_database()->FindFileByFileID(
        file.file_id(), &file_in_metadata_db));
    EXPECT_TRUE(AreEquivalentProtobufs(file, file_in_metadata_db));
  }

 private:
  base::ScopedTempDir database_dir_;
  base::MessageLoop message_loop_;

  scoped_ptr<MetadataDatabase> metadata_database_;

  int64 next_file_id_number_;

  DISALLOW_COPY_AND_ASSIGN(MetadataDatabaseTest);
};

TEST_F(MetadataDatabaseTest, InitializationTest_Empty) {
  EXPECT_EQ(SYNC_STATUS_OK, InitializeDatabase());
  DropDatabase();
  EXPECT_EQ(SYNC_STATUS_OK, InitializeDatabase());
}

TEST_F(MetadataDatabaseTest, InitializationTest_SimpleTree) {
  std::string app_id = "app_id";
  DriveFileMetadata sync_root(CreateSyncRoot());
  DriveFileMetadata app_root(CreateFolder(app_id, kSyncRootFolderID, app_id));
  DriveFileMetadata file(CreateFile(app_id, app_root.file_id(), "file"));
  DriveFileMetadata folder(CreateFolder(app_id, app_root.file_id(), "folder"));
  DriveFileMetadata file_in_folder(
      CreateFile(app_id, folder.file_id(), "file_in_folder"));
  DriveFileMetadata orphaned(CreateUnknownFile(std::string(), "root"));

  {
    scoped_ptr<leveldb::DB> db = OpenDB();
    ASSERT_TRUE(db);
    db->Put(leveldb::WriteOptions(), "VERSION", base::Int64ToString(3));
    SetUpServiceMetadata(db.get());

    EXPECT_TRUE(PutFileToDB(db.get(), sync_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), app_root).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), folder).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), file_in_folder).ok());
    EXPECT_TRUE(PutFileToDB(db.get(), orphaned).ok());
  }

  EXPECT_EQ(SYNC_STATUS_OK, InitializeDatabase());

  VerifyMetadataExists(sync_root);
  VerifyMetadataExists(app_root);
  VerifyMetadataExists(file);
  VerifyMetadataExists(folder);
  VerifyMetadataExists(file_in_folder);
  EXPECT_FALSE(metadata_database()->FindFileByFileID(orphaned.file_id(), NULL));
}

}  // namespace drive_backend
}  // namespace sync_file_system
