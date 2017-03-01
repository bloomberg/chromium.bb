// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_
#define COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "components/sessions/core/session_id.h"

namespace sync_sessions {

// A pool for managing free/used tab sync nodes for the *local* session.
// Performs lazy creation of sync nodes when necessary.
// Note: We make use of the following "id's"
// - a tab_id: created by session service, unique to this client
// - a tab_node_id: the id for a particular sync tab node. This is used
//   to generate the sync tab node tag through:
//       tab_tag = StringPrintf("%s %d", local_session_tag, tab_node_id);
//
// A sync node can be in one of the two states:
// 1. Associated   : Sync node is used and associated with a tab.
// 2. Free         : Sync node is unused.

class TabNodePool {
 public:
  TabNodePool();
  ~TabNodePool();
  enum InvalidTab { kInvalidTabID = -1 };

  // If free nodes > kFreeNodesHighWatermark, delete all free nodes until
  // free nodes <= kFreeNodesLowWatermark.
  static const size_t kFreeNodesLowWatermark;

  // Maximum limit of FreeNodes allowed on the client.
  static const size_t kFreeNodesHighWatermark;

  static const int kInvalidTabNodeID;

  // Fills |tab_node_id| with a tab node associated with |tab_id|.
  // If tab_id is already associated with a tab_node_id, reuses the existing
  // association. Otherwise attempts to get the next free tab node and
  // associate it with |tab_id|. If none are available, will create a new tab
  // node.
  // Returns true if a pre-existing tab node could be reused, false if a new one
  // had to be created.
  bool GetTabNodeForTab(SessionID::id_type tab_id, int* tab_node_id);

  // Returns the tab_id for |tab_node_id| if it is associated else returns
  // kInvalidTabID.
  SessionID::id_type GetTabIdFromTabNodeId(int tab_node_id) const;

  // Reassociates |tab_node_id| with |tab_id|. If |tab_node_id| is not already
  // known, it is added to the tab node pool before being associated.
  void ReassociateTabNode(int tab_node_id, SessionID::id_type tab_id);

  // Removes association for |tab_id| and returns its tab node to the free node
  // pool.
  void FreeTab(int tab_id);

  // Fills |deleted_node_ids| with any free nodes to be deleted as proscribed
  // by the free node low/high watermarks, in order to ensure the free node pool
  // does not grow too large.
  void CleanupTabNodes(std::set<int>* deleted_node_ids);

  // Clear tab pool.
  void Clear();

  // Return the number of tab nodes this client currently has allocated
  // (including both free and associated nodes).
  size_t Capacity() const;

  // Return empty status (all tab nodes are in use).
  bool Empty() const;

  // Return full status (no tab nodes are in use).
  bool Full();

 private:
  friend class SyncTabNodePoolTest;
  using TabNodeIDToTabIDMap = std::map<int, SessionID::id_type>;
  using TabIDToTabNodeIDMap = std::map<SessionID::id_type, int>;

  // Adds |tab_node_id| to the tab node pool.
  // Note: this should only be called when we discover tab sync nodes from
  // previous sessions, not for freeing tab nodes we created through
  // GetTabNodeForTab (use FreeTab for that).
  void AddTabNode(int tab_node_id);

  // Associates |tab_node_id| with |tab_id|. |tab_node_id| must be free. In
  // order to associated a non-free tab node, ReassociateTabNode must be
  // used.
  void AssociateTabNode(int tab_node_id, SessionID::id_type tab_id);

  // Stores mapping of node ids associated with tab_ids, these are the used
  // nodes of tab node pool.
  // The nodes in the map can be returned to free tab node pool by calling
  // FreeTab(..).
  TabNodeIDToTabIDMap nodeid_tabid_map_;
  TabIDToTabNodeIDMap tabid_nodeid_map_;

  // The node ids for the set of free sync nodes.
  std::set<int> free_nodes_pool_;

  // The maximum used tab_node id for a sync node. A new sync node will always
  // be created with max_used_tab_node_id_ + 1.
  int max_used_tab_node_id_;

  DISALLOW_COPY_AND_ASSIGN(TabNodePool);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_
