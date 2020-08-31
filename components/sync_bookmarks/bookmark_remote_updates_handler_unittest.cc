// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_model_merger.h"
#include "components/sync_bookmarks/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::IsNull;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

// The parent tag for children of the root entity. Entities with this parent are
// referred to as top level entities.
const char kRootParentId[] = "0";
const char kBookmarksRootId[] = "root_id";
const char kBookmarkBarId[] = "bookmark_bar_id";
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksId[] = "synced_bookmarks_id";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksId[] = "other_bookmarks_id";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Fork of enum RemoteBookmarkUpdateError.
enum class ExpectedRemoteBookmarkUpdateError {
  kConflictingTypes = 0,
  kInvalidSpecifics = 1,
  kInvalidUniquePosition = 2,
  kPermanentNodeCreationAfterMerge = 3,
  kMissingParentEntity = 4,
  kMissingParentNode = 5,
  kMissingParentEntityInConflict = 6,
  kMissingParentNodeInConflict = 7,
  kCreationFailure = 8,
  kUnexpectedGuid = 9,
  kParentNotFolder = 10,
  kMaxValue = kParentNotFolder,
};

sync_pb::BookmarkMetadata CreateNodeMetadata(int64_t node_id,
                                             const std::string& server_id) {
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.set_id(node_id);
  bookmark_metadata.mutable_metadata()->set_server_id(server_id);
  return bookmark_metadata;
}

sync_pb::BookmarkMetadata CreateNodeMetadata(
    int64_t node_id,
    const std::string& server_id,
    const syncer::UniquePosition& unique_position) {
  sync_pb::BookmarkMetadata bookmark_metadata =
      CreateNodeMetadata(node_id, server_id);
  *bookmark_metadata.mutable_metadata()->mutable_unique_position() =
      unique_position.ToProto();
  return bookmark_metadata;
}

sync_pb::BookmarkMetadata CreateTombstoneMetadata(
    const std::string& server_id) {
  sync_pb::BookmarkMetadata bookmark_metadata;
  bookmark_metadata.mutable_metadata()->set_server_id(server_id);
  bookmark_metadata.mutable_metadata()->set_is_deleted(true);
  // A tombstone must be non-synced.
  bookmark_metadata.mutable_metadata()->set_sequence_number(1);
  bookmark_metadata.mutable_metadata()->set_acked_sequence_number(0);
  return bookmark_metadata;
}

sync_pb::BookmarkModelMetadata CreateMetadataForPermanentNodes(
    const bookmarks::BookmarkModel* bookmark_model) {
  sync_pb::BookmarkModelMetadata model_metadata;
  model_metadata.mutable_model_type_state()->set_initial_sync_done(true);

  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->bookmark_bar_node()->id(),
                         /*server_id=*/kBookmarkBarId);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->mobile_node()->id(),
                         /*server_id=*/kMobileBookmarksId);
  *model_metadata.add_bookmarks_metadata() =
      CreateNodeMetadata(bookmark_model->other_node()->id(),
                         /*server_id=*/kOtherBookmarksId);
  return model_metadata;
}

syncer::UpdateResponseData CreateUpdateResponseData(
    const std::string& server_id,
    const std::string& parent_id,
    const std::string& guid,
    const std::string& title,
    bool is_deletion,
    int version,
    const syncer::UniquePosition& unique_position) {
  syncer::EntityData data;
  data.id = server_id;
  data.parent_id = parent_id;
  data.unique_position = unique_position.ToProto();
  data.originator_client_item_id = guid;

  // EntityData would be considered a deletion if its specifics hasn't been set.
  if (!is_deletion) {
    sync_pb::BookmarkSpecifics* bookmark_specifics =
        data.specifics.mutable_bookmark();
    bookmark_specifics->set_guid(guid);
    bookmark_specifics->set_legacy_canonicalized_title(title);
  }
  data.is_folder = true;
  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  response_data.response_version = version;
  return response_data;
}

// Overload that assigns a random GUID. Should only be used when GUID is not
// relevant.
syncer::UpdateResponseData CreateUpdateResponseData(
    const std::string& server_id,
    const std::string& parent_id,
    const std::string& title,
    bool is_deletion,
    int version,
    const syncer::UniquePosition& unique_position) {
  return CreateUpdateResponseData(server_id, parent_id, base::GenerateGUID(),
                                  title, is_deletion, version, unique_position);
}

// Overload that assign a random position. Should only be used when position
// is not relevant.
syncer::UpdateResponseData CreateUpdateResponseData(
    const std::string& server_id,
    const std::string& parent_id,
    bool is_deletion,
    int version = 0) {
  return CreateUpdateResponseData(server_id, parent_id, /*title=*/server_id,
                                  is_deletion, version,
                                  syncer::UniquePosition::InitialPosition(
                                      syncer::UniquePosition::RandomSuffix()));
}

syncer::UpdateResponseData CreateBookmarkRootUpdateData() {
  syncer::EntityData data;
  data.id = syncer::ModelTypeToRootTag(syncer::BOOKMARKS);
  data.parent_id = kRootParentId;
  data.server_defined_unique_tag =
      syncer::ModelTypeToRootTag(syncer::BOOKMARKS);

  data.specifics.mutable_bookmark();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UpdateResponseData CreatePermanentFolderUpdateData(
    const std::string& id,
    const std::string& tag) {
  syncer::EntityData data;
  data.id = id;
  data.parent_id = "root_id";
  data.server_defined_unique_tag = tag;

  data.specifics.mutable_bookmark();

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  return response_data;
}

syncer::UpdateResponseDataList CreatePermanentFoldersUpdateData() {
  syncer::UpdateResponseDataList updates;
  updates.push_back(
      CreatePermanentFolderUpdateData(kBookmarkBarId, kBookmarkBarTag));
  updates.push_back(
      CreatePermanentFolderUpdateData(kOtherBookmarksId, kOtherBookmarksTag));
  updates.push_back(
      CreatePermanentFolderUpdateData(kMobileBookmarksId, kMobileBookmarksTag));
  return updates;
}

class BookmarkRemoteUpdatesHandlerWithInitialMergeTest : public testing::Test {
 public:
  BookmarkRemoteUpdatesHandlerWithInitialMergeTest()
      : bookmark_model_(bookmarks::TestBookmarkClient::CreateModel()),
        tracker_(SyncedBookmarkTracker::CreateEmpty(sync_pb::ModelTypeState())),
        updates_handler_(bookmark_model_.get(),
                         &favicon_service_,
                         tracker_.get()) {
    BookmarkModelMerger(CreatePermanentFoldersUpdateData(),
                        bookmark_model_.get(), &favicon_service_,
                        tracker_.get())
        .Merge();
  }

  bookmarks::BookmarkModel* bookmark_model() { return bookmark_model_.get(); }
  SyncedBookmarkTracker* tracker() { return tracker_.get(); }
  favicon::MockFaviconService* favicon_service() { return &favicon_service_; }
  BookmarkRemoteUpdatesHandler* updates_handler() { return &updates_handler_; }

 private:
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;
  std::unique_ptr<SyncedBookmarkTracker> tracker_;
  testing::NiceMock<favicon::MockFaviconService> favicon_service_;
  BookmarkRemoteUpdatesHandler updates_handler_;
};

TEST(BookmarkRemoteUpdatesHandlerReorderUpdatesTest, ShouldIgnoreRootNodes) {
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateBookmarkRootUpdateData());
  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkRemoteUpdatesHandler::ReorderUpdatesForTest(&updates);
  // Root node update should be filtered out.
  EXPECT_THAT(ordered_updates.size(), Eq(0U));
}

