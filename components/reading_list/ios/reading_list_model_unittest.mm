// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_model.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#import "base/test/ios/wait_util.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const GURL callback_url("http://example.com");
const std::string callback_title("test title");

class TestReadingListStorageObserver {
 public:
  virtual void ReadingListDidSaveEntry() = 0;
  virtual void ReadingListDidRemoveEntry() = 0;
};

class TestReadingListStorage : public ReadingListModelStorage {
 public:
  TestReadingListStorage(TestReadingListStorageObserver* observer)
      : read_(new std::vector<ReadingListEntry>()),
        unread_(new std::vector<ReadingListEntry>()),
        observer_(observer) {}

  void AddSampleEntries() {
    // Adds timer and interlace read/unread entry creation to avoid having two
    // entries with the same creation timestamp.
    ReadingListEntry unread_a(GURL("http://unread_a.com"), "unread_a");
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
    ReadingListEntry read_a(GURL("http://read_a.com"), "read_a");
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
    ReadingListEntry unread_b(GURL("http://unread_b.com"), "unread_b");
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
    ReadingListEntry read_b(GURL("http://read_b.com"), "read_b");
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
    ReadingListEntry unread_c(GURL("http://unread_c.com"), "unread_c");
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
    ReadingListEntry read_c(GURL("http://read_c.com"), "read_c");
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
    ReadingListEntry unread_d(GURL("http://unread_d.com"), "unread_d");
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
    read_->push_back(std::move(read_c));
    read_->push_back(std::move(read_a));
    read_->push_back(std::move(read_b));

    unread_->push_back(std::move(unread_a));
    unread_->push_back(std::move(unread_d));
    unread_->push_back(std::move(unread_c));
    unread_->push_back(std::move(unread_b));
  }

  void SetReadingListModel(ReadingListModel* model,
                           ReadingListStoreDelegate* delegate_) override {
    delegate_->StoreLoaded(std::move(unread_), std::move(read_));
  }

  syncer::ModelTypeSyncBridge* GetModelTypeSyncBridge() override {
    return nullptr;
  }

  std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() override {
    return std::unique_ptr<ScopedBatchUpdate>();
  }

  // Saves or updates an entry. If the entry is not yet in the database, it is
  // created.
  void SaveEntry(const ReadingListEntry& entry, bool read) override {
    observer_->ReadingListDidSaveEntry();
  }

  // Removed an entry from the storage.
  void RemoveEntry(const ReadingListEntry& entry) override {
    observer_->ReadingListDidRemoveEntry();
  }

 private:
  std::unique_ptr<std::vector<ReadingListEntry>> read_;
  std::unique_ptr<std::vector<ReadingListEntry>> unread_;
  TestReadingListStorageObserver* observer_;
};

