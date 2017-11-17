// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/leveldb/public/cpp/util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/fake_leveldb_database.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
using CacheMode = LevelDBWrapperImpl::CacheMode;
using DatabaseError = leveldb::mojom::DatabaseError;

const char* kTestSource = "source";
const size_t kTestSizeLimit = 512;

std::string ToString(const std::vector<uint8_t>& input) {
  return leveldb::Uint8VectorToStdString(input);
}

std::vector<uint8_t> ToBytes(const std::string& input) {
  return leveldb::StdStringToUint8Vector(input);
}

class GetAllCallback : public mojom::LevelDBWrapperGetAllCallback {
 public:
  static mojom::LevelDBWrapperGetAllCallbackAssociatedPtrInfo CreateAndBind(
      bool* result,
      base::OnceClosure callback) {
    mojom::LevelDBWrapperGetAllCallbackAssociatedPtrInfo ptr_info;
    auto request = mojo::MakeRequest(&ptr_info);
    mojo::MakeStrongAssociatedBinding(
        base::WrapUnique(new GetAllCallback(result, std::move(callback))),
        std::move(request));
    return ptr_info;
  }

 private:
  GetAllCallback(bool* result, base::OnceClosure callback)
      : m_result(result), m_callback(std::move(callback)) {}
  void Complete(bool success) override {
    *m_result = success;
    if (!m_callback.is_null())
      std::move(m_callback).Run();
  }

  bool* m_result;
  base::OnceClosure m_callback;
};

class MockDelegate : public LevelDBWrapperImpl::Delegate {
 public:
  void OnNoBindings() override {}
  std::vector<leveldb::mojom::BatchedOperationPtr> PrepareToCommit() override {
    return std::vector<leveldb::mojom::BatchedOperationPtr>();
  }
  void DidCommit(DatabaseError error) override {}
  void OnMapLoaded(DatabaseError error) override { map_load_count_++; }
  std::vector<LevelDBWrapperImpl::Change> FixUpData(
      const LevelDBWrapperImpl::ValueMap& data) override {
    return std::move(mock_changes_);
  }

  int map_load_count() const { return map_load_count_; }
  void set_mock_changes(std::vector<LevelDBWrapperImpl::Change> changes) {
    mock_changes_ = std::move(changes);
  }

 private:
  int map_load_count_ = 0;
  std::vector<LevelDBWrapperImpl::Change> mock_changes_;
};

void GetCallback(base::OnceClosure callback,
                 bool* success_out,
                 std::vector<uint8_t>* value_out,
                 bool success,
                 const std::vector<uint8_t>& value) {
  *success_out = success;
  *value_out = value;
  std::move(callback).Run();
}

void SuccessCallback(base::OnceClosure callback,
                     bool* success_out,
                     bool success) {
  *success_out = success;
  std::move(callback).Run();
}

void NoOpSuccessCallback(bool success) {}

}  // namespace

