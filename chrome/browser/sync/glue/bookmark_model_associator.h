// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_ASSOCIATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_error_handler.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"

class BookmarkModel;
class BookmarkNode;

namespace syncer {
class BaseNode;
class BaseTransaction;
struct UserShare;
}

namespace browser_sync {

// Contains all model association related logic:
// * Algorithm to associate bookmark model and sync model.
// * Methods to get a bookmark node for a given sync node and vice versa.
// * Persisting model associations and loading them back.
class BookmarkModelAssociator
    : public PerDataTypeAssociatorInterface<BookmarkNode, int64> {
 public:
  static syncer::ModelType model_type() { return syncer::BOOKMARKS; }
  // |expect_mobile_bookmarks_folder| controls whether or not we
  // expect the mobile bookmarks permanent folder to be created.
  // Should be set to true only by mobile clients.
  BookmarkModelAssociator(
      BookmarkModel* bookmark_model,
      syncer::UserShare* user_share,
      DataTypeErrorHandler* unrecoverable_error_handler,
      bool expect_mobile_bookmarks_folder);
  virtual ~BookmarkModelAssociator();

  // Updates the visibility of the permanents node in the BookmarkModel.
  void UpdatePermanentNodeVisibility();

  // AssociatorInterface implementation.
  //
  // AssociateModels iterates through both the sync and the browser
  // bookmark model, looking for matched pairs of items.  For any pairs it
  // finds, it will call AssociateSyncID.  For any unmatched items,
  // MergeAndAssociateModels will try to repair the match, e.g. by adding a new
  // node.  After successful completion, the models should be identical and
  // corresponding. Returns true on success.  On failure of this step, we
  // should abort the sync operation and report an error to the user.
  virtual syncer::SyncError AssociateModels(
      syncer::SyncMergeResult* local_merge_result,
      syncer::SyncMergeResult* syncer_merge_result) OVERRIDE;

  virtual syncer::SyncError DisassociateModels() OVERRIDE;

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes) OVERRIDE;

  // Returns sync id for the given bookmark node id.
  // Returns syncer::kInvalidId if the sync node is not found for the given
  // bookmark node id.
  virtual int64 GetSyncIdFromChromeId(const int64& node_id) OVERRIDE;

  // Returns the bookmark node for the given sync id.
  // Returns NULL if no bookmark node is found for the given sync id.
  virtual const BookmarkNode* GetChromeNodeFromSyncId(int64 sync_id) OVERRIDE;

  // Initializes the given sync node from the given bookmark node id.
  // Returns false if no sync node was found for the given bookmark node id or
  // if the initialization of sync node fails.
  virtual bool InitSyncNodeFromChromeId(
      const int64& node_id,
      syncer::BaseNode* sync_node) OVERRIDE;

  // Associates the given bookmark node with the given sync id.
  virtual void Associate(const BookmarkNode* node, int64 sync_id) OVERRIDE;
  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id) OVERRIDE;

  virtual void AbortAssociation() OVERRIDE {
    // No implementation needed, this associator runs on the main
    // thread.
  }

  // See ModelAssociator interface.
  virtual bool CryptoReadyIfNecessary() OVERRIDE;

 protected:
  // Stores the id of the node with the given tag in |sync_id|.
  // Returns of that node was found successfully.
  // Tests override this.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

 private:
  typedef std::map<int64, int64> BookmarkIdToSyncIdMap;
  typedef std::map<int64, const BookmarkNode*> SyncIdToBookmarkNodeMap;
  typedef std::set<int64> DirtyAssociationsSyncIds;

  // Posts a task to persist dirty associations.
  void PostPersistAssociationsTask();
  // Persists all dirty associations.
  void PersistAssociations();

  // Matches up the bookmark model and the sync model to build model
  // associations.
  syncer::SyncError BuildAssociations(
      syncer::SyncMergeResult* local_merge_result,
      syncer::SyncMergeResult* syncer_merge_result);

  // Removes bookmark nodes whose corresponding sync nodes have been deleted
  // according to sync delete journals. Return number of deleted bookmarks.
  int64 ApplyDeletesFromSyncJournal(syncer::BaseTransaction* trans);

  // Associate a top-level node of the bookmark model with a permanent node in
  // the sync domain.  Such permanent nodes are identified by a tag that is
  // well known to the server and the client, and is unique within a particular
  // user's share.  For example, "other_bookmarks" is the tag for the Other
  // Bookmarks folder.  The sync nodes are server-created.
  // Returns true on success, false if association failed.
  bool AssociateTaggedPermanentNode(
      const BookmarkNode* permanent_node,
      const std::string& tag) WARN_UNUSED_RESULT;

  // Compare the properties of a pair of nodes from either domain.
  bool NodesMatch(const BookmarkNode* bookmark,
                  const syncer::BaseNode* sync_node) const;

  // Check whether bookmark model and sync model are synced by comparing
  // their transaction versions.
  void CheckModelSyncState() const;

  BookmarkModel* bookmark_model_;
  syncer::UserShare* user_share_;
  DataTypeErrorHandler* unrecoverable_error_handler_;
  const bool expect_mobile_bookmarks_folder_;
  BookmarkIdToSyncIdMap id_map_;
  SyncIdToBookmarkNodeMap id_map_inverse_;
  // Stores sync ids for dirty associations.
  DirtyAssociationsSyncIds dirty_associations_sync_ids_;

  // Used to post PersistAssociation tasks to the current message loop and
  // guarantees no invocations can occur if |this| has been deleted. (This
  // allows this class to be non-refcounted).
  base::WeakPtrFactory<BookmarkModelAssociator> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_ASSOCIATOR_H_
