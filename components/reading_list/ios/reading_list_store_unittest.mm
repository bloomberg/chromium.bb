// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_store.h"

#include <map>
#include <set>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "components/sync/model/fake_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeModelTypeChangeProcessorObserver {
 public:
  virtual void Put(const std::string& client_tag,
                   std::unique_ptr<syncer::EntityData> entity_data,
                   syncer::MetadataChangeList* metadata_change_list) = 0;

  virtual void Delete(const std::string& client_tag,
                      syncer::MetadataChangeList* metadata_change_list) = 0;
};

class TestModelTypeChangeProcessor
    : public syncer::FakeModelTypeChangeProcessor {
 public:
  void SetObserver(FakeModelTypeChangeProcessorObserver* observer) {
    observer_ = observer;
  }

  void Put(const std::string& client_tag,
           std::unique_ptr<syncer::EntityData> entity_data,
           syncer::MetadataChangeList* metadata_change_list) override {
    observer_->Put(client_tag, std::move(entity_data), metadata_change_list);
  }

  void Delete(const std::string& client_tag,
              syncer::MetadataChangeList* metadata_change_list) override {
    observer_->Delete(client_tag, metadata_change_list);
  }

 private:
  FakeModelTypeChangeProcessorObserver* observer_;
};

class ReadingListStoreTest : public testing::Test,
                             public FakeModelTypeChangeProcessorObserver,
                             public ReadingListStoreDelegate {
 protected:
  ReadingListStoreTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    ClearState();
    reading_list_store_ = base::MakeUnique<ReadingListStore>(
        base::Bind(&syncer::ModelTypeStoreTestUtil::MoveStoreToCallback,
                   base::Passed(&store_)),
        base::Bind(&ReadingListStoreTest::CreateModelTypeChangeProcessor,
                   base::Unretained(this)));
    model_ = base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr);
    reading_list_store_->SetReadingListModel(model_.get(), this);

    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<syncer::ModelTypeChangeProcessor>
  CreateModelTypeChangeProcessor(syncer::ModelType type,
                                 syncer::ModelTypeSyncBridge* service) {
    auto processor = base::MakeUnique<TestModelTypeChangeProcessor>();
    processor->SetObserver(this);
    return std::move(processor);
  }

  void Put(const std::string& storage_key,
           std::unique_ptr<syncer::EntityData> entity_data,
           syncer::MetadataChangeList* metadata_changes) override {
    put_multimap_.insert(std::make_pair(storage_key, std::move(entity_data)));
    put_called_++;
  }

  void Delete(const std::string& storage_key,
              syncer::MetadataChangeList* metadata_changes) override {
    delete_set_.insert(storage_key);
    delete_called_++;
  }

  void AssertCounts(int put_called,
                    int delete_called,
                    int sync_add_called,
                    int sync_remove_called,
                    int sync_merge_called) {
    EXPECT_EQ(put_called, put_called_);
    EXPECT_EQ(delete_called, delete_called_);
    EXPECT_EQ(sync_add_called, sync_add_called_);
    EXPECT_EQ(sync_remove_called, sync_remove_called_);
    EXPECT_EQ(sync_merge_called, sync_merge_called_);
  }

  void ClearState() {
    delete_called_ = 0;
    put_called_ = 0;
    delete_set_.clear();
    put_multimap_.clear();
    sync_add_called_ = 0;
    sync_remove_called_ = 0;
    sync_merge_called_ = 0;
    sync_added_.clear();
    sync_removed_.clear();
    sync_merged_.clear();
  }

  // These three mathods handle callbacks from a ReadingListStore.
  void StoreLoaded(std::unique_ptr<ReadingListEntries> entries) override {}

  // Handle sync events.
  void SyncAddEntry(std::unique_ptr<ReadingListEntry> entry) override {
    sync_add_called_++;
    sync_added_[entry->URL().spec()] = entry->IsRead();
  }

  void SyncRemoveEntry(const GURL& gurl) override {
    sync_remove_called_++;
    sync_removed_.insert(gurl.spec());
  }

  ReadingListEntry* SyncMergeEntry(
      std::unique_ptr<ReadingListEntry> entry) override {
    sync_merge_called_++;
    sync_merged_[entry->URL().spec()] = entry->IsRead();
    return model_->SyncMergeEntry(std::move(entry));
  }

  // In memory model type store needs a MessageLoop.
  base::MessageLoop message_loop_;

  std::unique_ptr<syncer::ModelTypeStore> store_;
  std::unique_ptr<ReadingListModelImpl> model_;
  std::unique_ptr<ReadingListStore> reading_list_store_;
  int put_called_;
  int delete_called_;
  int sync_add_called_;
  int sync_remove_called_;
  int sync_merge_called_;
  std::map<std::string, std::unique_ptr<syncer::EntityData>> put_multimap_;
  std::set<std::string> delete_set_;
  std::map<std::string, bool> sync_added_;
  std::set<std::string> sync_removed_;
  std::map<std::string, bool> sync_merged_;
};

TEST_F(ReadingListStoreTest, CheckEmpties) {
  EXPECT_EQ(0ul, model_->size());
}

