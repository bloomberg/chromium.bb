// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/driver/fake_sync_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::Eq;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

// The parent tag for children of the root entity. Entities with this parent are
// referred to as top level enities.
const char kRootParentTag[] = "0";
const char kBookmarkBarTag[] = "bookmark_bar";
const char kBookmarksRootId[] = "32904_google_chrome_bookmarks";

syncer::UpdateResponseData CreateUpdateData(const std::string& id,
                                            const std::string& title,
                                            const std::string& url,
                                            const std::string& parent_id,
                                            const std::string& server_tag) {
  syncer::EntityData data;
  data.id = id;
  data.parent_id = parent_id;
  data.server_defined_unique_tag = server_tag;

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_title(title);
  bookmark_specifics->set_url(url);

  syncer::UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UpdateResponseData CreateBookmarkRootUpdateData() {
  return CreateUpdateData(syncer::ModelTypeToRootTag(syncer::BOOKMARKS),
                          std::string(), std::string(), kRootParentTag,
                          syncer::ModelTypeToRootTag(syncer::BOOKMARKS));
}

class TestSyncClient : public syncer::FakeSyncClient {
 public:
  explicit TestSyncClient(bookmarks::BookmarkModel* bookmark_model)
      : bookmark_model_(bookmark_model) {}

  bookmarks::BookmarkModel* GetBookmarkModel() override {
    return bookmark_model_;
  }

 private:
  bookmarks::BookmarkModel* bookmark_model_;
};

class BookmarkModelTypeProcessorTest : public testing::Test {
 public:
  BookmarkModelTypeProcessorTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        sync_client_(bookmark_model_.get()) {}

  TestSyncClient* sync_client() { return &sync_client_; }
  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }

 private:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  TestSyncClient sync_client_;
};

TEST_F(BookmarkModelTypeProcessorTest, ReorderUpdatesShouldIgnoreRootNodes) {
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkRootUpdateData());
  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkModelTypeProcessor::ReorderUpdatesForTest(updates);
  // Root node update should be filtered out.
  EXPECT_THAT(ordered_updates.size(), Eq(0U));
}

// TODO(crbug.com/516866): This should change to cover the general case of
// testing parents before children for non-deletions.
TEST_F(BookmarkModelTypeProcessorTest,
       ReorderUpdatesShouldPlacePermanentNodesFirstForNonDeletions) {
  std::string node1_id = "node1";
  std::string node2_id = "node2";
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateData(node1_id, std::string(), std::string(),
                                     node2_id, std::string()));
  updates.push_back(CreateUpdateData(node2_id, std::string(), std::string(),
                                     kBookmarksRootId, kBookmarkBarTag));
  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkModelTypeProcessor::ReorderUpdatesForTest(updates);

  // No update should be dropped.
  ASSERT_THAT(ordered_updates.size(), Eq(2U));

  // Updates should be ordered such that parent node update comes first.
  EXPECT_THAT(ordered_updates[0]->entity.value().id, Eq(node2_id));
  EXPECT_THAT(ordered_updates[1]->entity.value().id, Eq(node1_id));
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldUpdateModelAfterRemoteCreation) {
  BookmarkModelTypeProcessor processor(sync_client());

  syncer::UpdateResponseDataList updates;
  // Add update for the permanent folder "Bookmarks bar"
  updates.push_back(CreateUpdateData("bookmark_bar", std::string(),
                                     std::string(), kBookmarksRootId,
                                     kBookmarkBarTag));

  // Add update for another node under the bookmarks bar.
  std::string kNodeId = "node_id";
  std::string kTitle = "title";
  std::string kUrl = "http://www.url.com";
  updates.push_back(CreateUpdateData(kNodeId, kTitle, kUrl, kBookmarkBarTag,
                                     /*server_tag=*/std::string()));

  const bookmarks::BookmarkNode* bookmarkbar =
      bookmark_model()->bookmark_bar_node();
  EXPECT_TRUE(bookmarkbar->empty());

  const sync_pb::ModelTypeState model_type_state;
  processor.OnUpdateReceived(model_type_state, updates);

  ASSERT_THAT(bookmarkbar->GetChild(0), NotNull());
  EXPECT_THAT(bookmarkbar->GetChild(0)->GetTitle(), Eq(ASCIIToUTF16(kTitle)));
  EXPECT_THAT(bookmarkbar->GetChild(0)->url(), Eq(GURL(kUrl)));
}

}  // namespace

}  // namespace sync_bookmarks