TEST(BookmarkRemoteUpdatesHandlerReorderUpdatesTest,
     ShouldReorderParentsUpdateBeforeChildrenAndBothBeforeDeletions) {
  // Prepare creation updates to build this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2
  // and another sub hierarchy under node3 that won't receive any update.
  // node4
  //  |- node5
  // and a deletion for node6 under node3.

  // Constuct the updates list to have deletion first, and then all creations in
  // reverse shuffled order (from child to parent).

  std::vector<std::string> ids;
  for (int i = 0; i < 7; i++) {
    ids.push_back("node" + base::NumberToString(i));
  }
  // Construct updates list
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*server_id=*/ids[6],
                                             /*parent_id=*/ids[3],
                                             /*is_deletion=*/true));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/ids[5],
                                             /*parent_id=*/ids[4],
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/ids[2],
                                             /*parent_id=*/ids[1],
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/ids[1],
                                             /*parent_id=*/ids[0],
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/ids[4],
                                             /*parent_id=*/ids[3],
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/ids[0],
                                             /*parent_id=*/kBookmarksRootId,
                                             /*is_deletion=*/false));

  std::vector<const syncer::UpdateResponseData*> ordered_updates =
      BookmarkRemoteUpdatesHandler::ReorderUpdatesForTest(&updates);

  // No update should be dropped.
  ASSERT_THAT(ordered_updates.size(), Eq(6U));

  // Updates should be ordered such that parent node update comes first, and
  // deletions come last.
  // node4 --> node5 --> node0 --> node1 --> node2 --> node6.
  // This is test is over verifying since the order requirements are
  // within subtrees only. (e.g it doesn't matter whether node1 comes before or
  // after node4). However, it's implemented this way for simplicity.
  EXPECT_THAT(ordered_updates[0]->entity.id, Eq(ids[4]));
  EXPECT_THAT(ordered_updates[1]->entity.id, Eq(ids[5]));
  EXPECT_THAT(ordered_updates[2]->entity.id, Eq(ids[0]));
  EXPECT_THAT(ordered_updates[3]->entity.id, Eq(ids[1]));
  EXPECT_THAT(ordered_updates[4]->entity.id, Eq(ids[2]));
  EXPECT_THAT(ordered_updates[5]->entity.id, Eq(ids[6]));
}

// This test checks the situation when there are two duplicate nodes in the
// bookmark model. One of them has GUID with generated server_id and another
// one has only GUID (it could happen due to some past bug, see
// crbug.com/1004205).
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessRemoteUpdateOnDuplicateTombstone) {
  const std::string kId = "id";

  // Add first bookmark with server_id.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId,
                                             /*parent_id=*/kBookmarkBarId,
                                             /*is_deletion=*/false));
  syncer::UpdateResponseData& update_data = updates.front();
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should be tracked including the "bookmark bar", "other
  // bookmarks" node and "mobile bookmarks".
  ASSERT_EQ(tracker()->TrackedEntitiesCountForTest(), 4U);

  // Copy new entity to another parent locally. The new node should have the
  // same URL and GUID. But it should be created only locally.
  const bookmarks::BookmarkNode* mobile_node = bookmark_model()->mobile_node();
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(bookmark_bar_node->children().size(), 1U);
  const bookmarks::BookmarkNode* user_node =
      bookmark_bar_node->children().front().get();

  bookmark_model()->Copy(user_node, mobile_node, 0);
  ASSERT_EQ(mobile_node->children().size(), 1U);
  const bookmarks::BookmarkNode* user_node_copy =
      mobile_node->children().front().get();

  // Add the new item to tracker to mimic a scenario when there are two
  // duplicate nodes:
  // - one node is created locally and the client is not aware of server_id for
  // it.
  // - the second node is the copy with the same GUID (but it was committed
  // successfully and the client knows the new server id).
  const SyncedBookmarkTracker::Entity* entity = tracker()->Add(
      user_node_copy, update_data.entity.originator_client_item_id,
      update_data.response_version, update_data.entity.creation_time,
      update_data.entity.unique_position, update_data.entity.specifics);
  ASSERT_THAT(entity, NotNull());

  // Remove local node without committing.
  bookmark_model()->Remove(user_node_copy);
  tracker()->MarkDeleted(entity);
  tracker()->IncrementSequenceNumber(entity);

  // Move the previous entity to a new parent.
  update_data.entity.parent_id = kMobileBookmarksId;
  update_data.response_version++;

  ASSERT_THAT(tracker()->GetEntityForSyncId(
                  update_data.entity.originator_client_item_id),
              Eq(entity));
  ASSERT_THAT(tracker()->GetEntityForSyncId(update_data.entity.id), NotNull());
  ASSERT_EQ(tracker()->TrackedEntitiesCountForTest(), 5U);

  // Run update and check that the local node is removed from tracker.
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  EXPECT_THAT(tracker()->GetEntityForSyncId(
                  update_data.entity.originator_client_item_id),
              IsNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(update_data.entity.id), NotNull());
  EXPECT_EQ(tracker()->TrackedEntitiesCountForTest(), 4U);

  // Double-check that the duplicate node is not removed.
  EXPECT_EQ(bookmark_bar_node->children().size(), 1U);
  EXPECT_EQ(mobile_node->children().size(), 0U);
}

