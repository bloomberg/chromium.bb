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

const int64 kInitialChangeID = 1234;

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
    std::string value;
    ASSERT_TRUE(service_metadata.SerializeToString(&value));
    db->Put(leveldb::WriteOptions(), "SERVICE", value);
  }

  void PushToChangeList(scoped_ptr<google_apis::ChangeResource> change,
                        ScopedVector<google_apis::ChangeResource>* changes) {
    changes->push_back(change.release());
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

}  // namespace drive_backend
}  // namespace sync_file_system
