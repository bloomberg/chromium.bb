// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class BookmarkExpandedStateTrackerTest : public testing::Test {
 public:
  BookmarkExpandedStateTrackerTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  BookmarkModel* GetModel();

 private:
  scoped_ptr<TestingProfile> profile_;
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkExpandedStateTrackerTest);
};

BookmarkExpandedStateTrackerTest::BookmarkExpandedStateTrackerTest()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      file_thread_(BrowserThread::FILE, &message_loop_) {
}

void BookmarkExpandedStateTrackerTest::SetUp() {
  profile_.reset(new TestingProfile);
  profile_->CreateBookmarkModel(true);
  ui_test_utils::WaitForBookmarkModelToLoad(GetModel());
}

BookmarkModel* BookmarkExpandedStateTrackerTest::GetModel() {
  return BookmarkModelFactory::GetForProfile(profile_.get());
}

void BookmarkExpandedStateTrackerTest::TearDown() {
  profile_.reset(NULL);
}

// Various assertions for SetExpandedNodes.
TEST_F(BookmarkExpandedStateTrackerTest, SetExpandedNodes) {
  BookmarkModel* model = GetModel();
  BookmarkExpandedStateTracker* tracker = model->expanded_state_tracker();

  // Should start out initially empty.
  EXPECT_TRUE(tracker->GetExpandedNodes().empty());

  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(model->bookmark_bar_node());
  tracker->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());

  // Add a folder and mark it expanded.
  const BookmarkNode* n1 = model->AddFolder(model->bookmark_bar_node(), 0,
                                            ASCIIToUTF16("x"));
  nodes.insert(n1);
  tracker->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());

  // Remove the folder, which should remove it from the list of expanded nodes.
  model->Remove(model->bookmark_bar_node(), 0);
  nodes.erase(n1);
  n1 = NULL;
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());
}
