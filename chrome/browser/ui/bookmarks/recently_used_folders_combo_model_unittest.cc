// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/combobox_model_observer.h"

using content::BrowserThread;

// Implementation of ComboboxModelObserver that records when OnModelChanged()
// is invoked.
class TestComboboxModelObserver : public ui::ComboboxModelObserver {
 public:
  TestComboboxModelObserver() : changed_(false) {}
  virtual ~TestComboboxModelObserver() {}

  // Returns whether the model changed and clears changed state.
  bool GetAndClearChanged() {
    const bool changed = changed_;
    changed_ = false;
    return changed;
  }

  // ComboboxModelObserver:
  virtual void OnModelChanged() OVERRIDE {
    changed_ = true;
  }

 private:
  bool changed_;

  DISALLOW_COPY_AND_ASSIGN(TestComboboxModelObserver);
};

class RecentlyUsedFoldersComboModelTest : public testing::Test {
 public:
  RecentlyUsedFoldersComboModelTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  BookmarkModel* GetModel();

 private:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyUsedFoldersComboModelTest);
};

RecentlyUsedFoldersComboModelTest::RecentlyUsedFoldersComboModelTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

void RecentlyUsedFoldersComboModelTest::SetUp() {
  profile_.reset(new TestingProfile());
  profile_->CreateBookmarkModel(true);
  test::WaitForBookmarkModelToLoad(GetModel());
}

void RecentlyUsedFoldersComboModelTest::TearDown() {
  // Flush the message loop to make application verifiers happy.
  message_loop_.RunUntilIdle();
}

BookmarkModel* RecentlyUsedFoldersComboModelTest::GetModel() {
  return BookmarkModelFactory::GetForProfile(profile_.get());
}

// Verifies there are no duplicate nodes in the model.
TEST_F(RecentlyUsedFoldersComboModelTest, NoDups) {
  const BookmarkNode* new_node = GetModel()->AddURL(
      GetModel()->bookmark_bar_node(), 0, ASCIIToUTF16("a"),
      GURL("http://a"));
  RecentlyUsedFoldersComboModel model(GetModel(), new_node);
  std::set<string16> items;
  for (int i = 0; i < model.GetItemCount(); ++i) {
    if (!model.IsItemSeparatorAt(i))
      EXPECT_EQ(0u, items.count(model.GetItemAt(i)));
  }
}

// Verifies that observers are notified on changes.
TEST_F(RecentlyUsedFoldersComboModelTest, NotifyObserver) {
  const BookmarkNode* folder = GetModel()->AddFolder(
      GetModel()->bookmark_bar_node(), 0, ASCIIToUTF16("a"));
  const BookmarkNode* sub_folder = GetModel()->AddFolder(
      folder, 0, ASCIIToUTF16("b"));
  const BookmarkNode* new_node = GetModel()->AddURL(
      sub_folder, 0, ASCIIToUTF16("a"), GURL("http://a"));
  RecentlyUsedFoldersComboModel model(GetModel(), new_node);
  TestComboboxModelObserver observer;
  model.AddObserver(&observer);

  const int initial_count = model.GetItemCount();
  // Remove a folder, it should remove an item from the model too.
  GetModel()->Remove(folder, folder->GetIndexOf(sub_folder));
  EXPECT_TRUE(observer.GetAndClearChanged());
  const int updated_count = model.GetItemCount();
  EXPECT_LT(updated_count, initial_count);

  // Remove all, which should remove a folder too.
  GetModel()->RemoveAll();
  EXPECT_TRUE(observer.GetAndClearChanged());
  EXPECT_LT(model.GetItemCount(), updated_count);

  model.RemoveObserver(&observer);
}