class ReadingListModelTest : public ReadingListModelObserver,
                             public TestReadingListStorageObserver,
                             public testing::Test {
 public:
  ReadingListModelTest()
      : callback_called_(false), model_(new ReadingListModelImpl()) {
    ClearCounts();
    model_->AddObserver(this);
  }
  ~ReadingListModelTest() override {}

  void SetStorage(std::unique_ptr<TestReadingListStorage> storage) {
    model_ =
        base::MakeUnique<ReadingListModelImpl>(std::move(storage), nullptr);
    ClearCounts();
    model_->AddObserver(this);
  }

  void ClearCounts() {
    observer_loaded_ = observer_started_batch_update_ =
        observer_completed_batch_update_ = observer_deleted_ =
            observer_remove_unread_ = observer_remove_read_ = observer_move_ =
                observer_add_unread_ = observer_add_read_ =
                    observer_update_unread_ = observer_update_read_ =
                        observer_did_apply_ = storage_saved_ =
                            storage_removed_ = 0;
  }

  void AssertObserverCount(int observer_loaded,
                           int observer_started_batch_update,
                           int observer_completed_batch_update,
                           int observer_deleted,
                           int observer_remove_unread,
                           int observer_remove_read,
                           int observer_move,
                           int observer_add_unread,
                           int observer_add_read,
                           int observer_update_unread,
                           int observer_update_read,
                           int observer_did_apply) {
    ASSERT_EQ(observer_loaded, observer_loaded_);
    ASSERT_EQ(observer_started_batch_update, observer_started_batch_update_);
    ASSERT_EQ(observer_completed_batch_update,
              observer_completed_batch_update_);
    ASSERT_EQ(observer_deleted, observer_deleted_);
    ASSERT_EQ(observer_remove_unread, observer_remove_unread_);
    ASSERT_EQ(observer_remove_read, observer_remove_read_);
    ASSERT_EQ(observer_move, observer_move_);
    ASSERT_EQ(observer_add_unread, observer_add_unread_);
    ASSERT_EQ(observer_add_read, observer_add_read_);
    ASSERT_EQ(observer_update_unread, observer_update_unread_);
    ASSERT_EQ(observer_update_read, observer_update_read_);
    ASSERT_EQ(observer_did_apply, observer_did_apply_);
  }

  void AssertStorageCount(int storage_saved, int storage_removed) {
    ASSERT_EQ(storage_saved, storage_saved_);
    ASSERT_EQ(storage_removed, storage_removed_);
  }

  // ReadingListModelObserver
  void ReadingListModelLoaded(const ReadingListModel* model) override {
    observer_loaded_ += 1;
  }
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override {
    observer_started_batch_update_ += 1;
  }
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override {
    observer_completed_batch_update_ += 1;
  }
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override {
    observer_deleted_ += 1;
  }
  void ReadingListWillRemoveUnreadEntry(const ReadingListModel* model,
                                        size_t index) override {
    observer_remove_unread_ += 1;
  }
  void ReadingListWillMoveEntry(const ReadingListModel* model,
                                size_t index,
                                bool read) override {
    observer_move_ += 1;
  }
  void ReadingListWillRemoveReadEntry(const ReadingListModel* model,
                                      size_t index) override {
    observer_remove_read_ += 1;
  }
  void ReadingListWillAddUnreadEntry(const ReadingListModel* model,
                                     const ReadingListEntry& entry) override {
    observer_add_unread_ += 1;
  }
  void ReadingListWillAddReadEntry(const ReadingListModel* model,
                                   const ReadingListEntry& entry) override {
    observer_add_read_ += 1;
  }
  void ReadingListWillUpdateUnreadEntry(const ReadingListModel* model,
                                        size_t index) override {
    observer_update_unread_ += 1;
  }
  void ReadingListWillUpdateReadEntry(const ReadingListModel* model,
                                      size_t index) override {
    observer_update_read_ += 1;
  }
  void ReadingListDidApplyChanges(ReadingListModel* model) override {
    observer_did_apply_ += 1;
  }

  void ReadingListDidSaveEntry() override { storage_saved_ += 1; }
  void ReadingListDidRemoveEntry() override { storage_removed_ += 1; }

  void Callback(const ReadingListEntry& entry) {
    EXPECT_EQ(callback_url, entry.URL());
    EXPECT_EQ(callback_title, entry.Title());
    callback_called_ = true;
  }

  bool CallbackCalled() { return callback_called_; }

 protected:
  int observer_loaded_;
  int observer_started_batch_update_;
  int observer_completed_batch_update_;
  int observer_deleted_;
  int observer_remove_unread_;
  int observer_remove_read_;
  int observer_move_;
  int observer_add_unread_;
  int observer_add_read_;
  int observer_update_unread_;
  int observer_update_read_;
  int observer_did_apply_;
  int storage_saved_;
  int storage_removed_;
  bool callback_called_;

  std::unique_ptr<ReadingListModelImpl> model_;
};

