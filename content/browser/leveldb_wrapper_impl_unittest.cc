// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/run_loop.h"
#include "components/leveldb/public/cpp/util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
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

std::vector<uint8_t> successor(std::vector<uint8_t> data) {
  for (unsigned i = data.size(); i > 0; --i) {
    if (data[i - 1] < 255) {
      data[i - 1]++;
      return data;
    }
  }
  NOTREACHED();
  return data;
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
          mock_data_.erase(mock_data_.lower_bound(op->key),
                           mock_data_.lower_bound(successor(op->key)));
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

void GetCallback(const base::Closure& callback,
                 bool* success_out,
                 std::vector<uint8_t>* value_out,
                 bool success,
                 const std::vector<uint8_t>& value) {
  *success_out = success;
  *value_out = value;
  callback.Run();
}

void SuccessCallback(const base::Closure& callback,
                     bool* success_out,
                     bool success) {
  *success_out = success;
  callback.Run();
}

}  // namespace

class LevelDBWrapperImplTest : public testing::Test,
                               public mojom::LevelDBObserver {
 public:
  struct Observation {
    enum { kAdd, kChange, kDelete, kDeleteAll } type;
    std::string key;
    std::string old_value;
    std::string new_value;
    std::string source;
  };

  LevelDBWrapperImplTest()
      : db_(&mock_data_),
        level_db_wrapper_(&db_,
                          kTestPrefix,
                          kTestSizeLimit,
                          base::TimeDelta::FromSeconds(5),
                          10 * 1024 * 1024 /* max_bytes_per_hour */,
                          60 /* max_commits_per_hour */,
                          base::Bind(&NoOp)),
        observer_binding_(this) {
    set_mock_data(std::string(kTestPrefix) + "def", "defdata");
    set_mock_data(std::string(kTestPrefix) + "123", "123data");
    set_mock_data("123", "baddata");

    level_db_wrapper_.Bind(mojo::MakeRequest(&level_db_wrapper_ptr_));
    mojom::LevelDBObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(&ptr_info, level_db_wrapper_ptr_.associated_group());
    level_db_wrapper_ptr_->AddObserver(std::move(ptr_info));
  }

  void set_mock_data(const std::string& key, const std::string& value) {
    mock_data_[StdStringToUint8Vector(key)] = StdStringToUint8Vector(value);
  }

  void set_mock_data(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& value) {
    mock_data_[key] = value;
  }

  bool has_mock_data(const std::string& key) {
    return mock_data_.find(StdStringToUint8Vector(key)) != mock_data_.end();
  }

  std::string get_mock_data(const std::string& key) {
    return has_mock_data(key)
               ? Uint8VectorToStdString(mock_data_[StdStringToUint8Vector(key)])
               : "";
  }

  mojom::LevelDBWrapper* wrapper() { return level_db_wrapper_ptr_.get(); }

  bool GetSync(const std::vector<uint8_t>& key, std::vector<uint8_t>* result) {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->Get(key, base::Bind(&GetCallback, run_loop.QuitClosure(),
                                   &success, result));
    run_loop.Run();
    return success;
  }

  bool PutSync(const std::vector<uint8_t>& key,
               const std::vector<uint8_t>& value,
               std::string source = kTestSource) {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->Put(
        key, value, source,
        base::Bind(&SuccessCallback, run_loop.QuitClosure(), &success));
    run_loop.Run();
    return success;
  }

  bool DeleteSync(const std::vector<uint8_t>& key) {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->Delete(
        key, kTestSource,
        base::Bind(&SuccessCallback, run_loop.QuitClosure(), &success));
    run_loop.Run();
    return success;
  }

  bool DeleteAllSync() {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->DeleteAll(
        kTestSource,
        base::Bind(&SuccessCallback, run_loop.QuitClosure(), &success));
    run_loop.Run();
    return success;
  }

  void CommitChanges() {
    ASSERT_TRUE(level_db_wrapper_.commit_batch_);
    level_db_wrapper_.CommitChanges();
  }

  const std::vector<Observation>& observations() { return observations_; }

 private:
  // LevelDBObserver:
  void KeyAdded(const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& value,
                const std::string& source) override {
    observations_.push_back({Observation::kAdd, Uint8VectorToStdString(key), "",
                             Uint8VectorToStdString(value), source});
  }
  void KeyChanged(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& new_value,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kChange, Uint8VectorToStdString(key),
                             Uint8VectorToStdString(old_value),
                             Uint8VectorToStdString(new_value), source});
  }
  void KeyDeleted(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kDelete, Uint8VectorToStdString(key),
                             Uint8VectorToStdString(old_value), "", source});
  }
  void AllDeleted(const std::string& source) override {
    observations_.push_back({Observation::kDeleteAll, "", "", "", source});
  }
  void GetAllComplete(const std::string& source) override {}

  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  MockLevelDBDatabase db_;
  LevelDBWrapperImpl level_db_wrapper_;
  mojom::LevelDBWrapperPtr level_db_wrapper_ptr_;
  mojo::AssociatedBinding<mojom::LevelDBObserver> observer_binding_;
  std::vector<Observation> observations_;
};

