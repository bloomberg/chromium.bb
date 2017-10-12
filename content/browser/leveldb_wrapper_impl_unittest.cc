// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/leveldb/public/cpp/util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/mock_leveldb_database.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb::StdStringToUint8Vector;
using leveldb::Uint8VectorToStdString;

namespace content {

namespace {

const char* kTestPrefix = "abc";
const char* kTestSource = "source";
const size_t kTestSizeLimit = 512;

class GetAllCallback : public mojom::LevelDBWrapperGetAllCallback {
 public:
  static mojom::LevelDBWrapperGetAllCallbackAssociatedPtrInfo CreateAndBind(
      bool* result,
      const base::Closure& callback) {
    mojom::LevelDBWrapperGetAllCallbackAssociatedPtrInfo ptr_info;
    auto request = mojo::MakeRequest(&ptr_info);
    mojo::MakeStrongAssociatedBinding(
        base::WrapUnique(new GetAllCallback(result, callback)),
        std::move(request));
    return ptr_info;
  }

 private:
  GetAllCallback(bool* result, const base::Closure& callback)
      : m_result(result), m_callback(callback) {}
  void Complete(bool success) override {
    *m_result = success;
    m_callback.Run();
  }

  bool* m_result;
  base::Closure m_callback;
};

class MockDelegate : public LevelDBWrapperImpl::Delegate {
 public:
  void OnNoBindings() override {}
  std::vector<leveldb::mojom::BatchedOperationPtr> PrepareToCommit() override {
    return std::vector<leveldb::mojom::BatchedOperationPtr>();
  }
  void DidCommit(leveldb::mojom::DatabaseError error) override {}
  void OnMapLoaded(leveldb::mojom::DatabaseError error) override {
    map_load_count_++;
  }
  std::vector<LevelDBWrapperImpl::Change> FixUpData(
      const LevelDBWrapperImpl::ValueMap& data) override {
    return mock_changes_;
  }

  int map_load_count() const { return map_load_count_; }
  void set_mock_changes(std::vector<LevelDBWrapperImpl::Change> changes) {
    mock_changes_ = std::move(changes);
  }

 private:
  int map_load_count_ = 0;
  std::vector<LevelDBWrapperImpl::Change> mock_changes_;
};

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

void NoOpSuccessCallback(bool success) {}

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
                          &delegate_),
        observer_binding_(this) {
    set_mock_data(std::string(kTestPrefix) + "def", "defdata");
    set_mock_data(std::string(kTestPrefix) + "123", "123data");
    set_mock_data("123", "baddata");

    level_db_wrapper_.Bind(mojo::MakeRequest(&level_db_wrapper_ptr_));
    mojom::LevelDBObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
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

  void clear_mock_data() { mock_data_.clear(); }

  mojom::LevelDBWrapper* wrapper() { return level_db_wrapper_ptr_.get(); }
  LevelDBWrapperImpl* wrapper_impl() { return &level_db_wrapper_; }

  bool GetSync(const std::vector<uint8_t>& key, std::vector<uint8_t>* result) {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->Get(key, base::BindOnce(&GetCallback, run_loop.QuitClosure(),
                                       &success, result));
    run_loop.Run();
    return success;
  }

  bool PutSync(const std::vector<uint8_t>& key,
               const std::vector<uint8_t>& value,
               const base::Optional<std::vector<uint8_t>>& client_old_value,
               std::string source = kTestSource) {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->Put(
        key, value, client_old_value, source,
        base::BindOnce(&SuccessCallback, run_loop.QuitClosure(), &success));
    run_loop.Run();
    return success;
  }

  bool DeleteSync(
      const std::vector<uint8_t>& key,
      const base::Optional<std::vector<uint8_t>>& client_old_value) {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->Delete(
        key, client_old_value, kTestSource,
        base::BindOnce(&SuccessCallback, run_loop.QuitClosure(), &success));
    run_loop.Run();
    return success;
  }

  bool DeleteAllSync() {
    base::RunLoop run_loop;
    bool success = false;
    wrapper()->DeleteAll(
        kTestSource,
        base::BindOnce(&SuccessCallback, run_loop.QuitClosure(), &success));
    run_loop.Run();
    return success;
  }

  void CommitChanges() { level_db_wrapper_.ScheduleImmediateCommit(); }

  const std::vector<Observation>& observations() { return observations_; }

  MockDelegate* delegate() { return &delegate_; }

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
  void ShouldSendOldValueOnMutations(bool value) override {}

  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  MockLevelDBDatabase db_;
  MockDelegate delegate_;
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

  EXPECT_TRUE(PutSync(key, value, StdStringToUint8Vector("123data")));

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LevelDBWrapperImplTest, GetFromPutNewKey) {
  std::vector<uint8_t> key = StdStringToUint8Vector("newkey");
  std::vector<uint8_t> value = StdStringToUint8Vector("foo");

  EXPECT_TRUE(PutSync(key, value, base::nullopt));

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LevelDBWrapperImplTest, GetAll) {
  leveldb::mojom::DatabaseError status;
  std::vector<mojom::KeyValuePtr> data;
  base::RunLoop run_loop;
  bool result = false;
  EXPECT_TRUE(wrapper()->GetAll(
      GetAllCallback::CreateAndBind(&result, run_loop.QuitClosure()), &status,
      &data));
  EXPECT_EQ(leveldb::mojom::DatabaseError::OK, status);
  EXPECT_EQ(2u, data.size());
  EXPECT_FALSE(result);
  run_loop.Run();
  EXPECT_TRUE(result);
}

TEST_F(LevelDBWrapperImplTest, CommitPutToDB) {
  std::string key1 = "123";
  std::string value1 = "foo";
  std::string key2 = "abc";
  std::string value2 = "data abc";

  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key1),
                      StdStringToUint8Vector(value1),
                      StdStringToUint8Vector("123data")));
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key2),
                      StdStringToUint8Vector("old value"), base::nullopt));
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key2),
                      StdStringToUint8Vector(value2),
                      StdStringToUint8Vector("old value")));

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
                      StdStringToUint8Vector(value1), base::nullopt, source1));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kAdd, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value1, observations()[0].new_value);
  EXPECT_EQ(source1, observations()[0].source);

  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key),
                      StdStringToUint8Vector(value2),
                      StdStringToUint8Vector(value1), source2));
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kChange, observations()[1].type);
  EXPECT_EQ(key, observations()[1].key);
  EXPECT_EQ(value1, observations()[1].old_value);
  EXPECT_EQ(value2, observations()[1].new_value);
  EXPECT_EQ(source2, observations()[1].source);

  // Same put should not cause another observation.
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key),
                      StdStringToUint8Vector(value2), base::nullopt, source2));
  ASSERT_EQ(2u, observations().size());
}