// This test checks the situation when there are two duplicate nodes in the
// bookmark model. One of them has GUID with generated server_id and another
// one has only GUID (it could happen due to some past bug, see
// crbug.com/1004205).
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessRemoteUpdateOnDuplicateUrlNode) {
  const std::string kId = "id";
  const GURL kUrl("http://www.url.com");

  // Add first bookmark with server_id.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId,
                                             /*parent_id=*/kBookmarkBarId,
                                             /*is_deletion=*/false));
  syncer::UpdateResponseData& update_data = updates.front();
  update_data.entity.specifics.mutable_bookmark()->set_url(kUrl.spec());
  update_data.entity.is_folder = false;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should be tracked including the "bookmark bar", "other
  // bookmarks" node and "mobile bookmarks".
  ASSERT_EQ(tracker()->TrackedEntitiesCountForTest(), 4U);

  // Copy new entity to another parent locally. The new node should have the
  // same URL and GUID. But it should be created only locally.
  const bookmarks::BookmarkNode* mobile_node = bookmark_model()->mobile_node();
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(bookmark_bar_node->children().size(), 1U);
  const bookmarks::BookmarkNode* user_node =
      bookmark_bar_node->children().front().get();

  bookmark_model()->Copy(user_node, mobile_node, 0);
  ASSERT_EQ(mobile_node->children().size(), 1U);
  const bookmarks::BookmarkNode* user_node_copy =
      mobile_node->children().front().get();

  // Add the new item to tracker to mimic a scenario when there are two
  // duplicate nodes:
  // - one node is created locally and the client is not aware of server_id for
  // it.
  // - the second node is the copy with the same GUID (but it was committed
  // successfully and the client knows the new server id).
  tracker()->Add(user_node_copy, update_data.entity.originator_client_item_id,
                 update_data.response_version, update_data.entity.creation_time,
                 update_data.entity.unique_position,
                 update_data.entity.specifics);

  // Move the previous entity to a new parent.
  update_data.entity.parent_id = kMobileBookmarksId;
  update_data.response_version++;

  ASSERT_THAT(tracker()->GetEntityForSyncId(
                  update_data.entity.originator_client_item_id),
              NotNull());
  ASSERT_THAT(tracker()->GetEntityForSyncId(update_data.entity.id), NotNull());
  ASSERT_EQ(tracker()->TrackedEntitiesCountForTest(), 5U);

  // Run update and check that the local node is removed from tracker.
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  EXPECT_THAT(tracker()->GetEntityForSyncId(
                  update_data.entity.originator_client_item_id),
              IsNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(update_data.entity.id), NotNull());
  EXPECT_EQ(tracker()->TrackedEntitiesCountForTest(), 4U);

  // Double-check that the duplicate node is not removed.
  EXPECT_EQ(bookmark_bar_node->children().size(), 1U);
  EXPECT_EQ(mobile_node->children().size(), 0U);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessRandomlyOrderedCreations) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2

  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";

  // Constuct the updates list to have creations randomly ordered.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId2,
                                             /*parent_id=*/kId1,
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId0,
                                             /*parent_id=*/kBookmarkBarId,
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId1,
                                             /*parent_id=*/kId0,
                                             /*is_deletion=*/false));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should be tracked including the "bookmark bar", "other
  // bookmarks" node and "mobile bookmarks".
  EXPECT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(6U));

  // All nodes should have been added to the model.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children().front()->GetTitle(),
              Eq(ASCIIToUTF16(kId0)));
  ASSERT_THAT(bookmark_bar_node->children().front()->children().size(), Eq(1u));
  const bookmarks::BookmarkNode* grandchild =
      bookmark_bar_node->children().front()->children().front().get();
  EXPECT_THAT(grandchild->GetTitle(), Eq(ASCIIToUTF16(kId1)));
  ASSERT_THAT(grandchild->children().size(), Eq(1u));
  EXPECT_THAT(grandchild->children().front()->GetTitle(),
              Eq(ASCIIToUTF16(kId2)));
  EXPECT_THAT(grandchild->children().front()->children().size(), Eq(0u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessRandomlyOrderedDeletions) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0
  //    |- node1
  //      |- node2

  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";

  // Construct the updates list to create that structure
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId0,
                                             /*parent_id=*/kBookmarkBarId,
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId1,
                                             /*parent_id=*/kId0,
                                             /*is_deletion=*/false));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId2,
                                             /*parent_id=*/kId1,
                                             /*is_deletion=*/false));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should be tracked including the "bookmark bar", "other
  // bookmarks" node and "mobile bookmarks".
  EXPECT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(6U));

  // Construct the updates list to have random deletions order.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId1,
                                             /*parent_id=*/kId0,
                                             /*is_deletion=*/true,
                                             /*version=*/1));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId0,
                                             /*parent_id=*/kBookmarksRootId,
                                             /*is_deletion=*/true,
                                             /*version=*/1));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId2,
                                             /*parent_id=*/kId1,
                                             /*is_deletion=*/true,
                                             /*version=*/1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // |tracker| should have only permanent nodes now.
  EXPECT_THAT(tracker()->TrackedEntitiesCountForTest(), Eq(3U));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreRemoteCreationWithInvalidGuidInSpecifics) {
  const std::string kId = "id";
  const std::string kTitle = "title";
  const syncer::UniquePosition kPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;

  // Create update with an invalid GUID.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/"invalidguid",
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/kPosition));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId), IsNull());

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kInvalidSpecifics,
      /*count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreRemoteCreationWithUnexpectedGuidInSpecifics) {
  const std::string kId = "id";
  const std::string kTitle = "title";
  const syncer::UniquePosition kPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;

  // Create update with empty GUID.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/base::GenerateGUID(),
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/kPosition));

  // Override the originator client item ID to mimic a mismatch.
  updates.back().entity.originator_client_item_id = base::GenerateGUID();

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId), IsNull());

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kUnexpectedGuid,
      /*count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldUpdateGUIDValueUponRemoteUpdate) {
  const std::string kId = "id";
  const std::string kTitle = "title";
  const std::string kOldGuid = base::GenerateGUID();
  const std::string kNewGuid = base::GenerateGUID();
  const syncer::UniquePosition kPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;

  // Create update with GUID.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kOldGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/kPosition));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId), NotNull());

  // Push an update for the same entity with a new GUID.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kNewGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // The GUID should have been updated.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children().front()->guid(), Eq(kNewGuid));

  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kUnexpectedGuid,
      /*count=*/0);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldKeepOldGUIDUponRemoteUpdate) {
  const std::string kId = "id";
  const std::string kTitle = "title";
  const std::string kGuid = base::GenerateGUID();
  const syncer::UniquePosition kPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;

  // Create update with GUID.
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/kPosition));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId), NotNull());

  // Push an update for the same entity with a new GUID.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/"",
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // The GUID should not have been updated.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children().front()->guid(), Eq(kGuid));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteCreations) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2

  const std::string kId0 = "id0";
  const std::string kId1 = "id1";
  const std::string kId2 = "id2";

  syncer::UniquePosition pos0 = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UniquePosition pos1 = syncer::UniquePosition::After(
      pos0, syncer::UniquePosition::RandomSuffix());
  syncer::UniquePosition pos2 = syncer::UniquePosition::After(
      pos1, syncer::UniquePosition::RandomSuffix());

  // Constuct the updates list to have creations randomly ordered.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId2, /*parent_id=*/kBookmarkBarId, /*title=*/kId2,
      /*is_deletion=*/false, /*version=*/0, /*unique_position=*/pos2));
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId0,
                                             /*parent_id=*/kBookmarkBarId,
                                             /*title=*/kId0,
                                             /*is_deletion=*/false,
                                             /*version=*/0,
                                             /*unique_position=*/pos0));
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId1, /*parent_id=*/kBookmarkBarId, /*title=*/kId1,
      /*is_deletion=*/false, /*version=*/0, /*unique_position=*/pos1));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // All nodes should have been added to the model in the correct order.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(3u));
  EXPECT_THAT(bookmark_bar_node->children()[0]->GetTitle(),
              Eq(ASCIIToUTF16(kId0)));
  EXPECT_THAT(bookmark_bar_node->children()[1]->GetTitle(),
              Eq(ASCIIToUTF16(kId1)));
  EXPECT_THAT(bookmark_bar_node->children()[2]->GetTitle(),
              Eq(ASCIIToUTF16(kId2)));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteMovesToTheLeft) {
  // Start with structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2
  //  |- node3
  //  |- node4

  std::vector<std::string> ids;
  std::vector<std::string> guids;
  std::vector<syncer::UniquePosition> positions;

  syncer::UniquePosition position = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UpdateResponseDataList updates;
  for (int i = 0; i < 5; i++) {
    ids.push_back("node" + base::NumberToString(i));
    guids.push_back(base::GenerateGUID());
    position = syncer::UniquePosition::After(
        position, syncer::UniquePosition::RandomSuffix());
    positions.push_back(position);
    updates.push_back(CreateUpdateResponseData(
        /*server_id=*/ids[i], /*parent_id=*/kBookmarkBarId, /*guid=*/guids[i],
        /*title=*/ids[i],
        /*is_deletion=*/false, /*version=*/0,
        /*unique_position=*/positions[i]));
  }

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(5u));

  // Change it to this structure by moving node3 after node1.
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node3
  //  |- node2
  //  |- node4

  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/ids[3],
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/guids[3],
      /*title=*/ids[3],
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::Between(positions[1], positions[2],
                                      syncer::UniquePosition::RandomSuffix())));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // Model should have been updated.
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(5u));
  EXPECT_THAT(bookmark_bar_node->children()[2]->GetTitle(),
              Eq(ASCIIToUTF16(ids[3])));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteMovesToTheRight) {
  // Start with structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2
  //  |- node3
  //  |- node4

  std::vector<std::string> ids;
  std::vector<std::string> guids;
  std::vector<syncer::UniquePosition> positions;

  syncer::UniquePosition position = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UpdateResponseDataList updates;
  for (int i = 0; i < 5; i++) {
    ids.push_back("node" + base::NumberToString(i));
    guids.push_back(base::GenerateGUID());
    position = syncer::UniquePosition::After(
        position, syncer::UniquePosition::RandomSuffix());
    positions.push_back(position);
    updates.push_back(CreateUpdateResponseData(
        /*server_id=*/ids[i], /*parent_id=*/kBookmarkBarId, /*guid=*/guids[i],
        /*title=*/ids[i],
        /*is_deletion=*/false, /*version=*/0,
        /*unique_position=*/positions[i]));
  }

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(5u));

  // Change it to this structure by moving node1 after node3.
  // bookmark_bar
  //  |- node0
  //  |- node2
  //  |- node3
  //  |- node1
  //  |- node4

  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/ids[1],
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/guids[1],
      /*title=*/ids[1],
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::Between(positions[3], positions[4],
                                      syncer::UniquePosition::RandomSuffix())));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // Model should have been updated.
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(5u));
  EXPECT_THAT(bookmark_bar_node->children()[3]->GetTitle(),
              Eq(ASCIIToUTF16(ids[1])));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldPositionRemoteReparenting) {
  // Start with structure:
  // bookmark_bar
  //  |- node0
  //  |- node1
  //  |- node2
  //  |- node3
  //  |- node4

  std::vector<std::string> ids;
  std::vector<std::string> guids;
  std::vector<syncer::UniquePosition> positions;

  syncer::UniquePosition position = syncer::UniquePosition::InitialPosition(
      syncer::UniquePosition::RandomSuffix());
  syncer::UpdateResponseDataList updates;
  for (int i = 0; i < 5; i++) {
    ids.push_back("node" + base::NumberToString(i));
    guids.push_back(base::GenerateGUID());
    position = syncer::UniquePosition::After(
        position, syncer::UniquePosition::RandomSuffix());
    positions.push_back(position);
    updates.push_back(CreateUpdateResponseData(
        /*server_id=*/ids[i], /*parent_id=*/kBookmarkBarId, /*guid=*/guids[i],
        /*title=*/ids[i],
        /*is_deletion=*/false, /*version=*/0,
        /*unique_position=*/positions[i]));
  }

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(5u));

  // Change it to this structure by moving node4 under node1.
  // bookmark_bar
  //  |- node0
  //  |- node1
  //    |- node4
  //  |- node2
  //  |- node3

  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/ids[4],
      /*parent_id=*/ids[1],
      /*guid=*/guids[4],
      /*title=*/ids[4],
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // Model should have been updated.
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(4u));
  ASSERT_THAT(bookmark_bar_node->children()[1]->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children()[1]->children()[0]->GetTitle(),
              Eq(ASCIIToUTF16(ids[4])));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIgnoreNodeIfParentIsNotFolder) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0 (is_folder=false)
  //    |- node1

  const std::string kParentId = "parent_id";
  const std::string kChildId = "child_id";
  const std::string kTitle = "Title";
  const GURL kUrl("http://www.url.com");

  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = kParentId;
  data.parent_id = kBookmarkBarId;
  data.unique_position = syncer::UniquePosition::InitialPosition(
                             syncer::UniquePosition::RandomSuffix())
                             .ToProto();
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(base::GenerateGUID());
  // Use the server id as the title for simplicity.
  bookmark_specifics->set_legacy_canonicalized_title(kTitle);
  bookmark_specifics->set_url(kUrl.spec());
  data.is_folder = false;
  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;

  updates.push_back(std::move(response_data));

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kChildId, /*parent_id=*/kParentId, /*is_deletion=*/false));

  base::HistogramTester histogram_tester;
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(bookmark_bar_node->children().size(), 1U);
  EXPECT_TRUE(bookmark_bar_node->children()[0]->children().empty());
  histogram_tester.ExpectBucketCount(
      "Sync.ProblematicServerSideBookmarks",
      /*sample=*/ExpectedRemoteBookmarkUpdateError::kParentNotFolder,
      /*count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldMergeFaviconUponRemoteCreationsWithFavicon) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0

  const std::string kTitle = "Title";
  const GURL kUrl("http://www.url.com");
  const GURL kIconUrl("http://www.icon-url.com");

  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = "server_id";
  data.parent_id = kBookmarkBarId;
  data.unique_position = syncer::UniquePosition::InitialPosition(
                             syncer::UniquePosition::RandomSuffix())
                             .ToProto();
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(base::GenerateGUID());
  // Use the server id as the title for simplicity.
  bookmark_specifics->set_legacy_canonicalized_title(kTitle);
  bookmark_specifics->set_url(kUrl.spec());
  bookmark_specifics->set_icon_url(kIconUrl.spec());
  bookmark_specifics->set_favicon("PNG");
  data.is_folder = false;
  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;

  updates.push_back(std::move(response_data));

  EXPECT_CALL(*favicon_service(),
              AddPageNoVisitForBookmark(kUrl, base::UTF8ToUTF16(kTitle)));
  EXPECT_CALL(*favicon_service(), MergeFavicon(kUrl, kIconUrl, _, _, _));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldDeleteFaviconUponRemoteCreationsWithoutFavicon) {
  // Prepare creation updates to construct this structure:
  // bookmark_bar
  //  |- node0

  const std::string kTitle = "Title";
  const GURL kUrl("http://www.url.com");

  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = "server_id";
  data.parent_id = kBookmarkBarId;
  data.unique_position = syncer::UniquePosition::InitialPosition(
                             syncer::UniquePosition::RandomSuffix())
                             .ToProto();
  sync_pb::BookmarkSpecifics* bookmark_specifics =
      data.specifics.mutable_bookmark();
  bookmark_specifics->set_guid(base::GenerateGUID());
  // Use the server id as the title for simplicity.
  bookmark_specifics->set_legacy_canonicalized_title(kTitle);
  bookmark_specifics->set_url(kUrl.spec());
  data.is_folder = false;
  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;

  updates.push_back(std::move(response_data));

  EXPECT_CALL(*favicon_service(),
              DeleteFaviconMappings(ElementsAre(kUrl),
                                    favicon_base::IconType::kFavicon));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
}

// This tests the case when a local creation is successfully committed to the
// server but the commit respone isn't received for some reason. Further updates
// to that entity should update the sync id in the tracker.
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldUpdateSyncIdWhenRecevingAnUpdateForNewlyCreatedLocalNode) {
  const std::string kCacheGuid = "generated_id";
  const std::string kOriginatorClientItemId = base::GenerateGUID();
  const std::string kSyncId = "server_id";
  const int64_t kServerVersion = 1000;
  const base::Time kModificationTime(base::Time::Now() -
                                     base::TimeDelta::FromSeconds(1));

  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_cache_guid(kCacheGuid);
  model_type_state.set_initial_sync_done(true);

  const sync_pb::UniquePosition unique_position;
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bookmark_specifics = specifics.mutable_bookmark();
  bookmark_specifics->set_guid(base::GenerateGUID());
  bookmark_specifics->set_legacy_canonicalized_title("Title");
  bookmarks::BookmarkNode node(/*id=*/1, base::GenerateGUID(), GURL());
  // Track a sync entity (similar to what happens after a local creation). The
  // |originator_client_item_id| is used a temp sync id and mark the entity that
  // it needs to be committed..
  const SyncedBookmarkTracker::Entity* entity =
      tracker()->Add(&node, /*sync_id=*/kOriginatorClientItemId, kServerVersion,
                     kModificationTime, unique_position, specifics);
  tracker()->IncrementSequenceNumber(entity);

  ASSERT_THAT(tracker()->GetEntityForSyncId(kOriginatorClientItemId),
              Eq(entity));

  // Now receive an update with the actual server id.
  syncer::UpdateResponseDataList updates;
  syncer::EntityData data;
  data.id = kSyncId;
  data.originator_cache_guid = kCacheGuid;
  data.originator_client_item_id = kOriginatorClientItemId;
  // Set the other required fields.
  data.unique_position = syncer::UniquePosition::InitialPosition(
                             syncer::UniquePosition::RandomSuffix())
                             .ToProto();
  data.specifics = specifics;
  data.specifics.mutable_bookmark()->set_guid(kOriginatorClientItemId);
  data.is_folder = true;

  syncer::UpdateResponseData response_data;
  response_data.entity = std::move(data);
  // Similar to what's done in the loopback_server.
  response_data.response_version = 0;
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // The sync id in the tracker should have been updated.
  EXPECT_THAT(tracker()->GetEntityForSyncId(kOriginatorClientItemId), IsNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(kSyncId), Eq(entity));
  EXPECT_THAT(entity->metadata()->server_id(), Eq(kSyncId));
  EXPECT_THAT(entity->bookmark_node(), Eq(&node));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldRecommitWhenEncryptionIsOutOfDate) {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_encryption_key_name("encryption_key_name");
  tracker()->set_model_type_state(model_type_state);

  const std::string kId0 = "id0";
  syncer::UpdateResponseDataList updates;
  syncer::UpdateResponseData response_data =
      CreateUpdateResponseData(/*server_id=*/kId0,
                               /*parent_id=*/kBookmarkBarId,
                               /*is_deletion=*/false);
  response_data.encryption_key_name = "out_of_date_encryption_key_name";
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId0), NotNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId0)->IsUnsynced(), Eq(true));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldRecommitWhenGotNewEncryptionRequirements) {
  const std::string kId0 = "id0";

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId0,
                                             /*parent_id=*/kBookmarkBarId,
                                             /*is_deletion=*/false));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId0), NotNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId0)->IsUnsynced(), Eq(false));

  updates_handler()->Process(syncer::UpdateResponseDataList(),
                             /*got_new_encryption_requirements=*/true);
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId0)->IsUnsynced(), Eq(true));
  // Permanent nodes shouldn't be committed. They are only created on the server
  // and synced down.
  EXPECT_THAT(tracker()->GetEntityForSyncId(kBookmarkBarId)->IsUnsynced(),
              Eq(false));
}

