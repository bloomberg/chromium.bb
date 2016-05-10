// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "components/filesystem/public/interfaces/directory.mojom.h"
#include "components/filesystem/public/interfaces/file_system.mojom.h"
#include "components/filesystem/public/interfaces/types.mojom.h"
#include "components/leveldb/public/interfaces/leveldb.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/util/capture_util.h"
#include "services/shell/public/cpp/shell_connection.h"
#include "services/shell/public/cpp/shell_test.h"

using filesystem::mojom::FileError;
using mojo::Capture;

namespace leveldb {
namespace {

class LevelDBServiceTest : public shell::test::ShellTest {
 public:
  LevelDBServiceTest() : ShellTest("exe:leveldb_service_unittests") {}
  ~LevelDBServiceTest() override {}

 protected:
  // Overridden from mojo::test::ApplicationTestBase:
  void SetUp() override {
    ShellTest::SetUp();
    connector()->ConnectToInterface("mojo:filesystem", &files_);
    connector()->ConnectToInterface("mojo:leveldb", &leveldb_);
  }

  void TearDown() override {
    leveldb_.reset();
    files_.reset();
    ShellTest::TearDown();
  }

  // Note: This has an out parameter rather than returning the |DirectoryPtr|,
  // since |ASSERT_...()| doesn't work with return values.
  void GetTempDirectory(filesystem::mojom::DirectoryPtr* directory) {
    FileError error = FileError::FAILED;
    files()->OpenTempDirectory(GetProxy(directory), mojo::Capture(&error));
    ASSERT_TRUE(files().WaitForIncomingResponse());
    ASSERT_EQ(FileError::OK, error);
  }

  filesystem::mojom::FileSystemPtr& files() { return files_; }
  LevelDBServicePtr& leveldb() { return leveldb_; }

