// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/leveldb_pref_store.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockPrefStoreObserver : public PrefStore::Observer {
 public:
  MOCK_METHOD1(OnPrefValueChanged, void(const std::string&));
  MOCK_METHOD1(OnInitializationCompleted, void(bool));
};

class MockReadErrorDelegate : public PersistentPrefStore::ReadErrorDelegate {
 public:
  MOCK_METHOD1(OnError, void(PersistentPrefStore::PrefReadError));
};

}  // namespace

class LevelDBPrefStoreTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_dir_));
    data_dir_ = data_dir_.AppendASCII("prefs");
    ASSERT_TRUE(PathExists(data_dir_));
  }

  virtual void TearDown() OVERRIDE {
    Close();
  }

  void Open() {
    pref_store_ = new LevelDBPrefStore(
        temp_dir_.path(), message_loop_.message_loop_proxy().get());
    EXPECT_EQ(LevelDBPrefStore::PREF_READ_ERROR_NONE, pref_store_->ReadPrefs());
  }

  void Close() {
    pref_store_ = NULL;
    base::RunLoop().RunUntilIdle();
  }

  void CloseAndReopen() {
    Close();
    Open();
  }

  // The path to temporary directory used to contain the test operations.
  base::ScopedTempDir temp_dir_;
  // The path to the directory where the test data is stored in the source tree.
  base::FilePath data_dir_;
  // A message loop that we can use as the file thread message loop.
  base::MessageLoop message_loop_;

  scoped_refptr<LevelDBPrefStore> pref_store_;
};

TEST_F(LevelDBPrefStoreTest, PutAndGet) {
  Open();
  const std::string key = "some.key";
  pref_store_->SetValue(key, new base::FundamentalValue(5));
  base::FundamentalValue orig_value(5);
  const base::Value* actual_value;
  EXPECT_TRUE(pref_store_->GetValue(key, &actual_value));
  EXPECT_TRUE(orig_value.Equals(actual_value));
}

TEST_F(LevelDBPrefStoreTest, PutAndGetPersistent) {
  Open();
  const std::string key = "some.key";
  pref_store_->SetValue(key, new base::FundamentalValue(5));

  CloseAndReopen();
  const base::Value* actual_value = NULL;
  base::FundamentalValue orig_value(5);
  EXPECT_TRUE(pref_store_->GetValue(key, &actual_value));
  EXPECT_TRUE(orig_value.Equals(actual_value));
}

TEST_F(LevelDBPrefStoreTest, BasicObserver) {
  scoped_refptr<LevelDBPrefStore> pref_store = new LevelDBPrefStore(
      temp_dir_.path(), message_loop_.message_loop_proxy().get());
  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_NONE, pref_store->ReadPrefs());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  const std::string key = "some.key";
  EXPECT_CALL(mock_observer, OnPrefValueChanged(key)).Times(1);
  pref_store->SetValue(key, new base::FundamentalValue(5));

  pref_store->RemoveObserver(&mock_observer);
}

TEST_F(LevelDBPrefStoreTest, SetValueSilently) {
  Open();

  MockPrefStoreObserver mock_observer;
  pref_store_->AddObserver(&mock_observer);
  const std::string key = "some.key";
  EXPECT_CALL(mock_observer, OnPrefValueChanged(key)).Times(0);
  pref_store_->SetValueSilently(key, new base::FundamentalValue(30));
  pref_store_->RemoveObserver(&mock_observer);

  CloseAndReopen();
  base::FundamentalValue value(30);
  const base::Value* actual_value = NULL;
  EXPECT_TRUE(pref_store_->GetValue(key, &actual_value));
  EXPECT_TRUE(base::Value::Equals(&value, actual_value));
}

TEST_F(LevelDBPrefStoreTest, GetMutableValue) {
  Open();

  const std::string key = "some.key";
  base::DictionaryValue* orig_value = new base::DictionaryValue;
  orig_value->SetInteger("key2", 25);
  pref_store_->SetValue(key, orig_value);

  base::Value* actual_value;
  EXPECT_TRUE(pref_store_->GetMutableValue(key, &actual_value));
  EXPECT_TRUE(orig_value->Equals(actual_value));
  base::DictionaryValue* dict_value;
  ASSERT_TRUE(actual_value->GetAsDictionary(&dict_value));
  dict_value->SetInteger("key2", 30);
  pref_store_->ReportValueChanged(key);

  // Ensure the new value is stored in memory.
  const base::Value* retrieved_value;
  EXPECT_TRUE(pref_store_->GetValue(key, &retrieved_value));
  scoped_ptr<base::DictionaryValue> golden_value(new base::DictionaryValue);
  golden_value->SetInteger("key2", 30);
  EXPECT_TRUE(base::Value::Equals(golden_value.get(), retrieved_value));

  // Ensure the new value is persisted to disk.
  CloseAndReopen();
  EXPECT_TRUE(pref_store_->GetValue(key, &retrieved_value));
  EXPECT_TRUE(base::Value::Equals(golden_value.get(), retrieved_value));
}

TEST_F(LevelDBPrefStoreTest, RemoveFromMemory) {
  Open();
  const std::string key = "some.key";
  pref_store_->SetValue(key, new base::FundamentalValue(5));

  MockPrefStoreObserver mock_observer;
  pref_store_->AddObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnPrefValueChanged(key)).Times(1);
  pref_store_->RemoveValue(key);
  pref_store_->RemoveObserver(&mock_observer);

  const base::Value* retrieved_value;
  EXPECT_FALSE(pref_store_->GetValue(key, &retrieved_value));
}