class LevelDBWrapperImplTest : public testing::Test,
                               public mojom::LevelDBObserver {
 public:
  struct Observation {
    enum { kAdd, kChange, kDelete, kDeleteAll, kSendOldValue } type;
    std::string key;
    std::string old_value;
    std::string new_value;
    std::string source;
    bool should_send_old_value;
  };

  LevelDBWrapperImplTest()
      : db_(&mock_data_),
        observer_binding_(this) {
    LevelDBWrapperImpl::Options options;
    options.max_size = kTestSizeLimit;
    options.default_commit_delay = base::TimeDelta::FromSeconds(5);
    options.max_bytes_per_hour = 10 * 1024 * 1024;
    options.max_commits_per_hour = 60;
    options.cache_mode = CacheMode::KEYS_ONLY_WHEN_POSSIBLE;
    level_db_wrapper_ = std::make_unique<LevelDBWrapperImpl>(
        &db_, test_prefix_, &delegate_, options);

    set_mock_data(test_prefix_ + test_key1_, test_value1_);
    set_mock_data(test_prefix_ + test_key2_, test_value2_);
    set_mock_data(test_key2_, "baddata");

    level_db_wrapper_->Bind(mojo::MakeRequest(&level_db_wrapper_ptr_));
    mojom::LevelDBObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(mojo::MakeRequest(&ptr_info));
    level_db_wrapper_ptr_->AddObserver(std::move(ptr_info));
  }

  void set_mock_data(const std::string& key, const std::string& value) {
    mock_data_[ToBytes(key)] = ToBytes(value);
  }

  void set_mock_data(const std::vector<uint8_t>& key,
                     const std::vector<uint8_t>& value) {
    mock_data_[key] = value;
  }

  bool has_mock_data(const std::string& key) {
    return mock_data_.find(ToBytes(key)) != mock_data_.end();
  }

  std::string get_mock_data(const std::string& key) {
    return has_mock_data(key) ? ToString(mock_data_[ToBytes(key)]) : "";
  }

  void clear_mock_data() { mock_data_.clear(); }

  mojom::LevelDBWrapper* wrapper() { return level_db_wrapper_ptr_.get(); }
  LevelDBWrapperImpl* wrapper_impl() { return level_db_wrapper_.get(); }

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

  void CommitChanges() { level_db_wrapper_->ScheduleImmediateCommit(); }

  const std::vector<Observation>& observations() { return observations_; }

  MockDelegate* delegate() { return &delegate_; }

  void should_record_send_old_value_observations(bool value) {
    should_record_send_old_value_observations_ = value;
  }

 protected:
  const std::string test_prefix_ = "abc";
  const std::string test_key1_ = "def";
  const std::string test_key2_ = "123";
  const std::string test_value1_ = "defdata";
  const std::string test_value2_ = "123data";

  const std::vector<uint8_t> test_prefix_bytes_ = ToBytes(test_prefix_);
  const std::vector<uint8_t> test_key1_bytes_ = ToBytes(test_key1_);
  const std::vector<uint8_t> test_key2_bytes_ = ToBytes(test_key2_);
  const std::vector<uint8_t> test_value2_bytes_ = ToBytes(test_value2_);

 private:
  // LevelDBObserver:
  void KeyAdded(const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& value,
                const std::string& source) override {
    observations_.push_back(
        {Observation::kAdd, ToString(key), "", ToString(value), source, false});
  }
  void KeyChanged(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& new_value,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kChange, ToString(key),
                             ToString(old_value), ToString(new_value), source,
                             false});
  }
  void KeyDeleted(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kDelete, ToString(key),
                             ToString(old_value), "", source, false});
  }
  void AllDeleted(const std::string& source) override {
    observations_.push_back(
        {Observation::kDeleteAll, "", "", "", source, false});
  }
  void ShouldSendOldValueOnMutations(bool value) override {
    if (should_record_send_old_value_observations_)
      observations_.push_back(
          {Observation::kSendOldValue, "", "", "", "", value});
  }

  TestBrowserThreadBundle thread_bundle_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  FakeLevelDBDatabase db_;
  MockDelegate delegate_;
  std::unique_ptr<LevelDBWrapperImpl> level_db_wrapper_;
  mojom::LevelDBWrapperPtr level_db_wrapper_ptr_;
  mojo::AssociatedBinding<mojom::LevelDBObserver> observer_binding_;
  std::vector<Observation> observations_;
  bool should_record_send_old_value_observations_ = false;
};

class LevelDBWrapperImplParamTest
    : public LevelDBWrapperImplTest,
      public testing::WithParamInterface<CacheMode> {
 public:
  LevelDBWrapperImplParamTest() {}
  ~LevelDBWrapperImplParamTest() override {}
};

INSTANTIATE_TEST_CASE_P(LevelDBWrapperImplTest,
                        LevelDBWrapperImplParamTest,
                        testing::Values(CacheMode::KEYS_ONLY_WHEN_POSSIBLE,
                                        CacheMode::KEYS_AND_VALUES));

TEST_F(LevelDBWrapperImplTest, GetLoadedFromMap) {
  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(test_key2_bytes_, &result));
  EXPECT_EQ(test_value2_bytes_, result);

  EXPECT_FALSE(GetSync(ToBytes("x"), &result));
}

TEST_F(LevelDBWrapperImplTest, GetFromPutOverwrite) {
  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");

  EXPECT_TRUE(PutSync(key, value, test_value2_bytes_));

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
}

TEST_F(LevelDBWrapperImplTest, GetFromPutNewKey) {
  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key = ToBytes("newkey");
  std::vector<uint8_t> value = ToBytes("foo");

  EXPECT_TRUE(PutSync(key, value, base::nullopt));

  std::vector<uint8_t> result;
  EXPECT_TRUE(GetSync(key, &result));
  EXPECT_EQ(value, result);
}

