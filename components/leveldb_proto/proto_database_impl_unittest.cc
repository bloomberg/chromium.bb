// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_database_impl.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/testing/proto/test.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"

using base::MessageLoop;
using base::ScopedTempDir;
using testing::Invoke;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::_;

namespace leveldb_proto {

namespace {

typedef std::map<std::string, TestProto> EntryMap;

const char kTestLevelDBClientName[] = "Test";

class MockDB : public LevelDB {
 public:
  MOCK_METHOD1(Init, bool(const leveldb_proto::Options& options));
  MOCK_METHOD2(Save, bool(const KeyValueVector&, const KeyVector&));
  MOCK_METHOD1(Load, bool(std::vector<std::string>*));
  MOCK_METHOD3(Get, bool(const std::string&, bool*, std::string*));

  MockDB() : LevelDB(kTestLevelDBClientName) {}
};

class MockDatabaseCaller {
 public:
  MOCK_METHOD1(InitCallback, void(bool));
  MOCK_METHOD1(DestroyCallback, void(bool));
  MOCK_METHOD1(SaveCallback, void(bool));
  void LoadCallback(bool success,
                    std::unique_ptr<std::vector<TestProto>> entries) {
    LoadCallback1(success, entries.get());
  }
  MOCK_METHOD2(LoadCallback1, void(bool, std::vector<TestProto>*));
  void GetCallback(bool success, std::unique_ptr<TestProto> entry) {
    GetCallback1(success, entry.get());
  }
  MOCK_METHOD2(GetCallback1, void(bool, TestProto*));
};

}  // namespace

bool operator==(const Options& lhs, const Options& rhs) {
  return lhs.database_dir == rhs.database_dir &&
         lhs.write_buffer_size == rhs.write_buffer_size;
}

EntryMap GetSmallModel() {
  EntryMap model;

  model["0"].set_id("0");
  model["0"].set_data("http://foo.com/1");

  model["1"].set_id("1");
  model["1"].set_data("http://bar.com/all");

  model["2"].set_id("2");
  model["2"].set_data("http://baz.com/1");

  return model;
}

void ExpectEntryPointersEquals(EntryMap expected,
                               const std::vector<TestProto>& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < actual.size(); i++) {
    EntryMap::iterator expected_it = expected.find(actual[i].id());
    EXPECT_TRUE(expected_it != expected.end());
    std::string serialized_expected = expected_it->second.SerializeAsString();
    std::string serialized_actual = actual[i].SerializeAsString();
    EXPECT_EQ(serialized_expected, serialized_actual);
    expected.erase(expected_it);
  }
}

class ProtoDatabaseImplTest : public testing::Test {
 public:
  void SetUp() override {
    main_loop_.reset(new MessageLoop());
    db_.reset(new ProtoDatabaseImpl<TestProto>(main_loop_->task_runner()));
  }

  void TearDown() override {
    db_.reset();
    base::RunLoop().RunUntilIdle();
    main_loop_.reset();
  }

  std::unique_ptr<ProtoDatabaseImpl<TestProto>> db_;
  std::unique_ptr<MessageLoop> main_loop_;
};

// Test that ProtoDatabaseImpl calls Init on the underlying database and that
// the caller's InitCallback is called with the correct value.
TEST_F(ProtoDatabaseImplTest, TestDBInitSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  EXPECT_CALL(*mock_db, Init(Options(path))).WillOnce(Return(true));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(true));

  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBInitFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  EXPECT_CALL(*mock_db, Init(Options(path))).WillOnce(Return(false));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(false));

  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(AppendLoadEntries, model) {
  std::vector<std::string>* output = arg0;
  for (const auto& pair : model)
    output->push_back(pair.second.SerializeAsString());

  return true;
}

ACTION_P(VerifyLoadEntries, expected) {
  std::vector<TestProto>* actual = arg1;
  ExpectEntryPointersEquals(expected, *actual);
}