TEST_F(ReadingListModelTest, EmptyLoaded) {
  EXPECT_TRUE(model_->loaded());
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(0ul, model_->unread_size());
  EXPECT_EQ(0ul, model_->read_size());
  model_->Shutdown();
  EXPECT_FALSE(model_->loaded());
  AssertObserverCount(1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
}

TEST_F(ReadingListModelTest, ModelLoaded) {
  ClearCounts();
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  storage->AddSampleEntries();
  SetStorage(std::move(storage));

  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(model_->read_size(), 3u);
  EXPECT_EQ(model_->GetReadEntryAtIndex(0).Title(), "read_c");
  EXPECT_EQ(model_->GetReadEntryAtIndex(1).Title(), "read_b");
  EXPECT_EQ(model_->GetReadEntryAtIndex(2).Title(), "read_a");

  EXPECT_EQ(model_->unread_size(), 4u);
  EXPECT_EQ(model_->GetUnreadEntryAtIndex(0).Title(), "unread_d");
  EXPECT_EQ(model_->GetUnreadEntryAtIndex(1).Title(), "unread_c");
  EXPECT_EQ(model_->GetUnreadEntryAtIndex(2).Title(), "unread_b");
  EXPECT_EQ(model_->GetUnreadEntryAtIndex(3).Title(), "unread_a");
}

TEST_F(ReadingListModelTest, AddEntry) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  ClearCounts();

  const ReadingListEntry& entry =
      model_->AddEntry(GURL("http://example.com"), "\n  \tsample Test ");
  EXPECT_EQ(GURL("http://example.com"), entry.URL());
  EXPECT_EQ("sample Test", entry.Title());

  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1);
  AssertStorageCount(1, 0);
  EXPECT_EQ(1ul, model_->unread_size());
  EXPECT_EQ(0ul, model_->read_size());
  EXPECT_TRUE(model_->HasUnseenEntries());

  const ReadingListEntry& other_entry = model_->GetUnreadEntryAtIndex(0);
  EXPECT_EQ(GURL("http://example.com"), other_entry.URL());
  EXPECT_EQ("sample Test", other_entry.Title());
}

TEST_F(ReadingListModelTest, SyncAddEntry) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  auto entry =
      base::MakeUnique<ReadingListEntry>(GURL("http://example.com"), "sample");
  ClearCounts();

  model_->SyncAddEntry(std::move(entry), true);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1);
  AssertStorageCount(0, 0);
  ASSERT_EQ(model_->unread_size(), 0u);
  ASSERT_EQ(model_->read_size(), 1u);
  ClearCounts();
}

TEST_F(ReadingListModelTest, SyncMergeEntry) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  model_->AddEntry(GURL("http://example.com"), "sample");
  model_->SetEntryDistilledPath(GURL("http://example.com"),
                                base::FilePath("distilled/page.html"));
  const ReadingListEntry* local_entry =
      model_->GetEntryFromURL(GURL("http://example.com"), nullptr);
  int64_t local_update_time = local_entry->UpdateTime();

  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromMilliseconds(10));
  auto sync_entry =
      base::MakeUnique<ReadingListEntry>(GURL("http://example.com"), "sample");
  ASSERT_GT(sync_entry->UpdateTime(), local_update_time);
  int64_t sync_update_time = sync_entry->UpdateTime();
  EXPECT_TRUE(sync_entry->DistilledPath().empty());

  EXPECT_EQ(model_->unread_size(), 1u);
  EXPECT_EQ(model_->read_size(), 0u);

  ReadingListEntry* merged_entry =
      model_->SyncMergeEntry(std::move(sync_entry), true);
  EXPECT_EQ(model_->unread_size(), 0u);
  EXPECT_EQ(model_->read_size(), 1u);
  EXPECT_EQ(merged_entry->DistilledPath(),
            base::FilePath("distilled/page.html"));
  EXPECT_EQ(merged_entry->UpdateTime(), sync_update_time);
}

TEST_F(ReadingListModelTest, RemoveEntryByUrl) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  model_->AddEntry(GURL("http://example.com"), "sample");
  ClearCounts();
  EXPECT_NE(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);
  EXPECT_EQ(model_->unread_size(), 1u);
  model_->RemoveEntryByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(model_->unread_size(), 0u);
  EXPECT_EQ(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);

  model_->AddEntry(GURL("http://example.com"), "sample");
  model_->MarkReadByURL(GURL("http://example.com"));
  ClearCounts();
  EXPECT_NE(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);
  EXPECT_EQ(model_->read_size(), 1u);
  model_->RemoveEntryByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(model_->read_size(), 0u);
  EXPECT_EQ(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);
}

