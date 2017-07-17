// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/download_store.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/proto/entry.pb.h"
#include "components/download/internal/proto_conversions.h"
#include "components/download/internal/test/entry_utils.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace download {

class DownloadStoreTest : public testing::Test {
 public:
  DownloadStoreTest() : db_(nullptr) {}

  ~DownloadStoreTest() override = default;

  void CreateDatabase() {
    auto db = base::MakeUnique<leveldb_proto::test::FakeDB<protodb::Entry>>(
        &db_entries_);
    db_ = db.get();
    store_.reset(new DownloadStore(
        base::FilePath(FILE_PATH_LITERAL("/test/db/fakepath")), std::move(db)));
  }

  void InitCallback(std::vector<Entry>* loaded_entries,
                    bool success,
                    std::unique_ptr<std::vector<Entry>> entries) {
    loaded_entries->swap(*entries);
  }

  void LoadCallback(std::vector<protodb::Entry>* loaded_entries,
                    bool success,
                    std::unique_ptr<std::vector<protodb::Entry>> entries) {
    loaded_entries->swap(*entries);
  }

  void RecoverCallback(bool success) { hard_recover_result_ = success; }

  MOCK_METHOD1(StoreCallback, void(bool));

  void PrepopulateSampleEntries() {
    Entry item1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
    Entry item2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
    db_entries_.insert(
        std::make_pair(item1.guid, ProtoConversions::EntryToProto(item1)));
    db_entries_.insert(
        std::make_pair(item2.guid, ProtoConversions::EntryToProto(item2)));
  }

 protected:
  std::map<std::string, protodb::Entry> db_entries_;
  leveldb_proto::test::FakeDB<protodb::Entry>* db_;
  std::unique_ptr<DownloadStore> store_;
  base::Optional<bool> hard_recover_result_;

  DISALLOW_COPY_AND_ASSIGN(DownloadStoreTest);
};

TEST_F(DownloadStoreTest, Initialize) {
  PrepopulateSampleEntries();
  CreateDatabase();
  ASSERT_FALSE(store_->IsInitialized());

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);

  ASSERT_TRUE(store_->IsInitialized());
  ASSERT_EQ(2u, preloaded_entries.size());
}

TEST_F(DownloadStoreTest, HardRecover) {
  PrepopulateSampleEntries();
  CreateDatabase();
  ASSERT_FALSE(store_->IsInitialized());

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);

  ASSERT_TRUE(store_->IsInitialized());
  ASSERT_EQ(2u, preloaded_entries.size());

  store_->HardRecover(
      base::Bind(&DownloadStoreTest::RecoverCallback, base::Unretained(this)));

  ASSERT_FALSE(store_->IsInitialized());

  db_->DestroyCallback(true);
  db_->InitCallback(true);

  ASSERT_TRUE(store_->IsInitialized());
  ASSERT_TRUE(hard_recover_result_.has_value());
  ASSERT_TRUE(hard_recover_result_.value());
}

TEST_F(DownloadStoreTest, HardRecoverDestroyFails) {
  PrepopulateSampleEntries();
  CreateDatabase();
  ASSERT_FALSE(store_->IsInitialized());

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);

  ASSERT_TRUE(store_->IsInitialized());
  ASSERT_EQ(2u, preloaded_entries.size());

  store_->HardRecover(
      base::Bind(&DownloadStoreTest::RecoverCallback, base::Unretained(this)));

  ASSERT_FALSE(store_->IsInitialized());

  db_->DestroyCallback(false);

  ASSERT_FALSE(store_->IsInitialized());
  ASSERT_TRUE(hard_recover_result_.has_value());
  ASSERT_FALSE(hard_recover_result_.value());
}

TEST_F(DownloadStoreTest, HardRecoverInitFails) {
  PrepopulateSampleEntries();
  CreateDatabase();
  ASSERT_FALSE(store_->IsInitialized());

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);

  ASSERT_TRUE(store_->IsInitialized());
  ASSERT_EQ(2u, preloaded_entries.size());

  store_->HardRecover(
      base::Bind(&DownloadStoreTest::RecoverCallback, base::Unretained(this)));

  ASSERT_FALSE(store_->IsInitialized());

  db_->DestroyCallback(true);
  db_->InitCallback(false);

  ASSERT_FALSE(store_->IsInitialized());
  ASSERT_TRUE(hard_recover_result_.has_value());
  ASSERT_FALSE(hard_recover_result_.value());
}

