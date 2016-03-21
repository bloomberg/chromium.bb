// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/util/capture_util.h"

using filesystem::FileError;
using mojo::Capture;

namespace leveldb {
namespace {

class LevelDBApptest : public mojo::test::ApplicationTestBase {
 public:
  LevelDBApptest() {}
  ~LevelDBApptest() override {}

 protected:
  // Overridden from mojo::test::ApplicationTestBase:
  void SetUp() override {
    ApplicationTestBase::SetUp();
    connector()->ConnectToInterface("mojo:filesystem", &files_);
    connector()->ConnectToInterface("mojo:leveldb", &leveldb_);
  }

  // Note: This has an out parameter rather than returning the |DirectoryPtr|,
  // since |ASSERT_...()| doesn't work with return values.
  void GetUserDataDir(filesystem::DirectoryPtr* directory) {
    FileError error = FileError::FAILED;
    files()->OpenPersistentFileSystem(GetProxy(directory),
                                      mojo::Capture(&error));
    ASSERT_TRUE(files().WaitForIncomingResponse());
    ASSERT_EQ(FileError::OK, error);
  }

  filesystem::FileSystemPtr& files() { return files_; }
  LevelDBServicePtr& leveldb() { return leveldb_; }

 private:
  filesystem::FileSystemPtr files_;
  LevelDBServicePtr leveldb_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBApptest);
};

#if defined(OS_LINUX)
// Flaky on Linux: http://crbug.com/594977,
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST_F(LevelDBApptest, MAYBE_Basic) {
  filesystem::DirectoryPtr directory;
  GetUserDataDir(&directory);

  DatabaseError error;
  LevelDBDatabasePtr database;
  leveldb()->Open(std::move(directory), "test", GetProxy(&database),
                  Capture(&error));
  ASSERT_TRUE(leveldb().WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Write a key to the database.
  error = DatabaseError::INVALID_ARGUMENT;
  database->Put(mojo::Array<uint8_t>::From(std::string("key")),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Read the key back from the database.
  error = DatabaseError::INVALID_ARGUMENT;
  mojo::Array<uint8_t> value;
  database->Get(mojo::Array<uint8_t>::From(std::string("key")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ("value", value.To<std::string>());

  // Delete the key from the database.
  error = DatabaseError::INVALID_ARGUMENT;
  database->Delete(mojo::Array<uint8_t>::From(std::string("key")),
                   Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Read the key back from the database.
  error = DatabaseError::INVALID_ARGUMENT;
  value.SetToEmpty();
  database->Get(mojo::Array<uint8_t>::From(std::string("key")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", value.To<std::string>());
}

TEST_F(LevelDBApptest, WriteBatch) {
  filesystem::DirectoryPtr directory;
  GetUserDataDir(&directory);

  DatabaseError error;
  LevelDBDatabasePtr database;
  leveldb()->Open(std::move(directory), "test", GetProxy(&database),
                  Capture(&error));
  ASSERT_TRUE(leveldb().WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Write a key to the database.
  database->Put(mojo::Array<uint8_t>::From(std::string("key")),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Create a batched operation which both deletes "key" and adds another write.
  mojo::Array<BatchedOperationPtr> operations;
  BatchedOperationPtr item = BatchedOperation::New();
  item->type = BatchOperationType::DELETE_KEY;
  item->key = mojo::Array<uint8_t>::From(std::string("key"));
  operations.push_back(std::move(item));

  item = BatchedOperation::New();
  item->type = BatchOperationType::PUT_KEY;
  item->key = mojo::Array<uint8_t>::From(std::string("other"));
  item->value = mojo::Array<uint8_t>::From(std::string("more"));
  operations.push_back(std::move(item));

  database->Write(std::move(operations), Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Reading "key" should be invalid now.
  error = DatabaseError::INVALID_ARGUMENT;
  mojo::Array<uint8_t> value;
  database->Get(mojo::Array<uint8_t>::From(std::string("key")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", value.To<std::string>());

  // Reading "other" should return "more"
  error = DatabaseError::INVALID_ARGUMENT;
  database->Get(mojo::Array<uint8_t>::From(std::string("other")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ("more", value.To<std::string>());
}

TEST_F(LevelDBApptest, Reconnect) {
  DatabaseError error;

  {
    filesystem::DirectoryPtr directory;
    GetUserDataDir(&directory);

    LevelDBDatabasePtr database;
    leveldb()->Open(std::move(directory), "test", GetProxy(&database),
                    Capture(&error));
    ASSERT_TRUE(leveldb().WaitForIncomingResponse());
    EXPECT_EQ(DatabaseError::OK, error);

    // Write a key to the database.
    error = DatabaseError::INVALID_ARGUMENT;
    database->Put(mojo::Array<uint8_t>::From(std::string("key")),
                  mojo::Array<uint8_t>::From(std::string("value")),
                  Capture(&error));
    ASSERT_TRUE(database.WaitForIncomingResponse());
    EXPECT_EQ(DatabaseError::OK, error);

    // The database should go out of scope here.
  }

  {
    filesystem::DirectoryPtr directory;
    GetUserDataDir(&directory);

    // Reconnect to the database.
    LevelDBDatabasePtr database;
    leveldb()->Open(std::move(directory), "test", GetProxy(&database),
                    Capture(&error));
    ASSERT_TRUE(leveldb().WaitForIncomingResponse());
    EXPECT_EQ(DatabaseError::OK, error);

    // We should still be able to read the key back from the database.
    error = DatabaseError::INVALID_ARGUMENT;
    mojo::Array<uint8_t> value;
    database->Get(mojo::Array<uint8_t>::From(std::string("key")),
                  Capture(&error, &value));
    ASSERT_TRUE(database.WaitForIncomingResponse());
    EXPECT_EQ(DatabaseError::OK, error);
    EXPECT_EQ("value", value.To<std::string>());
  }
}

TEST_F(LevelDBApptest, GetSnapshotSimple) {
  DatabaseError error;

  filesystem::DirectoryPtr directory;
  GetUserDataDir(&directory);

  LevelDBDatabasePtr database;
  leveldb()->Open(std::move(directory), "test", GetProxy(&database),
                  Capture(&error));
  ASSERT_TRUE(leveldb().WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  uint64_t snapshot_id = 0;
  database->GetSnapshot(Capture(&snapshot_id));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_NE(static_cast<uint64_t>(0), snapshot_id);
}

TEST_F(LevelDBApptest, GetFromSnapshots) {
  DatabaseError error;

  filesystem::DirectoryPtr directory;
  GetUserDataDir(&directory);

  LevelDBDatabasePtr database;
  leveldb()->Open(std::move(directory), "test", GetProxy(&database),
                  Capture(&error));
  ASSERT_TRUE(leveldb().WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Write a key to the database.
  error = DatabaseError::INVALID_ARGUMENT;
  database->Put(mojo::Array<uint8_t>::From(std::string("key")),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Take a snapshot where key=value.
  uint64_t key_value_snapshot = 0;
  database->GetSnapshot(Capture(&key_value_snapshot));
  ASSERT_TRUE(database.WaitForIncomingResponse());

  // Change key to "yek".
  error = DatabaseError::INVALID_ARGUMENT;
  database->Put(mojo::Array<uint8_t>::From(std::string("key")),
                mojo::Array<uint8_t>::From(std::string("yek")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // (Ensure this change is live on the database.)
  error = DatabaseError::INVALID_ARGUMENT;
  mojo::Array<uint8_t> value;
  database->Get(mojo::Array<uint8_t>::From(std::string("key")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ("yek", value.To<std::string>());

  // But if we were to read from the snapshot, we'd still get value.
  error = DatabaseError::INVALID_ARGUMENT;
  value.SetToEmpty();
  database->GetFromSnapshot(
      key_value_snapshot,
      mojo::Array<uint8_t>::From(std::string("key")),
      Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ("value", value.To<std::string>());
}

#if defined(OS_LINUX)
// Flaky on Linux: http://crbug.com/594977,
#define MAYBE_InvalidArgumentOnInvalidSnapshot DISABLED_InvalidArgumentOnInvalidSnapshot
#else
#define MAYBE_InvalidArgumentOnInvalidSnapshot InvalidArgumentOnInvalidSnapshot
#endif
TEST_F(LevelDBApptest, InvalidArgumentOnInvalidSnapshot) {
  filesystem::DirectoryPtr directory;
  GetUserDataDir(&directory);

  LevelDBDatabasePtr database;
  DatabaseError error = DatabaseError::INVALID_ARGUMENT;
  leveldb()->Open(std::move(directory), "test", GetProxy(&database),
                  Capture(&error));
  ASSERT_TRUE(leveldb().WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  uint64_t invalid_snapshot = 8;

  error = DatabaseError::OK;
  mojo::Array<uint8_t> value;
  database->GetFromSnapshot(
      invalid_snapshot,
      mojo::Array<uint8_t>::From(std::string("key")),
      Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::INVALID_ARGUMENT, error);
}

}  // namespace
}  // namespace leveldb