// Test that ProtoDatabaseImpl calls Load on the underlying database and that
// the caller's LoadCallback is called with the correct success value. Also
// confirms that on success, the expected entries are passed to the caller's
// LoadCallback.
TEST_F(ProtoDatabaseImplTest, TestDBLoadSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Load(_)).WillOnce(AppendLoadEntries(model));
  EXPECT_CALL(caller, LoadCallback1(true, _))
      .WillOnce(VerifyLoadEntries(testing::ByRef(model)));
  db_->LoadEntries(
      base::Bind(&MockDatabaseCaller::LoadCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBLoadFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Load(_)).WillOnce(Return(false));
  EXPECT_CALL(caller, LoadCallback1(false, _));
  db_->LoadEntries(
      base::Bind(&MockDatabaseCaller::LoadCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(SetGetEntry, model) {
  const std::string& key = arg0;
  bool* found = arg1;
  std::string* output = arg2;
  auto it = model.find(key);
  if (it == model.end()) {
    *found = false;
  } else {
    *found = true;
    *output = it->second.SerializeAsString();
  }
  return true;
}

ACTION_P(VerifyGetEntry, expected) {
  TestProto* actual = arg1;
  EXPECT_EQ(expected.SerializeAsString(), actual->SerializeAsString());
}

TEST_F(ProtoDatabaseImplTest, TestDBGetSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  std::string key("1");
  ASSERT_TRUE(model.count(key));
  EXPECT_CALL(*mock_db, Get(key, _, _)).WillOnce(SetGetEntry(model));
  EXPECT_CALL(caller, GetCallback1(true, _))
      .WillOnce(VerifyGetEntry(model[key]));
  db_->GetEntry(key, base::Bind(&MockDatabaseCaller::GetCallback,
                                base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST(ProtoDatabaseImplLevelDBTest, TestDBSaveAndLoadKeys) {
  base::MessageLoop main_loop;

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::Thread db_thread("dbthread");
  ASSERT_TRUE(db_thread.Start());
  std::unique_ptr<ProtoDatabaseImpl<TestProto>> db(
      new ProtoDatabaseImpl<TestProto>(db_thread.task_runner()));

  auto expect_init_success =
      base::Bind([](bool success) { EXPECT_TRUE(success); });
  db->Init(kTestLevelDBClientName, temp_dir.GetPath(), expect_init_success);

  base::RunLoop run_update_entries;
  auto expect_update_success = base::Bind(
      [](base::Closure signal, bool success) {
        EXPECT_TRUE(success);
        signal.Run();
      },
      run_update_entries.QuitClosure());
  TestProto test_proto;
  test_proto.set_data("some data");
  ProtoDatabase<TestProto>::KeyEntryVector data_set(
          {{"0", test_proto}, {"1", test_proto}, {"2", test_proto}});
  db->UpdateEntries(
      base::MakeUnique<ProtoDatabase<TestProto>::KeyEntryVector>(data_set),
      base::MakeUnique<std::vector<std::string>>(), expect_update_success);
  run_update_entries.Run();

  base::RunLoop run_load_keys;
  auto verify_loaded_keys = base::Bind(
      [](base::Closure signal, bool success,
         std::unique_ptr<std::vector<std::string>> keys) {
        EXPECT_TRUE(success);
        EXPECT_THAT(*keys, UnorderedElementsAre("0", "1", "2"));
        signal.Run();
      },
      run_load_keys.QuitClosure());
  db->LoadKeys(verify_loaded_keys);
  run_load_keys.Run();

  // Shutdown database.
  db.reset();
  base::RunLoop run_destruction;
  db_thread.task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(base::DoNothing), run_destruction.QuitClosure());
  run_destruction.Run();
}

TEST_F(ProtoDatabaseImplTest, TestDBGetNotFound) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  std::string key("does_not_exist");
  ASSERT_FALSE(model.count(key));
  EXPECT_CALL(*mock_db, Get(key, _, _)).WillOnce(SetGetEntry(model));
  EXPECT_CALL(caller, GetCallback1(true, nullptr));
  db_->GetEntry(key, base::Bind(&MockDatabaseCaller::GetCallback,
                                base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBGetFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  std::string key("does_not_exist");
  ASSERT_FALSE(model.count(key));
  EXPECT_CALL(*mock_db, Get(key, _, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, GetCallback1(false, nullptr));
  db_->GetEntry(key, base::Bind(&MockDatabaseCaller::GetCallback,
                                base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(VerifyUpdateEntries, expected) {
  const KeyValueVector actual = arg0;
  // Create a vector of TestProto from |actual| to reuse the comparison
  // function.
  std::vector<TestProto> extracted_entries;
  for (const auto& pair : actual) {
    TestProto entry;
    if (!entry.ParseFromString(pair.second)) {
      ADD_FAILURE() << "Unable to deserialize the protobuf";
      return false;
    }

    extracted_entries.push_back(entry);
  }

  ExpectEntryPointersEquals(expected, extracted_entries);
  return true;
}

// Test that ProtoDatabaseImpl calls Save on the underlying database with the
// correct entries to save and that the caller's SaveCallback is called with the
// correct success value.
TEST_F(ProtoDatabaseImplTest, TestDBSaveSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  for (const auto& pair : model)
    entries->push_back(std::make_pair(pair.second.id(), pair.second));

  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(VerifyUpdateEntries(model));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(
      std::move(entries), std::move(keys_to_remove),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBSaveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(
      std::move(entries), std::move(keys_to_remove),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

// Test that ProtoDatabaseImpl calls Save on the underlying database with the
// correct entries to delete and that the caller's SaveCallback is called with
// the correct success value.
TEST_F(ProtoDatabaseImplTest, TestDBRemoveSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());
  for (const auto& pair : model)
    keys_to_remove->push_back(pair.second.id());

  KeyVector keys_copy(*keys_to_remove.get());
  EXPECT_CALL(*mock_db, Save(_, keys_copy)).WillOnce(Return(true));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(
      std::move(entries), std::move(keys_to_remove),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBRemoveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  std::unique_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  std::unique_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      base::WrapUnique(mock_db), path,
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(
      std::move(entries), std::move(keys_to_remove),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

// This tests that normal usage of the real database does not cause any
// threading violations.
TEST(ProtoDatabaseImplThreadingTest, TestDBDestruction) {
  base::MessageLoop main_loop;

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::Thread db_thread("dbthread");
  ASSERT_TRUE(db_thread.Start());

  std::unique_ptr<ProtoDatabaseImpl<TestProto>> db(
      new ProtoDatabaseImpl<TestProto>(db_thread.task_runner()));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(_));
  db->Init(
      kTestLevelDBClientName, temp_dir.GetPath(),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  db.reset();

  base::RunLoop run_loop;
  db_thread.task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(base::DoNothing), run_loop.QuitClosure());
  run_loop.Run();
}

// This tests that normal usage of the real database does not cause any
// threading violations.
TEST(ProtoDatabaseImplThreadingTest, TestDBDestroy) {
  base::MessageLoop main_loop;

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::Thread db_thread("dbthread");
  ASSERT_TRUE(db_thread.Start());

  std::unique_ptr<ProtoDatabaseImpl<TestProto>> db(
      new ProtoDatabaseImpl<TestProto>(db_thread.task_runner()));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(_));
  db->Init(
      kTestLevelDBClientName, temp_dir.GetPath(),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(caller, DestroyCallback(_));
  db->Destroy(base::Bind(&MockDatabaseCaller::DestroyCallback,
                         base::Unretained(&caller)));

  db.reset();

  base::RunLoop run_loop;
  db_thread.task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(base::DoNothing), run_loop.QuitClosure());
  run_loop.Run();
}

// Test that the LevelDB properly saves entries and that load returns the saved
// entries. If |close_after_save| is true, the database will be closed after
// saving and then re-opened to ensure that the data is properly persisted.
void TestLevelDBSaveAndLoad(bool close_after_save) {
  base::MessageLoop main_loop;

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EntryMap model = GetSmallModel();

  KeyValueVector save_entries;
  std::vector<std::string> load_entries;
  KeyVector remove_keys;

  for (const auto& pair : model) {
    save_entries.push_back(
        std::make_pair(pair.second.id(), pair.second.SerializeAsString()));
  }

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  EXPECT_TRUE(db->Init(temp_dir.GetPath()));
  EXPECT_TRUE(db->Save(save_entries, remove_keys));

  if (close_after_save) {
    db.reset(new LevelDB(kTestLevelDBClientName));
    EXPECT_TRUE(db->Init(temp_dir.GetPath()));
  }

  EXPECT_TRUE(db->Load(&load_entries));

  // Convert the strings back to TestProto.
  std::vector<TestProto> loaded_protos;
  for (const auto& serialized_entry : load_entries) {
    TestProto entry;
    ASSERT_TRUE(entry.ParseFromString(serialized_entry));
    loaded_protos.push_back(entry);
  }

  ExpectEntryPointersEquals(model, loaded_protos);
}

TEST(ProtoDatabaseImplLevelDBTest, TestDBSaveAndLoad) {
  TestLevelDBSaveAndLoad(false);
}

TEST(ProtoDatabaseImplLevelDBTest, TestDBCloseAndReopen) {
  TestLevelDBSaveAndLoad(true);
}

TEST(ProtoDatabaseImplLevelDBTest, TestDBInitFail) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  leveldb::Options options;
  options.create_if_missing = false;
  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));

  KeyValueVector save_entries;
  std::vector<std::string> load_entries;
  KeyVector remove_keys;

  EXPECT_FALSE(db->InitWithOptions(temp_dir.GetPath(), options));
  EXPECT_FALSE(db->Load(&load_entries));
  EXPECT_FALSE(db->Save(save_entries, remove_keys));
}

TEST(ProtoDatabaseImplLevelDBTest, TestMemoryDatabase) {
  base::MessageLoop main_loop;

  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));

  std::vector<std::string> load_entries;

  ASSERT_TRUE(db->Init(base::FilePath()));

  ASSERT_TRUE(db->Load(&load_entries));
  EXPECT_EQ(0u, load_entries.size());

  KeyValueVector save_entries(1, std::make_pair("foo", "bar"));
  KeyVector remove_keys;

  ASSERT_TRUE(db->Save(save_entries, remove_keys));

  std::vector<std::string> second_load_entries;

  ASSERT_TRUE(db->Load(&second_load_entries));
  EXPECT_EQ(1u, second_load_entries.size());
}

TEST(ProtoDatabaseImplLevelDBTest, TestOnMemoryDumpEmitsData) {
  base::MessageLoop main_loop;
  std::unique_ptr<LevelDB> db(new LevelDB(kTestLevelDBClientName));
  std::vector<std::string> load_entries;
  ASSERT_TRUE(db->Init(base::FilePath()));
  KeyValueVector save_entries(1, std::make_pair("foo", "bar"));
  KeyVector remove_keys;
  ASSERT_TRUE(db->Save(save_entries, remove_keys));

  base::trace_event::MemoryDumpArgs dump_args = {
      base::trace_event::MemoryDumpLevelOfDetail::DETAILED};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump(
      new base::trace_event::ProcessMemoryDump(nullptr, dump_args));
  db->OnMemoryDump(dump_args, process_memory_dump.get());

  const auto& allocator_dumps = process_memory_dump->allocator_dumps();
  const char* system_allocator_pool_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  size_t expected_dump_count = system_allocator_pool_name ? 2 : 1;
  EXPECT_EQ(expected_dump_count, allocator_dumps.size());
  for (const auto& dump : allocator_dumps) {
    ASSERT_TRUE(dump.first.find("leveldb/leveldb_proto/") == 0 ||
                dump.first.find(system_allocator_pool_name) == 0);
  }
}

}  // namespace leveldb_proto
