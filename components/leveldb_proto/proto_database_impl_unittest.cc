// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/proto_database_impl.h"

#include <map>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "components/leveldb_proto/leveldb_database.h"
#include "components/leveldb_proto/testing/proto/test.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MessageLoop;
using base::ScopedTempDir;
using testing::Invoke;
using testing::Return;
using testing::_;

namespace leveldb_proto {

namespace {

typedef std::map<std::string, TestProto> EntryMap;

class MockDB : public LevelDB {
 public:
  MOCK_METHOD1(Init, bool(const base::FilePath&));
  MOCK_METHOD2(Save, bool(const KeyValueVector&, const KeyVector&));
  MOCK_METHOD1(Load, bool(std::vector<std::string>*));

  MockDB() {
    ON_CALL(*this, Init(_)).WillByDefault(Return(true));
    ON_CALL(*this, Save(_, _)).WillByDefault(Return(true));
    ON_CALL(*this, Load(_)).WillByDefault(Return(true));
  }
};

class MockDatabaseCaller {
 public:
  MOCK_METHOD1(InitCallback, void(bool));
  MOCK_METHOD1(SaveCallback, void(bool));
  void LoadCallback(bool success, scoped_ptr<std::vector<TestProto> > entries) {
    LoadCallback1(success, entries.get());
  }
  MOCK_METHOD2(LoadCallback1, void(bool, std::vector<TestProto>*));
};

}  // namespace

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
  virtual void SetUp() {
    main_loop_.reset(new MessageLoop());
    db_.reset(
        new ProtoDatabaseImpl<TestProto>(main_loop_->message_loop_proxy()));
  }

  virtual void TearDown() {
    db_.reset();
    base::RunLoop().RunUntilIdle();
    main_loop_.reset();
  }

  scoped_ptr<ProtoDatabaseImpl<TestProto> > db_;
  scoped_ptr<MessageLoop> main_loop_;
};

// Test that ProtoDatabaseImpl calls Init on the underlying database and that
// the caller's InitCallback is called with the correct value.
TEST_F(ProtoDatabaseImplTest, TestDBInitSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  EXPECT_CALL(*mock_db, Init(path)).WillOnce(Return(true));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(true));

  db_->InitWithDatabase(
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBInitFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  EXPECT_CALL(*mock_db, Init(path)).WillOnce(Return(false));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(false));

  db_->InitWithDatabase(
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(AppendLoadEntries, model) {
  std::vector<std::string>* output = arg0;
  for (EntryMap::const_iterator it = model.begin(); it != model.end(); ++it) {
    output->push_back(it->second.SerializeAsString());
  }
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
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
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
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Load(_)).WillOnce(Return(false));
  EXPECT_CALL(caller, LoadCallback1(false, _));
  db_->LoadEntries(
      base::Bind(&MockDatabaseCaller::LoadCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(VerifyUpdateEntries, expected) {
  const KeyValueVector actual = arg0;
  // Create a vector of TestProto from |actual| to reuse the comparison
  // function.
  std::vector<TestProto> extracted_entries;
  for (KeyValueVector::const_iterator it = actual.begin(); it != actual.end();
       ++it) {
    TestProto entry;
    entry.ParseFromString(it->second);
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
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  scoped_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  for (EntryMap::iterator it = model.begin(); it != model.end(); ++it) {
    entries->push_back(std::make_pair(it->second.id(), it->second));
  }
  scoped_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(VerifyUpdateEntries(model));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(
      entries.Pass(), keys_to_remove.Pass(),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBSaveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  scoped_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  scoped_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(
      entries.Pass(), keys_to_remove.Pass(),
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
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  scoped_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  scoped_ptr<KeyVector> keys_to_remove(new KeyVector());
  for (EntryMap::iterator it = model.begin(); it != model.end(); ++it) {
    keys_to_remove->push_back(it->second.id());
  }

  KeyVector keys_copy(*keys_to_remove.get());
  EXPECT_CALL(*mock_db, Save(_, keys_copy)).WillOnce(Return(true));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(
      entries.Pass(), keys_to_remove.Pass(),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ProtoDatabaseImplTest, TestDBRemoveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  scoped_ptr<ProtoDatabase<TestProto>::KeyEntryVector> entries(
      new ProtoDatabase<TestProto>::KeyEntryVector());
  scoped_ptr<KeyVector> keys_to_remove(new KeyVector());

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<LevelDB>(mock_db), base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(
      entries.Pass(), keys_to_remove.Pass(),
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

  scoped_ptr<ProtoDatabaseImpl<TestProto> > db(
      new ProtoDatabaseImpl<TestProto>(db_thread.message_loop_proxy()));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(_));
  db->Init(temp_dir.path(), base::Bind(&MockDatabaseCaller::InitCallback,
                                       base::Unretained(&caller)));

  db.reset();

  base::RunLoop run_loop;
  db_thread.message_loop_proxy()->PostTaskAndReply(
      FROM_HERE, base::Bind(base::DoNothing), run_loop.QuitClosure());
  run_loop.Run();
}

// Test that the LevelDB properly saves entries and that load returns the saved
// entries. If |close_after_save| is true, the database will be closed after
// saving and then re-opened to ensure that the data is properly persisted.
void TestLevelDBSaveAndLoad(bool close_after_save) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  EntryMap model = GetSmallModel();

  KeyValueVector save_entries;
  std::vector<std::string> load_entries;
  KeyVector remove_keys;

  for (EntryMap::iterator it = model.begin(); it != model.end(); ++it) {
    save_entries.push_back(
        std::make_pair(it->second.id(), it->second.SerializeAsString()));
  }

  scoped_ptr<LevelDB> db(new LevelDB());
  EXPECT_TRUE(db->Init(temp_dir.path()));
  EXPECT_TRUE(db->Save(save_entries, remove_keys));

  if (close_after_save) {
    db.reset(new LevelDB());
    EXPECT_TRUE(db->Init(temp_dir.path()));
  }

  EXPECT_TRUE(db->Load(&load_entries));
  // Convert the strings back to TestProto.
  std::vector<TestProto> loaded_protos;
  for (std::vector<std::string>::iterator it = load_entries.begin();
       it != load_entries.end(); ++it) {
    TestProto entry;
    entry.ParseFromString(*it);
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

}  // namespace leveldb_proto
