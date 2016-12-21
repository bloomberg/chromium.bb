// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/debug/stack_trace.h"

#include "components/leveldb/public/cpp/util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb::StdStringToUint8Vector;
using leveldb::Uint8VectorToStdString;

namespace content {

namespace {

const char* kTestPrefix = "abc";
const char* kTestSource = "source";
const size_t kTestSizeLimit = 512;

leveldb::mojom::KeyValuePtr CreateKeyValue(std::vector<uint8_t> key,
                                           std::vector<uint8_t> value) {
  leveldb::mojom::KeyValuePtr result = leveldb::mojom::KeyValue::New();
  result->key = std::move(key);
  result->value = std::move(value);
  return result;
}

base::StringPiece AsStringPiece(const std::vector<uint8_t>& data) {
  return base::StringPiece(reinterpret_cast<const char*>(data.data()),
                           data.size());
}

bool StartsWith(const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& prefix) {
  return base::StartsWith(AsStringPiece(key), AsStringPiece(prefix),
                          base::CompareCase::SENSITIVE);
}

class MockLevelDBDatabase : public leveldb::mojom::LevelDBDatabase {
 public:
  explicit MockLevelDBDatabase(
      std::map<std::vector<uint8_t>, std::vector<uint8_t>>* mock_data)
      : mock_data_(*mock_data) {}

  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           const PutCallback& callback) override {
    FAIL();
  }

  void Delete(const std::vector<uint8_t>& key,
              const DeleteCallback& callback) override {
    FAIL();
  }

  void DeletePrefixed(const std::vector<uint8_t>& key_prefix,
                      const DeletePrefixedCallback& callback) override {
    FAIL();
  }

  void Write(std::vector<leveldb::mojom::BatchedOperationPtr> operations,
             const WriteCallback& callback) override {
    for (const auto& op : operations) {
      switch (op->type) {
        case leveldb::mojom::BatchOperationType::PUT_KEY:
          mock_data_[op->key] = *op->value;
          break;
        case leveldb::mojom::BatchOperationType::DELETE_KEY:
          mock_data_.erase(op->key);
          break;
        case leveldb::mojom::BatchOperationType::DELETE_PREFIXED_KEY:
          FAIL();
          break;
      }
    }
    callback.Run(leveldb::mojom::DatabaseError::OK);
  }

  void Get(const std::vector<uint8_t>& key,
           const GetCallback& callback) override {
    FAIL();
  }

  void GetPrefixed(const std::vector<uint8_t>& key_prefix,
                   const GetPrefixedCallback& callback) override {
    std::vector<leveldb::mojom::KeyValuePtr> data;
    for (const auto& row : mock_data_) {
      if (StartsWith(row.first, key_prefix)) {
        data.push_back(CreateKeyValue(row.first, row.second));
      }
    }
    callback.Run(leveldb::mojom::DatabaseError::OK, std::move(data));
  }

  void GetSnapshot(const GetSnapshotCallback& callback) override { FAIL(); }

  void ReleaseSnapshot(const base::UnguessableToken& snapshot) override {
    FAIL();
  }

  void GetFromSnapshot(const base::UnguessableToken& snapshot,
                       const std::vector<uint8_t>& key,
                       const GetCallback& callback) override {
    FAIL();
  }

  void NewIterator(const NewIteratorCallback& callback) override { FAIL(); }

  void NewIteratorFromSnapshot(
      const base::UnguessableToken& snapshot,
      const NewIteratorFromSnapshotCallback& callback) override {
    FAIL();
  }

  void ReleaseIterator(const base::UnguessableToken& iterator) override {
    FAIL();
  }

  void IteratorSeekToFirst(
      const base::UnguessableToken& iterator,
      const IteratorSeekToFirstCallback& callback) override {
    FAIL();
  }

  void IteratorSeekToLast(const base::UnguessableToken& iterator,
                          const IteratorSeekToLastCallback& callback) override {
    FAIL();
  }

  void IteratorSeek(const base::UnguessableToken& iterator,
                    const std::vector<uint8_t>& target,
                    const IteratorSeekToLastCallback& callback) override {
    FAIL();
  }

  void IteratorNext(const base::UnguessableToken& iterator,
                    const IteratorNextCallback& callback) override {
    FAIL();
  }

