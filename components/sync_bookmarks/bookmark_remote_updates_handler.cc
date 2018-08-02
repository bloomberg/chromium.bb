// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_remote_updates_handler.h"

#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/strings/string_piece.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/protocol/unique_position.pb.h"
#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

namespace sync_bookmarks {

namespace {

// The sync protocol identifies top-level entities by means of well-known tags,
// (aka server defined tags) which should not be confused with titles or client
// tags that aren't supported by bookmarks (at the time of writing). Each tag
// corresponds to a singleton instance of a particular top-level node in a
// user's share; the tags are consistent across users. The tags allow us to
// locate the specific folders whose contents we care about synchronizing,
// without having to do a lookup by name or path.  The tags should not be made
// user-visible. For example, the tag "bookmark_bar" represents the permanent
// node for bookmarks bar in Chrome. The tag "other_bookmarks" represents the
// permanent folder Other Bookmarks in Chrome.
//
// It is the responsibility of something upstream (at time of writing, the sync
// server) to create these tagged nodes when initializing sync for the first
// time for a user.  Thus, once the backend finishes initializing, the
// ProfileSyncService can rely on the presence of tagged nodes.
const char kBookmarkBarTag[] = "bookmark_bar";
const char kMobileBookmarksTag[] = "synced_bookmarks";
const char kOtherBookmarksTag[] = "other_bookmarks";

// Id is created by concatenating the specifics field number and the server tag
// similar to LookbackServerEntity::CreateId() that uses
// GetSpecificsFieldNumberFromModelType() to compute the field number.
const char kBookmarksRootId[] = "32904_google_chrome_bookmarks";

// Recursive method to traverse a forest created by ReorderUpdates() to to
// emit updates in top-down order. |ordered_updates| must not be null because
// traversed updates are appended to |*ordered_updates|.
void TraverseAndAppendChildren(
    const base::StringPiece& node_id,
    const std::unordered_map<base::StringPiece,
                             const syncer::UpdateResponseData*,
                             base::StringPieceHash>& id_to_updates,
    const std::unordered_map<base::StringPiece,
                             std::vector<base::StringPiece>,
                             base::StringPieceHash>& node_to_children,
    std::vector<const syncer::UpdateResponseData*>* ordered_updates) {
  // If no children to traverse, we are done.
  if (node_to_children.count(node_id) == 0) {
    return;
  }
  // Recurse over all children.
  for (const base::StringPiece& child : node_to_children.at(node_id)) {
    DCHECK_NE(id_to_updates.count(child), 0U);
    ordered_updates->push_back(id_to_updates.at(child));
    TraverseAndAppendChildren(child, id_to_updates, node_to_children,
                              ordered_updates);
  }
}

int ComputeChildNodeIndex(const bookmarks::BookmarkNode* parent,
                          const sync_pb::UniquePosition& unique_position,
                          const SyncedBookmarkTracker* bookmark_tracker) {
  const syncer::UniquePosition position =
      syncer::UniquePosition::FromProto(unique_position);
  for (int i = 0; i < parent->child_count(); ++i) {
    const bookmarks::BookmarkNode* child = parent->GetChild(i);
    const SyncedBookmarkTracker::Entity* child_entity =
        bookmark_tracker->GetEntityForBookmarkNode(child);
    DCHECK(child_entity);
    const syncer::UniquePosition child_position =
        syncer::UniquePosition::FromProto(
            child_entity->metadata()->unique_position());
    if (position.LessThan(child_position)) {
      return i;
    }
  }
  return parent->child_count();
}

}  // namespace

BookmarkRemoteUpdatesHandler::BookmarkRemoteUpdatesHandler(
    bookmarks::BookmarkModel* bookmark_model,
    SyncedBookmarkTracker* bookmark_tracker)
    : bookmark_model_(bookmark_model), bookmark_tracker_(bookmark_tracker) {
  DCHECK(bookmark_model);
  DCHECK(bookmark_tracker);
}

void BookmarkRemoteUpdatesHandler::Process(
    const syncer::UpdateResponseDataList& updates) {
  for (const syncer::UpdateResponseData* update : ReorderUpdates(&updates)) {
    const syncer::EntityData& update_entity = update->entity.value();
    // TODO(crbug.com/516866): Check |update_entity| for sanity.
    // 1. Has bookmark specifics or no specifics in case of delete.
    // 2. All meta info entries in the specifics have unique keys.
    // 3. Unique position is valid.
    const SyncedBookmarkTracker::Entity* tracked_entity =
        bookmark_tracker_->GetEntityForSyncId(update_entity.id);
    if (update_entity.is_deleted()) {
      ProcessRemoteDelete(update_entity, tracked_entity);
      continue;
    }
    if (!tracked_entity) {
      ProcessRemoteCreate(*update);
      continue;
    }
    // Ignore changes to the permanent nodes (e.g. bookmarks bar). We only care
    // about their children.
    if (bookmark_model_->is_permanent_node(tracked_entity->bookmark_node())) {
      continue;
    }
    ProcessRemoteUpdate(*update, tracked_entity);
  }
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderUpdatesForTest(
    const syncer::UpdateResponseDataList* updates) {
  return ReorderUpdates(updates);
}

// static
std::vector<const syncer::UpdateResponseData*>
BookmarkRemoteUpdatesHandler::ReorderUpdates(
    const syncer::UpdateResponseDataList* updates) {
  // This method sorts the remote updates according to the following rules:
  // 1. Creations and updates come before deletions.
  // 2. Parent creation/update should come before child creation/update.
  // 3. No need to further order deletions. Parent deletions can happen before
  //    child deletions. This is safe because all updates (e.g. moves) should
  //    have been processed already.

  // The algorithm works by constructing a forest of all non-deletion updates
  // and then traverses each tree in the forest recursively: Forest
  // Construction:
  // 1. Iterate over all updates and construct the |parent_to_children| map and
  //    collect all parents in |roots|.
  // 2. Iterate over all updates again and drop any parent that has a
  //    coressponding update. What's left in |roots| are the roots of the
  //    forest.
  // 3. Start at each root in |roots|, emit the update and recurse over its
  //    children.

  std::unordered_map<base::StringPiece, const syncer::UpdateResponseData*,
                     base::StringPieceHash>
      id_to_updates;
  std::set<base::StringPiece> roots;
  std::unordered_map<base::StringPiece, std::vector<base::StringPiece>,
                     base::StringPieceHash>
      parent_to_children;

  // Add only non-deletions to |id_to_updates|.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity.value();
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      continue;
    }
    if (update_entity.is_deleted()) {
      continue;
    }
    id_to_updates[update_entity.id] = &update;
  }
  // Iterate over |id_to_updates| and construct |roots| and
  // |parent_to_children|.
  for (const std::pair<base::StringPiece, const syncer::UpdateResponseData*>&
           pair : id_to_updates) {
    const syncer::EntityData& update_entity = pair.second->entity.value();
    parent_to_children[update_entity.parent_id].push_back(update_entity.id);
    // If this entity's parent has no pending update, add it to |roots|.
    if (id_to_updates.count(update_entity.parent_id) == 0) {
      roots.insert(update_entity.parent_id);
    }
  }
  // |roots| contains only root of all trees in the forest all of which are
  // ready to be processed because none has a pending update.
  std::vector<const syncer::UpdateResponseData*> ordered_updates;
  for (const base::StringPiece& root : roots) {
    TraverseAndAppendChildren(root, id_to_updates, parent_to_children,
                              &ordered_updates);
  }

