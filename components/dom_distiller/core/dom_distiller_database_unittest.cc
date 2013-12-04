// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_database.h"

#include <map>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "components/dom_distiller/core/article_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MessageLoop;
using base::ScopedTempDir;
using testing::Invoke;
using testing::Return;
using testing::_;

namespace dom_distiller {

namespace {

typedef std::map<std::string, ArticleEntry> EntryMap;

class MockDB : public DomDistillerDatabase::Database {
 public:
  MOCK_METHOD1(Init, bool(const base::FilePath&));
  MOCK_METHOD2(Save, bool(const EntryVector&, const EntryVector&));
  MOCK_METHOD1(Load, bool(EntryVector*));

  MockDB() {
    ON_CALL(*this, Init(_)).WillByDefault(Return(true));
    ON_CALL(*this, Save(_, _)).WillByDefault(Return(true));
    ON_CALL(*this, Load(_)).WillByDefault(Return(true));
  }

  bool LoadEntries(EntryVector* entries);
};

class MockDatabaseCaller {
 public:
  MOCK_METHOD1(InitCallback, void(bool));
  MOCK_METHOD1(SaveCallback, void(bool));
  void LoadCallback(bool success, scoped_ptr<EntryVector> entries) {
    LoadCallback1(success, entries.get());
  }
  MOCK_METHOD2(LoadCallback1, void(bool, EntryVector*));
};

}  // namespace

EntryMap GetSmallModel() {
  EntryMap model;

  model["key0"].set_entry_id("key0");
  model["key0"].add_pages()->set_url("http://foo.com/1");
  model["key0"].add_pages()->set_url("http://foo.com/2");
  model["key0"].add_pages()->set_url("http://foo.com/3");

  model["key1"].set_entry_id("key1");
  model["key1"].add_pages()->set_url("http://bar.com/all");

  model["key2"].set_entry_id("key2");
  model["key2"].add_pages()->set_url("http://baz.com/1");

  return model;
}

void ExpectEntryPointersEquals(EntryMap expected, const EntryVector& actual) {
  EXPECT_EQ(expected.size(), actual.size());
  for (size_t i = 0; i < actual.size(); i++) {
    EntryMap::iterator expected_it =
        expected.find(std::string(actual[i].entry_id()));
    EXPECT_TRUE(expected_it != expected.end());
    std::string serialized_expected = expected_it->second.SerializeAsString();
    std::string serialized_actual = actual[i].SerializeAsString();
    EXPECT_EQ(serialized_expected, serialized_actual);
    expected.erase(expected_it);
  }
}

class DomDistillerDatabaseTest : public testing::Test {
 public:
  virtual void SetUp() {
    main_loop_.reset(new MessageLoop());
    db_.reset(new DomDistillerDatabase(main_loop_->message_loop_proxy()));
  }

  virtual void TearDown() {
    db_.reset();
    base::RunLoop().RunUntilIdle();
    main_loop_.reset();
  }