TEST_F(ReadingListStoreTest, SaveOneRead) {
  ReadingListEntry entry(GURL("http://read.example.com/"), "read title");
  entry.SetRead(true);
  reading_list_store_->SaveEntry(entry);
  AssertCounts(1, 0, 0, 0, 0);
  syncer::EntityData* data = put_multimap_["http://read.example.com/"].get();
  const sync_pb::ReadingListSpecifics& specifics =
      data->specifics.reading_list();
  EXPECT_EQ(specifics.title(), "read title");
  EXPECT_EQ(specifics.url(), "http://read.example.com/");
  EXPECT_EQ(specifics.status(), sync_pb::ReadingListSpecifics::READ);
}

TEST_F(ReadingListStoreTest, SaveOneUnread) {
  ReadingListEntry entry(GURL("http://unread.example.com/"), "unread title");
  reading_list_store_->SaveEntry(entry);
  AssertCounts(1, 0, 0, 0, 0);
  syncer::EntityData* data = put_multimap_["http://unread.example.com/"].get();
  const sync_pb::ReadingListSpecifics& specifics =
      data->specifics.reading_list();
  EXPECT_EQ(specifics.title(), "unread title");
  EXPECT_EQ(specifics.url(), "http://unread.example.com/");
  EXPECT_EQ(specifics.status(), sync_pb::ReadingListSpecifics::UNREAD);
}

TEST_F(ReadingListStoreTest, SyncMergeOneEntry) {
  syncer::EntityDataMap remote_input;
  ReadingListEntry entry(GURL("http://read.example.com/"), "read title");
  entry.SetRead(true);
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry.AsReadingListSpecifics();

  syncer::EntityData data;
  data.client_tag_hash = "http://read.example.com/";
  *data.specifics.mutable_reading_list() = *specifics;

  remote_input["http://read.example.com/"] = data.PassToPtr();

  std::unique_ptr<syncer::MetadataChangeList> metadata_changes(
      reading_list_store_->CreateMetadataChangeList());
  const syncer::SyncError error = reading_list_store_->MergeSyncData(
      std::move(metadata_changes), remote_input);
  AssertCounts(0, 0, 1, 0, 0);
  EXPECT_EQ(sync_added_.size(), 1u);
  EXPECT_EQ(sync_added_.count("http://read.example.com/"), 1u);
  EXPECT_EQ(sync_added_["http://read.example.com/"], true);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneAdd) {
  syncer::EntityDataMap remote_input;
  ReadingListEntry entry(GURL("http://read.example.com/"), "read title");
  entry.SetRead(true);
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      entry.AsReadingListSpecifics();
  syncer::EntityData data;
  data.client_tag_hash = "http://read.example.com/";
  *data.specifics.mutable_reading_list() = *specifics;

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://read.example.com/", data.PassToPtr()));
  syncer::SyncError error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(), add_changes);
  AssertCounts(0, 0, 1, 0, 0);
  EXPECT_EQ(sync_added_.size(), 1u);
  EXPECT_EQ(sync_added_.count("http://read.example.com/"), 1u);
  EXPECT_EQ(sync_added_["http://read.example.com/"], true);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneMerge) {
  syncer::EntityDataMap remote_input;
  model_->AddEntry(GURL("http://unread.example.com/"), "unread title");
  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromMilliseconds(10));

  ReadingListEntry new_entry(GURL("http://unread.example.com/"),
                             "unread title");
  new_entry.SetRead(true);
  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      new_entry.AsReadingListSpecifics();
  syncer::EntityData data;
  data.client_tag_hash = "http://unread.example.com/";
  *data.specifics.mutable_reading_list() = *specifics;

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://unread.example.com/", data.PassToPtr()));
  syncer::SyncError error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(), add_changes);
  AssertCounts(1, 0, 0, 0, 1);
  EXPECT_EQ(sync_merged_.size(), 1u);
  EXPECT_EQ(sync_merged_.count("http://unread.example.com/"), 1u);
  EXPECT_EQ(sync_merged_["http://unread.example.com/"], true);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneIgnored) {
  // Read entry but with unread URL as it must update the other one.
  ReadingListEntry old_entry(GURL("http://unread.example.com/"),
                             "unread title");
  old_entry.SetRead(true);

  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromMilliseconds(10));
  syncer::EntityDataMap remote_input;
  model_->AddEntry(GURL("http://unread.example.com/"), "unread title");
  AssertCounts(0, 0, 0, 0, 0);

  std::unique_ptr<sync_pb::ReadingListSpecifics> specifics =
      old_entry.AsReadingListSpecifics();
  syncer::EntityData data;
  data.client_tag_hash = "http://unread.example.com/";
  *data.specifics.mutable_reading_list() = *specifics;

  syncer::EntityChangeList add_changes;
  add_changes.push_back(syncer::EntityChange::CreateAdd(
      "http://unread.example.com/", data.PassToPtr()));
  syncer::SyncError error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(), add_changes);
  AssertCounts(1, 0, 0, 0, 0);
  EXPECT_EQ(sync_merged_.size(), 0u);
}

TEST_F(ReadingListStoreTest, ApplySyncChangesOneRemove) {
  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete("http://read.example.com/"));
  syncer::SyncError error = reading_list_store_->ApplySyncChanges(
      reading_list_store_->CreateMetadataChangeList(), delete_changes);
  AssertCounts(0, 0, 0, 1, 0);
  EXPECT_EQ(sync_removed_.size(), 1u);
  EXPECT_EQ(sync_removed_.count("http://read.example.com/"), 1u);
}