TEST_F(ReadingListModelTest, RemoveSyncEntryByUrl) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  model_->AddEntry(GURL("http://example.com"), "sample");
  ClearCounts();
  EXPECT_NE(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);
  EXPECT_EQ(model_->unread_size(), 1u);
  model_->SyncRemoveEntry(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 0);
  EXPECT_EQ(model_->unread_size(), 0u);
  EXPECT_EQ(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);

  model_->AddEntry(GURL("http://example.com"), "sample");
  model_->MarkReadByURL(GURL("http://example.com"));
  ClearCounts();
  EXPECT_NE(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);
  EXPECT_EQ(model_->read_size(), 1u);
  model_->SyncRemoveEntry(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1);
  AssertStorageCount(0, 0);
  EXPECT_EQ(model_->read_size(), 0u);
  EXPECT_EQ(model_->GetEntryFromURL(GURL("http://example.com"), nullptr),
            nullptr);
}

TEST_F(ReadingListModelTest, ReadEntry) {
  model_->AddEntry(GURL("http://example.com"), "sample");

  ClearCounts();
  model_->MarkReadByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  EXPECT_EQ(0ul, model_->unread_size());
  EXPECT_EQ(1ul, model_->read_size());
  EXPECT_FALSE(model_->HasUnseenEntries());

  const ReadingListEntry& other_entry = model_->GetReadEntryAtIndex(0);
  EXPECT_EQ(GURL("http://example.com"), other_entry.URL());
  EXPECT_EQ("sample", other_entry.Title());
}

TEST_F(ReadingListModelTest, EntryFromURL) {
  GURL url1("http://example.com");
  GURL url2("http://example2.com");
  std::string entry1_title = "foo bar qux";
  model_->AddEntry(url1, entry1_title);

  // Check call with nullptr |read| parameter.
  const ReadingListEntry* entry1 = model_->GetEntryFromURL(url1, nullptr);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());

  bool read;
  entry1 = model_->GetEntryFromURL(url1, &read);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());
  EXPECT_EQ(read, false);
  model_->MarkReadByURL(url1);
  entry1 = model_->GetEntryFromURL(url1, &read);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());
  EXPECT_EQ(read, true);

  const ReadingListEntry* entry2 = model_->GetEntryFromURL(url2, &read);
  EXPECT_EQ(nullptr, entry2);
}

TEST_F(ReadingListModelTest, UnreadEntry) {
  // Setup.
  model_->AddEntry(GURL("http://example.com"), "sample");
  model_->MarkReadByURL(GURL("http://example.com"));
  ClearCounts();
  ASSERT_EQ(0ul, model_->unread_size());
  ASSERT_EQ(1ul, model_->read_size());

  // Action.
  model_->MarkUnreadByURL(GURL("http://example.com"));

  // Tests.
  AssertObserverCount(0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  EXPECT_EQ(1ul, model_->unread_size());
  EXPECT_EQ(0ul, model_->read_size());
  EXPECT_TRUE(model_->HasUnseenEntries());

  const ReadingListEntry& other_entry = model_->GetUnreadEntryAtIndex(0);
  EXPECT_EQ(GURL("http://example.com"), other_entry.URL());
  EXPECT_EQ("sample", other_entry.Title());
}

TEST_F(ReadingListModelTest, BatchUpdates) {
  auto token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete token.release();
  AssertObserverCount(1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

TEST_F(ReadingListModelTest, BatchUpdatesReentrant) {
  // When two updates happen at the same time, the notification is only sent
  // for beginning of first update and completion of last update.
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());

  auto token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  auto second_token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete token.release();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete second_token.release();
  AssertObserverCount(1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());

  // Consequent updates send notifications.
  auto third_token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete third_token.release();
  AssertObserverCount(1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

TEST_F(ReadingListModelTest, UpdateEntryTitle) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddEntry(gurl, "sample");
  ClearCounts();

  model_->SetEntryTitle(gurl, "ping");
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1);
  EXPECT_EQ("ping", entry.Title());
}

TEST_F(ReadingListModelTest, UpdateEntryState) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddEntry(gurl, "sample");
  ClearCounts();

  model_->SetEntryDistilledState(gurl, ReadingListEntry::PROCESSING);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSING, entry.DistilledState());
}