  void IteratorPrev(const base::UnguessableToken& iterator,
                    const IteratorPrevCallback& callback) override {
    FAIL();
  }

 private:
  std::map<std::vector<uint8_t>, std::vector<uint8_t>>& mock_data_;
};

void NoOp() {}

void GetCallback(bool* called,
                 bool* success_out,
                 std::vector<uint8_t>* value_out,
                 bool success,
                 const std::vector<uint8_t>& value) {
  *called = true;
  *success_out = success;
  *value_out = value;
}

void GetAllCallback(bool* called,
                    leveldb::mojom::DatabaseError* status_out,
                    std::vector<mojom::KeyValuePtr>* data_out,
                    leveldb::mojom::DatabaseError status,
                    std::vector<mojom::KeyValuePtr> data) {
  *called = true;
  *status_out = status;
  *data_out = std::move(data);
}

void SuccessCallback(bool* called, bool* success_out, bool success) {
  *called = true;
  *success_out = success;
}

void NoOpSuccess(bool success) {}

}  // namespace

class LevelDBWrapperImplTest : public testing::Test {
 public:
  LevelDBWrapperImplTest()
      : db_(&mock_data_),
        level_db_wrapper_(&db_,
                          kTestPrefix,
                          kTestSizeLimit,
                          base::TimeDelta::FromSeconds(5),
                          10 * 1024 * 1024 /* max_bytes_per_hour */,
                          60 /* max_commits_per_hour */,
                          base::Bind(&NoOp)) {
    set_mock_data(std::string(kTestPrefix) + "def", "defdata");
    set_mock_data(std::string(kTestPrefix) + "123", "123data");
    set_mock_data("123", "baddata");
  }

  void set_mock_data(const std::string& key, const std::string& value) {
    mock_data_[StdStringToUint8Vector(key)] = StdStringToUint8Vector(value);
  }

  bool has_mock_data(const std::string& key) {
    return mock_data_.find(StdStringToUint8Vector(key)) != mock_data_.end();
  }

  std::string get_mock_data(const std::string& key) {
    return has_mock_data(key)
               ? Uint8VectorToStdString(mock_data_[StdStringToUint8Vector(key)])
               : "";
  }

  mojom::LevelDBWrapper* wrapper() { return &level_db_wrapper_; }

  void CommitChanges() {
    ASSERT_TRUE(level_db_wrapper_.commit_batch_);
    level_db_wrapper_.CommitChanges();
  }

 private:
  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  MockLevelDBDatabase db_;
  LevelDBWrapperImpl level_db_wrapper_;
};

TEST_F(LevelDBWrapperImplTest, GetLoadedFromMap) {
  bool called = false;
  bool success;
  std::vector<uint8_t> result;
  wrapper()->Get(StdStringToUint8Vector("123"),
                 base::Bind(&GetCallback, &called, &success, &result));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);
  EXPECT_EQ(StdStringToUint8Vector("123data"), result);

  called = false;
  wrapper()->Get(StdStringToUint8Vector("x"),
                 base::Bind(&GetCallback, &called, &success, &result));
  ASSERT_TRUE(called);
  EXPECT_FALSE(success);
}

TEST_F(LevelDBWrapperImplTest, GetFromPutOverwrite) {
  std::vector<uint8_t> key = StdStringToUint8Vector("123");
  std::vector<uint8_t> value = StdStringToUint8Vector("foo");

  bool called = false;
  bool success;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  called = false;
  std::vector<uint8_t> result;
  wrapper()->Get(key, base::Bind(&GetCallback, &called, &success, &result));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);
  EXPECT_EQ(value, result);
}

TEST_F(LevelDBWrapperImplTest, GetFromPutNewKey) {
  std::vector<uint8_t> key = StdStringToUint8Vector("newkey");
  std::vector<uint8_t> value = StdStringToUint8Vector("foo");

  bool called = false;
  bool success;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  called = false;
  std::vector<uint8_t> result;
  wrapper()->Get(key, base::Bind(&GetCallback, &called, &success, &result));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);
  EXPECT_EQ(value, result);
}