TEST_F(DownloadStoreTest, Update) {
  PrepopulateSampleEntries();
  CreateDatabase();

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_TRUE(store_->IsInitialized());
  ASSERT_EQ(2u, preloaded_entries.size());

  Entry item1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry item2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  EXPECT_CALL(*this, StoreCallback(true)).Times(2);
  store_->Update(item1, base::Bind(&DownloadStoreTest::StoreCallback,
                                   base::Unretained(this)));
  db_->UpdateCallback(true);
  store_->Update(item2, base::Bind(&DownloadStoreTest::StoreCallback,
                                   base::Unretained(this)));
  db_->UpdateCallback(true);

  // Query the database directly and check for the entry.
  auto protos = base::MakeUnique<std::vector<protodb::Entry>>();
  db_->LoadEntries(base::Bind(&DownloadStoreTest::LoadCallback,
                              base::Unretained(this), protos.get()));
  db_->LoadCallback(true);
  ASSERT_EQ(4u, protos->size());
  ASSERT_TRUE(test::CompareEntryList(
      {preloaded_entries[0], preloaded_entries[1], item1, item2},
      *ProtoConversions::EntryVectorFromProto(std::move(protos))));
}

TEST_F(DownloadStoreTest, Remove) {
  PrepopulateSampleEntries();
  CreateDatabase();

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_EQ(2u, preloaded_entries.size());

  // Remove the entry.
  EXPECT_CALL(*this, StoreCallback(true)).Times(1);
  store_->Remove(
      preloaded_entries[0].guid,
      base::Bind(&DownloadStoreTest::StoreCallback, base::Unretained(this)));
  db_->UpdateCallback(true);

  // Query the database directly and check for the entry removed.
  auto protos = base::MakeUnique<std::vector<protodb::Entry>>();
  db_->LoadEntries(base::Bind(&DownloadStoreTest::LoadCallback,
                              base::Unretained(this), protos.get()));
  db_->LoadCallback(true);
  ASSERT_EQ(1u, protos->size());
  ASSERT_TRUE(test::CompareEntryList(
      {preloaded_entries[1]},
      *ProtoConversions::EntryVectorFromProto(std::move(protos))));
}

TEST_F(DownloadStoreTest, InitializeFailed) {
  PrepopulateSampleEntries();
  CreateDatabase();

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(false);
  ASSERT_FALSE(store_->IsInitialized());
  ASSERT_TRUE(preloaded_entries.empty());
}

TEST_F(DownloadStoreTest, InitialLoadFailed) {
  PrepopulateSampleEntries();
  CreateDatabase();

  std::vector<Entry> preloaded_entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &preloaded_entries));
  db_->InitCallback(true);
  db_->LoadCallback(false);
  ASSERT_FALSE(store_->IsInitialized());
  ASSERT_TRUE(preloaded_entries.empty());
}

TEST_F(DownloadStoreTest, UnsuccessfulUpdateOrRemove) {
  Entry item1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  CreateDatabase();

  std::vector<Entry> entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_TRUE(store_->IsInitialized());
  ASSERT_TRUE(entries.empty());

  // Update failed.
  EXPECT_CALL(*this, StoreCallback(false)).Times(1);
  store_->Update(item1, base::Bind(&DownloadStoreTest::StoreCallback,
                                   base::Unretained(this)));
  db_->UpdateCallback(false);

  // Remove failed.
  EXPECT_CALL(*this, StoreCallback(false)).Times(1);
  store_->Remove(item1.guid, base::Bind(&DownloadStoreTest::StoreCallback,
                                        base::Unretained(this)));
  db_->UpdateCallback(false);
}

TEST_F(DownloadStoreTest, AddThenRemove) {
  CreateDatabase();

  std::vector<Entry> entries;
  store_->Initialize(base::Bind(&DownloadStoreTest::InitCallback,
                                base::Unretained(this), &entries));
  db_->InitCallback(true);
  db_->LoadCallback(true);
  ASSERT_TRUE(entries.empty());

  Entry item1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry item2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  EXPECT_CALL(*this, StoreCallback(true)).Times(2);
  store_->Update(item1, base::Bind(&DownloadStoreTest::StoreCallback,
                                   base::Unretained(this)));
  db_->UpdateCallback(true);
  store_->Update(item2, base::Bind(&DownloadStoreTest::StoreCallback,
                                   base::Unretained(this)));
  db_->UpdateCallback(true);

  // Query the database directly and check for the entry.
  auto protos = base::MakeUnique<std::vector<protodb::Entry>>();
  db_->LoadEntries(base::Bind(&DownloadStoreTest::LoadCallback,
                              base::Unretained(this), protos.get()));
  db_->LoadCallback(true);
  ASSERT_EQ(2u, protos->size());

  // Remove the entry.
  EXPECT_CALL(*this, StoreCallback(true)).Times(1);
  store_->Remove(item1.guid, base::Bind(&DownloadStoreTest::StoreCallback,
                                        base::Unretained(this)));
  db_->UpdateCallback(true);

  // Query the database directly and check for the entry removed.
  protos->clear();
  db_->LoadEntries(base::Bind(&DownloadStoreTest::LoadCallback,
                              base::Unretained(this), protos.get()));
  db_->LoadCallback(true);
  ASSERT_EQ(1u, protos->size());
  ASSERT_TRUE(test::CompareEntryList(
      {item2}, *ProtoConversions::EntryVectorFromProto(std::move(protos))));
}

}  // namespace download
