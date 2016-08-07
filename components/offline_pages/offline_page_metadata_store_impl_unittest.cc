// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_metadata_store.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store_sql.h"
#include "components/offline_pages/offline_page_model.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

#define OFFLINE_PAGES_TABLE_V1 "offlinepages_v1"

const char kTestClientNamespace[] = "CLIENT_NAMESPACE";
const char kTestURL[] = "https://example.com";
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "5678");
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/offline_pages/example_com.mhtml");
int64_t kFileSize = 234567LL;
int64_t kOfflineId = 12345LL;

// Build a store with outdated schema to simulate the upgrading process.
// TODO(romax): move it to sql_unittests.
void BuildTestStoreWithSchemaFromM52(const base::FilePath& file) {
  sql::Connection connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("OfflinePages.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute("CREATE TABLE " OFFLINE_PAGES_TABLE_V1
                                 "(offline_id INTEGER PRIMARY KEY NOT NULL, "
                                 "creation_time INTEGER NOT NULL, "
                                 "file_size INTEGER NOT NULL, "
                                 "version INTEGER NOT NULL, "
                                 "last_access_time INTEGER NOT NULL, "
                                 "access_count INTEGER NOT NULL, "
                                 "status INTEGER NOT NULL DEFAULT 0, "
                                 "user_initiated INTEGER, "
                                 "client_namespace VARCHAR NOT NULL, "
                                 "client_id VARCHAR NOT NULL, "
                                 "online_url VARCHAR NOT NULL, "
                                 "offline_url VARCHAR NOT NULL DEFAULT '', "
                                 "file_path VARCHAR NOT NULL "
                                 ")"));
  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT INTO " OFFLINE_PAGES_TABLE_V1
      "(offline_id, creation_time, file_size, version, "
      "last_access_time, access_count, client_namespace, "
      "client_id, online_url, file_path) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, kOfflineId);
  statement.BindInt(1, 0);
  statement.BindInt64(2, kFileSize);
  statement.BindInt(3, 0);
  statement.BindInt(4, 0);
  statement.BindInt(5, 1);
  statement.BindCString(6, kTestClientNamespace);
  statement.BindString(7, kTestClientId2.id);
  statement.BindCString(8, kTestURL);
  statement.BindString(9, base::FilePath(kFilePath).MaybeAsASCII());
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(OFFLINE_PAGES_TABLE_V1));
  ASSERT_FALSE(
      connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "expiration_time"));
}

void BuildTestStoreWithSchemaFromM53(const base::FilePath& file) {
  sql::Connection connection;
  ASSERT_TRUE(
      connection.Open(file.Append(FILE_PATH_LITERAL("OfflinePages.db"))));
  ASSERT_TRUE(connection.is_open());
  ASSERT_TRUE(connection.BeginTransaction());
  ASSERT_TRUE(connection.Execute("CREATE TABLE " OFFLINE_PAGES_TABLE_V1
                                 "(offline_id INTEGER PRIMARY KEY NOT NULL, "
                                 "creation_time INTEGER NOT NULL, "
                                 "file_size INTEGER NOT NULL, "
                                 "version INTEGER NOT NULL, "
                                 "last_access_time INTEGER NOT NULL, "
                                 "access_count INTEGER NOT NULL, "
                                 "status INTEGER NOT NULL DEFAULT 0, "
                                 "user_initiated INTEGER, "
                                 "expiration_time INTEGER NOT NULL DEFAULT 0, "
                                 "client_namespace VARCHAR NOT NULL, "
                                 "client_id VARCHAR NOT NULL, "
                                 "online_url VARCHAR NOT NULL, "
                                 "offline_url VARCHAR NOT NULL DEFAULT '', "
                                 "file_path VARCHAR NOT NULL "
                                 ")"));
  ASSERT_TRUE(connection.CommitTransaction());
  sql::Statement statement(connection.GetUniqueStatement(
      "INSERT INTO " OFFLINE_PAGES_TABLE_V1
      "(offline_id, creation_time, file_size, version, "
      "last_access_time, access_count, client_namespace, "
      "client_id, online_url, file_path, expiration_time) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
  statement.BindInt64(0, kOfflineId);
  statement.BindInt(1, 0);
  statement.BindInt64(2, kFileSize);
  statement.BindInt(3, 0);
  statement.BindInt(4, 0);
  statement.BindInt(5, 1);
  statement.BindCString(6, kTestClientNamespace);
  statement.BindString(7, kTestClientId2.id);
  statement.BindCString(8, kTestURL);
  statement.BindString(9, base::FilePath(kFilePath).MaybeAsASCII());
  statement.BindInt64(10, base::Time::Now().ToInternalValue());
  ASSERT_TRUE(statement.Run());
  ASSERT_TRUE(connection.DoesTableExist(OFFLINE_PAGES_TABLE_V1));
  ASSERT_FALSE(connection.DoesColumnExist(OFFLINE_PAGES_TABLE_V1, "title"));
}

class OfflinePageMetadataStoreFactory {
 public:
  OfflinePageMetadataStore* BuildStore(const base::FilePath& file) {
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file);
    return store;
  }

  OfflinePageMetadataStore* BuildStoreM52(const base::FilePath& file) {
    BuildTestStoreWithSchemaFromM52(file);
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file);
    return store;
  }

  OfflinePageMetadataStore* BuildStoreM53(const base::FilePath& file) {
    BuildTestStoreWithSchemaFromM53(file);
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file);
    return store;
  }
};