TEST_F(LevelDBWrapperImplTest, GetAll) {
  bool called = false;
  leveldb::mojom::DatabaseError status;
  std::vector<mojom::KeyValuePtr> data;
  wrapper()->GetAll(kTestSource,
                    base::Bind(&GetAllCallback, &called, &status, &data));
  ASSERT_TRUE(called);
  EXPECT_EQ(leveldb::mojom::DatabaseError::OK, status);
  EXPECT_EQ(2u, data.size());
}

TEST_F(LevelDBWrapperImplTest, CommitPutToDB) {
  std::string key1 = "123";
  std::string value1 = "foo";
  std::string key2 = "abc";
  std::string value2 = "data abc";

  wrapper()->Put(StdStringToUint8Vector(key1), StdStringToUint8Vector(value1),
                 kTestSource, base::Bind(&NoOpSuccess));
  wrapper()->Put(StdStringToUint8Vector(key2),
                 StdStringToUint8Vector("old value"), kTestSource,
                 base::Bind(&NoOpSuccess));
  wrapper()->Put(StdStringToUint8Vector(key2), StdStringToUint8Vector(value2),
                 kTestSource, base::Bind(&NoOpSuccess));

  CommitChanges();

  EXPECT_TRUE(has_mock_data(kTestPrefix + key1));
  EXPECT_EQ(value1, get_mock_data(kTestPrefix + key1));
  EXPECT_TRUE(has_mock_data(kTestPrefix + key2));
  EXPECT_EQ(value2, get_mock_data(kTestPrefix + key2));
}

TEST_F(LevelDBWrapperImplTest, PutOverQuotaLargeValue) {
  std::vector<uint8_t> key = StdStringToUint8Vector("newkey");
  std::vector<uint8_t> value(kTestSizeLimit, 4);

  bool called = false;
  bool success;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_FALSE(success);

  value.resize(kTestSizeLimit / 2);
  called = false;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);
}

TEST_F(LevelDBWrapperImplTest, PutOverQuotaLargeKey) {
  std::vector<uint8_t> key(kTestSizeLimit, 'a');
  std::vector<uint8_t> value = StdStringToUint8Vector("newvalue");

  bool called = false;
  bool success;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_FALSE(success);

  key.resize(kTestSizeLimit / 2);
  called = false;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);
}

TEST_F(LevelDBWrapperImplTest, PutWhenAlreadyOverQuota) {
  std::string key = "largedata";
  std::vector<uint8_t> value(kTestSizeLimit, 4);

  set_mock_data(kTestPrefix + key, Uint8VectorToStdString(value));

  // Put with same data should succeed.
  bool called = false;
  bool success;
  wrapper()->Put(StdStringToUint8Vector(key), value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  // Put with same data size should succeed.
  called = false;
  value[1] = 13;
  wrapper()->Put(StdStringToUint8Vector(key), value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  // Adding a new key when already over quota should not succeed.
  called = false;
  wrapper()->Put(StdStringToUint8Vector("newkey"), {1, 2, 3}, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_FALSE(success);

  // Reducing size should also succeed.
  value.resize(kTestSizeLimit / 2);
  called = false;
  wrapper()->Put(StdStringToUint8Vector(key), value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  // Increasing size again should succeed, as still under the limit.
  value.resize(value.size() + 1);
  called = false;
  wrapper()->Put(StdStringToUint8Vector(key), value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  // But increasing back to original size should fail.
  value.resize(kTestSizeLimit);
  called = false;
  wrapper()->Put(StdStringToUint8Vector(key), value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_FALSE(success);
}

TEST_F(LevelDBWrapperImplTest, PutWhenAlreadyOverQuotaBecauseOfLargeKey) {
  std::vector<uint8_t> key(kTestSizeLimit, 'x');
  std::vector<uint8_t> value = StdStringToUint8Vector("value");

  set_mock_data(kTestPrefix + Uint8VectorToStdString(key),
                Uint8VectorToStdString(value));

  // Put with same data size should succeed.
  bool called = false;
  bool success;
  value[0] = 'X';
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  // Reducing size should also succeed.
  value.clear();
  called = false;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_TRUE(success);

  // Increasing size should fail.
  value.resize(1, 'a');
  called = false;
  wrapper()->Put(key, value, kTestSource,
                 base::Bind(&SuccessCallback, &called, &success));
  ASSERT_TRUE(called);
  EXPECT_FALSE(success);
}

}  // namespace content