TEST_P(LevelDBWrapperImplParamTest, GetAll) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  DatabaseError status;
  std::vector<mojom::KeyValuePtr> data;
  base::RunLoop run_loop;
  bool result = false;
  EXPECT_TRUE(wrapper()->GetAll(
      GetAllCallback::CreateAndBind(&result, run_loop.QuitClosure()), &status,
      &data));
  EXPECT_EQ(DatabaseError::OK, status);
  EXPECT_EQ(2u, data.size());
  EXPECT_FALSE(result);
  run_loop.Run();
  EXPECT_TRUE(result);
}

TEST_P(LevelDBWrapperImplParamTest, CommitPutToDB) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::string key1 = test_key2_;
  std::string value1 = "foo";
  std::string key2 = test_prefix_;
  std::string value2 = "data abc";

  EXPECT_TRUE(PutSync(ToBytes(key1), ToBytes(value1), test_value2_bytes_));
  EXPECT_TRUE(PutSync(ToBytes(key2), ToBytes("old value"), base::nullopt));
  EXPECT_TRUE(PutSync(ToBytes(key2), ToBytes(value2), ToBytes("old value")));

  EXPECT_FALSE(has_mock_data(test_prefix_ + key2));

  CommitChanges();
  EXPECT_TRUE(has_mock_data(test_prefix_ + key1));
  EXPECT_EQ(value1, get_mock_data(test_prefix_ + key1));
  EXPECT_TRUE(has_mock_data(test_prefix_ + key2));
  EXPECT_EQ(value2, get_mock_data(test_prefix_ + key2));
}

TEST_P(LevelDBWrapperImplParamTest, PutObservations) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "new_key";
  std::string value1 = "foo";
  std::string value2 = "data abc";
  std::string source1 = "source1";
  std::string source2 = "source2";

  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value1), base::nullopt, source1));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kAdd, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value1, observations()[0].new_value);
  EXPECT_EQ(source1, observations()[0].source);

  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value2), ToBytes(value1), source2));
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kChange, observations()[1].type);
  EXPECT_EQ(key, observations()[1].key);
  EXPECT_EQ(value1, observations()[1].old_value);
  EXPECT_EQ(value2, observations()[1].new_value);
  EXPECT_EQ(source2, observations()[1].source);

  // Same put should not cause another observation.
  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value2), base::nullopt, source2));
  ASSERT_EQ(2u, observations().size());
}

TEST_P(LevelDBWrapperImplParamTest, DeleteNonExistingKey) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  EXPECT_TRUE(DeleteSync(ToBytes("doesn't exist"), std::vector<uint8_t>()));
  EXPECT_EQ(0u, observations().size());
}

TEST_P(LevelDBWrapperImplParamTest, DeleteExistingKey) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  set_mock_data(test_prefix_ + key, value);

  EXPECT_TRUE(DeleteSync(ToBytes(key), ToBytes(value)));
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDelete, observations()[0].type);
  EXPECT_EQ(key, observations()[0].key);
  EXPECT_EQ(value, observations()[0].old_value);
  EXPECT_EQ(kTestSource, observations()[0].source);

  EXPECT_TRUE(has_mock_data(test_prefix_ + key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(test_prefix_ + key));
}

TEST_P(LevelDBWrapperImplParamTest, DeleteAllWithoutLoadedMap) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  set_mock_data(dummy_key, value);
  set_mock_data(test_prefix_ + key, value);

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(1u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[0].type);
  EXPECT_EQ(kTestSource, observations()[0].source);

  EXPECT_TRUE(has_mock_data(test_prefix_ + key));
  EXPECT_TRUE(has_mock_data(dummy_key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(test_prefix_ + key));
  EXPECT_TRUE(has_mock_data(dummy_key));

  // Deleting all again should still work, but not cause an observation.
  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(1u, observations().size());

  // And now we've deleted all, writing something the quota size should work.
  EXPECT_TRUE(PutSync(std::vector<uint8_t>(kTestSizeLimit, 'b'),
                      std::vector<uint8_t>(), base::nullopt));
}

TEST_P(LevelDBWrapperImplParamTest, DeleteAllWithLoadedMap) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  set_mock_data(dummy_key, value);

  EXPECT_TRUE(PutSync(ToBytes(key), ToBytes(value), base::nullopt));

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(kTestSource, observations()[1].source);

  EXPECT_TRUE(has_mock_data(dummy_key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(test_prefix_ + key));
  EXPECT_TRUE(has_mock_data(dummy_key));
}

