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

TEST_F(BookmarkModelObserverImplTest,
       BookmarkChangedShouldUpdateTheTrackerAndNudgeForCommit) {
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";
  const std::string kNewUrl1 = "http://www.new-url1.com";
  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";
  const std::string kNewTitle2 = "new_title2";
  const size_t kMaxEntries = 10;

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node1 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle1),
      GURL(kUrl1));
  const bookmarks::BookmarkNode* bookmark_node2 = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle2),
      GURL(kUrl2));
  // Both bookmarks should be tracked now.
  ASSERT_THAT(bookmark_tracker()->TrackedEntitiesCountForTest(), 3U);

  const std::string id1 = bookmark_tracker()
                              ->GetEntityForBookmarkNode(bookmark_node1)
                              ->metadata()
                              ->server_id();
  const std::string id2 = bookmark_tracker()
                              ->GetEntityForBookmarkNode(bookmark_node2)
                              ->metadata()
                              ->server_id();
  // There should be two local changes now for both entities.
  ASSERT_THAT(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).size(), 2U);

  // Record commits responses for both nodes (without change in ids for
  // simplcitiy of this test).
  bookmark_tracker()->UpdateUponCommitResponse(
      id1, id1, /*acked_sequence_number=*/1, /*server_version=*/1);
  bookmark_tracker()->UpdateUponCommitResponse(
      id2, id2, /*acked_sequence_number=*/1, /*server_version=*/1);

  // There should be no local changes now.
  ASSERT_TRUE(
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries).empty());

  // Now update the title of the 2nd node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetTitle(bookmark_node2, base::UTF8ToUTF16(kNewTitle2));
  // Node 2 should be in the local changes list.
  std::vector<const SyncedBookmarkTracker::Entity*> local_changes =
      bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), 1U);
  EXPECT_THAT(local_changes[0]->bookmark_node(), Eq(bookmark_node2));

  // Now update the url of the 1st node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetURL(bookmark_node1, GURL(kNewUrl1));

  // Node 1 and 2 should be in the local changes list.
  local_changes = bookmark_tracker()->GetEntitiesWithLocalChanges(kMaxEntries);
  ASSERT_THAT(local_changes.size(), 2U);

  // Constuct a set of the bookmark nodes in the local changes.
  std::set<const bookmarks::BookmarkNode*> nodes_in_local_changes;
  for (const SyncedBookmarkTracker::Entity* entity : local_changes) {
    nodes_in_local_changes.insert(entity->bookmark_node());
  }
  // Both bookmarks should exist in the set.
  EXPECT_TRUE(nodes_in_local_changes.find(bookmark_node1) !=
              nodes_in_local_changes.end());
  EXPECT_TRUE(nodes_in_local_changes.find(bookmark_node2) !=
              nodes_in_local_changes.end());

  // Now update metainfo of the 1st node.
  EXPECT_CALL(*nudge_for_commit_closure(), Run());
  bookmark_model()->SetNodeMetaInfo(bookmark_node1, "key", "value");
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
