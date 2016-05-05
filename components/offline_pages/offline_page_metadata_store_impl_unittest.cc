// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_metadata_store_impl.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_metadata_store_sql.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/proto/offline_pages.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::ProtoDatabaseImpl;

namespace offline_pages {

namespace {

const char kTestClientNamespace[] = "CLIENT_NAMESPACE";
const char kTestURL[] = "https://example.com";
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "5678");
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/offline_pages/example_com.mhtml");
int64_t kFileSize = 234567;

class OfflinePageMetadataStoreFactory {
 public:
  virtual OfflinePageMetadataStore* BuildStore(const base::FilePath& file) = 0;
};

class OfflinePageMetadataStoreImplFactory
    : public OfflinePageMetadataStoreFactory {
 public:
  OfflinePageMetadataStore* BuildStore(const base::FilePath& file) override {
    return new OfflinePageMetadataStoreImpl(base::ThreadTaskRunnerHandle::Get(),
                                            file);
  }
};

class OfflinePageMetadataStoreSQLFactory
    : public OfflinePageMetadataStoreFactory {
 public:
  OfflinePageMetadataStore* BuildStore(const base::FilePath& file) override {
    OfflinePageMetadataStoreSQL* store = new OfflinePageMetadataStoreSQL(
        base::ThreadTaskRunnerHandle::Get(), file);
    return store;
  }
};

enum CalledCallback { NONE, LOAD, ADD, REMOVE, DESTROY };
enum Status { STATUS_NONE, STATUS_TRUE, STATUS_FALSE };

class OfflinePageMetadataStoreTestBase : public testing::Test {
 public:
  OfflinePageMetadataStoreTestBase();
  ~OfflinePageMetadataStoreTestBase() override;

  void TearDown() override {
    // Wait for all the pieces of the store to delete itself properly.
    PumpLoop();
  }

  std::unique_ptr<OfflinePageMetadataStoreImpl> BuildStore();
  void PumpLoop();

  void LoadCallback(OfflinePageMetadataStore::LoadStatus load_status,
                    const std::vector<OfflinePageItem>& offline_pages);
  void UpdateCallback(CalledCallback called_callback, bool success);

  void ClearResults();

 protected:
  CalledCallback last_called_callback_;
  Status last_status_;
  std::vector<OfflinePageItem> offline_pages_;

  base::ScopedTempDir temp_directory_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
};

OfflinePageMetadataStoreTestBase::OfflinePageMetadataStoreTestBase()
    : last_called_callback_(NONE),
      last_status_(STATUS_NONE),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

OfflinePageMetadataStoreTestBase::~OfflinePageMetadataStoreTestBase() {}

void OfflinePageMetadataStoreTestBase::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void OfflinePageMetadataStoreTestBase::LoadCallback(
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  last_called_callback_ = LOAD;
  last_status_ = load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED ?
      STATUS_TRUE : STATUS_FALSE;
  offline_pages_.swap(const_cast<std::vector<OfflinePageItem>&>(offline_pages));
}

void OfflinePageMetadataStoreTestBase::UpdateCallback(
    CalledCallback called_callback,
    bool status) {
  last_called_callback_ = called_callback;
  last_status_ = status ? STATUS_TRUE : STATUS_FALSE;
}

void OfflinePageMetadataStoreTestBase::ClearResults() {
  last_called_callback_ = NONE;
  last_status_ = STATUS_NONE;
  offline_pages_.clear();
}

template <typename T>
class OfflinePageMetadataStoreTest : public OfflinePageMetadataStoreTestBase {
 public:
  std::unique_ptr<OfflinePageMetadataStore> BuildStore();

 protected:
  T factory_;
};

template <typename T>
std::unique_ptr<OfflinePageMetadataStore>
OfflinePageMetadataStoreTest<T>::BuildStore() {
  std::unique_ptr<OfflinePageMetadataStore> store(
      factory_.BuildStore(temp_directory_.path()));
  store->Load(base::Bind(&OfflinePageMetadataStoreTestBase::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  return store;
}

typedef testing::Types<OfflinePageMetadataStoreImplFactory,
                       OfflinePageMetadataStoreSQLFactory>
    MyTypes;
TYPED_TEST_CASE(OfflinePageMetadataStoreTest, MyTypes);

// Loads empty store and makes sure that there are no offline pages stored in
// it.
TYPED_TEST(OfflinePageMetadataStoreTest, LoadEmptyStore) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());
  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(0U, this->offline_pages_.size());
}

// Adds metadata of an offline page into a store and then opens the store
// again to make sure that stored metadata survives store restarts.
TYPED_TEST(OfflinePageMetadataStoreTest, AddOfflinePage) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
  this->PumpLoop();
  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Close the store first to ensure file lock is removed.
  store.reset();
  store = this->BuildStore();
  this->PumpLoop();

  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(1U, this->offline_pages_.size());
  EXPECT_EQ(offline_page.url, this->offline_pages_[0].url);
  EXPECT_EQ(offline_page.offline_id, this->offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page.version, this->offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, this->offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, this->offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, this->offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time,
            this->offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.access_count, this->offline_pages_[0].access_count);
  EXPECT_EQ(offline_page.client_id, this->offline_pages_[0].client_id);
}