TEST_P(LevelDBWrapperImplParamTest, DeleteAllWithPendingMapLoad) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::string key = "newkey";
  std::string value = "foo";
  std::string dummy_key = "foobar";
  set_mock_data(dummy_key, value);

  wrapper()->Put(ToBytes(key), ToBytes(value), base::nullopt, kTestSource,
                 base::BindOnce(&NoOpSuccessCallback));

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kDeleteAll, observations()[1].type);
  EXPECT_EQ(kTestSource, observations()[1].source);

  EXPECT_TRUE(has_mock_data(dummy_key));

  CommitChanges();
  EXPECT_FALSE(has_mock_data(test_prefix_ + key));
  EXPECT_TRUE(has_mock_data(dummy_key));
}

TEST_P(LevelDBWrapperImplParamTest, DeleteAllWithoutLoadedEmptyMap) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  clear_mock_data();

  EXPECT_TRUE(DeleteAllSync());
  ASSERT_EQ(0u, observations().size());
}

TEST_F(LevelDBWrapperImplParamTest, PutOverQuotaLargeValue) {
  wrapper_impl()->SetCacheModeForTesting(
      LevelDBWrapperImpl::CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key = ToBytes("newkey");
  std::vector<uint8_t> value(kTestSizeLimit, 4);

  EXPECT_FALSE(PutSync(key, value, base::nullopt));

  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value, base::nullopt));
}

TEST_F(LevelDBWrapperImplParamTest, PutOverQuotaLargeKey) {
  wrapper_impl()->SetCacheModeForTesting(
      LevelDBWrapperImpl::CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key(kTestSizeLimit, 'a');
  std::vector<uint8_t> value = ToBytes("newvalue");

  EXPECT_FALSE(PutSync(key, value, base::nullopt));

  key.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(key, value, base::nullopt));
}

TEST_F(LevelDBWrapperImplParamTest, PutWhenAlreadyOverQuota) {
  wrapper_impl()->SetCacheModeForTesting(
      LevelDBWrapperImpl::CacheMode::KEYS_AND_VALUES);
  std::string key = "largedata";
  std::vector<uint8_t> value(kTestSizeLimit, 4);
  std::vector<uint8_t> old_value = value;

  set_mock_data(test_prefix_ + key, ToString(value));

  // Put with same data should succeed.
  EXPECT_TRUE(PutSync(ToBytes(key), value, base::nullopt));

  // Put with same data size should succeed.
  value[1] = 13;
  EXPECT_TRUE(PutSync(ToBytes(key), value, old_value));

  // Adding a new key when already over quota should not succeed.
  EXPECT_FALSE(PutSync(ToBytes("newkey"), {1, 2, 3}, base::nullopt));

  // Reducing size should also succeed.
  old_value = value;
  value.resize(kTestSizeLimit / 2);
  EXPECT_TRUE(PutSync(ToBytes(key), value, old_value));

  // Increasing size again should succeed, as still under the limit.
  old_value = value;
  value.resize(value.size() + 1);
  EXPECT_TRUE(PutSync(ToBytes(key), value, old_value));

  // But increasing back to original size should fail.
  old_value = value;
  value.resize(kTestSizeLimit);
  EXPECT_FALSE(PutSync(ToBytes(key), value, old_value));
}

TEST_F(LevelDBWrapperImplParamTest, PutWhenAlreadyOverQuotaBecauseOfLargeKey) {
  wrapper_impl()->SetCacheModeForTesting(
      LevelDBWrapperImpl::CacheMode::KEYS_AND_VALUES);
  std::vector<uint8_t> key(kTestSizeLimit, 'x');
  std::vector<uint8_t> value = ToBytes("value");
  std::vector<uint8_t> old_value = value;

  set_mock_data(test_prefix_ + ToString(key), ToString(value));

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

TEST_P(LevelDBWrapperImplParamTest, PutAfterPurgeMemory) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::vector<uint8_t> result;
  const auto key = test_key2_bytes_;
  const auto value = test_value2_bytes_;
  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  // Adding again doesn't load map again.
  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  wrapper_impl()->PurgeMemory();

  // Now adding should still work, and load map again.
  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 2);
}

