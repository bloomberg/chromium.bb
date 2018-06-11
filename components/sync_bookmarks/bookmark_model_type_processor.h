// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync_bookmarks/synced_bookmark_tracker.h"

class BookmarkUndoService;

namespace bookmarks {
class BookmarkModel;
}

namespace sync_bookmarks {

class BookmarkModelTypeProcessor : public syncer::ModelTypeProcessor,
                                   public syncer::ModelTypeControllerDelegate {
 public:
  // |bookmark_undo_service| and |bookmark_undo_service| must not be nullptr and
  // must outlive this object.
  BookmarkModelTypeProcessor(bookmarks::BookmarkModel* bookmark_model,
                             BookmarkUndoService* bookmark_undo_service);
  ~BookmarkModelTypeProcessor() override;

  // ModelTypeProcessor implementation.
  void ConnectSync(std::unique_ptr<syncer::CommitQueue> worker) override;
  void DisconnectSync() override;
  void GetLocalChanges(size_t max_entries,
                       const GetLocalChangesCallback& callback) override;
  void OnCommitCompleted(
      const sync_pb::ModelTypeState& type_state,
      const syncer::CommitResponseDataList& response_list) override;
  void OnUpdateReceived(const sync_pb::ModelTypeState& type_state,
                        const syncer::UpdateResponseDataList& updates) override;

  // ModelTypeControllerDelegate implementation.
  void OnSyncStarting(const syncer::ModelErrorHandler& error_handler,
                      StartCallback start_callback) override;
  void DisableSync() override;
  void GetAllNodesForDebugging(AllNodesCallback callback) override;
  void GetStatusCountersForDebugging(StatusCountersCallback callback) override;
  void RecordMemoryUsageHistogram() override;

  // Public for testing.
  static std::vector<const syncer::UpdateResponseData*> ReorderUpdatesForTest(
      const syncer::UpdateResponseDataList& updates);

  const SyncedBookmarkTracker* GetTrackerForTest() const;

  base::WeakPtr<syncer::ModelTypeControllerDelegate> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Reorders incoming updates such that parent creation is before child
  // creation and child deletion is before parent deletion, and deletions should
  // come last. The returned pointers point to the elements in the original
  // |updates|.
  static std::vector<const syncer::UpdateResponseData*> ReorderUpdates(
      const syncer::UpdateResponseDataList& updates);

  // Given a remote update entity, it returns the parent bookmark node of the
  // corresponding node. It returns null if the parent node cannot be found.
  const bookmarks::BookmarkNode* GetParentNode(
      const syncer::EntityData& update_data) const;

  // Processes a remote creation of a bookmark node.
  // 1. For permanent folders, they are only registered in |bookmark_tracker_|.
  // 2. If the nodes parent cannot be found, the remote creation update is
  //    ignored.
  // 3. Otherwise, a new node is created in the local bookmark model and
  //    registered in |bookmark_tracker_|.
  void ProcessRemoteCreate(const syncer::EntityData& update_data);

  // Processes a remote update of a bookmark node. |update_data| must not be a
  // deletion, and the server_id must be already tracked, otherwise, it is a
  // creation that gets handeled in ProcessRemoteCreate(). |tracked_entity| is
  // the tracked entity for that server_id. It is passed as a dependency instead
  // of performing a lookup inside ProcessRemoteUpdate() to avoid wasting CPU
  // cycles for doing another lookup (this code runs on the UI thread).
  void ProcessRemoteUpdate(const syncer::EntityData& update_data,
                           const SyncedBookmarkTracker::Entity* tracked_entity);

  // Process a remote delete of a bookmark node. |update_data| must not be a
  // deletion. |tracked_entity| is the tracked entity for that server_id. It is
  // passed as a dependency instead of performing a lookup inside
  // ProcessRemoteDelete() to avoid wasting CPU cycles for doing another lookup
  // (this code runs on the UI thread).
  void ProcessRemoteDelete(const syncer::EntityData& update_data,
                           const SyncedBookmarkTracker::Entity* tracked_entity);

  // Associates the permanent bookmark folders with the corresponding server
  // side ids and registers the association in |bookmark_tracker_|.
  // |update_data| must contain server_defined_unique_tag that is used to
  // determine the corresponding permanent node. All permanent nodes are assumed
  // to be directly children nodes of |kBookmarksRootId|. This method is used in
  // the initial sync cycle only.
  void AssociatePermanentFolder(const syncer::EntityData& update_data);

  // The bookmark model we are processing local changes from and forwarding
  // remote changes to.
  bookmarks::BookmarkModel* const bookmark_model_;

  // Used to suspend bookmark undo when processing remote changes.
  BookmarkUndoService* const bookmark_undo_service_;

  // Reference to the CommitQueue.
  //
  // The interface hides the posting of tasks across threads as well as the
  // CommitQueue's implementation.  Both of these features are
  // useful in tests.
  std::unique_ptr<syncer::CommitQueue> worker_;

  // Keeps the mapping between server ids and bookmarks nodes. It also caches
  // the metadata upon a local change until the commit configration is received.
  SyncedBookmarkTracker bookmark_tracker_;

  base::WeakPtrFactory<BookmarkModelTypeProcessor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelTypeProcessor);
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_PROCESSOR_H_