TEST_F(
    BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
    ShouldNotRecommitWhenEncryptionKeyNameMistmatchWithConflictWithDeletions) {
  sync_pb::ModelTypeState model_type_state;
  model_type_state.set_encryption_key_name("encryption_key_name");
  tracker()->set_model_type_state(model_type_state);

  // Create the bookmark with same encryption key name.
  const std::string kId = "id";
  const std::string kTitle = "title";
  syncer::UpdateResponseDataList updates;
  syncer::UpdateResponseData response_data =
      CreateUpdateResponseData(/*server_id=*/kId,
                               /*parent_id=*/kBookmarkBarId,
                               /*is_deletion=*/false);
  response_data.encryption_key_name = "encryption_key_name";
  updates.push_back(std::move(response_data));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  // The bookmark has been added and tracked.
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());

  // Remove the bookmark from the local bookmark model.
  bookmark_model()->Remove(bookmark_bar_node->children().front().get());
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(0u));

  // Mark the entity as deleted locally.
  tracker()->MarkDeleted(entity);
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId)->IsUnsynced(), Eq(true));

  // Push a remote deletion for the same entity with an out of date encryption
  // key name.
  updates.clear();
  syncer::UpdateResponseData response_data2 =
      CreateUpdateResponseData(/*server_id=*/kId,
                               /*parent_id=*/kBookmarkBarId,
                               /*is_deletion=*/true);
  response_data2.encryption_key_name = "out_of_date_encryption_key_name";
  // Increment the server version to make sure the update isn't discarded as
  // reflection.
  response_data2.response_version++;
  updates.push_back(std::move(response_data2));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved by
  // removing local entity since both changes are deletions.
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId), IsNull());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldNotRecommitUptoDateEntitiesWhenGotNewEncryptionRequirements) {
  const std::string kId0 = "id0";

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(/*server_id=*/kId0,
                                             /*parent_id=*/kBookmarkBarId,
                                             /*is_deletion=*/false));
  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId0), NotNull());
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId0)->IsUnsynced(), Eq(false));

  // Push another update to for the same entity.
  syncer::UpdateResponseData response_data =
      CreateUpdateResponseData(/*server_id=*/kId0,
                               /*parent_id=*/kBookmarkBarId,
                               /*is_deletion=*/false);

  // Increment the server version to make sure the update isn't discarded as
  // reflection.
  response_data.response_version++;
  syncer::UpdateResponseDataList new_updates;
  new_updates.push_back(std::move(response_data));
  updates_handler()->Process(new_updates,
                             /*got_new_encryption_requirements=*/true);
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId0)->IsUnsynced(), Eq(false));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalAndRemoteDeletionsByMatchingThem) {
  const std::string kId = "id";
  const std::string kTitle = "title";

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));

  // Remove the bookmark from the local bookmark model.
  bookmark_model()->Remove(bookmark_bar_node->children().front().get());
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(0u));

  // Mark the entity as deleted locally.
  tracker()->MarkDeleted(entity);
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push a remote deletion for the same entity.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*title=*/std::string(),
      /*is_deletion=*/true,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved by
  // removing local entity since both changes.
  EXPECT_THAT(tracker()->GetEntityForSyncId(kId), IsNull());
  // Make sure the bookmark hasn't been resurrected.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(0u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalUpdateAndRemoteDeletionWithLocal) {
  const std::string kId = "id";
  const std::string kTitle = "title";

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  // Mark the entity as modified locally.
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push a remote deletion for the same entity.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*title=*/std::string(),
      /*is_deletion=*/true,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved with the
  // local version that will be committed later.
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId), Eq(entity));
  EXPECT_THAT(entity->IsUnsynced(), Eq(true));
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalDeletionAndRemoteUpdateByRemote) {
  const std::string kId = "id";
  const std::string kTitle = "title";

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);
  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));

  // Remove the bookmark from the local bookmark model.
  bookmark_model()->Remove(bookmark_bar_node->children().front().get());
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(0u));

  // Mark the entity as deleted locally.
  tracker()->MarkDeleted(entity);
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push an update for the same entity.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved with the
  // remote version. The implementation may or may not reuse |entity|, so let's
  // look it up again.
  entity = tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  EXPECT_THAT(entity->IsUnsynced(), Eq(false));
  EXPECT_THAT(entity->metadata()->is_deleted(), Eq(false));

  // The bookmark should have been resurrected.
  EXPECT_THAT(bookmark_bar_node->children().size(), Eq(1u));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalAndRemoteUpdatesWithMatchingThem) {
  const std::string kId = "id";
  const std::string kGuid = base::GenerateGUID();
  const std::string kTitle = "title";
  const syncer::UniquePosition kPosition =
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix());

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/kPosition));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  // Mark the entity as modified locally.
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push an update for the same entity with the same information.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/kPosition));

  updates_handler()->Process(updates,
                             /*got_new_encryption_requirements=*/false);

  // There should have been conflict but both local and remote updates should
  // match. The conflict should have been resolved.
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId), Eq(entity));
  EXPECT_THAT(entity->IsUnsynced(), Eq(false));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldResolveConflictBetweenLocalAndRemoteUpdatesWithRemote) {
  const std::string kId = "id";
  const std::string kGuid = base::GenerateGUID();
  const std::string kTitle = "title";
  const std::string kNewRemoteTitle = "remote title";

  syncer::UpdateResponseDataList updates;

  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                               favicon_service(), tracker());
  updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);

  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_THAT(entity->IsUnsynced(), Eq(false));

  // Mark the entity as modified locally.
  tracker()->IncrementSequenceNumber(entity);
  ASSERT_THAT(entity->IsUnsynced(), Eq(true));

  // Push an update for the same entity with a new title.
  updates.clear();
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kNewRemoteTitle,
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));

  updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);

  // There should have been conflict, and it should have been resolved with the
  // remote version.
  ASSERT_THAT(tracker()->GetEntityForSyncId(kId), Eq(entity));
  EXPECT_THAT(entity->IsUnsynced(), Eq(false));
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_THAT(bookmark_bar_node->children().size(), Eq(1u));
  EXPECT_THAT(bookmark_bar_node->children().front()->GetTitle(),
              Eq(ASCIIToUTF16(kNewRemoteTitle)));
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIncrementSequenceNumberOnConflict) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(
      switches::kSyncReuploadBookmarkFullTitles);

  const std::string kId = "id";
  const std::string kGuid = base::GenerateGUID();
  const std::string kTitle = "title";
  const std::string kNewTitle = "New title";

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));
  updates.back().entity.specifics.mutable_bookmark()->clear_full_title();

  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }

  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  EXPECT_TRUE(entity->IsUnsynced());

  // Check reupload on conflict with new title.
  updates.back()
      .entity.specifics.mutable_bookmark()
      ->set_legacy_canonicalized_title(kNewTitle);
  updates.back().response_version++;
  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  const bookmarks::BookmarkNode* node =
      bookmark_bar_node->children().front().get();
  EXPECT_EQ(base::UTF16ToUTF8(node->GetTitle()), kNewTitle);
  EXPECT_TRUE(entity->IsUnsynced());

  // Check reupload on conflict when specifics matches.
  updates.back().response_version++;
  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }
  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  EXPECT_EQ(bookmark_bar_node->children().front().get(), node);
  EXPECT_EQ(base::UTF16ToUTF8(node->GetTitle()), kNewTitle);
  EXPECT_TRUE(entity->IsUnsynced());
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldIncrementSequenceNumberOnUpdate) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(
      switches::kSyncReuploadBookmarkFullTitles);

  const std::string kId = "id";
  const std::string kGuid = base::GenerateGUID();
  const std::string kTitle = "title";
  const std::string kRemoteTitle = "New title";

  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kTitle,
      /*is_deletion=*/false,
      /*version=*/0,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));
  updates.back().entity.specifics.mutable_bookmark()->set_full_title(kTitle);

  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }

  const SyncedBookmarkTracker::Entity* entity =
      tracker()->GetEntityForSyncId(kId);
  ASSERT_THAT(entity, NotNull());
  ASSERT_FALSE(entity->IsUnsynced());

  // Check reupload on conflict.
  updates.back()
      .entity.specifics.mutable_bookmark()
      ->set_legacy_canonicalized_title(kRemoteTitle);
  updates.back().entity.specifics.mutable_bookmark()->clear_full_title();
  updates.back().response_version++;
  {
    BookmarkRemoteUpdatesHandler updates_handler(bookmark_model(),
                                                 favicon_service(), tracker());
    updates_handler.Process(updates, /*got_new_encryption_requirements=*/false);
  }
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  ASSERT_EQ(1u, bookmark_bar_node->children().size());
  const bookmarks::BookmarkNode* node =
      bookmark_bar_node->children().front().get();
  EXPECT_EQ(node->GetTitle(), base::UTF8ToUTF16(kRemoteTitle));
  EXPECT_TRUE(entity->IsUnsynced());
}

