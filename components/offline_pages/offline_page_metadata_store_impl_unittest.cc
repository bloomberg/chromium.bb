// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_metadata_store_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/proto/offline_pages.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::ProtoDatabaseImpl;

namespace offline_pages {

namespace {

const char kTestURL[] = "https://example.com";
const int64 kTestBookmarkId = 1234LL;
const base::FilePath::CharType kFilePath[] =
    FILE_PATH_LITERAL("/offline_pages/example_com.mhtml");
int64 kFileSize = 234567;

class OfflinePageMetadataStoreImplTest : public testing::Test {
 public:
  enum CalledCallback { NONE, LOAD, ADD, REMOVE, DESTROY };
  enum Status { STATUS_NONE, STATUS_TRUE, STATUS_FALSE };

  OfflinePageMetadataStoreImplTest();
  ~OfflinePageMetadataStoreImplTest() override;

  void TearDown() override { message_loop_.RunUntilIdle(); }

  scoped_ptr<OfflinePageMetadataStoreImpl> BuildStore();
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
  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
};

OfflinePageMetadataStoreImplTest::OfflinePageMetadataStoreImplTest()
    : last_called_callback_(NONE), last_status_(STATUS_NONE) {
  EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
}

OfflinePageMetadataStoreImplTest::~OfflinePageMetadataStoreImplTest() {
}

void OfflinePageMetadataStoreImplTest::PumpLoop() {
  base::RunLoop().RunUntilIdle();
}

scoped_ptr<OfflinePageMetadataStoreImpl>
OfflinePageMetadataStoreImplTest::BuildStore() {
  scoped_ptr<OfflinePageMetadataStoreImpl> store(
      new OfflinePageMetadataStoreImpl(message_loop_.task_runner(),
                                       temp_directory_.path()));
  store->Load(base::Bind(&OfflinePageMetadataStoreImplTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  return store.Pass();
}

void OfflinePageMetadataStoreImplTest::LoadCallback(
    OfflinePageMetadataStore::LoadStatus load_status,
    const std::vector<OfflinePageItem>& offline_pages) {
  last_called_callback_ = LOAD;
  last_status_ = load_status == OfflinePageMetadataStore::LOAD_SUCCEEDED ?
      STATUS_TRUE : STATUS_FALSE;
  offline_pages_.swap(const_cast<std::vector<OfflinePageItem>&>(offline_pages));
}

void OfflinePageMetadataStoreImplTest::UpdateCallback(
    CalledCallback called_callback,
    bool status) {
  last_called_callback_ = called_callback;
  last_status_ = status ? STATUS_TRUE : STATUS_FALSE;
}

void OfflinePageMetadataStoreImplTest::ClearResults() {
  last_called_callback_ = NONE;
  last_status_ = STATUS_NONE;
  offline_pages_.clear();
}

// Loads empty store and makes sure that there are no offline pages stored in
// it.
TEST_F(OfflinePageMetadataStoreImplTest, LoadEmptyStore) {
  scoped_ptr<OfflinePageMetadataStoreImpl> store(BuildStore());
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(0U, offline_pages_.size());
}

// Adds metadata of an offline page into a store and then opens the store
// again to make sure that stored metadata survives store restarts.
TEST_F(OfflinePageMetadataStoreImplTest, AddOfflinePage) {
  scoped_ptr<OfflinePageMetadataStoreImpl> store(BuildStore());

  OfflinePageItem offline_page(GURL(kTestURL), kTestBookmarkId,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Close the store first to ensure file lock is removed.
  store.reset();
  store = BuildStore().Pass();
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page.bookmark_id, offline_pages_[0].bookmark_id);
  EXPECT_EQ(offline_page.version, offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time, offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.access_count, offline_pages_[0].access_count);
}

// Tests removing offline page metadata from the store, for which it first adds
// metadata of an offline page.
TEST_F(OfflinePageMetadataStoreImplTest, RemoveOfflinePage) {
  scoped_ptr<OfflinePageMetadataStoreImpl> store(BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page(GURL(kTestURL), kTestBookmarkId,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreImplTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(1U, offline_pages_.size());

  // Remove the offline page.
  std::vector<int64> ids_to_remove;
  ids_to_remove.push_back(offline_page.bookmark_id);
  store->RemoveOfflinePages(
      ids_to_remove,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), REMOVE));
  PumpLoop();
  EXPECT_EQ(REMOVE, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreImplTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(0U, offline_pages_.size());

  ClearResults();

  // Close and reload the store.
  store.reset();
  store = BuildStore().Pass();
  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(0U, offline_pages_.size());
}

// Adds metadata of multiple offline pages into a store and removes some.
TEST_F(OfflinePageMetadataStoreImplTest, AddRemoveMultipleOfflinePages) {
  scoped_ptr<OfflinePageMetadataStoreImpl> store(BuildStore());

  // Add an offline page.
  OfflinePageItem offline_page_1(GURL(kTestURL), kTestBookmarkId,
                                 base::FilePath(kFilePath), kFileSize);
  base::FilePath file_path_2 =
      base::FilePath(FILE_PATH_LITERAL("//other.page.com.mhtml"));
  OfflinePageItem offline_page_2(GURL("https://other.page.com"), 5678LL,
                                 file_path_2, 12345, base::Time::Now());
  store->AddOrUpdateOfflinePage(
      offline_page_1,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Add anther offline page.
  store->AddOrUpdateOfflinePage(
      offline_page_2,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Load the store.
  store->Load(base::Bind(&OfflinePageMetadataStoreImplTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(2U, offline_pages_.size());

  // Remove the offline page.
  std::vector<int64> ids_to_remove;
  ids_to_remove.push_back(offline_page_1.bookmark_id);
  store->RemoveOfflinePages(
      ids_to_remove,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), REMOVE));
  PumpLoop();
  EXPECT_EQ(REMOVE, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();

  // Close and reload the store.
  store.reset();
  store = BuildStore().Pass();
  store->Load(base::Bind(&OfflinePageMetadataStoreImplTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page_2.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page_2.bookmark_id, offline_pages_[0].bookmark_id);
  EXPECT_EQ(offline_page_2.version, offline_pages_[0].version);
  EXPECT_EQ(offline_page_2.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page_2.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page_2.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page_2.last_access_time,
            offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page_2.access_count, offline_pages_[0].access_count);
}

// Tests updating offline page metadata from the store.
TEST_F(OfflinePageMetadataStoreImplTest, UpdateOfflinePage) {
  scoped_ptr<OfflinePageMetadataStoreImpl> store(BuildStore());

  // First, adds a fresh page.
  OfflinePageItem offline_page(GURL(kTestURL), kTestBookmarkId,
                               base::FilePath(kFilePath), kFileSize);
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();
  store->Load(base::Bind(&OfflinePageMetadataStoreImplTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page.bookmark_id, offline_pages_[0].bookmark_id);
  EXPECT_EQ(offline_page.version, offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time, offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.access_count, offline_pages_[0].access_count);

  // Then updates some data.
  offline_page.file_size = kFileSize + 1;
  offline_page.access_count++;
  store->AddOrUpdateOfflinePage(
      offline_page,
      base::Bind(&OfflinePageMetadataStoreImplTest::UpdateCallback,
                 base::Unretained(this), ADD));
  PumpLoop();
  EXPECT_EQ(ADD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);

  ClearResults();
  store->Load(base::Bind(&OfflinePageMetadataStoreImplTest::LoadCallback,
                         base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(LOAD, last_called_callback_);
  EXPECT_EQ(STATUS_TRUE, last_status_);
  EXPECT_EQ(1U, offline_pages_.size());
  EXPECT_EQ(offline_page.url, offline_pages_[0].url);
  EXPECT_EQ(offline_page.bookmark_id, offline_pages_[0].bookmark_id);
  EXPECT_EQ(offline_page.version, offline_pages_[0].version);
  EXPECT_EQ(offline_page.file_path, offline_pages_[0].file_path);
  EXPECT_EQ(offline_page.file_size, offline_pages_[0].file_size);
  EXPECT_EQ(offline_page.creation_time, offline_pages_[0].creation_time);
  EXPECT_EQ(offline_page.last_access_time, offline_pages_[0].last_access_time);
  EXPECT_EQ(offline_page.access_count, offline_pages_[0].access_count);
}

}  // namespace

}  // namespace offline_pages