TEST_P(LevelDBWrapperImplParamTest, PurgeMemoryWithPendingChanges) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  EXPECT_TRUE(PutSync(key, value, test_value2_bytes_));
  EXPECT_EQ(delegate()->map_load_count(), 1);

  // Purge memory, and read. Should not actually have purged, so should not have
  // triggered a load.
  wrapper_impl()->PurgeMemory();

  EXPECT_TRUE(PutSync(key, value, value));
  EXPECT_EQ(delegate()->map_load_count(), 1);
}

TEST_P(LevelDBWrapperImplParamTest, FixUpData) {
  wrapper_impl()->SetCacheModeForTesting(GetParam());
  std::vector<LevelDBWrapperImpl::Change> changes;
  changes.push_back(std::make_pair(test_key1_bytes_, ToBytes("foo")));
  changes.push_back(std::make_pair(test_key2_bytes_, base::nullopt));
  changes.push_back(std::make_pair(test_prefix_bytes_, ToBytes("bla")));
  delegate()->set_mock_changes(std::move(changes));

  std::vector<mojom::KeyValuePtr> data;
  DatabaseError status;
  bool success = false;
  base::RunLoop run_loop;
  EXPECT_TRUE(wrapper()->GetAll(
      GetAllCallback::CreateAndBind(&success, run_loop.QuitClosure()), &status,
      &data));
  run_loop.Run();
  EXPECT_EQ(DatabaseError::OK, status);
  ASSERT_EQ(2u, data.size());
  EXPECT_EQ(test_prefix_, ToString(data[0]->key));
  EXPECT_EQ("bla", ToString(data[0]->value));
  EXPECT_EQ(test_key1_, ToString(data[1]->key));
  EXPECT_EQ("foo", ToString(data[1]->value));

  EXPECT_FALSE(has_mock_data(test_prefix_ + test_key2_));
  EXPECT_EQ("foo", get_mock_data(test_prefix_ + test_key1_));
  EXPECT_EQ("bla", get_mock_data(test_prefix_ + test_prefix_));
}

TEST_F(LevelDBWrapperImplTest, SetOnlyKeysWithoutDatabase) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  MockDelegate delegate;
  LevelDBWrapperImpl::Options options = {
      CacheMode::KEYS_ONLY_WHEN_POSSIBLE, kTestSizeLimit,
      base::TimeDelta::FromSeconds(5),
      10 * 1024 * 1024 /* max_bytes_per_hour */, 60 /* max_commits_per_hour */};
  LevelDBWrapperImpl level_db_wrapper(nullptr, test_prefix_, &delegate,
                                      options);
  mojom::LevelDBWrapperPtr level_db_wrapper_ptr;
  level_db_wrapper.Bind(mojo::MakeRequest(&level_db_wrapper_ptr));
  // Setting only keys mode is noop.
  level_db_wrapper.SetCacheModeForTesting(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::UNLOADED,
            level_db_wrapper.CurrentLoadState());
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::KEYS_AND_VALUES,
            level_db_wrapper.desired_load_state_);

  // Put and Get can work synchronously without reload.
  bool put_callback_called = false;
  level_db_wrapper.Put(key, value, base::nullopt, "source",
                       base::BindOnce(
                           [](bool* put_callback_called, bool success) {
                             EXPECT_TRUE(success);
                             *put_callback_called = true;
                           },
                           &put_callback_called));
  EXPECT_TRUE(put_callback_called);

  std::vector<uint8_t> expected_value;
  level_db_wrapper.Get(
      key, base::BindOnce(
               [](std::vector<uint8_t>* expected_value, bool success,
                  const std::vector<uint8_t>& value) {
                 EXPECT_TRUE(success);
                 *expected_value = value;
               },
               &expected_value));
  EXPECT_EQ(expected_value, value);
}