// Tests removing offline page metadata from the store, for which it first adds
// metadata of an offline page.
TYPED_TEST(OfflinePageMetadataStoreTest, RemoveOfflinePage) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
  this->PumpLoop();
  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTestBase::LoadCallback,
                         base::Unretained(this)));
  this->PumpLoop();
  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(1U, this->offline_pages_.size());

  // Remove the offline page.
  std::vector<int64_t> ids_to_remove;
  ids_to_remove.push_back(offline_page.offline_id);
  store->RemoveOfflinePages(
      ids_to_remove,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), REMOVE));
  this->PumpLoop();
  EXPECT_EQ(REMOVE, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTestBase::LoadCallback,
                         base::Unretained(this)));
  this->PumpLoop();
  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(0U, this->offline_pages_.size());

  this->ClearResults();

  // Close and reload the store.
  store.reset();
  store = this->BuildStore();
  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(0U, this->offline_pages_.size());
}

// Adds metadata of multiple offline pages into a store and removes some.
TYPED_TEST(OfflinePageMetadataStoreTest, AddRemoveMultipleOfflinePages) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page_1(GURL(kTestURL), 12345LL, kTestClientId1,
                                 base::FilePath(kFilePath), kFileSize);
  base::FilePath file_path_2 =
      base::FilePath(FILE_PATH_LITERAL("//other.page.com.mhtml"));
  OfflinePageItem offline_page_2(GURL("https://other.page.com"), 5678LL,
                                 kTestClientId2, file_path_2, 12345,
                                 base::Time::Now());
  store->AddOrUpdateOfflinePage(
      offline_page_1,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
  this->PumpLoop();
  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Add anther offline page.
  store->AddOrUpdateOfflinePage(
      offline_page_2,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
  this->PumpLoop();
  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreTestBase::LoadCallback,
                         base::Unretained(this)));
  this->PumpLoop();

  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(2U, this->offline_pages_.size());

  // Remove the offline page.
  std::vector<int64_t> ids_to_remove;
  ids_to_remove.push_back(offline_page_1.offline_id);
  store->RemoveOfflinePages(
      ids_to_remove,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), REMOVE));
  this->PumpLoop();
  EXPECT_EQ(REMOVE, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Close and reload the store.
  store.reset();
  store = this->BuildStore();
  store->Load(base::Bind(&OfflinePageMetadataStoreTestBase::LoadCallback,
                         base::Unretained(this)));
  this->PumpLoop();

  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(1U, this->offline_pages_.size());
  EXPECT_EQ(offline_page_2.url, this->offline_pages_[0].url);
  EXPECT_EQ(offline_page_2.offline_id, this->offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page_2.version, this->offline_pages_[0].version);
  EXPECT_EQ(offline_page_2.file_path, this->offline_pages_[0].file_path);
  EXPECT_EQ(offline_page_2.file_size, this->offline_pages_[0].file_size);
  EXPECT_EQ(offline_page_2.creation_time,
            this->offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page_2.last_access_time,
            this->offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page_2.access_count, this->offline_pages_[0].access_count);
  EXPECT_EQ(offline_page_2.client_id, this->offline_pages_[0].client_id);
}

// Tests updating offline page metadata from the store.
TYPED_TEST(OfflinePageMetadataStoreTest, UpdateOfflinePage) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());

  // First, adds a fresh page.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
  this->PumpLoop();
  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();
  store->Load(base::Bind(&OfflinePageMetadataStoreTestBase::LoadCallback,
                         base::Unretained(this)));
  this->PumpLoop();

  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(1U, this->offline_pages_.size());
  EXPECT_EQ(offline_page.url, this->offline_pages_[0].url);
  EXPECT_EQ(offline_page.offline_id, this->offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page.version, this->offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, this->offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, this->offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, this->offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time,
            this->offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.access_count, this->offline_pages_[0].access_count);
  EXPECT_EQ(offline_page.client_id, this->offline_pages_[0].client_id);

  // Then update some data.
  offline_page.file_size = kFileSize + 1;
  offline_page.access_count++;
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
  this->PumpLoop();
  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();
  store->Load(base::Bind(&OfflinePageMetadataStoreTestBase::LoadCallback,
                         base::Unretained(this)));
  this->PumpLoop();

  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(1U, this->offline_pages_.size());
  EXPECT_EQ(offline_page.url, this->offline_pages_[0].url);
  EXPECT_EQ(offline_page.offline_id, this->offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page.version, this->offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, this->offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, this->offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, this->offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time,
            this->offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.access_count, this->offline_pages_[0].access_count);
  EXPECT_EQ(offline_page.client_id, this->offline_pages_[0].client_id);
}

}  // namespace

class OfflinePageMetadataStoreImplTest
    : public OfflinePageMetadataStoreTest<OfflinePageMetadataStoreImplFactory> {
 public:
  void UpdateStoreEntries(
      OfflinePageMetadataStoreImpl* store,
      std::unique_ptr<leveldb_proto::ProtoDatabase<
          OfflinePageEntry>::KeyEntryVector> entries_to_save);
};