  int root_node_updates_count = 0;
  // Add deletions.
  for (const syncer::UpdateResponseData& update : *updates) {
    const syncer::EntityData& update_entity = update.entity.value();
    // Ignore updates to root nodes.
    if (update_entity.parent_id == "0") {
      root_node_updates_count++;
      continue;
    }
    if (update_entity.is_deleted()) {
      ordered_updates.push_back(&update);
    }
  }
  // All non root updates should have been included in |ordered_updates|.
  DCHECK_EQ(updates->size(), ordered_updates.size() + root_node_updates_count);
  return ordered_updates;
}

void BookmarkRemoteUpdatesHandler::ProcessRemoteCreate(
    const syncer::UpdateResponseData& update) {
  // Because the Synced Bookmarks node can be created server side, it's possible
  // it'll arrive at the client as an update. In that case it won't have been
  // associated at startup, the GetChromeNodeFromSyncId call above will return
  // null, and we won't detect it as a permanent node, resulting in us trying to
  // create it here (which will fail). Therefore, we add special logic here just
  // to detect the Synced Bookmarks folder.
  const syncer::EntityData& update_entity = update.entity.value();
  DCHECK(!update_entity.is_deleted());
  if (update_entity.parent_id == kBookmarksRootId) {
    // Associate permanent folders.
    // TODO(crbug.com/516866): Method documentation says this method should be
    // used in initial sync only. Make sure this is the case.
    AssociatePermanentFolder(update);
    return;
  }
  if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                update_entity.is_folder)) {
    // Ignore creations with invalid specifics.
    DLOG(ERROR) << "Couldn't add bookmark with an invalid specifics.";
    return;
  }
  const bookmarks::BookmarkNode* parent_node = GetParentNode(update_entity);
  if (!parent_node) {
    // If we cannot find the parent, we can do nothing.
    DLOG(ERROR) << "Could not find parent of node being added."
                << " Node title: " << update_entity.specifics.bookmark().title()
                << ", parent id = " << update_entity.parent_id;
    return;
  }
  const bookmarks::BookmarkNode* bookmark_node =
      CreateBookmarkNodeFromSpecifics(
          update_entity.specifics.bookmark(), parent_node,
          ComputeChildNodeIndex(parent_node, update_entity.unique_position,
                                bookmark_tracker_),
          update_entity.is_folder, bookmark_model_);
  if (!bookmark_node) {
    // We ignore bookmarks we can't add.
    DLOG(ERROR) << "Failed to create bookmark node with title "
                << update_entity.specifics.bookmark().title() << " and url "
                << update_entity.specifics.bookmark().url();
    return;
  }
  bookmark_tracker_->Add(update_entity.id, bookmark_node,
                         update.response_version, update_entity.creation_time,
                         update_entity.unique_position,
                         update_entity.specifics);
}

