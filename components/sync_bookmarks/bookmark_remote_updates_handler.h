// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_REMOTE_UPDATES_HANDLER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_REMOTE_UPDATES_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace sync_bookmarks {

// Responsible for processing one batch of remote updates received from the sync
// server.
class BookmarkRemoteUpdatesHandler {
 public:
  enum class DuplicateBookmarkEntityOnRemoteUpdateCondition {
    kServerIdTombstone = 0,
    kTempSyncIdTombstone = 1,
    kBothEntitiesNonTombstone = 2,

    kMaxValue = kBothEntitiesNonTombstone,
  };

  // |bookmark_model|, |favicon_service| and |bookmark_tracker| must not be null
  // and must outlive this object.
  BookmarkRemoteUpdatesHandler(bookmarks::BookmarkModel* bookmark_model,
                               favicon::FaviconService* favicon_service,
                               SyncedBookmarkTracker* bookmark_tracker);
  // Processes the updates received from the sync server in |updates| and
  // updates the |bookmark_model_| and |bookmark_tracker_| accordingly. If
  // |got_new_encryption_requirements| is true, it recommits all tracked
  // entities except those in |updates| which should use the latest encryption
  // key and hence don't need recommitting.
  void Process(const syncer::UpdateResponseDataList& updates,
               bool got_new_encryption_requirements);

  // Public for testing.
  static std::vector<const syncer::UpdateResponseData*> ReorderUpdatesForTest(
      const syncer::UpdateResponseDataList* updates);

 private:
  // Reorders incoming updates such that parent creation is before child
  // creation and child deletion is before parent deletion, and deletions should
  // come last. The returned pointers point to the elements in the original
  // |updates|.
  static std::vector<const syncer::UpdateResponseData*> ReorderUpdates(
      const syncer::UpdateResponseDataList* updates);

  // Given a remote update entity, it returns the parent bookmark node of the
  // corresponding node. It returns null if the parent node cannot be found.
  const bookmarks::BookmarkNode* GetParentNode(
      const syncer::EntityData& update_entity) const;

  // Processes a remote creation of a bookmark node.
  // 1. For permanent folders, they are only registered in |bookmark_tracker_|.
  // 2. If the nodes parent cannot be found, the remote creation update is
  //    ignored.
  // 3. Otherwise, a new node is created in the local bookmark model and
  //    registered in |bookmark_tracker_|.
  //
  // Returns the newly tracked entity or null if the creation failed.
  const SyncedBookmarkTracker::Entity* ProcessCreate(
      const syncer::UpdateResponseData& update);

  // Processes a remote update of a bookmark node. |update| must not be a
  // deletion, and the server_id must be already tracked, otherwise, it is a
  // creation that gets handled in ProcessCreate(). |tracked_entity| is
  // the tracked entity for that server_id. It is passed as a dependency instead
  // of performing a lookup inside ProcessUpdate() to avoid wasting CPU
  // cycles for doing another lookup (this code runs on the UI thread).
  void ProcessUpdate(const syncer::UpdateResponseData& update,
                     const SyncedBookmarkTracker::Entity* tracked_entity);

  // Processes a remote delete of a bookmark node. |update_entity| must not be a
  // deletion. |tracked_entity| is the tracked entity for that server_id. It is
  // passed as a dependency instead of performing a lookup inside
  // ProcessDelete() to avoid wasting CPU cycles for doing another lookup
  // (this code runs on the UI thread).
  void ProcessDelete(const syncer::EntityData& update_entity,
                     const SyncedBookmarkTracker::Entity* tracked_entity);

  // Processes a conflict where the bookmark has been changed both locally and
  // remotely. It applies the general policy the server wins except in the case
  // of remote deletions in which local wins. |tracked_entity| is the tracked
  // entity for that server_id. It is passed as a dependency instead of
  // performing a lookup inside ProcessDelete() to avoid wasting CPU cycles for
  // doing another lookup (this code runs on the UI thread).
  void ProcessConflict(const syncer::UpdateResponseData& update,
                       const SyncedBookmarkTracker::Entity* tracked_entity);

  // Recursively removes the entities corresponding to |node| and its children
  // from |bookmark_tracker_|.
  void RemoveEntityAndChildrenFromTracker(const bookmarks::BookmarkNode* node);

  void ReuploadEntityIfNeeded(
      const sync_pb::BookmarkSpecifics& specifics,
      const SyncedBookmarkTracker::Entity* tracked_entity);

  bookmarks::BookmarkModel* const bookmark_model_;
  favicon::FaviconService* const favicon_service_;
  SyncedBookmarkTracker* const bookmark_tracker_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkRemoteUpdatesHandler);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_REMOTE_UPDATES_HANDLER_H_