TEST_F(LevelDBWrapperImplTest, CommitOnDifferentCacheModes) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foobar");

  ASSERT_TRUE(PutSync(key, value, base::nullopt));
  ASSERT_TRUE(wrapper_impl()->commit_batch_);
  auto* changes = &wrapper_impl()->commit_batch_->changed_values;
  EXPECT_EQ(1u, changes->size());
  auto it = changes->find(key);
  ASSERT_NE(it, changes->end());
  EXPECT_FALSE(it->second);

  wrapper_impl()->CommitChanges();
  EXPECT_EQ("foo", get_mock_data(test_prefix_ + test_key2_));
  ASSERT_TRUE(wrapper_impl()->keys_only_map_);
  EXPECT_EQ(2u, wrapper_impl()->keys_only_map_->size());
  ASSERT_TRUE(PutSync(key, value, value));
  EXPECT_FALSE(wrapper_impl()->commit_batch_);
  ASSERT_TRUE(PutSync(key, value2, value));
  EXPECT_TRUE(wrapper_impl()->keys_only_map_);
  ASSERT_TRUE(wrapper_impl()->commit_batch_);
  changes = &wrapper_impl()->commit_batch_->changed_values;
  EXPECT_EQ(1u, changes->size());
  it = changes->find(key);
  ASSERT_NE(it, changes->end());
  EXPECT_EQ(value2, it->second.value());

  clear_mock_data();
  wrapper_impl()->CommitChanges();
  EXPECT_EQ("foobar", get_mock_data(test_prefix_ + test_key2_));
}

TEST_F(LevelDBWrapperImplTest, GetAllWhenCacheOnlyKeys) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foobar");

  // Go to load state only keys.
  ASSERT_TRUE(PutSync(key, value, base::nullopt));
  CommitChanges();
  EXPECT_TRUE(wrapper_impl()->keys_only_map_);
  ASSERT_TRUE(PutSync(key, value2, value));
  EXPECT_TRUE(wrapper_impl()->commit_batch_);

  auto get_all_callback = [](const std::vector<uint8_t>& expected_value,
                             DatabaseError status,
                             std::vector<mojom::KeyValuePtr> data) {
    EXPECT_EQ(1u, data.size());
    EXPECT_EQ(expected_value, data[0]->value);
    EXPECT_EQ(DatabaseError::OK, status);
  };
  clear_mock_data();
  base::RunLoop run_loop;
  bool result = false;
  // This GetAll() should get |value2| from the previous Put().
  wrapper()->GetAll(
      GetAllCallback::CreateAndBind(&result, run_loop.QuitClosure()),
      base::BindOnce(get_all_callback, value2));
  // This Put should not affect the value returned by GetAll().
  EXPECT_TRUE(PutSync(key, value, value2));
  EXPECT_TRUE(result);
  run_loop.Run();
  // GetAll should trigger a commit first and database gets the value from the
  // last Put() before GetAll().
  EXPECT_EQ("foobar", get_mock_data(test_prefix_ + test_key2_));
}