  scoped_ptr<DomDistillerDatabase> db_;
  scoped_ptr<MessageLoop> main_loop_;
};

// Test that DomDistillerDatabase calls Init on the underlying database and that
// the caller's InitCallback is called with the correct value.
TEST_F(DomDistillerDatabaseTest, TestDBInitSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  EXPECT_CALL(*mock_db, Init(path)).WillOnce(Return(true));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(true));

  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(DomDistillerDatabaseTest, TestDBInitFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  EXPECT_CALL(*mock_db, Init(path)).WillOnce(Return(false));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(false));

  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(AppendLoadEntries, model) {
  EntryVector* output = arg0;
  for (EntryMap::const_iterator it = model.begin(); it != model.end(); ++it) {
    output->push_back(it->second);
  }
  return true;
}

ACTION_P(VerifyLoadEntries, expected) {
  EntryVector* actual = arg1;
  ExpectEntryPointersEquals(expected, *actual);
}

// Test that DomDistillerDatabase calls Load on the underlying database and that
// the caller's LoadCallback is called with the correct success value. Also
// confirms that on success, the expected entries are passed to the caller's
// LoadCallback.
TEST_F(DomDistillerDatabaseTest, TestDBLoadSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Load(_)).WillOnce(AppendLoadEntries(model));
  EXPECT_CALL(caller, LoadCallback1(true, _))
      .WillOnce(VerifyLoadEntries(testing::ByRef(model)));
  db_->LoadEntries(
      base::Bind(&MockDatabaseCaller::LoadCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(DomDistillerDatabaseTest, TestDBLoadFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Load(_)).WillOnce(Return(false));
  EXPECT_CALL(caller, LoadCallback1(false, _));
  db_->LoadEntries(
      base::Bind(&MockDatabaseCaller::LoadCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(VerifyUpdateEntries, expected) {
  const EntryVector& actual = arg0;
  ExpectEntryPointersEquals(expected, actual);
  return true;
}

// Test that DomDistillerDatabase calls Save on the underlying database with the
// correct entries to save and that the caller's SaveCallback is called with the
// correct success value.
TEST_F(DomDistillerDatabaseTest, TestDBSaveSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  scoped_ptr<EntryVector> entries(new EntryVector());
  for (EntryMap::iterator it = model.begin(); it != model.end(); ++it) {
    entries->push_back(it->second);
  }
  scoped_ptr<EntryVector> entries_to_remove(new EntryVector());

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(VerifyUpdateEntries(model));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(
      entries.Pass(),
      entries_to_remove.Pass(),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(DomDistillerDatabaseTest, TestDBSaveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  scoped_ptr<EntryVector> entries(new EntryVector());
  scoped_ptr<EntryVector> entries_to_remove(new EntryVector());

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(
      entries.Pass(),
      entries_to_remove.Pass(),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

ACTION_P(VerifyRemoveEntries, expected) {
  const EntryVector& actual = arg1;
  ExpectEntryPointersEquals(expected, actual);
  return true;
}

// Test that DomDistillerDatabase calls Save on the underlying database with the
// correct entries to delete and that the caller's SaveCallback is called with
// the correct success value.
TEST_F(DomDistillerDatabaseTest, TestDBRemoveSuccess) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  EntryMap model = GetSmallModel();

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  scoped_ptr<EntryVector> entries(new EntryVector());
  scoped_ptr<EntryVector> entries_to_remove(new EntryVector());
  for (EntryMap::iterator it = model.begin(); it != model.end(); ++it) {
    entries_to_remove->push_back(it->second);
  }

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(VerifyRemoveEntries(model));
  EXPECT_CALL(caller, SaveCallback(true));
  db_->UpdateEntries(
      entries.Pass(),
      entries_to_remove.Pass(),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(DomDistillerDatabaseTest, TestDBRemoveFailure) {
  base::FilePath path(FILE_PATH_LITERAL("/fake/path"));

  MockDB* mock_db = new MockDB();
  MockDatabaseCaller caller;
  scoped_ptr<EntryVector> entries(new EntryVector());
  scoped_ptr<EntryVector> entries_to_remove(new EntryVector());

  EXPECT_CALL(*mock_db, Init(_));
  EXPECT_CALL(caller, InitCallback(_));
  db_->InitWithDatabase(
      scoped_ptr<DomDistillerDatabase::Database>(mock_db),
      base::FilePath(path),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

  EXPECT_CALL(*mock_db, Save(_, _)).WillOnce(Return(false));
  EXPECT_CALL(caller, SaveCallback(false));
  db_->UpdateEntries(
      entries.Pass(),
      entries_to_remove.Pass(),
      base::Bind(&MockDatabaseCaller::SaveCallback, base::Unretained(&caller)));

  base::RunLoop().RunUntilIdle();
}


// This tests that normal usage of the real database does not cause any
// threading violations.
TEST(DomDistillerDatabaseThreadingTest, TestDBDestruction) {
  base::MessageLoop main_loop;

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::Thread db_thread("dbthread");
  ASSERT_TRUE(db_thread.Start());

  scoped_ptr<DomDistillerDatabase> db(
      new DomDistillerDatabase(db_thread.message_loop_proxy()));

  MockDatabaseCaller caller;
  EXPECT_CALL(caller, InitCallback(_));
  db->Init(
      temp_dir.path(),
      base::Bind(&MockDatabaseCaller::InitCallback, base::Unretained(&caller)));

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
  EntryVector save_entries;
  EntryVector load_entries;
  EntryVector remove_entries;

  for (EntryMap::iterator it = model.begin(); it != model.end(); ++it) {
    save_entries.push_back(it->second);
  }

  scoped_ptr<DomDistillerDatabase::LevelDB> db(
      new DomDistillerDatabase::LevelDB());
  EXPECT_TRUE(db->Init(temp_dir.path()));
  EXPECT_TRUE(db->Save(save_entries, remove_entries));

  if (close_after_save) {
    db.reset(new DomDistillerDatabase::LevelDB());
    EXPECT_TRUE(db->Init(temp_dir.path()));
  }

  EXPECT_TRUE(db->Load(&load_entries));

  ExpectEntryPointersEquals(model, load_entries);
}

TEST(DomDistillerDatabaseLevelDBTest, TestDBSaveAndLoad) {
  TestLevelDBSaveAndLoad(false);
}

TEST(DomDistillerDatabaseLevelDBTest, TestDBCloseAndReopen) {
  TestLevelDBSaveAndLoad(true);
}

}  // namespace dom_distiller