enum CalledCallback { NONE, LOAD, ADD, REMOVE, RESET };
enum Status { STATUS_NONE, STATUS_TRUE, STATUS_FALSE };

class OfflinePageMetadataStoreTest : public testing::Test {
 public:
  OfflinePageMetadataStoreTest();
  ~OfflinePageMetadataStoreTest() override;

  void TearDown() override {
    // Wait for all the pieces of the store to delete itself properly.
    PumpLoop();
  }

  std::unique_ptr<OfflinePageMetadataStore> BuildStore();
  std::unique_ptr<OfflinePageMetadataStore> BuildStoreWithSchemaFromM52();
  std::unique_ptr<OfflinePageMetadataStore> BuildStoreWithSchemaFromM53();

  void PumpLoop();

  void LoadCallback(OfflinePageMetadataStore::LoadStatus load_status,
                    const std::vector<OfflinePageItem>& offline_pages);
  void UpdateCallback(CalledCallback called_callback, bool success);

  void ClearResults();

  OfflinePageItem CheckThatStoreHasOneItem();
  void CheckThatOfflinePageCanBeSaved(
      std::unique_ptr<OfflinePageMetadataStore> store);

 protected:
  CalledCallback last_called_callback_;
  Status last_status_;
  std::vector<OfflinePageItem> offline_pages_;
  OfflinePageMetadataStoreFactory factory_;

  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

OfflinePageMetadataStoreTest::OfflinePageMetadataStoreTest()
    : last_called_callback_(NONE),
      last_status_(STATUS_NONE),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

OfflinePageMetadataStoreTest::~OfflinePageMetadataStoreTest() {}

void OfflinePageMetadataStoreTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflinePageMetadataStoreTest::LoadCallback(
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  last_called_callback_ = LOAD;
  last_status_ = load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED ?
      STATUS_TRUE : STATUS_FALSE;
  offline_pages_.swap(const_cast<std::vector<OfflinePageItem>&>(offline_pages));
}

void OfflinePageMetadataStoreTest::UpdateCallback(
    CalledCallback called_callback,
    bool status) {
  last_called_callback_ = called_callback;
  last_status_ = status ? STATUS_TRUE : STATUS_FALSE;
}

void OfflinePageMetadataStoreTest::ClearResults() {
  last_called_callback_ = NONE;
  last_status_ = STATUS_NONE;
  offline_pages_.clear();
}

OfflinePageItem OfflinePageMetadataStoreTest::CheckThatStoreHasOneItem() {
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(1U, offline_pages_.size());

  return offline_pages_[0];
}

void OfflinePageMetadataStoreTest::CheckThatOfflinePageCanBeSaved(
    std::unique_ptr<OfflinePageMetadataStore> store) {
  size_t store_size = offline_pages_.size();
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  offline_page.title = base::UTF8ToUTF16("a title");
  base::Time expiration_time = base::Time::Now();
  offline_page.expiration_time = expiration_time;

  store->AddOrUpdateOfflinePage(
      offline_page, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                               base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ClearResults();

  // Close the store first to ensure file lock is removed.
  store.reset();
  store = BuildStore();
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_EQ(store_size + 1, offline_pages_.size());
  if (store_size > 0 &&
      offline_pages_[0].offline_id != offline_page.offline_id) {
    std::swap(offline_pages_[0], offline_pages_[1]);
  }
  EXPECT_EQ(offline_page.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page.offline_id, offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.last_access_time, offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.expiration_time, offline_pages_[0].expiration_time);
  EXPECT_EQ(offline_page.title, offline_pages_[0].title);
  EXPECT_EQ(offline_page.client_id, offline_pages_[0].client_id);
  EXPECT_EQ(1234LL, offline_pages_[0].offline_id);
  EXPECT_EQ(kFileSize, offline_pages_[0].file_size);
  EXPECT_EQ(kTestClientId1, offline_pages_[0].client_id);
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStore() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStore(temp_directory_.path()));
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  return store;
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStoreWithSchemaFromM52() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStoreM52(temp_directory_.path()));
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  return store;
}

std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest::BuildStoreWithSchemaFromM53() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStoreM53(temp_directory_.path()));
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  return store;
}

// Loads empty store and makes sure that there are no offline pages stored in
// it.
TEST_F(OfflinePageMetadataStoreTest, LoadEmptyStore) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(0U, offline_pages_.size());
}

// Loads a store which has an outdated schema.
// This test case would crash if it's not handling correctly when we're loading
// old version stores.
// TODO(romax): Move this to sql_unittest.
TEST_F(OfflinePageMetadataStoreTest, LoadVersion52Store) {
  std::unique_ptr<OfflinePageMetadataStore> store(
      BuildStoreWithSchemaFromM52());

  CheckThatStoreHasOneItem();
  CheckThatOfflinePageCanBeSaved(std::move(store));
}

