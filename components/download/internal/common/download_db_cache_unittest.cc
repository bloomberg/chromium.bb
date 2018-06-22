// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/download_db_cache.h"

#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/download/database/download_db_conversions.h"
#include "components/download/database/download_db_entry.h"
#include "components/download/database/download_db_impl.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace download {

namespace {

DownloadDBEntry CreateDownloadDBEntry() {
  DownloadDBEntry entry;
  DownloadInfo download_info;
  download_info.guid = base::GenerateGUID();
  entry.download_info = download_info;
  return entry;
}

std::string GetKey(const std::string& guid) {
  return DownloadNamespaceToString(
             DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD) +
         "," + guid;
}

}  // namespace

class DownloadDBCacheTest : public testing::Test {
 public:
  DownloadDBCacheTest()
      : db_(nullptr), task_runner_(new base::TestMockTimeTaskRunner) {}

  ~DownloadDBCacheTest() override = default;

  void CreateDBCache() {
    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<download_pb::DownloadDBEntry>>(
        &db_entries_);
    db_ = db.get();
    auto download_db = std::make_unique<DownloadDBImpl>(
        DownloadNamespace::NAMESPACE_BROWSER_DOWNLOAD,
        base::FilePath(FILE_PATH_LITERAL("/test/db/fakepath")), std::move(db));
    db_cache_ = std::make_unique<DownloadDBCache>(std::move(download_db));
    db_cache_->SetTimerTaskRunnerForTesting(task_runner_);
  }

  void InitCallback(std::vector<DownloadDBEntry>* loaded_entries,
                    bool success,
                    std::unique_ptr<std::vector<DownloadDBEntry>> entries) {
    loaded_entries->swap(*entries);
  }

  void PrepopulateSampleEntries() {
    DownloadDBEntry first = CreateDownloadDBEntry();
    DownloadDBEntry second = CreateDownloadDBEntry();
    DownloadDBEntry third = CreateDownloadDBEntry();
    db_entries_.insert(
        std::make_pair("unknown," + first.GetGuid(),
                       DownloadDBConversions::DownloadDBEntryToProto(first)));
    db_entries_.insert(
        std::make_pair(GetKey(second.GetGuid()),
                       DownloadDBConversions::DownloadDBEntryToProto(second)));
    db_entries_.insert(
        std::make_pair(GetKey(third.GetGuid()),
                       DownloadDBConversions::DownloadDBEntryToProto(third)));
  }

  DownloadDB* GetDownloadDB() { return db_cache_->download_db_.get(); }

 protected:
  std::map<std::string, download_pb::DownloadDBEntry> db_entries_;
  leveldb_proto::test::FakeDB<download_pb::DownloadDBEntry>* db_;
  std::unique_ptr<DownloadDBCache> db_cache_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  DISALLOW_COPY_AND_ASSIGN(DownloadDBCacheTest);
};

TEST_F(DownloadDBCacheTest, InitializeAndRetrieve) {
  PrepopulateSampleEntries();
  CreateDBCache();
  std::vector<DownloadDBEntry> loaded_entries;
  db_cache_->Initialize(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                       base::Unretained(this), &loaded_entries,
                                       true));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 2u);

  for (auto db_entry : loaded_entries) {
    ASSERT_EQ(db_entry, db_cache_->RetrieveEntry(db_entry.GetGuid()));
    ASSERT_EQ(db_entry,
              DownloadDBConversions::DownloadDBEntryFromProto(
                  db_entries_.find(GetKey(db_entry.GetGuid()))->second));
  }
}

// Test that new entry is added immediately to the database
TEST_F(DownloadDBCacheTest, AddNewEntry) {
  PrepopulateSampleEntries();
  CreateDBCache();
  std::vector<DownloadDBEntry> loaded_entries;
  db_cache_->Initialize(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                       base::Unretained(this), &loaded_entries,
                                       true));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 2u);

  DownloadDBEntry new_entry = CreateDownloadDBEntry();
  db_cache_->AddOrReplaceEntry(new_entry);
  ASSERT_EQ(new_entry, db_cache_->RetrieveEntry(new_entry.GetGuid()));
  db_->UpdateCallback(true);
  loaded_entries.clear();
  DownloadDB* download_db = GetDownloadDB();
  download_db->LoadEntries(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                          base::Unretained(this),
                                          &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 3u);
}

