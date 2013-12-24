// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkExpandedStateTrackerTest : public testing::Test {
 public:
  BookmarkExpandedStateTrackerTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  BookmarkModel* GetModel();

 private:
  scoped_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkExpandedStateTrackerTest);
};

BookmarkExpandedStateTrackerTest::BookmarkExpandedStateTrackerTest() {}

void BookmarkExpandedStateTrackerTest::SetUp() {
  profile_.reset(new TestingProfile);
  profile_->CreateBookmarkModel(true);
  test::WaitForBookmarkModelToLoad(GetModel());
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

TEST_F(BookmarkExpandedStateTrackerTest, RemoveAll) {
  BookmarkModel* model = GetModel();
  BookmarkExpandedStateTracker* tracker = model->expanded_state_tracker();

  // Add a folder and mark it expanded.
  const BookmarkNode* n1 =
      model->AddFolder(model->bookmark_bar_node(), 0, ASCIIToUTF16("x"));
  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(n1);
  tracker->SetExpandedNodes(nodes);
  // Verify that the node is present.
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());
  // Call remove all.
  model->RemoveAll();
  // Verify node is not present.
  EXPECT_TRUE(tracker->GetExpandedNodes().empty());
}