// Loads a store which has an outdated schema.
// This test case would crash if it's not handling correctly when we're loading
// old version stores.
// TODO(romax): Move this to sql_unittest.
TEST_F(OfflinePageMetadataStoreTest, LoadVersion53Store) {
  std::unique_ptr<OfflinePageMetadataStore> store(
      BuildStoreWithSchemaFromM53());

  OfflinePageItem item = CheckThatStoreHasOneItem();
  // We should have a valid expiration time after upgrade.
  EXPECT_NE(base::Time::FromInternalValue(0),
            offline_pages_[0].expiration_time);

  CheckThatOfflinePageCanBeSaved(std::move(store));
}

// Adds metadata of an offline page into a store and then opens the store
// again to make sure that stored metadata survives store restarts.
TEST_F(OfflinePageMetadataStoreTest, AddOfflinePage) {
  CheckThatOfflinePageCanBeSaved(BuildStore());
}

// Tests removing offline page metadata from the store, for which it first adds
// metadata of an offline page.
TEST_F(OfflinePageMetadataStoreTest, RemoveOfflinePage) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                               base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(1U, offline_pages_.size());

  // Remove the offline page.
  std::vector<int64_t> ids_to_remove;
  ids_to_remove.push_back(offline_page.offline_id);
  store->RemoveOfflinePages(
      ids_to_remove, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                base::Unretained(this), REMOVE));
  PumpLoop();
  EXPECT_EQ(REMOVE, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0U, offline_pages_.size());

  ClearResults();

  // Close and reload the store.
  store.reset();
  store = BuildStore();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(0U, offline_pages_.size());
}

// Adds metadata of multiple offline pages into a store and removes some.
TEST_F(OfflinePageMetadataStoreTest, AddRemoveMultipleOfflinePages) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page_1(GURL(kTestURL), 12345LL, kTestClientId1,
                                 base::FilePath(kFilePath), kFileSize);
  base::FilePath file_path_2 =
      base::FilePath(FILE_PATH_LITERAL("//other.page.com.mhtml"));
  OfflinePageItem offline_page_2(GURL("https://other.page.com"), 5678LL,
                                 kTestClientId2, file_path_2, 12345,
                                 base::Time::Now());
  offline_page_2.expiration_time = base::Time::Now();
  store->AddOrUpdateOfflinePage(
      offline_page_1, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Add anther offline page.
  store->AddOrUpdateOfflinePage(
      offline_page_2, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(2U, offline_pages_.size());

  // Remove the offline page.
  std::vector<int64_t> ids_to_remove;
  ids_to_remove.push_back(offline_page_1.offline_id);
  store->RemoveOfflinePages(
      ids_to_remove, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                base::Unretained(this), REMOVE));
  PumpLoop();
  EXPECT_EQ(REMOVE, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Close and reload the store.
  store.reset();
  store = BuildStore();
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page_2.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page_2.offline_id, offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page_2.version, offline_pages_[0].version);
  EXPECT_EQ(offline_page_2.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page_2.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page_2.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page_2.last_access_time,
            offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page_2.expiration_time, offline_pages_[0].expiration_time);
  EXPECT_EQ(offline_page_2.access_count, offline_pages_[0].access_count);
  EXPECT_EQ(offline_page_2.client_id, offline_pages_[0].client_id);
}

// Tests updating offline page metadata from the store.
TEST_F(OfflinePageMetadataStoreTest, UpdateOfflinePage) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // First, adds a fresh page.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                               base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page.offline_id, offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page.version, offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time, offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.expiration_time, offline_pages_[0].expiration_time);
  EXPECT_EQ(offline_page.access_count, offline_pages_[0].access_count);
  EXPECT_EQ(offline_page.client_id, offline_pages_[0].client_id);

  // Then update some data.
  offline_page.file_size = kFileSize + 1;
  offline_page.access_count++;
  offline_page.expiration_time = base::Time::Now();
  store->AddOrUpdateOfflinePage(
      offline_page, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                               base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page.offline_id, offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page.version, offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time, offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.expiration_time, offline_pages_[0].expiration_time);
  EXPECT_EQ(offline_page.access_count, offline_pages_[0].access_count);
  EXPECT_EQ(offline_page.client_id, offline_pages_[0].client_id);
}

TEST_F(OfflinePageMetadataStoreTest, ClearAllOfflinePages) {
  std::unique_ptr<OfflinePageMetadataStore> store(BuildStore());

  // Add 2 offline pages.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                               base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  OfflinePageItem offline_page2(GURL("http://test.com"), 5678LL, kTestClientId2,
                                base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page2, base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                                base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(2U, offline_pages_.size());

  // Clear all records from the store.
  store->Reset(base::Bind(&OfflinePageMetadataStoreTest::UpdateCallback,
                          base::Unretained(this), RESET));
  PumpLoop();
  EXPECT_EQ(RESET, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  ASSERT_EQ(0U, offline_pages_.size());
}

}  // namespace
}  // namespace offline_pages