// Test that modifying an existing entry could take some time to update the DB.
TEST_F(DownloadDBCacheTest, ModifyExistingEntry) {
  PrepopulateSampleEntries();
  CreateDBCache();
  std::vector<DownloadDBEntry> loaded_entries;
  db_cache_->Initialize(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                       base::Unretained(this), &loaded_entries,
                                       true));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 2u);

  loaded_entries[0].download_info->id = 100;
  loaded_entries[1].download_info->id = 100;
  db_cache_->AddOrReplaceEntry(loaded_entries[0]);
  db_cache_->AddOrReplaceEntry(loaded_entries[1]);

  ASSERT_EQ(task_runner_->GetPendingTaskCount(), 1u);
  ASSERT_GT(task_runner_->NextPendingTaskDelay(), base::TimeDelta());
  task_runner_->FastForwardUntilNoTasksRemain();
  db_->UpdateCallback(true);

  loaded_entries.clear();
  DownloadDB* download_db = GetDownloadDB();
  download_db->LoadEntries(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                          base::Unretained(this),
                                          &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 2u);
  ASSERT_EQ(loaded_entries[0].download_info->id, 100);
  ASSERT_EQ(loaded_entries[1].download_info->id, 100);
}

// Test that modifying current path will immediately update the DB.
TEST_F(DownloadDBCacheTest, FilePathChange) {
  DownloadDBEntry entry = CreateDownloadDBEntry();
  InProgressInfo info;
  base::FilePath test_path = base::FilePath(FILE_PATH_LITERAL("/tmp"));
  info.current_path = test_path;
  entry.download_info->in_progress_info = info;
  db_entries_.insert(
      std::make_pair(GetKey(entry.GetGuid()),
                     DownloadDBConversions::DownloadDBEntryToProto(entry)));
  CreateDBCache();
  std::vector<DownloadDBEntry> loaded_entries;
  db_cache_->Initialize(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                       base::Unretained(this), &loaded_entries,
                                       true));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 1u);
  ASSERT_EQ(loaded_entries[0].download_info->in_progress_info->current_path,
            test_path);

  test_path = base::FilePath(FILE_PATH_LITERAL("/test"));
  loaded_entries[0].download_info->in_progress_info->current_path = test_path;
  db_cache_->AddOrReplaceEntry(loaded_entries[0]);
  db_->UpdateCallback(true);

  loaded_entries.clear();
  DownloadDB* download_db = GetDownloadDB();
  download_db->LoadEntries(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                          base::Unretained(this),
                                          &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 1u);
  ASSERT_EQ(loaded_entries[0].download_info->in_progress_info->current_path,
            test_path);
}

TEST_F(DownloadDBCacheTest, RemoveEntry) {
  PrepopulateSampleEntries();
  CreateDBCache();
  std::vector<DownloadDBEntry> loaded_entries;
  db_cache_->Initialize(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                       base::Unretained(this), &loaded_entries,
                                       true));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 2u);

  std::string guid = loaded_entries[0].GetGuid();
  std::string guid2 = loaded_entries[1].GetGuid();
  db_cache_->RemoveEntry(loaded_entries[0].GetGuid());
  db_->UpdateCallback(true);
  ASSERT_FALSE(db_cache_->RetrieveEntry(guid));
  ASSERT_TRUE(db_cache_->RetrieveEntry(guid2));

  loaded_entries.clear();
  DownloadDB* download_db = GetDownloadDB();
  download_db->LoadEntries(base::BindOnce(&DownloadDBCacheTest::InitCallback,
                                          base::Unretained(this),
                                          &loaded_entries));
  db_->LoadCallback(true);
  ASSERT_EQ(loaded_entries.size(), 1u);
  ASSERT_EQ(guid2, loaded_entries[0].GetGuid());
}

}  // namespace download