TEST_F(LevelDBWrapperImplTest, GetLoadedFromMap) {
  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(StdStringToUint8Vector("123"), &result));
  EXPECT_EQ(StdStringToUint8Vector("123data"), result);

  EXPECT_FALSE(GetSync(StdStringToUint8Vector("x"), &result));
}

TEST_F(LevelDBWrapperImplTest, GetFromPutOverwrite) {
  std::vector<uint8_t> key = StdStringToUint8Vector("123");
  std::vector<uint8_t> value = StdStringToUint8Vector("foo");

  EXPECT_TRUE(PutSync(key, value));

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LevelDBWrapperImplTest, GetFromPutNewKey) {
  std::vector<uint8_t> key = StdStringToUint8Vector("newkey");
  std::vector<uint8_t> value = StdStringToUint8Vector("foo");

  EXPECT_TRUE(PutSync(key, value));

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LevelDBWrapperImplTest, GetAll) {
  leveldb::mojom::DatabaseError status;
  std::vector<mojom::KeyValuePtr> data;
  EXPECT_TRUE(wrapper()->GetAll(kTestSource, &status, &data));
  EXPECT_EQ(leveldb::mojom::DatabaseError::OK, status);
  EXPECT_EQ(2u, data.size());
}

TEST_F(LevelDBWrapperImplTest, CommitPutToDB) {
  std::string key1 = "123";
  std::string value1 = "foo";
  std::string key2 = "abc";
  std::string value2 = "data abc";

  EXPECT_TRUE(
      PutSync(StdStringToUint8Vector(key1), StdStringToUint8Vector(value1)));
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key2),
                      StdStringToUint8Vector("old value")));
  EXPECT_TRUE(
      PutSync(StdStringToUint8Vector(key2), StdStringToUint8Vector(value2)));

  EXPECT_FALSE(has_mock_data(kTestPrefix + key2));

  CommitChanges();
  EXPECT_TRUE(has_mock_data(kTestPrefix + key1));
  EXPECT_EQ(value1, get_mock_data(kTestPrefix + key1));
  EXPECT_TRUE(has_mock_data(kTestPrefix + key2));
  EXPECT_EQ(value2, get_mock_data(kTestPrefix + key2));
}

TEST_F(LevelDBWrapperImplTest, PutObservations) {
  std::string key = "new_key";
  std::string value1 = "foo";
  std::string value2 = "data abc";
  std::string source1 = "source1";
  std::string source2 = "source2";

  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key),
                      StdStringToUint8Vector(value1), source1));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kAdd, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value1, observations()[0].new_value);
  EXPECT_EQ(source1, observations()[0].source);

  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key),
                      StdStringToUint8Vector(value2), source2));
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kChange, observations()[1].type);
  EXPECT_EQ(key, observations()[1].key);
  EXPECT_EQ(value1, observations()[1].old_value);
  EXPECT_EQ(value2, observations()[1].new_value);
  EXPECT_EQ(source2, observations()[1].source);
}