// Tests the case when the bookmark was removed and restored with a new sync_id.
// In this case the tracker contains two entities: a tombstone with a real
// server_id and a new entity with local sync_id. This was possible due to some
// past bug (see https://crbug.com/1071061).
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessUpdateWhileDuplicateEntities) {
  const std::string kTitle = "Title";
  const std::string kNewTitle = "New Title";
  const GURL kUrl("http://www.url.com");
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = bookmark_model()->AddURL(
      bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle), kUrl);

  // The only way to get into incorrect state is to generate stored metadata
  // for the entity tracker.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());

  // Add two entities for the same bookmark node. One is a tombstone and another
  // one is a new entity. A new entity is not committed yet and has local id
  // which is GUID. A tombstone has the server id. This is will be used to
  // generate an update.
  const std::string kLocalId = node->guid();
  const std::string kServerId = "server_id";
  sync_pb::BookmarkMetadata* node_metadata =
      model_metadata.add_bookmarks_metadata();
  *node_metadata =
      CreateNodeMetadata(node->id(), kLocalId,
                         syncer::UniquePosition::InitialPosition(
                             syncer::UniquePosition::RandomSuffix()));
  sync_pb::BookmarkMetadata* tombstone_metadata =
      model_metadata.add_bookmarks_metadata();
  *tombstone_metadata = CreateTombstoneMetadata(kServerId);

  // Be sure that there is not client tag hashes yet.
  ASSERT_FALSE(node_metadata->metadata().has_client_tag_hash());
  ASSERT_FALSE(tombstone_metadata->metadata().has_client_tag_hash());

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          bookmark_model(), std::move(model_metadata));
  ASSERT_THAT(tracker, NotNull());
  ASSERT_EQ(5u, tracker->GetAllEntities().size());

  // Generate an update with two different ids.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kServerId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/node->guid(),
      /*title=*/kNewTitle,
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));
  updates.back().entity.specifics.mutable_bookmark()->set_url(kUrl.spec());
  updates.back().entity.is_folder = false;
  BookmarkRemoteUpdatesHandler updates_handler(
      bookmark_model(), favicon_service(), tracker.get());
  updates_handler.Process(std::move(updates),
                          /*got_new_encryption_requirements=*/false);

  // Check that one entity was removed while updating.
  EXPECT_EQ(4u, tracker->GetAllEntities().size());
  const SyncedBookmarkTracker::Entity* entity =
      tracker->GetEntityForBookmarkNode(node);
  EXPECT_THAT(entity, NotNull());
  EXPECT_EQ(entity->metadata()->server_id(), kServerId);
  EXPECT_FALSE(entity->metadata()->is_deleted());
  EXPECT_EQ(base::UTF16ToUTF8(node->GetTitledUrlNodeTitle()), kNewTitle);
}