void BookmarkRemoteUpdatesHandler::ProcessRemoteUpdate(
    const syncer::UpdateResponseData& update,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  const syncer::EntityData& update_entity = update.entity.value();
  // Can only update existing nodes.
  DCHECK(tracked_entity);
  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));
  // Must not be a deletion.
  DCHECK(!update_entity.is_deleted());
  if (!IsValidBookmarkSpecifics(update_entity.specifics.bookmark(),
                                update_entity.is_folder)) {
    // Ignore updates with invalid specifics.
    DLOG(ERROR) << "Couldn't update bookmark with an invalid specifics.";
    return;
  }
  if (tracked_entity->IsUnsynced()) {
    // TODO(crbug.com/516866): Handle conflict resolution.
    return;
  }

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  const bookmarks::BookmarkNode* old_parent = node->parent();

  const SyncedBookmarkTracker::Entity* new_parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!new_parent_entity) {
    DLOG(ERROR) << "Could not update node. Parent node doesn't exist: "
                << update_entity.parent_id;
    return;
  }
  const bookmarks::BookmarkNode* new_parent =
      new_parent_entity->bookmark_node();
  if (!new_parent) {
    DLOG(ERROR)
        << "Could not update node. Parent node has been deleted already.";
    return;
  }
  // Node update could be either in the node data (e.g. title or
  // unique_position), or it could be that the node has moved under another
  // parent without any data change. Should check both the data and the parent
  // to confirm that no updates to the model are needed.
  if (tracked_entity->MatchesDataIgnoringParent(update_entity) &&
      new_parent == old_parent) {
    bookmark_tracker_->Update(update_entity.id, update.response_version,
                              update_entity.modification_time,
                              update_entity.unique_position,
                              update_entity.specifics);
    return;
  }
  if (update_entity.is_folder != node->is_folder()) {
    DLOG(ERROR) << "Could not update node. Remote node is a "
                << (update_entity.is_folder ? "folder" : "bookmark")
                << " while local node is a "
                << (node->is_folder() ? "folder" : "bookmark");
    return;
  }
  UpdateBookmarkNodeFromSpecifics(update_entity.specifics.bookmark(), node,
                                  bookmark_model_);
  // Compute index information before updating the |bookmark_tracker_|.
  const int old_index = old_parent->GetIndexOf(node);
  const int new_index = ComputeChildNodeIndex(
      new_parent, update_entity.unique_position, bookmark_tracker_);
  bookmark_tracker_->Update(update_entity.id, update.response_version,
                            update_entity.modification_time,
                            update_entity.unique_position,
                            update_entity.specifics);

  if (new_parent == old_parent &&
      (new_index == old_index || new_index == old_index + 1)) {
    // Node hasn't moved. No more work to do.
    return;
  }
  // Node has moved to another position under the same parent. Update the model.
  // BookmarkModel takes care of placing the node in the correct position if the
  // node is move to the left. (i.e. no need to subtract one from |new_index|).
  bookmark_model_->Move(node, new_parent, new_index);
}

