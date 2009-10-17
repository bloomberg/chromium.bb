// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(BROWSER_SYNC)

#ifndef CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_
#define CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"

class BookmarkNode;

namespace sync_api {
class BaseNode;
class BaseTransaction;
class ReadNode;
}

class ProfileSyncService;

namespace browser_sync {

class ChangeProcessor;

// Contains all model association related logic:
// * Algorithm to associate bookmark model and sync model.
// * Methods to get a bookmark node for a given sync node and vice versa.
// * Persisting model associations and loading them back.
class ModelAssociator
    : public base::RefCountedThreadSafe<ModelAssociator> {
 public:
  explicit ModelAssociator(ProfileSyncService* sync_service);
  virtual ~ModelAssociator() { }

  // Clears all associations.
  void ClearAll();

  // Returns sync id for the given bookmark node id.
  // Returns sync_api::kInvalidId if the sync node is not found for the given
  // bookmark node id.
  int64 GetSyncIdFromBookmarkId(int64 node_id) const;

  // Stores bookmark node id for the given sync id in bookmark_id. Returns true
  // if the bookmark id was successfully found; false otherwise.
  bool GetBookmarkIdFromSyncId(int64 sync_id, int64* bookmark_id) const;

  // Initializes the given sync node from the given bookmark node id.
  // Returns false if no sync node was found for the given bookmark node id or
  // if the initialization of sync node fails.
  bool InitSyncNodeFromBookmarkId(int64 node_id, sync_api::BaseNode* sync_node);

  // Returns the bookmark node for the given sync id.
  // Returns NULL if no bookmark node is found for the given sync id.
  const BookmarkNode* GetBookmarkNodeFromSyncId(int64 sync_id);

  // Associates the given bookmark node id with the given sync id.
  void AssociateIds(int64 node_id, int64 sync_id);
  // Disassociate the ids that correspond to the given sync id.
  void DisassociateIds(int64 sync_id);

  // Returns whether the bookmark model has user created nodes or not. That is,
  // whether there are nodes in the bookmark model except the bookmark bar and
  // other bookmarks.
  bool BookmarkModelHasUserCreatedNodes() const;

  // Returns whether the sync model has nodes other than the permanent tagged
  // nodes.
  bool SyncModelHasUserCreatedNodes();

  // AssociateModels iterates through both the sync and the browser
  // bookmark model, looking for matched pairs of items.  For any pairs it
  // finds, it will call AssociateSyncID.  For any unmatched items,
  // MergeAndAssociateModels will try to repair the match, e.g. by adding a new
  // node.  After successful completion, the models should be identical and
  // corresponding. Returns true on success.  On failure of this step, we
  // should abort the sync operation and report an error to the user.
  bool AssociateModels();

 protected:
  // Stores the id of the node with the given tag in |sync_id|.
  // Returns of that node was found successfully.
  // Tests override this.
  virtual bool GetSyncIdForTaggedNode(const string16& tag, int64* sync_id);

  // Returns sync service instance.
  ProfileSyncService* sync_service() { return sync_service_; }

 private:
  typedef std::map<int64, int64> BookmarkIdToSyncIdMap;
  typedef std::map<int64, int64> SyncIdToBookmarkIdMap;
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
                                    const string16& tag);

  // Compare the properties of a pair of nodes from either domain.
  bool NodesMatch(const BookmarkNode* bookmark,
                  const sync_api::BaseNode* sync_node) const;

  ProfileSyncService* sync_service_;
  BookmarkIdToSyncIdMap id_map_;
  SyncIdToBookmarkIdMap id_map_inverse_;
  // Stores sync ids for dirty associations.
  DirtyAssociationsSyncIds dirty_associations_sync_ids_;

  // Indicates whether there is already a pending task to persist dirty model
  // associations.
  bool task_pending_;

  DISALLOW_COPY_AND_ASSIGN(ModelAssociator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_MODEL_ASSOCIATOR_H_
#endif  // defined(BROWSER_SYNC)