// This test is similar to ShouldProcessUpdateWhileDuplicateEntities. The
// difference is that in this test both entities are tombstones.
TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessUpdateWhileDuplicateTombstones) {
  const std::string kTitle = "Title";
  const std::string kNewTitle = "New Title";
  const GURL kUrl("http://www.url.com");
  const std::string kGuid = base::GenerateGUID();

  // The only way to get into incorrect state is to generate stored metadata
  // for the entity tracker.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());

  // Add two entities for the same bookmark node. Both are tombstones. One of
  // tombstones is not committed yet and has local id which is GUID. Another one
  // has the server id.
  const std::string kLocalId = kGuid;
  const std::string kServerId = "server_id";
  sync_pb::BookmarkMetadata* node_metadata =
      model_metadata.add_bookmarks_metadata();
  *node_metadata = CreateTombstoneMetadata(kLocalId);
  sync_pb::BookmarkMetadata* tombstone_metadata =
      model_metadata.add_bookmarks_metadata();
  *tombstone_metadata = CreateTombstoneMetadata(kServerId);

  // Be sure that there is not client tag hashes yet.
  ASSERT_FALSE(node_metadata->metadata().has_client_tag_hash());
  ASSERT_FALSE(tombstone_metadata->metadata().has_client_tag_hash());

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          bookmark_model(), std::move(model_metadata));
  ASSERT_THAT(tracker, NotNull());
  ASSERT_EQ(5u, tracker->GetAllEntities().size());

  // Generate an update with two different ids.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kServerId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kGuid,
      /*title=*/kNewTitle,
      /*is_deletion=*/false,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));
  updates.back().entity.specifics.mutable_bookmark()->set_url(kUrl.spec());
  updates.back().entity.is_folder = false;

  base::HistogramTester histogram_tester;
  BookmarkRemoteUpdatesHandler updates_handler(
      bookmark_model(), favicon_service(), tracker.get());
  updates_handler.Process(std::move(updates),
                          /*got_new_encryption_requirements=*/false);

  // Check that one entity was removed while updating.
  EXPECT_EQ(4u, tracker->GetAllEntities().size());
  const SyncedBookmarkTracker::Entity* entity =
      tracker->GetEntityForSyncId(kServerId);
  ASSERT_THAT(entity, NotNull());
  EXPECT_EQ(entity->metadata()->server_id(), kServerId);
  EXPECT_FALSE(entity->metadata()->is_deleted());

  const bookmarks::BookmarkNode* node = entity->bookmark_node();
  ASSERT_THAT(entity, NotNull());
  EXPECT_EQ(base::UTF16ToUTF8(node->GetTitledUrlNodeTitle()), kNewTitle);

  histogram_tester.ExpectBucketCount(
      "Sync.DuplicateBookmarkEntityOnRemoteUpdateCondition",
      /*sample=*/
      BookmarkRemoteUpdatesHandler::
          DuplicateBookmarkEntityOnRemoteUpdateCondition::kServerIdTombstone,
      /*count=*/1);
}

