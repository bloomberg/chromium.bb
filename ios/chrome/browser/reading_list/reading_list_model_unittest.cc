// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "ios/chrome/browser/reading_list/reading_list_model_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const GURL callback_url("http://example.com");
const std::string callback_title("test title");

class ReadingListModelTest : public ReadingListModelObserver,
                             public testing::Test {
 public:
  ReadingListModelTest()
      : callback_called_(false), model_(new ReadingListModelImpl()) {
    ClearCounts();
    model_->AddObserver(this);
  }
  ~ReadingListModelTest() override {}

  void ClearCounts() {
    observer_loaded_ = observer_started_batch_update_ =
        observer_completed_batch_update_ = observer_deleted_ =
            observer_remove_unread_ = observer_remove_read_ = observer_move_ =
                observer_add_unread_ = observer_add_read_ =
                    observer_update_unread_ = observer_update_read_ =
                        observer_did_apply_ = 0;
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
                                size_t index) override {
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

TEST_F(ReadingListModelTest, AddEntry) {
  ClearCounts();
  const ReadingListEntry& entry =
      model_->AddEntry(GURL("http://example.com"), "sample");
  EXPECT_EQ(GURL("http://example.com"), entry.URL());
  EXPECT_EQ("sample", entry.Title());

  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1);
  EXPECT_EQ(1ul, model_->unread_size());
  EXPECT_EQ(0ul, model_->read_size());
  EXPECT_TRUE(model_->HasUnseenEntries());

  const ReadingListEntry& other_entry = model_->GetUnreadEntryAtIndex(0);
  EXPECT_EQ(GURL("http://example.com"), other_entry.URL());
  EXPECT_EQ("sample", other_entry.Title());
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

  const ReadingListEntry* entry1 = model_->GetEntryFromURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());
  model_->MarkReadByURL(url1);
  entry1 = model_->GetEntryFromURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());

  const ReadingListEntry* entry2 = model_->GetEntryFromURL(url2);
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

TEST_F(ReadingListModelTest, UpdateDistilledURL) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddEntry(gurl, "sample");
  ClearCounts();

  model_->SetEntryDistilledURL(gurl, gurl);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry.DistilledState());
  EXPECT_EQ(gurl, entry.DistilledURL());
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

TEST_F(ReadingListModelTest, UpdateReadDistilledURL) {
  const GURL gurl("http://example.com");
  model_->AddEntry(gurl, "sample");
  model_->MarkReadByURL(gurl);
  const ReadingListEntry& entry = model_->GetReadEntryAtIndex(0);
  ClearCounts();

  model_->SetEntryDistilledURL(gurl, gurl);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry.DistilledState());
  EXPECT_EQ(gurl, entry.DistilledURL());
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
