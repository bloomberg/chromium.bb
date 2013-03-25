// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/recently_used_folders_combo_model.h"

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class RecentlyUsedFoldersComboModelTest : public testing::Test {
 public:
  RecentlyUsedFoldersComboModelTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  BookmarkModel* GetModel();

 private:
  MessageLoopForUI message_loop_;
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
  ui_test_utils::WaitForBookmarkModelToLoad(GetModel());
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
  std::set<const BookmarkNode*> nodes;
  for (int i = 0; i < model.GetItemCount(); ++i) {
    const BookmarkNode* node = model.GetNodeAt(i);
    EXPECT_EQ(0u, nodes.count(node));
    nodes.insert(node);
  }
}