TEST_F(BookmarkRemoteUpdatesHandlerWithInitialMergeTest,
       ShouldProcessRemoveUpdateWhileDuplicateEntities) {
  const std::string kTitle = "Title";
  const std::string kNewTitle = "New Title";
  const GURL kUrl("http://www.url.com");
  const bookmarks::BookmarkNode* bookmark_bar_node =
      bookmark_model()->bookmark_bar_node();
  const bookmarks::BookmarkNode* node = bookmark_model()->AddURL(
      bookmark_bar_node, /*index=*/0, base::UTF8ToUTF16(kTitle), kUrl);

  // The only way to get into incorrect state is to generate stored metadata
  // for the entity tracker.
  sync_pb::BookmarkModelMetadata model_metadata =
      CreateMetadataForPermanentNodes(bookmark_model());

  // Add two entities for the same bookmark node. One is a tombstone and another
  // one is a new entity. A new entity is not committed yet and has local id
  // which is GUID. A tombstone has the server id. This is will be used to
  // generate an update.
  const std::string kLocalId = node->guid();
  const std::string kServerId = "server_id";
  sync_pb::BookmarkMetadata* node_metadata =
      model_metadata.add_bookmarks_metadata();
  *node_metadata =
      CreateNodeMetadata(node->id(), kLocalId,
                         syncer::UniquePosition::InitialPosition(
                             syncer::UniquePosition::RandomSuffix()));
  sync_pb::BookmarkMetadata* tombstone_metadata =
      model_metadata.add_bookmarks_metadata();
  *tombstone_metadata = CreateTombstoneMetadata(kServerId);

  // Be sure that there is not client tag hashes yet.
  ASSERT_FALSE(node_metadata->metadata().has_client_tag_hash());
  ASSERT_FALSE(tombstone_metadata->metadata().has_client_tag_hash());

  std::unique_ptr<SyncedBookmarkTracker> tracker =
      SyncedBookmarkTracker::CreateFromBookmarkModelAndMetadata(
          bookmark_model(), std::move(model_metadata));
  ASSERT_THAT(tracker, NotNull());
  ASSERT_EQ(5u, tracker->GetAllEntities().size());

  // Generate a remote deletion with two different ids.
  syncer::UpdateResponseDataList updates;
  updates.push_back(CreateUpdateResponseData(
      /*server_id=*/kServerId,
      /*parent_id=*/kBookmarkBarId,
      /*guid=*/kLocalId,
      /*title=*/"",
      /*is_deletion=*/true,
      /*version=*/1,
      /*unique_position=*/
      syncer::UniquePosition::InitialPosition(
          syncer::UniquePosition::RandomSuffix())));
  updates.back().entity.is_folder = false;
  BookmarkRemoteUpdatesHandler updates_handler(
      bookmark_model(), favicon_service(), tracker.get());
  updates_handler.Process(std::move(updates),
                          /*got_new_encryption_requirements=*/false);

  // Check that all entities were removed after update.
  EXPECT_EQ(3u, tracker->GetAllEntities().size());
  EXPECT_THAT(tracker->GetEntityForSyncId(kServerId), IsNull());
  EXPECT_THAT(tracker->GetEntityForSyncId(kLocalId), IsNull());
  EXPECT_TRUE(bookmark_bar_node->children().empty());
}

}  // namespace

}  // namespace sync_bookmarks