void BookmarkRemoteUpdatesHandler::ProcessRemoteDelete(
    const syncer::EntityData& update_entity,
    const SyncedBookmarkTracker::Entity* tracked_entity) {
  DCHECK(update_entity.is_deleted());

  DCHECK_EQ(tracked_entity,
            bookmark_tracker_->GetEntityForSyncId(update_entity.id));

  // Handle corner cases first.
  if (tracked_entity == nullptr) {
    // Process deletion only if the entity is still tracked. It could have
    // been recursively deleted already with an earlier deletion of its
    // parent.
    DVLOG(1) << "Received remote delete for a non-existing item.";
    return;
  }

  const bookmarks::BookmarkNode* node = tracked_entity->bookmark_node();
  // Ignore changes to the permanent top-level nodes.  We only care about
  // their children.
  if (bookmark_model_->is_permanent_node(node)) {
    return;
  }
  // Remove the entities of |node| and its children.
  RemoveEntityAndChildrenFromTracker(node);
  // Remove the node and its children from the model.
  bookmark_model_->Remove(node);
}

void BookmarkRemoteUpdatesHandler::RemoveEntityAndChildrenFromTracker(
    const bookmarks::BookmarkNode* node) {
  const SyncedBookmarkTracker::Entity* entity =
      bookmark_tracker_->GetEntityForBookmarkNode(node);
  DCHECK(entity);
  bookmark_tracker_->Remove(entity->metadata()->server_id());

  for (int i = 0; i < node->child_count(); ++i) {
    const bookmarks::BookmarkNode* child = node->GetChild(i);
    RemoveEntityAndChildrenFromTracker(child);
  }
}

const bookmarks::BookmarkNode* BookmarkRemoteUpdatesHandler::GetParentNode(
    const syncer::EntityData& update_entity) const {
  const SyncedBookmarkTracker::Entity* parent_entity =
      bookmark_tracker_->GetEntityForSyncId(update_entity.parent_id);
  if (!parent_entity) {
    return nullptr;
  }
  return parent_entity->bookmark_node();
}

void BookmarkRemoteUpdatesHandler::AssociatePermanentFolder(
    const syncer::UpdateResponseData& update) {
  const syncer::EntityData& update_entity = update.entity.value();
  DCHECK_EQ(update_entity.parent_id, kBookmarksRootId);

  const bookmarks::BookmarkNode* permanent_node = nullptr;
  if (update_entity.server_defined_unique_tag == kBookmarkBarTag) {
    permanent_node = bookmark_model_->bookmark_bar_node();
  } else if (update_entity.server_defined_unique_tag == kOtherBookmarksTag) {
    permanent_node = bookmark_model_->other_node();
  } else if (update_entity.server_defined_unique_tag == kMobileBookmarksTag) {
    permanent_node = bookmark_model_->mobile_node();
  }

  if (permanent_node != nullptr) {
    bookmark_tracker_->Add(update_entity.id, permanent_node,
                           update.response_version, update_entity.creation_time,
                           update_entity.unique_position,
                           update_entity.specifics);
  }
}

}  // namespace sync_bookmarks
