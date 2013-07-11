// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TAB_NODE_POOL_H_
#define CHROME_BROWSER_SYNC_GLUE_TAB_NODE_POOL_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/sessions/session_id.h"

class ProfileSyncService;

namespace browser_sync {

// A pool for managing free/used tab sync nodes. Performs lazy creation
// of sync nodes when necessary.
// Note: We make use of the following "id's"
// - a sync_id: an int64 used in |syncer::InitByIdLookup|
// - a tab_id: created by session service, unique to this client
// - a tab_node_id: the id for a particular sync tab node. This is used
//   to generate the sync tab node tag through:
//       tab_tag = StringPrintf("%s_%ui", local_session_tag, tab_node_id);
// tab_node_id and sync_id are both unique to a particular sync node. The
// difference is that tab_node_id is controlled by the model associator and
// is used when creating a new sync node, which returns the sync_id, created
// by the sync db.
class TabNodePool {
 public:
  explicit TabNodePool(ProfileSyncService* sync_service);
  ~TabNodePool();
  enum InvalidTab {
    kInvalidTabID = -1
  };
  // Build a sync tag from tab_node_id.
  static std::string TabIdToTag(const std::string machine_tag,
                                size_t tab_node_id);

  // Returns the sync_id for the next free tab node. If none are available,
  // creates a new tab node and adds it to free nodes pool. The free node can
  // then be used to associate with a tab by calling AssociateTabNode.
  // Note: The node is considered free until it has been associated. Repeated
  // calls to GetFreeTabNode will return the same sync_id until node has been
  // associated.
  int64 GetFreeTabNode();

  // Removes the node from |syncid_tabid_map_| and returns the node to free
  // pool.
  void FreeTabNode(int64 sync_id);

  // Removes |sync_id| from free node pool and associates it with |tab_id|.
  // sync_id should be id of a free node. In order to associate a non free
  // sync node, use ReassociateTabNode.
  void AssociateTabNode(int64 sync_id, SessionID::id_type tab_id);

  // Associate sync_id with tab_id but does not add it to free node pool.
  // Note: this should only be called when we discover tab sync nodes from
  // previous sessions, not for freeing tab nodes we created through
  // GetFreeTabNode (use FreeTabNode below for that).
  // The difference between AddTabNode and AssociateTabNode is that
  // AssociateTabNode requires the sync node to be free to be associated.
  // AddTabNode just adds the association.
  void AddTabNode(int64 sync_id, const SessionID& tab_id, size_t tab_node_id);

  // Returns the tab_id for |sync_id| if it exists else returns kInvalidTabID.
  SessionID::id_type GetTabIdFromSyncId(int64 sync_id) const;

  // Reassociates sync node with id |sync_id| with tab node with |tab_id|.
  // Returns true if it is able to reassociate the node, false if a sync node
  // with id |sync_id| is not associated with any tab.
  bool ReassociateTabNode(int64 sync_id, SessionID::id_type tab_id);

  // Returns any nodes with sync_ids not in |used_synced_ids| to free node pool.
  void FreeUnusedTabNodes(const std::set<int64>& used_sync_ids);

  // Clear tab pool.
  void Clear();

  // Return the number of tab nodes this client currently has allocated
  // (including both free and used nodes)
  size_t Capacity() const;

  // Return empty status (all tab nodes are in use).
  bool Empty() const;

  // Return full status (no tab nodes are in use).
  bool Full();

  void SetMachineTag(const std::string& machine_tag);

 private:
  friend class SyncTabNodePoolTest;
  typedef std::map<int64, SessionID::id_type> SyncIDToTabIDMap;

  // Stores mapping of sync_ids associated with tab_ids, these are the used
  // nodes of tab node pool.
  // The nodes in the map can be returned to free tab node pool by calling
  // FreeTabNode(sync_id).
  SyncIDToTabIDMap syncid_tabid_map_;

  // The sync ids for the set of free sync nodes.
  std::set<int64> free_nodes_pool_;

  // The maximum used tab_node id for a sync node. A new sync node will always
  // be created with max_used_tab_node_id_ + 1.
  size_t max_used_tab_node_id_;

  // The machine tag associated with this tab pool. Used in the title of new
  // sync nodes.
  std::string machine_tag_;

  // Our sync service profile (for making changes to the sync db)
  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(TabNodePool);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TAB_NODE_POOL_H_