TEST_F(LevelDBWrapperImplTest, DeleteNonExistingKey) {
  EXPECT_TRUE(DeleteSync(StdStringToUint8Vector("doesn't exist")));
  EXPECT_EQ(0u, observations().size());
}

TEST_F(LevelDBWrapperImplTest, DeleteExistingKey) {
  std::string key = "newkey";
  std::string value = "foo";
  set_mock_data(kTestPrefix + key, value);

  EXPECT_TRUE(DeleteSync(StdStringToUint8Vector(key)));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDelete, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value, observations()[0].old_value);
  EXPECT_EQ(kTestSource, observations()[0].source);

  EXPECT_TRUE(has_mock_data(kTestPrefix + key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(kTestPrefix + key));
}

TEST_F(LevelDBWrapperImplTest, DeleteAll) {
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  set_mock_data(dummy_key, value);
  set_mock_data(kTestPrefix + key, value);

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[0].type);
  EXPECT_EQ(kTestSource, observations()[0].source);

  EXPECT_TRUE(has_mock_data(kTestPrefix + key));
  EXPECT_TRUE(has_mock_data(dummy_key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(kTestPrefix + key));
  EXPECT_TRUE(has_mock_data(dummy_key));

  // Deleting all again should still work, an cause an observation.
  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(kTestSource, observations()[1].source);

  // And now we've deleted all, writing something the quota size should work.
  EXPECT_TRUE(PutSync(std::vector<uint8_t>(kTestSizeLimit, 'b'),
                      std::vector<uint8_t>()));
}

TEST_F(LevelDBWrapperImplTest, PutOverQuotaLargeValue) {
  std::vector<uint8_t> key = StdStringToUint8Vector("newkey");
  std::vector<uint8_t> value(kTestSizeLimit, 4);

  EXPECT_FALSE(PutSync(key, value));

  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value));
}

TEST_F(LevelDBWrapperImplTest, PutOverQuotaLargeKey) {
  std::vector<uint8_t> key(kTestSizeLimit, 'a');
  std::vector<uint8_t> value = StdStringToUint8Vector("newvalue");

  EXPECT_FALSE(PutSync(key, value));

  key.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value));
}

TEST_F(LevelDBWrapperImplTest, PutWhenAlreadyOverQuota) {
  std::string key = "largedata";
  std::vector<uint8_t> value(kTestSizeLimit, 4);

  set_mock_data(kTestPrefix + key, Uint8VectorToStdString(value));

  // Put with same data should succeed.
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value));

  // Put with same data size should succeed.
  value[1] = 13;
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value));

  // Adding a new key when already over quota should not succeed.
  EXPECT_FALSE(PutSync(StdStringToUint8Vector("newkey"), {1, 2, 3}));

  // Reducing size should also succeed.
  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value));

  // Increasing size again should succeed, as still under the limit.
  value.resize(value.size() + 1);
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value));

  // But increasing back to original size should fail.
  value.resize(kTestSizeLimit);
  EXPECT_FALSE(PutSync(StdStringToUint8Vector(key), value));
}

TEST_F(LevelDBWrapperImplTest, PutWhenAlreadyOverQuotaBecauseOfLargeKey) {
  std::vector<uint8_t> key(kTestSizeLimit, 'x');
  std::vector<uint8_t> value = StdStringToUint8Vector("value");

  set_mock_data(kTestPrefix + Uint8VectorToStdString(key),
                Uint8VectorToStdString(value));

  // Put with same data size should succeed.
  value[0] = 'X';
  EXPECT_TRUE(PutSync(key, value));

  // Reducing size should also succeed.
  value.clear();
  EXPECT_TRUE(PutSync(key, value));

  // Increasing size should fail.
  value.resize(1, 'a');
  EXPECT_FALSE(PutSync(key, value));
}

}  // namespace content
