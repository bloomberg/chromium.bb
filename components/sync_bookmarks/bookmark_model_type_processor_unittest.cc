// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_model_type_processor.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/model/data_type_activation_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::Eq;
using testing::NiceMock;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

const char kBookmarkBarTag[] = "bookmark_bar";
const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarksRootId[] = "32904_google_chrome_bookmarks";
const char kCacheGuid[] = "generated_id";

struct BookmarkInfo {
  std::string server_id;
  std::string title;
  std::string url;  // empty for folders.
  std::string parent_id;
  std::string server_tag;
};

syncer::UpdateResponseData CreateUpdateResponseData(
    const BookmarkInfo& bookmark_info,
    const syncer::UniquePosition& unique_position,
    int response_version) {
  syncer::EntityData data;
  data.id = bookmark_info.server_id;
  data.parent_id = bookmark_info.parent_id;
  data.server_defined_unique_tag = bookmark_info.server_tag;
  data.unique_position = unique_position.ToProto();

  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_title(bookmark_info.title);
  if (bookmark_info.url.empty()) {
    data.is_folder = true;
  } else {
    bookmark_specifics->set_url(bookmark_info.url);
  }

  syncer::UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  response_data.response_version = response_version;
  return response_data;
}

sync_pb::ModelTypeState CreateDummyModelTypeState() {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_cache_guid(kCacheGuid);
  model_type_state.set_initial_sync_done(true);
  return model_type_state;
}

void AssertState(const BookmarkModelTypeProcessor* processor,
                 const std::vector<BookmarkInfo>& bookmarks) {
  const SyncedBookmarkTracker* tracker = processor->GetTrackerForTest();

  // Make sure the tracker contains all bookmarks in |bookmarks| + the
  // bookmark bar node.
  ASSERT_THAT(tracker->TrackedEntitiesCountForTest(), Eq(bookmarks.size() + 1));

  for (BookmarkInfo bookmark : bookmarks) {
    const SyncedBookmarkTracker::Entity* entity =
        tracker->GetEntityForSyncId(bookmark.server_id);
    ASSERT_THAT(entity, NotNull());
    const bookmarks::BookmarkNode* node = entity->bookmark_node();
    ASSERT_THAT(node->GetTitle(), Eq(ASCIIToUTF16(bookmark.title)));
    ASSERT_THAT(node->url(), Eq(GURL(bookmark.url)));
    const SyncedBookmarkTracker::Entity* parent_entity =
        tracker->GetEntityForSyncId(bookmark.parent_id);
    ASSERT_THAT(node->parent(), Eq(parent_entity->bookmark_node()));
  }
}

// TODO(crbug.com/516866): Replace with a simpler implementation (e.g. by
// simulating loading from metadata) instead of receiving remote updates.
// Inititalizes the processor with the bookmarks in |bookmarks|.
void InitWithSyncedBookmarks(const std::vector<BookmarkInfo>& bookmarks,
                             BookmarkModelTypeProcessor* processor) {
  syncer::UpdateResponseDataList updates;
  syncer::UniquePosition pos = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  // Add update for the permanent folder "Bookmarks bar".
  updates.push_back(
      CreateUpdateResponseData({kBookmarkBarId, std::string(), std::string(),
                                kBookmarksRootId, kBookmarkBarTag},
                               pos, /*response_version=*/0));
  for (BookmarkInfo bookmark : bookmarks) {
    pos = syncer::UniquePosition::After(pos,
                                        syncer::UniquePosition::RandomSuffix());
    updates.push_back(
        CreateUpdateResponseData(bookmark, pos, /*response_version=*/0));
  }
  // TODO(crbug.com/516866): Remove after a proper positioning for remote
  // updates is implemented. Reversing the updates because the sorting algorithm
  // isn't stable. This is OK for now because once proper positioning is
  // implemented, the siblings update requests order would be irrelvant.
  std::reverse(updates.begin(), updates.end());
  processor->OnUpdateReceived(CreateDummyModelTypeState(), updates);
  AssertState(processor, bookmarks);
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
        sync_client_(bookmark_model_.get()),
        processor_(sync_client()->GetBookmarkUndoServiceIfExists()) {
    // TODO(crbug.com/516866): This class assumes model is loaded and sync has
    // started before running tests. We should test other variations (i.e. model
    // isn't loaded yet and/or sync didn't start yet).
    processor_.ModelReadyToSync(/*metadata_str=*/std::string(),
                                schedule_save_closure_.Get(),
                                bookmark_model_.get());
    syncer::DataTypeActivationRequest request;
    request.cache_guid = kCacheGuid;
    processor_.OnSyncStarting(request, base::DoNothing());
  }

  TestSyncClient* sync_client() { return &sync_client_; }
  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }
  BookmarkModelTypeProcessor* processor() { return &processor_; }
  base::MockCallback<base::RepeatingClosure>* schedule_save_closure() {
    return &schedule_save_closure_;
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  NiceMock<base::MockCallback<base::RepeatingClosure>> schedule_save_closure_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  TestSyncClient sync_client_;
  BookmarkModelTypeProcessor processor_;
};

