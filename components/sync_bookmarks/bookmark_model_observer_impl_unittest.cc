// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_observer_impl.h"

#include <memory>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/time.h"
#include "components/sync/base/unique_position.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_bookmarks {

namespace {

using testing::Eq;
using testing::NiceMock;
using testing::NotNull;

const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarkBarTag[] = "bookmark_bar";

class BookmarkModelObserverImplTest : public testing::Test {
 public:
  BookmarkModelObserverImplTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        bookmark_tracker_(std::vector<NodeMetadataPair>(),
                          std::make_unique<sync_pb::ModelTypeState>()),
        observer_(nudge_for_commit_closure_.Get(), &bookmark_tracker_) {
    bookmark_model_->AddObserver(&observer_);
    sync_pb::EntitySpecifics specifics;
    specifics.mutable_bookmark()->set_title(kBookmarkBarTag);
    bookmark_tracker_.Add(
        /*sync_id=*/kBookmarkBarId,
        /*bookmark_node=*/bookmark_model()->bookmark_bar_node(),
        /*server_version=*/0, /*creation_time=*/base::Time::Now(),
        syncer::UniquePosition::InitialPosition(
            syncer::UniquePosition::RandomSuffix())
            .ToProto(),
        specifics);
  }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }
  SyncedBookmarkTracker* bookmark_tracker() { return &bookmark_tracker_; }
  BookmarkModelObserverImpl* observer() { return &observer_; }
  base::MockCallback<base::RepeatingClosure>* nudge_for_commit_closure() {
    return &nudge_for_commit_closure_;
  }

 private:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  NiceMock<base::MockCallback<base::RepeatingClosure>>
      nudge_for_commit_closure_;
  SyncedBookmarkTracker bookmark_tracker_;
  BookmarkModelObserverImpl observer_;
};

TEST_F(BookmarkModelObserverImplTest,
       BookmarkAddedShouldPutInTheTrackerAndNudgeForCommit) {
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  const size_t kMaxEntries = 10;

  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 2U);

  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), 1U);
  EXPECT_THAT(local_changes[0]->bookmark_node(), Eq(bookmark_node));
}

TEST_F(BookmarkModelObserverImplTest, ShouldPositionSiblings) {
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";

  // Build this structure:
  // bookmark_bar
  //  |- node1
  //  |- node2
  // Expectation:
  //  p1 < p2

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node1 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  const bookmarks::BookmarkNode* bookmark_node2 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/1, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), Eq(3U));
  const SyncedBookmarkTracker::Entity* entity1 =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node1);
  ASSERT_THAT(entity1, NotNull());
  syncer::UniquePosition p1 =
      syncer::UniquePosition::FromProto(entity1->metadata()->unique_position());

  const SyncedBookmarkTracker::Entity* entity2 =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node2);
  ASSERT_THAT(entity2, NotNull());
  syncer::UniquePosition p2 =
      syncer::UniquePosition::FromProto(entity2->metadata()->unique_position());
  EXPECT_TRUE(p1.LessThan(p2));

  // Now insert node3 at index 1 to build this structure:
  // bookmark_bar
  //  |- node1
  //  |- node3
  //  |- node2
  // Expectation:
  //  p1 < p2 (still holds)
  //  p1 < p3
  //  p3 < p2

  const bookmarks::BookmarkNode* bookmark_node3 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/1, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));
  EXPECT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), Eq(4U));

  const SyncedBookmarkTracker::Entity* entity3 =
      bookmark_tracker()->GetEntityForBookmarkNode(bookmark_node3);
  ASSERT_THAT(entity3, NotNull());
  syncer::UniquePosition p3 =
      syncer::UniquePosition::FromProto(entity3->metadata()->unique_position());
  EXPECT_TRUE(p1.LessThan(p2));
  EXPECT_TRUE(p1.LessThan(p3));
  EXPECT_TRUE(p3.LessThan(p2));
}

}  // namespace

}  // namespace sync_bookmarks
