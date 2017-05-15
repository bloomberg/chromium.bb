// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/model_impl.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/download/internal/entry.h"
#include "components/download/internal/test/entry_utils.h"
#include "components/download/internal/test/mock_model_client.h"
#include "components/download/internal/test/test_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::_;

namespace download {

namespace {

class DownloadServiceModelImplTest : public testing::Test {
 public:
  DownloadServiceModelImplTest() : store_(nullptr) {}

  ~DownloadServiceModelImplTest() override = default;

  void SetUp() override {
    auto store = base::MakeUnique<test::TestStore>();
    store_ = store.get();
    model_ = base::MakeUnique<ModelImpl>(&client_, std::move(store));
  }

 protected:
  test::MockModelClient client_;
  test::TestStore* store_;
  std::unique_ptr<ModelImpl> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadServiceModelImplTest);
};

}  // namespace

TEST_F(DownloadServiceModelImplTest, SuccessfulLifecycle) {
  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);
  EXPECT_CALL(client_, OnDestroyed(true)).Times(1);

  model_->Initialize();
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());

  model_->Destroy();
  EXPECT_TRUE(store_->destroy_called());
  store_->TriggerDestroy(true);
}

TEST_F(DownloadServiceModelImplTest, SuccessfulInitWithEntries) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry entry2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  std::vector<Entry> entries = {entry1, entry2};

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);

  model_->Initialize();
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  EXPECT_TRUE(test::SuperficialEntryCompare(&entry1, model_->Get(entry1.guid)));
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry2, model_->Get(entry2.guid)));
}

TEST_F(DownloadServiceModelImplTest, BadInit) {
  EXPECT_CALL(client_, OnInitialized(false)).Times(1);

  model_->Initialize();
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(false, base::MakeUnique<std::vector<Entry>>());
}

TEST_F(DownloadServiceModelImplTest, BadDestroy) {
  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);
  EXPECT_CALL(client_, OnDestroyed(false)).Times(1);

  model_->Initialize();
  EXPECT_TRUE(store_->init_called());
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());

  model_->Destroy();
  EXPECT_TRUE(store_->destroy_called());
  store_->TriggerDestroy(false);
}

TEST_F(DownloadServiceModelImplTest, Add) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry entry2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);
  EXPECT_CALL(client_, OnItemAdded(true, entry1.client, entry1.guid)).Times(1);
  EXPECT_CALL(client_, OnItemAdded(false, entry2.client, entry2.guid)).Times(1);

  model_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());

  model_->Add(entry1);
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry1, model_->Get(entry1.guid)));
  EXPECT_TRUE(
      test::SuperficialEntryCompare(&entry1, store_->LastUpdatedEntry()));
  store_->TriggerUpdate(true);

  model_->Add(entry2);
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry2, model_->Get(entry2.guid)));
  EXPECT_TRUE(
      test::SuperficialEntryCompare(&entry2, store_->LastUpdatedEntry()));

  store_->TriggerUpdate(false);
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));
}

TEST_F(DownloadServiceModelImplTest, Update) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());

  Entry entry2(entry1);
  entry2.state = Entry::State::AVAILABLE;

  Entry entry3(entry1);
  entry3.state = Entry::State::ACTIVE;

  std::vector<Entry> entries = {entry1};

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);
  EXPECT_CALL(client_, OnItemUpdated(true, entry1.client, entry1.guid))
      .Times(1);
  EXPECT_CALL(client_, OnItemUpdated(false, entry1.client, entry1.guid))
      .Times(1);

  model_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  model_->Update(entry2);
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry2, model_->Get(entry2.guid)));
  EXPECT_TRUE(
      test::SuperficialEntryCompare(&entry2, store_->LastUpdatedEntry()));
  store_->TriggerUpdate(true);

  model_->Update(entry3);
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry3, model_->Get(entry3.guid)));
  EXPECT_TRUE(
      test::SuperficialEntryCompare(&entry3, store_->LastUpdatedEntry()));

  store_->TriggerUpdate(false);
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry3, model_->Get(entry3.guid)));
}

TEST_F(DownloadServiceModelImplTest, Remove) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry entry2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  std::vector<Entry> entries = {entry1, entry2};

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);
  EXPECT_CALL(client_, OnItemRemoved(true, entry1.client, entry1.guid))
      .Times(1);
  EXPECT_CALL(client_, OnItemRemoved(false, entry2.client, entry2.guid))
      .Times(1);

  model_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  model_->Remove(entry1.guid);
  EXPECT_EQ(entry1.guid, store_->LastRemovedEntry());
  EXPECT_EQ(nullptr, model_->Get(entry1.guid));
  store_->TriggerRemove(true);

  model_->Remove(entry2.guid);
  EXPECT_EQ(entry2.guid, store_->LastRemovedEntry());
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));
  store_->TriggerRemove(false);
}

TEST_F(DownloadServiceModelImplTest, Get) {
  Entry entry = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());

  std::vector<Entry> entries = {entry};

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);

  model_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  EXPECT_TRUE(test::SuperficialEntryCompare(&entry, model_->Get(entry.guid)));
  EXPECT_EQ(nullptr, model_->Get(base::GenerateGUID()));
}

TEST_F(DownloadServiceModelImplTest, PeekEntries) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  Entry entry2 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());
  std::vector<Entry> entries = {entry1, entry2};

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);

  model_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));

  std::vector<Entry*> expected_peek = {&entry1, &entry2};

  EXPECT_TRUE(
      test::SuperficialEntryListCompare(expected_peek, model_->PeekEntries()));
}

TEST_F(DownloadServiceModelImplTest, TestRemoveAfterAdd) {
  Entry entry = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);
  EXPECT_CALL(client_, OnItemAdded(_, _, _)).Times(0);
  EXPECT_CALL(client_, OnItemRemoved(true, entry.client, entry.guid)).Times(1);

  model_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>());

  model_->Add(entry);
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry, model_->Get(entry.guid)));

  model_->Remove(entry.guid);
  EXPECT_EQ(nullptr, model_->Get(entry.guid));

  store_->TriggerUpdate(true);
  store_->TriggerRemove(true);
}

TEST_F(DownloadServiceModelImplTest, TestRemoveAfterUpdate) {
  Entry entry1 = test::BuildEntry(DownloadClient::TEST, base::GenerateGUID());

  Entry entry2(entry1);
  entry2.state = Entry::State::AVAILABLE;

  std::vector<Entry> entries = {entry1};

  InSequence sequence;
  EXPECT_CALL(client_, OnInitialized(true)).Times(1);
  EXPECT_CALL(client_, OnItemUpdated(_, _, _)).Times(0);
  EXPECT_CALL(client_, OnItemRemoved(true, entry1.client, entry1.guid))
      .Times(1);

  model_->Initialize();
  store_->TriggerInit(true, base::MakeUnique<std::vector<Entry>>(entries));
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry1, model_->Get(entry1.guid)));

  model_->Update(entry2);
  EXPECT_TRUE(test::SuperficialEntryCompare(&entry2, model_->Get(entry2.guid)));

  model_->Remove(entry2.guid);
  EXPECT_EQ(nullptr, model_->Get(entry2.guid));

  store_->TriggerUpdate(true);
  store_->TriggerRemove(true);
}

}  // namespace download
