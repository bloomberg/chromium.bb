// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_expanded_state_tracker.h"

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_service_factory.h"
#include "base/prefs/testing_pref_store.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace bookmarks {

scoped_ptr<PrefService> PrefServiceForTesting() {
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable());
  registry->RegisterListPref(prefs::kBookmarkEditorExpandedNodes,
                             new base::ListValue,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(prefs::kManagedBookmarks,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  base::PrefServiceFactory factory;
  factory.set_user_prefs(make_scoped_refptr(new TestingPrefStore()));
  return factory.Create(registry.get());
}

class BookmarkExpandedStateTrackerTest : public testing::Test {
 public:
  BookmarkExpandedStateTrackerTest();
  virtual ~BookmarkExpandedStateTrackerTest();

 protected:
  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  base::MessageLoop message_loop_;
  test::TestBookmarkClient client_;
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<BookmarkModel> model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkExpandedStateTrackerTest);
};

BookmarkExpandedStateTrackerTest::BookmarkExpandedStateTrackerTest() {}

BookmarkExpandedStateTrackerTest::~BookmarkExpandedStateTrackerTest() {}

void BookmarkExpandedStateTrackerTest::SetUp() {
  prefs_ = PrefServiceForTesting();
  model_.reset(new BookmarkModel(&client_, false));
  model_->Load(prefs_.get(),
               std::string(),
               base::FilePath(),
               base::MessageLoopProxy::current(),
               base::MessageLoopProxy::current());
  test::WaitForBookmarkModelToLoad(model_.get());
}

void BookmarkExpandedStateTrackerTest::TearDown() {
  model_.reset();
}

// Various assertions for SetExpandedNodes.
TEST_F(BookmarkExpandedStateTrackerTest, SetExpandedNodes) {
  BookmarkExpandedStateTracker* tracker = model_->expanded_state_tracker();

  // Should start out initially empty.
  EXPECT_TRUE(tracker->GetExpandedNodes().empty());

  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(model_->bookmark_bar_node());
  tracker->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());

  // Add a folder and mark it expanded.
  const BookmarkNode* n1 = model_->AddFolder(
      model_->bookmark_bar_node(), 0, base::ASCIIToUTF16("x"));
  nodes.insert(n1);
  tracker->SetExpandedNodes(nodes);
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());

  // Remove the folder, which should remove it from the list of expanded nodes.
  model_->Remove(model_->bookmark_bar_node(), 0);
  nodes.erase(n1);
  n1 = NULL;
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());
}

TEST_F(BookmarkExpandedStateTrackerTest, RemoveAllUserBookmarks) {
  BookmarkExpandedStateTracker* tracker = model_->expanded_state_tracker();

  // Add a folder and mark it expanded.
  const BookmarkNode* n1 = model_->AddFolder(
      model_->bookmark_bar_node(), 0, base::ASCIIToUTF16("x"));
  BookmarkExpandedStateTracker::Nodes nodes;
  nodes.insert(n1);
  tracker->SetExpandedNodes(nodes);
  // Verify that the node is present.
  EXPECT_EQ(nodes, tracker->GetExpandedNodes());
  // Call remove all.
  model_->RemoveAllUserBookmarks();
  // Verify node is not present.
  EXPECT_TRUE(tracker->GetExpandedNodes().empty());
}

}  // namespace bookmarks
