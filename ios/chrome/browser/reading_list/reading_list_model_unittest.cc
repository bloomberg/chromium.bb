// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_model_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ReadingListModelTest : public ReadingListModelObserver,
                             public testing::Test {
 public:
  ReadingListModelTest() : model_(new ReadingListModelMemory()) {
    ClearCounts();
    model_->AddObserver(this);
  }
  ~ReadingListModelTest() override {}

  void ClearCounts() {
    observer_loaded_ = observer_deleted_ = observer_remove_unread_ =
        observer_remove_read_ = observer_add_unread_ = observer_add_read_ =
            observer_did_apply_ = 0;
  }

  void AssertObserverCount(int observer_loaded,
                           int observer_deleted,
                           int observer_remove_unread,
                           int observer_remove_read,
                           int observer_add_unread,
                           int observer_add_read,
                           int observer_did_apply) {
    ASSERT_EQ(observer_loaded, observer_loaded_);
    ASSERT_EQ(observer_deleted, observer_deleted_);
    ASSERT_EQ(observer_remove_unread, observer_remove_unread_);
    ASSERT_EQ(observer_remove_read, observer_remove_read_);
    ASSERT_EQ(observer_add_unread, observer_add_unread_);
    ASSERT_EQ(observer_add_read, observer_add_read_);
    ASSERT_EQ(observer_did_apply, observer_did_apply_);
  }

  // ReadingListModelObserver
  void ReadingListModelLoaded(const ReadingListModel* model) override {
    observer_loaded_ += 1;
  }
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override {
    observer_deleted_ += 1;
  }
  void ReadingListWillRemoveUnreadEntry(const ReadingListModel* model,
                                        size_t index) override {
    observer_remove_unread_ += 1;
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
  void ReadingListDidApplyChanges(ReadingListModel* model) override {
    observer_did_apply_ += 1;
  }

 protected:
  int observer_loaded_;
  int observer_deleted_;
  int observer_remove_unread_;
  int observer_remove_read_;
  int observer_add_unread_;
  int observer_add_read_;
  int observer_did_apply_;

  scoped_ptr<ReadingListModelMemory> model_;
};

TEST_F(ReadingListModelTest, EmptyLoaded) {
  EXPECT_TRUE(model_->loaded());
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(0ul, model_->unread_size());
  EXPECT_EQ(0ul, model_->read_size());
  model_->Shutdown();
  EXPECT_FALSE(model_->loaded());
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0);
}

TEST_F(ReadingListModelTest, AddEntry) {
  ClearCounts();
  const ReadingListEntry entry =
      model_->AddEntry(GURL("http://example.com"), "sample");
  EXPECT_EQ(GURL("http://example.com"), entry.url());
  EXPECT_EQ("sample", entry.title());

  AssertObserverCount(0, 0, 0, 0, 1, 0, 1);
  EXPECT_EQ(1ul, model_->unread_size());
  EXPECT_EQ(0ul, model_->read_size());
  EXPECT_TRUE(model_->HasUnseenEntries());

  const ReadingListEntry other_entry = model_->GetUnreadEntryAtIndex(0);
  EXPECT_EQ(GURL("http://example.com"), other_entry.url());
  EXPECT_EQ("sample", other_entry.title());
}

TEST_F(ReadingListModelTest, ReadEntry) {
  const ReadingListEntry entry =
      model_->AddEntry(GURL("http://example.com"), "sample");

  ClearCounts();
  model_->MarkReadByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 1, 0, 0, 1, 1);
  EXPECT_EQ(0ul, model_->unread_size());
  EXPECT_EQ(1ul, model_->read_size());
  EXPECT_FALSE(model_->HasUnseenEntries());

  const ReadingListEntry other_entry = model_->GetReadEntryAtIndex(0);
  EXPECT_EQ(GURL("http://example.com"), other_entry.url());
  EXPECT_EQ("sample", other_entry.title());
}

}  // namespace
