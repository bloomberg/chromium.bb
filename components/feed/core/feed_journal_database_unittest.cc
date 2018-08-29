// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_journal_database.h"

#include <map>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "components/feed/core/proto/journal_storage.pb.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using testing::Mock;
using testing::NotNull;
using testing::_;

namespace feed {

namespace {

const char kJournalKey1[] = "JournalKey1";
const char kJournalKey2[] = "JournalKey2";
const char kJournalData1[] = "Journal Data1";
const char kJournalData2[] = "Journal Data2";
const char kJournalData3[] = "Journal Data3";
const char kJournalData4[] = "Journal Data4";

}  // namespace

class FeedJournalDatabaseTest : public testing::Test {
 public:
  FeedJournalDatabaseTest() : journal_db_(nullptr) {}

  void CreateDatabase(bool init_database) {
    // The FakeDBs are owned by |feed_db_|, so clear our pointers before
    // resetting |feed_db_| itself.
    journal_db_ = nullptr;
    // Explicitly destroy any existing database before creating a new one.
    feed_db_.reset();

    auto storage_db =
        std::make_unique<FakeDB<JournalStorageProto>>(&journal_db_storage_);

    journal_db_ = storage_db.get();
    feed_db_ = std::make_unique<FeedJournalDatabase>(base::FilePath(),
                                                     std::move(storage_db));
    if (init_database) {
      journal_db_->InitCallback(true);
      ASSERT_TRUE(db()->IsInitialized());
    }
  }

  void InjectJournalStorageProto(const std::string& key,
                                 const std::vector<std::string>& entries) {
    JournalStorageProto storage_proto;
    storage_proto.set_key(key);
    for (const std::string& entry : entries) {
      storage_proto.add_journal_data(entry);
    }
    journal_db_storage_[key] = storage_proto;
  }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

  FakeDB<JournalStorageProto>* storage_db() { return journal_db_; }

  FeedJournalDatabase* db() { return feed_db_.get(); }

  MOCK_METHOD1(OnJournalEntryReceived, void(std::vector<std::string>));

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::map<std::string, JournalStorageProto> journal_db_storage_;

  // Owned by |feed_db_|.
  FakeDB<JournalStorageProto>* journal_db_;

  std::unique_ptr<FeedJournalDatabase> feed_db_;

  DISALLOW_COPY_AND_ASSIGN(FeedJournalDatabaseTest);
};

TEST_F(FeedJournalDatabaseTest, Init) {
  ASSERT_FALSE(db());

  CreateDatabase(/*init_database=*/false);

  EXPECT_FALSE(db()->IsInitialized());
  storage_db()->InitCallback(true);
  EXPECT_TRUE(db()->IsInitialized());
}

TEST_F(FeedJournalDatabaseTest, LoadJournalEntry) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Store |kJournalKey1| and |kJournalKey2|.
  InjectJournalStorageProto(kJournalKey1,
                            {kJournalData1, kJournalData2, kJournalData3});
  InjectJournalStorageProto(kJournalKey2, {kJournalData4});

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 3U);
        EXPECT_EQ(results[0], kJournalData1);
        EXPECT_EQ(results[1], kJournalData2);
        EXPECT_EQ(results[2], kJournalData3);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
}

TEST_F(FeedJournalDatabaseTest, LoadNonExistingJournalEntry) {
  CreateDatabase(/*init_database=*/true);
  EXPECT_TRUE(db()->IsInitialized());

  // Try to Load |kJournalKey1|.
  EXPECT_CALL(*this, OnJournalEntryReceived(_))
      .WillOnce([](std::vector<std::string> results) {
        ASSERT_EQ(results.size(), 0U);
      });
  db()->LoadJournal(
      kJournalKey1,
      base::BindOnce(&FeedJournalDatabaseTest::OnJournalEntryReceived,
                     base::Unretained(this)));
  storage_db()->GetCallback(true);
}

}  // namespace feed