TEST_F(ReadingListModelTest, UpdateDistilledPath) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddEntry(gurl, "sample");
  ClearCounts();

  model_->SetEntryDistilledPath(gurl, base::FilePath("distilled/page.html"));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry.DistilledState());
  EXPECT_EQ(base::FilePath("distilled/page.html"), entry.DistilledPath());
}

TEST_F(ReadingListModelTest, UpdateReadEntryTitle) {
  const GURL gurl("http://example.com");
  model_->AddEntry(gurl, "sample");
  model_->MarkReadByURL(gurl);
  const ReadingListEntry& entry = model_->GetReadEntryAtIndex(0);
  ClearCounts();

  model_->SetEntryTitle(gurl, "ping");
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ("ping", entry.Title());
}

TEST_F(ReadingListModelTest, UpdateReadEntryState) {
  const GURL gurl("http://example.com");
  model_->AddEntry(gurl, "sample");
  model_->MarkReadByURL(gurl);
  const ReadingListEntry& entry = model_->GetReadEntryAtIndex(0);
  ClearCounts();

  model_->SetEntryDistilledState(gurl, ReadingListEntry::PROCESSING);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSING, entry.DistilledState());
}

TEST_F(ReadingListModelTest, UpdateReadDistilledPath) {
  const GURL gurl("http://example.com");
  model_->AddEntry(gurl, "sample");
  model_->MarkReadByURL(gurl);
  const ReadingListEntry& entry = model_->GetReadEntryAtIndex(0);
  ClearCounts();

  model_->SetEntryDistilledPath(gurl, base::FilePath("distilled/page.html"));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry.DistilledState());
  EXPECT_EQ(base::FilePath("distilled/page.html"), entry.DistilledPath());
}

// Tests that the callback is called when the entry is unread.
TEST_F(ReadingListModelTest, CallbackEntryURLUnread) {
  // Setup.
  model_->AddEntry(callback_url, callback_title);

  ASSERT_EQ(0UL, model_->read_size());
  ASSERT_EQ(1UL, model_->unread_size());

  // Action.
  bool result = model_->CallbackEntryURL(
      callback_url,
      base::Bind(&ReadingListModelTest::Callback, base::Unretained(this)));

  // Test.
  EXPECT_TRUE(result);
  EXPECT_TRUE(CallbackCalled());
}

// Tests that the callback is called when the entry is read.
TEST_F(ReadingListModelTest, CallbackEntryURLRead) {
  // Setup.
  model_->AddEntry(callback_url, callback_title);
  model_->MarkReadByURL(callback_url);

  ASSERT_EQ(1UL, model_->read_size());
  ASSERT_EQ(0UL, model_->unread_size());

  // Action.
  bool result = model_->CallbackEntryURL(
      callback_url,
      base::Bind(&ReadingListModelTest::Callback, base::Unretained(this)));

  // Test.
  EXPECT_TRUE(result);
  EXPECT_TRUE(CallbackCalled());
}

// Tests that the callback is not called when the entry is not present.
TEST_F(ReadingListModelTest, CallbackEntryURLNotPresent) {
  // Setup.
  const GURL gurl("http://foo.bar");
  ASSERT_NE(gurl, callback_url);
  model_->AddEntry(gurl, callback_title);

  // Action.
  bool result = model_->CallbackEntryURL(
      callback_url,
      base::Bind(&ReadingListModelTest::Callback, base::Unretained(this)));

  // Test.
  EXPECT_FALSE(result);
  EXPECT_FALSE(CallbackCalled());
}

// Tests that ReadingListModel calls CallbackModelBeingDeleted when destroyed.
TEST_F(ReadingListModelTest, CallbackModelBeingDeleted) {
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  model_.reset();
  AssertObserverCount(1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
}

}  // namespace