TEST_F(LevelDBPrefStoreTest, RemoveFromDisk) {
  Open();
  const std::string key = "some.key";
  pref_store_->SetValue(key, new base::FundamentalValue(5));

  CloseAndReopen();

  pref_store_->RemoveValue(key);

  CloseAndReopen();

  const base::Value* retrieved_value;
  EXPECT_FALSE(pref_store_->GetValue(key, &retrieved_value));
}

TEST_F(LevelDBPrefStoreTest, OpenAsync) {
  // First set a key/value with a synchronous connection.
  Open();
  const std::string key = "some.key";
  pref_store_->SetValue(key, new base::FundamentalValue(5));
  Close();

  scoped_refptr<LevelDBPrefStore> pref_store(new LevelDBPrefStore(
      temp_dir_.path(), message_loop_.message_loop_proxy().get()));
  MockReadErrorDelegate* delegate = new MockReadErrorDelegate;
  pref_store->ReadPrefsAsync(delegate);

  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
  base::RunLoop().RunUntilIdle();
  pref_store->RemoveObserver(&mock_observer);

  const base::Value* result;
  EXPECT_TRUE(pref_store->GetValue("some.key", &result));
  int int_value;
  EXPECT_TRUE(result->GetAsInteger(&int_value));
  EXPECT_EQ(5, int_value);

  pref_store = NULL;
}

TEST_F(LevelDBPrefStoreTest, OpenAsyncError) {
  // Open a connection that will lock the database.
  Open();

  // Try to open an async connection to the same database.
  scoped_refptr<LevelDBPrefStore> pref_store(new LevelDBPrefStore(
      temp_dir_.path(), message_loop_.message_loop_proxy().get()));
  MockReadErrorDelegate* delegate = new MockReadErrorDelegate;
  pref_store->ReadPrefsAsync(delegate);

  MockPrefStoreObserver mock_observer;
  pref_store->AddObserver(&mock_observer);
  EXPECT_CALL(*delegate,
              OnError(PersistentPrefStore::PREF_READ_ERROR_LEVELDB_IO))
      .Times(1);
  EXPECT_CALL(mock_observer, OnInitializationCompleted(true)).Times(1);
  base::RunLoop().RunUntilIdle();
  pref_store->RemoveObserver(&mock_observer);

  EXPECT_TRUE(pref_store->ReadOnly());
  EXPECT_EQ(PersistentPrefStore::PREF_READ_ERROR_LEVELDB_IO,
            pref_store->GetReadError());

  // Sync connection to the database will be closed by the destructor.
}

TEST_F(LevelDBPrefStoreTest, RepairCorrupt) {
  // Open a database where CURRENT has no newline. Ensure that repair is called
  // and there is no error reading the database.
  base::FilePath corrupted_dir = data_dir_.AppendASCII("corrupted_leveldb");
  base::FilePath dest = temp_dir_.path().AppendASCII("corrupted_leveldb");
  const bool kRecursive = true;
  ASSERT_TRUE(CopyDirectory(corrupted_dir, dest, kRecursive));
  pref_store_ =
      new LevelDBPrefStore(dest, message_loop_.message_loop_proxy().get());
  EXPECT_EQ(LevelDBPrefStore::PREF_READ_ERROR_LEVELDB_CORRUPTION,
            pref_store_->ReadPrefs());
}

TEST_F(LevelDBPrefStoreTest, Values) {
  Open();
  pref_store_->SetValue("boolean", new base::FundamentalValue(false));
  pref_store_->SetValue("integer", new base::FundamentalValue(10));
  pref_store_->SetValue("double", new base::FundamentalValue(10.3));
  pref_store_->SetValue("string", new base::StringValue("some string"));

  base::DictionaryValue* dict_value = new base::DictionaryValue;
  dict_value->Set("boolean", new base::FundamentalValue(true));
  scoped_ptr<base::DictionaryValue> golden_dict_value(dict_value->DeepCopy());
  pref_store_->SetValue("dictionary", dict_value);

  base::ListValue* list_value = new base::ListValue;
  list_value->Set(2, new base::StringValue("string in list"));
  scoped_ptr<base::ListValue> golden_list_value(list_value->DeepCopy());
  pref_store_->SetValue("list", list_value);

  // Do something nontrivial as well.
  base::DictionaryValue* compound_value = new base::DictionaryValue;
  base::ListValue* outer_list = new base::ListValue;
  base::ListValue* inner_list = new base::ListValue;
  inner_list->Set(0, new base::FundamentalValue(5));
  outer_list->Set(1, inner_list);
  compound_value->Set("compound_lists", outer_list);
  scoped_ptr<base::DictionaryValue> golden_compound_value(
      compound_value->DeepCopy());
  pref_store_->SetValue("compound_value", compound_value);

  CloseAndReopen();

  const base::Value* value;
  EXPECT_TRUE(pref_store_->GetValue("boolean", &value));
  EXPECT_TRUE(base::FundamentalValue(false).Equals(value));

  EXPECT_TRUE(pref_store_->GetValue("integer", &value));
  EXPECT_TRUE(base::FundamentalValue(10).Equals(value));

  EXPECT_TRUE(pref_store_->GetValue("double", &value));
  EXPECT_TRUE(base::FundamentalValue(10.3).Equals(value));

  EXPECT_TRUE(pref_store_->GetValue("string", &value));
  EXPECT_TRUE(base::StringValue("some string").Equals(value));

  EXPECT_TRUE(pref_store_->GetValue("dictionary", &value));
  EXPECT_TRUE(base::Value::Equals(golden_dict_value.get(), value));

  EXPECT_TRUE(pref_store_->GetValue("list", &value));
  EXPECT_TRUE(base::Value::Equals(golden_list_value.get(), value));

  EXPECT_TRUE(pref_store_->GetValue("compound_value", &value));
  EXPECT_TRUE(base::Value::Equals(golden_compound_value.get(), value));
}