TEST_F(BookmarkModelTypeProcessorTest, ShouldUpdateModelAfterRemoteCreation) {
  syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;
  // Add update for the permanent folder "Bookmarks bar".
  updates.push_back(
      CreateUpdateResponseData({kBookmarkBarId, std::string(), std::string(),
                                kBookmarksRootId, kBookmarkBarTag},
                               kRandomPosition, /*response_version=*/0));

  // Add update for another node under the bookmarks bar.
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/0));

  const bookmarks::BookmarkNode* bookmarkbar =
      bookmark_model()->bookmark_bar_node();
  EXPECT_TRUE(bookmarkbar->empty());

  processor()->OnUpdateReceived(CreateDummyModelTypeState(), updates);

  ASSERT_THAT(bookmarkbar->GetChild(0), NotNull());
  EXPECT_THAT(bookmarkbar->GetChild(0)->GetTitle(), Eq(ASCIIToUTF16(kTitle)));
  EXPECT_THAT(bookmarkbar->GetChild(0)->url(), Eq(GURL(kUrl)));
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldUpdateModelAfterRemoteUpdate) {
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId, kTitle, kUrl, kBookmarkBarId, /*server_tag=*/std::string()}};

  InitWithSyncedBookmarks(bookmarks, processor());

  // Make sure original bookmark exists.
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_bar->GetChild(0);
  ASSERT_THAT(bookmark_node, NotNull());
  ASSERT_THAT(bookmark_node->GetTitle(), Eq(ASCIIToUTF16(kTitle)));
  ASSERT_THAT(bookmark_node->url(), Eq(GURL(kUrl)));

  // Process an update for the same bookmark.
  const std::string kNewTitle = "new-title";
  const std::string kNewUrl = "http://www.new-url.com";
  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreateUpdateResponseData({kNodeId, kNewTitle, kNewUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/1));

  processor()->OnUpdateReceived(CreateDummyModelTypeState(), updates);

  // Check if the bookmark has been updated properly.
  EXPECT_THAT(bookmark_bar->GetChild(0), Eq(bookmark_node));
  EXPECT_THAT(bookmark_node->GetTitle(), Eq(ASCIIToUTF16(kNewTitle)));
  EXPECT_THAT(bookmark_node->url(), Eq(GURL(kNewUrl)));
}

TEST_F(BookmarkModelTypeProcessorTest,
       ShouldScheduleSaveAfterRemoteUpdateWithOnlyMetadataChange) {
  const std::string kNodeId = "node_id";
  const std::string kTitle = "title";
  const std::string kUrl = "http://www.url.com";
  syncer::UniquePosition kRandomPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId, kTitle, kUrl, kBookmarkBarId, /*server_tag=*/std::string()}};

  InitWithSyncedBookmarks(bookmarks, processor());

  // Make sure original bookmark exists.
  const bookmarks::BookmarkNode* bookmark_bar =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmark_node = bookmark_bar->GetChild(0);
  ASSERT_THAT(bookmark_node, NotNull());

  // Process an update for the same bookmark with the same data.
  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreateUpdateResponseData({kNodeId, kTitle, kUrl, kBookmarkBarId,
                                /*server_tag=*/std::string()},
                               kRandomPosition, /*response_version=*/1));
  updates[0].response_version++;

  EXPECT_CALL(*schedule_save_closure(), Run());
  processor()->OnUpdateReceived(CreateDummyModelTypeState(), updates);
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldDecodeSyncMetadata) {
  const std::string kNodeId = "node_id1";
  const std::string kTitle = "title1";
  const std::string kUrl = "http://www.url1.com";

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId, kTitle, kUrl, kBookmarkBarId, /*server_tag=*/std::string()}};

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* bookmarknode = bookmark_model()->AddURL(
      /*parent=*/bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle),
      GURL(kUrl));

  // TODO(crbug.com/516866): Remove this after initial sync done is properly set
  // within the processor.
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);
  // Add an entry for bookmark bar.
  sync_pb::BookmarkMetadata* bookmark_metadata =
      model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmark_bar_node->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kBookmarkBarId);
  // Add an entry for the bookmark node.
  bookmark_metadata = model_metadata.add_bookmarks_metadata();
  bookmark_metadata->set_id(bookmarknode->id());
  bookmark_metadata->mutable_metadata()->set_server_id(kNodeId);

  // Create a new processor and init it with the metadata str.
  BookmarkModelTypeProcessor new_processor(
      sync_client()->GetBookmarkUndoServiceIfExists());
  std::string metadata_str;
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
                                 bookmark_model());

  AssertState(&new_processor, bookmarks);
}

TEST_F(BookmarkModelTypeProcessorTest, ShouldDecodeEncodedSyncMetadata) {
  const std::string kNodeId1 = "node_id1";
  const std::string kTitle1 = "title1";
  const std::string kUrl1 = "http://www.url1.com";

  const std::string kNodeId2 = "node_id2";
  const std::string kTitle2 = "title2";
  const std::string kUrl2 = "http://www.url2.com";

  std::vector<BookmarkInfo> bookmarks = {
      {kNodeId1, kTitle1, kUrl1, kBookmarkBarId, /*server_tag=*/std::string()},
      {kNodeId2, kTitle2, kUrl2, kBookmarkBarId,
       /*server_tag=*/std::string()}};

  InitWithSyncedBookmarks(bookmarks, processor());

  std::string metadata_str = processor()->EncodeSyncMetadata();
  // TODO(crbug.com/516866): Remove this after initial sync done is properly set
  // within the processor.
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.ParseFromString(metadata_str);
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);

  // Create a new processor and init it with the same metadata str.
  BookmarkModelTypeProcessor new_processor(
      sync_client()->GetBookmarkUndoServiceIfExists());
  model_metadata.SerializeToString(&metadata_str);
  new_processor.ModelReadyToSync(metadata_str, base::DoNothing(),
                                 bookmark_model());

  AssertState(&new_processor, bookmarks);
}

}  // namespace

}  // namespace sync_bookmarks