void OfflinePageMetadataStoreImplTest::UpdateStoreEntries(
    OfflinePageMetadataStoreImpl* store,
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        OfflinePageEntry>::KeyEntryVector> entries_to_save) {
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  store->UpdateEntries(
      std::move(entries_to_save), std::move(keys_to_remove),
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
}

// Test that loading a store with a bad value still loads.
// Needs to be outside of the anonymous namespace in order for FRIEND_TEST
// to work.
TEST_F(OfflinePageMetadataStoreImplTest, LoadCorruptedStore) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());

  // Write one ok page.
  OfflinePageItem offline_page(GURL(kTestURL), 1234LL, kTestClientId1,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreTestBase::UpdateCallback,
                 base::Unretained(this), ADD));
  this->PumpLoop();
  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  // Manually write one broken page (no id)
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
      entries_to_save(
          new leveldb_proto::ProtoDatabase<OfflinePageEntry>::KeyEntryVector());

  OfflinePageEntry offline_page_proto;
  entries_to_save->push_back(std::make_pair("0", offline_page_proto));

  UpdateStoreEntries((OfflinePageMetadataStoreImpl*)store.get(),
                     std::move(entries_to_save));
  this->PumpLoop();

  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Close the store first to ensure file lock is removed.
  store.reset();
  store = this->BuildStore();
  this->PumpLoop();

  // One of the pages was busted, so only expect one page.
  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);
  EXPECT_EQ(1U, this->offline_pages_.size());
  EXPECT_EQ(offline_page.url, this->offline_pages_[0].url);
  EXPECT_EQ(offline_page.offline_id, this->offline_pages_[0].offline_id);
  EXPECT_EQ(offline_page.version, this->offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, this->offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, this->offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, this->offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time,
            this->offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.access_count, this->offline_pages_[0].access_count);
}

// Test that loading a store with nothing but bad values errors.
// Needs to be outside of the anonymous namespace in order for FRIEND_TEST
// to work.
TEST_F(OfflinePageMetadataStoreImplTest, LoadTotallyCorruptedStore) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());

  // Manually write two broken pages (no id)
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
      entries_to_save(
          new leveldb_proto::ProtoDatabase<OfflinePageEntry>::KeyEntryVector());

  OfflinePageEntry offline_page_proto;
  entries_to_save->push_back(std::make_pair("0", offline_page_proto));
  entries_to_save->push_back(std::make_pair("1", offline_page_proto));

  UpdateStoreEntries((OfflinePageMetadataStoreImpl*)store.get(),
                     std::move(entries_to_save));
  ;
  this->PumpLoop();

  EXPECT_EQ(ADD, this->last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, this->last_status_);

  this->ClearResults();

  // Close the store first to ensure file lock is removed.
  store.reset();
  store = this->BuildStore();
  this->PumpLoop();

  // One of the pages was busted, so only expect one page.
  EXPECT_EQ(LOAD, this->last_called_callback_);
  EXPECT_EQ(STATUS_FALSE, this->last_status_);
}

TEST_F(OfflinePageMetadataStoreImplTest, UpgradeStoreFromBookmarkIdToClientId) {
  std::unique_ptr<OfflinePageMetadataStore> store(this->BuildStore());

  // Manually write a page referring to legacy bookmark id.
  std::unique_ptr<
      leveldb_proto::ProtoDatabase<OfflinePageEntry>::KeyEntryVector>
      entries_to_save(
          new leveldb_proto::ProtoDatabase<OfflinePageEntry>::KeyEntryVector());

  OfflinePageEntry offline_page_proto;
  offline_page_proto.set_deprecated_bookmark_id(1LL);
  offline_page_proto.set_version(1);
  offline_page_proto.set_url(kTestURL);
  offline_page_proto.set_file_path("/foo/bar");
  entries_to_save->push_back(std::make_pair("1", offline_page_proto));

  UpdateStoreEntries((OfflinePageMetadataStoreImpl*)store.get(),
                     std::move(entries_to_save));
  PumpLoop();

  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Close the store first to ensure file lock is removed.
  store.reset();
  store = BuildStore();
  PumpLoop();

  // The page should be upgraded with new Client ID format.
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(1U, offline_pages_.size());
  EXPECT_TRUE(offline_pages_[0].offline_id != 0);
  EXPECT_EQ(offline_pages::kBookmarkNamespace,
            offline_pages_[0].client_id.name_space);
  EXPECT_EQ(base::Int64ToString(offline_page_proto.deprecated_bookmark_id()),
            offline_pages_[0].client_id.id);
  EXPECT_EQ(GURL(kTestURL), offline_pages_[0].url);
  EXPECT_EQ(offline_page_proto.version(), offline_pages_[0].version);
  EXPECT_EQ(offline_page_proto.file_path(),
            offline_pages_[0].file_path.MaybeAsASCII());
}

}  // namespace offline_pages