TEST_F(LevelDBWrapperImplTest, DeleteNonExistingKey) {
  EXPECT_TRUE(DeleteSync(StdStringToUint8Vector("doesn't exist"),
                         std::vector<uint8_t>()));
  EXPECT_EQ(0u, observations().size());
}

TEST_F(LevelDBWrapperImplTest, DeleteExistingKey) {
  std::string key = "newkey";
  std::string value = "foo";
  set_mock_data(kTestPrefix + key, value);

  EXPECT_TRUE(
      DeleteSync(StdStringToUint8Vector(key), StdStringToUint8Vector(value)));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDelete, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value, observations()[0].old_value);
  EXPECT_EQ(kTestSource, observations()[0].source);

  EXPECT_TRUE(has_mock_data(kTestPrefix + key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(kTestPrefix + key));
}

TEST_F(LevelDBWrapperImplTest, DeleteAllWithoutLoadedMap) {
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

  // Deleting all again should still work, but not cause an observation.
  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(1u, observations().size());

  // And now we've deleted all, writing something the quota size should work.
  EXPECT_TRUE(PutSync(std::vector<uint8_t>(kTestSizeLimit, 'b'),
                      std::vector<uint8_t>(), base::nullopt));
}

TEST_F(LevelDBWrapperImplTest, DeleteAllWithLoadedMap) {
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  set_mock_data(dummy_key, value);

  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key),
                      StdStringToUint8Vector(value), base::nullopt));

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(kTestSource, observations()[1].source);

  EXPECT_TRUE(has_mock_data(dummy_key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(kTestPrefix + key));
  EXPECT_TRUE(has_mock_data(dummy_key));
}

TEST_F(LevelDBWrapperImplTest, DeleteAllWithPendingMapLoad) {
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  set_mock_data(dummy_key, value);

  wrapper()->Put(StdStringToUint8Vector(key), StdStringToUint8Vector(value),
                 base::nullopt, kTestSource,
                 base::BindOnce(&NoOpSuccessCallback));

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(kTestSource, observations()[1].source);

  EXPECT_TRUE(has_mock_data(dummy_key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(kTestPrefix + key));
  EXPECT_TRUE(has_mock_data(dummy_key));
}

TEST_F(LevelDBWrapperImplTest, DeleteAllWithoutLoadedEmptyMap) {
  clear_mock_data();

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(0u, observations().size());
}

TEST_F(LevelDBWrapperImplTest, PutOverQuotaLargeValue) {
  std::vector<uint8_t> key = StdStringToUint8Vector("newkey");
  std::vector<uint8_t> value(kTestSizeLimit, 4);

  EXPECT_FALSE(PutSync(key, value, base::nullopt));

  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value, base::nullopt));
}

