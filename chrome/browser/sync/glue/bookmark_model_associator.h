// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_ASSOCIATOR_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/task.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"
#include "chrome/browser/sync/glue/model_associator.h"

class BookmarkNode;

namespace sync_api {
class BaseNode;
class BaseTransaction;
class ReadNode;
}

class ProfileSyncService;

namespace browser_sync {

class BookmarkChangeProcessor;

// Contains all model association related logic:
// * Algorithm to associate bookmark model and sync model.
// * Methods to get a bookmark node for a given sync node and vice versa.
// * Persisting model associations and loading them back.
class BookmarkModelAssociator
    : public PerDataTypeAssociatorInterface<BookmarkNode, int64> {
 public:
  static syncable::ModelType model_type() { return syncable::BOOKMARKS; }
  BookmarkModelAssociator(ProfileSyncService* sync_service,
      UnrecoverableErrorHandler* persist_ids_error_handler);
  virtual ~BookmarkModelAssociator();

  // AssociatorInterface implementation.
  //
  // AssociateModels iterates through both the sync and the browser
  // bookmark model, looking for matched pairs of items.  For any pairs it
  // finds, it will call AssociateSyncID.  For any unmatched items,
  // MergeAndAssociateModels will try to repair the match, e.g. by adding a new
  // node.  After successful completion, the models should be identical and
  // corresponding. Returns true on success.  On failure of this step, we
  // should abort the sync operation and report an error to the user.
  virtual bool AssociateModels();

  virtual bool DisassociateModels();

  // The has_nodes out param is true if the sync model has nodes other
  // than the permanent tagged nodes.
  virtual bool SyncModelHasUserCreatedNodes(bool* has_nodes);

  // Returns sync id for the given bookmark node id.
  // Returns sync_api::kInvalidId if the sync node is not found for the given
  // bookmark node id.
  virtual int64 GetSyncIdFromChromeId(const int64& node_id);

  // Returns the bookmark node for the given sync id.
  // Returns NULL if no bookmark node is found for the given sync id.
  virtual const BookmarkNode* GetChromeNodeFromSyncId(int64 sync_id);

  // Initializes the given sync node from the given bookmark node id.
  // Returns false if no sync node was found for the given bookmark node id or
  // if the initialization of sync node fails.
  virtual bool InitSyncNodeFromChromeId(const int64& node_id,
                                        sync_api::BaseNode* sync_node);

  // Associates the given bookmark node with the given sync id.
  virtual void Associate(const BookmarkNode* node, int64 sync_id);
  // Remove the association that corresponds to the given sync id.
  virtual void Disassociate(int64 sync_id);

  virtual void AbortAssociation() {
    // No implementation needed, this associator runs on the main
    // thread.
  }

 protected:
  // Stores the id of the node with the given tag in |sync_id|.
  // Returns of that node was found successfully.
  // Tests override this.
  virtual bool GetSyncIdForTaggedNode(const std::string& tag, int64* sync_id);

  // Used by TestBookmarkModelAssociator.
  ProfileSyncService* sync_service() { return sync_service_; }

 private:
  typedef std::map<int64, int64> BookmarkIdToSyncIdMap;
  typedef std::map<int64, const BookmarkNode*> SyncIdToBookmarkNodeMap;
  typedef std::set<int64> DirtyAssociationsSyncIds;

  // Posts a task to persist dirty associations.
  void PostPersistAssociationsTask();
  // Persists all dirty associations.
  void PersistAssociations();

  // Loads the persisted associations into in-memory maps.
  // If the persisted associations are out-of-date due to some reason, returns
  // false; otherwise returns true.
  bool LoadAssociations();

  // Matches up the bookmark model and the sync model to build model
  // associations.
  bool BuildAssociations();

  // Associate a top-level node of the bookmark model with a permanent node in
  // the sync domain.  Such permanent nodes are identified by a tag that is
  // well known to the server and the client, and is unique within a particular
  // user's share.  For example, "other_bookmarks" is the tag for the Other
  // Bookmarks folder.  The sync nodes are server-created.
  bool AssociateTaggedPermanentNode(const BookmarkNode* permanent_node,
                                    const std::string& tag);

  // Compare the properties of a pair of nodes from either domain.
  bool NodesMatch(const BookmarkNode* bookmark,
                  const sync_api::BaseNode* sync_node) const;

  ProfileSyncService* sync_service_;
  UnrecoverableErrorHandler* persist_ids_error_handler_;
  BookmarkIdToSyncIdMap id_map_;
  SyncIdToBookmarkNodeMap id_map_inverse_;
  // Stores sync ids for dirty associations.
  DirtyAssociationsSyncIds dirty_associations_sync_ids_;

  // Used to post PersistAssociation tasks to the current message loop and
  // guarantees no invocations can occur if |this| has been deleted. (This
  // allows this class to be non-refcounted).
  ScopedRunnableMethodFactory<BookmarkModelAssociator> persist_associations_;

  int number_of_new_sync_nodes_created_at_association_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_BOOKMARK_MODEL_ASSOCIATOR_H_