TEST_F(LevelDBWrapperImplTest, GetAllAfterSetCacheMode) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foobar");

  // Go to load state only keys.
  ASSERT_TRUE(PutSync(key, value, base::nullopt));
  CommitChanges();
  EXPECT_TRUE(wrapper_impl()->keys_only_map_);
  ASSERT_TRUE(PutSync(key, value2, value));
  EXPECT_TRUE(wrapper_impl()->commit_batch_);

  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::KEYS_AND_VALUES,
            wrapper_impl()->desired_load_state_);
  // Cache isn't cleared when commit batch exists.
  EXPECT_TRUE(wrapper_impl()->keys_only_map_);
  // Add a put task to on load tasks queue, which should affect the GetAll().
  wrapper()->Put(key, value, value2, "source",
                 base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  // Commit batch still has old value. New Put() task is queued.
  ASSERT_TRUE(wrapper_impl()->commit_batch_);
  EXPECT_EQ(value2,
            wrapper_impl()->commit_batch_->changed_values.find(key)->second);

  auto get_all_callback = [](const std::vector<uint8_t>& expected_value,
                             DatabaseError status,
                             std::vector<mojom::KeyValuePtr> data) {
    EXPECT_EQ(1u, data.size());
    EXPECT_EQ(expected_value, data[0]->value);
    EXPECT_EQ(DatabaseError::OK, status);
  };
  clear_mock_data();
  base::RunLoop run_loop;
  bool result = false;
  wrapper()->GetAll(
      GetAllCallback::CreateAndBind(&result, run_loop.QuitClosure()),
      base::BindOnce(get_all_callback, value));
  // This Delete() should not affect the value returned by GetAll().
  wrapper()->Delete(key, value, "source",
                    base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  run_loop.Run();
  EXPECT_TRUE(result);
  // GetAll should trigger a commit first and database gets the value from the
  // commit batch before GetAll(). The async Put() call is not part of that
  // commit.
  EXPECT_EQ("foobar", get_mock_data(test_prefix_ + test_key2_));
}

TEST_F(LevelDBWrapperImplTest, SetCacheModeConsistent) {
  std::vector<uint8_t> key = test_key2_bytes_;
  std::vector<uint8_t> value = ToBytes("foo");
  std::vector<uint8_t> value2 = ToBytes("foobar");

  EXPECT_EQ(LevelDBWrapperImpl::LoadState::UNLOADED,
            wrapper_impl()->CurrentLoadState());
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::KEYS_ONLY,
            wrapper_impl()->desired_load_state_);
  EXPECT_TRUE(PutSync(key, value, base::nullopt));
  EXPECT_FALSE(wrapper_impl()->keys_only_map_);
  ASSERT_TRUE(wrapper_impl()->keys_values_map_);
  EXPECT_EQ(2u, wrapper_impl()->keys_values_map_->size());
  EXPECT_EQ(value, wrapper_impl()->keys_values_map_->find(key)->second);
  EXPECT_TRUE(wrapper_impl()->commit_batch_);

  // Clear the default database and include only the new items.
  clear_mock_data();
  wrapper_impl()->CommitChanges();
  EXPECT_TRUE(PutSync(key, value2, value));
  EXPECT_TRUE(wrapper_impl()->commit_batch_);
  EXPECT_TRUE(wrapper_impl()->keys_only_map_);
  EXPECT_EQ(2u, wrapper_impl()->keys_only_map_->size());

  // Setting cache mode does not reload the cache till it is required.
  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::KEYS_AND_VALUES,
            wrapper_impl()->desired_load_state_);
  EXPECT_TRUE(wrapper_impl()->keys_only_map_);
  EXPECT_FALSE(wrapper_impl()->keys_values_map_);
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::KEYS_AND_VALUES,
            wrapper_impl()->desired_load_state_);

  // Reload deletes keys_only_map and reloads keys_values_map from database (has
  // only one entry from previous commit).
  EXPECT_TRUE(PutSync(key, value, value2));
  EXPECT_TRUE(wrapper_impl()->commit_batch_);
  ASSERT_TRUE(wrapper_impl()->keys_values_map_);
  EXPECT_EQ(1u, wrapper_impl()->keys_values_map_->size());
  EXPECT_EQ(value, wrapper_impl()->keys_values_map_->find(key)->second);
  EXPECT_FALSE(wrapper_impl()->keys_only_map_);
  wrapper_impl()->CommitChanges();

  // Changing back the cache mode still works.
  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::KEYS_ONLY,
            wrapper_impl()->desired_load_state_);

  EXPECT_TRUE(PutSync(key, value2, value));
  ASSERT_TRUE(wrapper_impl()->keys_only_map_);
  EXPECT_EQ(1u, wrapper_impl()->keys_only_map_->size());
  EXPECT_EQ(value2.size(), wrapper_impl()->keys_only_map_->find(key)->second);
  wrapper_impl()->CommitChanges();
  EXPECT_EQ(LevelDBWrapperImpl::LoadState::KEYS_ONLY,
            wrapper_impl()->desired_load_state_);
  ASSERT_TRUE(wrapper_impl()->keys_only_map_);
  EXPECT_EQ(1u, wrapper_impl()->keys_only_map_->size());
}

TEST_F(LevelDBWrapperImplTest, SendOldValueObservations) {
  should_record_send_old_value_observations(true);
  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_AND_VALUES);
  // Flush tasks on mojo thread to observe callback.
  EXPECT_TRUE(DeleteSync(ToBytes("doesn't exist"), base::nullopt));
  wrapper_impl()->SetCacheModeForTesting(CacheMode::KEYS_ONLY_WHEN_POSSIBLE);
  // Flush tasks on mojo thread to observe callback.
  EXPECT_TRUE(DeleteSync(ToBytes("doesn't exist"), base::nullopt));

  ASSERT_EQ(2u, observations().size());
  EXPECT_EQ(Observation::kSendOldValue, observations()[0].type);
  EXPECT_FALSE(observations()[0].should_send_old_value);
  EXPECT_EQ(Observation::kSendOldValue, observations()[1].type);
  EXPECT_TRUE(observations()[1].should_send_old_value);
}

}  // namespace content
