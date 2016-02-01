// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/combobox_model_observer.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::TestBookmarkClient;
using content::BrowserThread;

// Implementation of ComboboxModelObserver that records when
// OnComboboxModelChanged() is invoked.
class TestComboboxModelObserver : public ui::ComboboxModelObserver {
 public:
  TestComboboxModelObserver() : changed_(false) {}
  ~TestComboboxModelObserver() override {}

  // Returns whether the model changed and clears changed state.
  bool GetAndClearChanged() {
    const bool changed = changed_;
    changed_ = false;
    return changed;
  }

  // ui::ComboboxModelObserver:
  void OnComboboxModelChanged(ui::ComboboxModel* model) override {
    changed_ = true;
  }

 private:
  bool changed_;

  DISALLOW_COPY_AND_ASSIGN(TestComboboxModelObserver);
};

class RecentlyUsedFoldersComboModelTest : public testing::Test {
 public:
  RecentlyUsedFoldersComboModelTest();

 private:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(RecentlyUsedFoldersComboModelTest);
};

RecentlyUsedFoldersComboModelTest::RecentlyUsedFoldersComboModelTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

// Verifies there are no duplicate nodes in the model.
TEST_F(RecentlyUsedFoldersComboModelTest, NoDups) {
  scoped_ptr<BookmarkModel> bookmark_model(TestBookmarkClient::CreateModel());
  const BookmarkNode* new_node = bookmark_model->AddURL(
      bookmark_model->bookmark_bar_node(), 0, base::ASCIIToUTF16("a"),
      GURL("http://a"));
  RecentlyUsedFoldersComboModel model(bookmark_model.get(), new_node);
  std::set<base::string16> items;
  for (int i = 0; i < model.GetItemCount(); ++i) {
    if (!model.IsItemSeparatorAt(i))
      EXPECT_EQ(0u, items.count(model.GetItemAt(i)));
  }
}

// Verifies that observers are notified on changes.
TEST_F(RecentlyUsedFoldersComboModelTest, NotifyObserver) {
  scoped_ptr<BookmarkModel> bookmark_model(TestBookmarkClient::CreateModel());
  const BookmarkNode* folder = bookmark_model->AddFolder(
      bookmark_model->bookmark_bar_node(), 0, base::ASCIIToUTF16("a"));
  const BookmarkNode* sub_folder = bookmark_model->AddFolder(
      folder, 0, base::ASCIIToUTF16("b"));
  const BookmarkNode* new_node = bookmark_model->AddURL(
      sub_folder, 0, base::ASCIIToUTF16("a"), GURL("http://a"));
  RecentlyUsedFoldersComboModel model(bookmark_model.get(), new_node);
  TestComboboxModelObserver observer;
  model.AddObserver(&observer);

  const int initial_count = model.GetItemCount();
  // Remove a folder, it should remove an item from the model too.
  bookmark_model->Remove(sub_folder);
  EXPECT_TRUE(observer.GetAndClearChanged());
  const int updated_count = model.GetItemCount();
  EXPECT_LT(updated_count, initial_count);

  // Remove all, which should remove a folder too.
  bookmark_model->RemoveAllUserBookmarks();
  EXPECT_TRUE(observer.GetAndClearChanged());
  EXPECT_LT(model.GetItemCount(), updated_count);

  model.RemoveObserver(&observer);
}