 private:
  filesystem::mojom::FileSystemPtr files_;
  LevelDBServicePtr leveldb_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBServiceTest);
};

TEST_F(LevelDBServiceTest, Basic) {
  DatabaseError error;
  LevelDBDatabasePtr database;
  leveldb()->OpenInMemory(GetProxy(&database), Capture(&error));
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

TEST_F(LevelDBServiceTest, WriteBatch) {
  DatabaseError error;
  LevelDBDatabasePtr database;
  leveldb()->OpenInMemory(GetProxy(&database), Capture(&error));
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

  // Write a some prefixed keys to the database.
  database->Put(mojo::Array<uint8_t>::From(std::string("prefix-key1")),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  database->Put(mojo::Array<uint8_t>::From(std::string("prefix-key2")),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Create a batched operation to delete them.
  operations.SetToEmpty();
  item = BatchedOperation::New();
  item->type = BatchOperationType::DELETE_PREFIXED_KEY;
  item->key = mojo::Array<uint8_t>::From(std::string("prefix"));
  operations.push_back(std::move(item));
  database->Write(std::move(operations), Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  // Reading all "prefix" keys should be invalid now.
  error = DatabaseError::INVALID_ARGUMENT;
  value = nullptr;
  database->Get(mojo::Array<uint8_t>::From(std::string("prefix-key1")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", value.To<std::string>());
  // Reading "key" should be invalid now.
  error = DatabaseError::INVALID_ARGUMENT;
  value = nullptr;
  database->Get(mojo::Array<uint8_t>::From(std::string("prefix-key2")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", value.To<std::string>());
}

TEST_F(LevelDBServiceTest, Reconnect) {
  DatabaseError error;

  filesystem::mojom::DirectoryPtr temp_directory;
  GetTempDirectory(&temp_directory);

  {
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(GetProxy(&directory));

    LevelDBDatabasePtr database;
    leveldb::OpenOptionsPtr options = leveldb::OpenOptions::New();
    options->error_if_exists = true;
    options->create_if_missing = true;
    leveldb()->OpenWithOptions(std::move(options),
                               std::move(directory), "test",
                               GetProxy(&database),
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
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(GetProxy(&directory));

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

TEST_F(LevelDBServiceTest, GetSnapshotSimple) {
  DatabaseError error;
  LevelDBDatabasePtr database;
  leveldb()->OpenInMemory(GetProxy(&database), Capture(&error));
  ASSERT_TRUE(leveldb().WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  uint64_t snapshot_id = 0;
  database->GetSnapshot(Capture(&snapshot_id));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_NE(static_cast<uint64_t>(0), snapshot_id);
}

TEST_F(LevelDBServiceTest, GetFromSnapshots) {
  DatabaseError error;
  LevelDBDatabasePtr database;
  leveldb()->OpenInMemory(GetProxy(&database), Capture(&error));
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

TEST_F(LevelDBServiceTest, InvalidArgumentOnInvalidSnapshot) {
  LevelDBDatabasePtr database;
  DatabaseError error = DatabaseError::INVALID_ARGUMENT;
  leveldb()->OpenInMemory(GetProxy(&database), Capture(&error));
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

TEST_F(LevelDBServiceTest, MemoryDBReadWrite) {
  LevelDBDatabasePtr database;
  DatabaseError error = DatabaseError::INVALID_ARGUMENT;
  leveldb()->OpenInMemory(GetProxy(&database), Capture(&error));
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

TEST_F(LevelDBServiceTest, Prefixed) {
  // Open an in memory database for speed.
  DatabaseError error = DatabaseError::INVALID_ARGUMENT;
  LevelDBDatabasePtr database;
  leveldb()->OpenInMemory(GetProxy(&database), Capture(&error));
  ASSERT_TRUE(leveldb().WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);

  const std::string prefix("prefix");
  mojo::Array<KeyValuePtr> key_values;

  // Completely empty database.
  error = DatabaseError::INVALID_ARGUMENT;
  database->GetPrefixed(mojo::Array<uint8_t>::From(prefix),
                        Capture(&error, &key_values));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());

  // No values with our prefix, but values before and after.
  error = DatabaseError::INVALID_ARGUMENT;
  database->Put(mojo::Array<uint8_t>::From(std::string("a-before-prefix")),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  error = DatabaseError::INVALID_ARGUMENT;
  database->Put(mojo::Array<uint8_t>::From(std::string("z-after-prefix")),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  key_values.SetToEmpty();
  error = DatabaseError::INVALID_ARGUMENT;
  database->GetPrefixed(mojo::Array<uint8_t>::From(prefix),
                        Capture(&error, &key_values));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());

  // One value with the exact prefix.
  database->Put(mojo::Array<uint8_t>::From(prefix),
                mojo::Array<uint8_t>::From(std::string("value")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  error = DatabaseError::INVALID_ARGUMENT;
  key_values.SetToEmpty();
  database->GetPrefixed(mojo::Array<uint8_t>::From(prefix),
                        Capture(&error, &key_values));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ(1u, key_values.size());
  EXPECT_EQ("prefix", key_values[0]->key.To<std::string>());
  EXPECT_EQ("value", key_values[0]->value.To<std::string>());

  // Multiple values with starting with the prefix.
  database->Put(mojo::Array<uint8_t>::From(prefix + "2"),
                mojo::Array<uint8_t>::From(std::string("value2")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  error = DatabaseError::INVALID_ARGUMENT;
  key_values.SetToEmpty();
  database->GetPrefixed(mojo::Array<uint8_t>::From(prefix),
                        Capture(&error, &key_values));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ(2u, key_values.size());
  EXPECT_EQ("prefix", key_values[0]->key.To<std::string>());
  EXPECT_EQ("value", key_values[0]->value.To<std::string>());
  EXPECT_EQ("prefix2", key_values[1]->key.To<std::string>());
  EXPECT_EQ("value2", key_values[1]->value.To<std::string>());

  // Delete the prefixed values.
  error = DatabaseError::INVALID_ARGUMENT;
  database->DeletePrefixed(mojo::Array<uint8_t>::From(prefix),
                          Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  error = DatabaseError::INVALID_ARGUMENT;
  key_values.SetToEmpty();
  database->GetPrefixed(mojo::Array<uint8_t>::From(prefix),
                        Capture(&error, &key_values));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());

  // Make sure the others are not deleted.
  mojo::Array<uint8_t> value;
  database->Get(mojo::Array<uint8_t>::From(std::string("a-before-prefix")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ("value", value.To<std::string>());
  value.SetToEmpty();
  database->Get(mojo::Array<uint8_t>::From(std::string("z-after-prefix")),
                Capture(&error, &value));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ("value", value.To<std::string>());

  // A key having our prefix, but no key matching it exactly.
  // Even thought there is no exact matching key, GetPrefixed
  // and DeletePrefixed still operate on the values.
  error = DatabaseError::INVALID_ARGUMENT;
  database->Put(mojo::Array<uint8_t>::From(prefix + "2"),
                mojo::Array<uint8_t>::From(std::string("value2")),
                Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  error = DatabaseError::INVALID_ARGUMENT;
  key_values.SetToEmpty();
  database->GetPrefixed(mojo::Array<uint8_t>::From(prefix),
                        Capture(&error, &key_values));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_EQ(1u, key_values.size());
  EXPECT_EQ("prefix2", key_values[0]->key.To<std::string>());
  EXPECT_EQ("value2", key_values[0]->value.To<std::string>());
  error = DatabaseError::INVALID_ARGUMENT;
  database->DeletePrefixed(mojo::Array<uint8_t>::From(prefix),
                          Capture(&error));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  error = DatabaseError::INVALID_ARGUMENT;
  key_values.SetToEmpty();
  database->GetPrefixed(mojo::Array<uint8_t>::From(prefix),
                        Capture(&error, &key_values));
  ASSERT_TRUE(database.WaitForIncomingResponse());
  EXPECT_EQ(DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());
}

}  // namespace
}  // namespace leveldb