TEST_F(LevelDBWrapperImplTest, PutOverQuotaLargeKey) {
  std::vector<uint8_t> key(kTestSizeLimit, 'a');
  std::vector<uint8_t> value = StdStringToUint8Vector("newvalue");

  EXPECT_FALSE(PutSync(key, value, base::nullopt));

  key.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value, base::nullopt));
}

TEST_F(LevelDBWrapperImplTest, PutWhenAlreadyOverQuota) {
  std::string key = "largedata";
  std::vector<uint8_t> value(kTestSizeLimit, 4);
  std::vector<uint8_t> old_value = value;

  set_mock_data(kTestPrefix + key, Uint8VectorToStdString(value));

  // Put with same data should succeed.
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value, base::nullopt));

  // Put with same data size should succeed.
  value[1] = 13;
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value, old_value));

  // Adding a new key when already over quota should not succeed.
  EXPECT_FALSE(
      PutSync(StdStringToUint8Vector("newkey"), {1, 2, 3}, base::nullopt));

  // Reducing size should also succeed.
  old_value = value;
  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value, old_value));

  // Increasing size again should succeed, as still under the limit.
  old_value = value;
  value.resize(value.size() + 1);
  EXPECT_TRUE(PutSync(StdStringToUint8Vector(key), value, old_value));

  // But increasing back to original size should fail.
  old_value = value;
  value.resize(kTestSizeLimit);
  EXPECT_FALSE(PutSync(StdStringToUint8Vector(key), value, old_value));
}

TEST_F(LevelDBWrapperImplTest, PutWhenAlreadyOverQuotaBecauseOfLargeKey) {
  std::vector<uint8_t> key(kTestSizeLimit, 'x');
  std::vector<uint8_t> value = StdStringToUint8Vector("value");
  std::vector<uint8_t> old_value = value;

  set_mock_data(kTestPrefix + Uint8VectorToStdString(key),
                Uint8VectorToStdString(value));

  // Put with same data size should succeed.
  value[0] = 'X';
  EXPECT_TRUE(PutSync(key, value, old_value));

  // Reducing size should also succeed.
  old_value = value;
  value.clear();
  EXPECT_TRUE(PutSync(key, value, old_value));

  // Increasing size should fail.
  old_value = value;
  value.resize(1, 'a');
  EXPECT_FALSE(PutSync(key, value, old_value));
}

TEST_F(LevelDBWrapperImplTest, GetAfterPurgeMemory) {
  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(StdStringToUint8Vector("123"), &result));
  EXPECT_EQ(StdStringToUint8Vector("123data"), result);
  EXPECT_EQ(delegate()->map_load_count(), 1);

  // Reading again doesn't load map again.
  EXPECT_TRUE(GetSync(StdStringToUint8Vector("123"), &result));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  wrapper_impl()->PurgeMemory();

  // Now reading should still work, and load map again.
  result.clear();
  EXPECT_TRUE(GetSync(StdStringToUint8Vector("123"), &result));
  EXPECT_EQ(StdStringToUint8Vector("123data"), result);
  EXPECT_EQ(delegate()->map_load_count(), 2);
}

TEST_F(LevelDBWrapperImplTest, PurgeMemoryWithPendingChanges) {
  std::vector<uint8_t> key = StdStringToUint8Vector("123");
  std::vector<uint8_t> value = StdStringToUint8Vector("foo");
  EXPECT_TRUE(PutSync(key, value, StdStringToUint8Vector("123data")));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  // Purge memory, and read. Should not actually have purged, so should not have
  // triggered a load.
  wrapper_impl()->PurgeMemory();

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
  EXPECT_EQ(delegate()->map_load_count(), 1);
}

TEST_F(LevelDBWrapperImplTest, FixUpData) {
  std::vector<LevelDBWrapperImpl::Change> changes;
  changes.push_back(std::make_pair(StdStringToUint8Vector("def"),
                                   StdStringToUint8Vector("foo")));
  changes.push_back(
      std::make_pair(StdStringToUint8Vector("123"), base::nullopt));
  changes.push_back(std::make_pair(StdStringToUint8Vector("abc"),
                                   StdStringToUint8Vector("bla")));
  delegate()->set_mock_changes(std::move(changes));

  std::vector<uint8_t> result;
  EXPECT_FALSE(GetSync(StdStringToUint8Vector("123"), &result));
  EXPECT_TRUE(GetSync(StdStringToUint8Vector("def"), &result));
  EXPECT_EQ(StdStringToUint8Vector("foo"), result);
  EXPECT_TRUE(GetSync(StdStringToUint8Vector("abc"), &result));
  EXPECT_EQ(StdStringToUint8Vector("bla"), result);

  EXPECT_FALSE(has_mock_data(kTestPrefix + std::string("123")));
  EXPECT_EQ("foo", get_mock_data(kTestPrefix + std::string("def")));
  EXPECT_EQ("bla", get_mock_data(kTestPrefix + std::string("abc")));
}

}  // namespace content
