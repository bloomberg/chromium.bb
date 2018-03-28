// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/tab_node_pool.h"

#include <algorithm>

#include "base/logging.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_sessions/synced_tab_delegate.h"

namespace sync_sessions {

const size_t TabNodePool::kFreeNodesLowWatermark = 25;
const size_t TabNodePool::kFreeNodesHighWatermark = 100;

TabNodePool::TabNodePool() : max_used_tab_node_id_(kInvalidTabNodeID) {}

// static
// We start vending tab node IDs at 0.
const int TabNodePool::kInvalidTabNodeID = -1;

TabNodePool::~TabNodePool() {}

void TabNodePool::AddTabNode(int tab_node_id) {
  DCHECK_GT(tab_node_id, kInvalidTabNodeID);
  DCHECK(nodeid_tabid_map_.find(tab_node_id) == nodeid_tabid_map_.end());
  DVLOG(1) << "Adding tab node " << tab_node_id << " to pool.";
  max_used_tab_node_id_ = std::max(max_used_tab_node_id_, tab_node_id);
  free_nodes_pool_.insert(tab_node_id);
}

void TabNodePool::AssociateTabNode(int tab_node_id, SessionID tab_id) {
  DCHECK_GT(tab_node_id, kInvalidTabNodeID);
  DCHECK(tab_id.is_valid());

  // This is a new node association, the sync node should be free.
  // Remove node from free node pool and then associate it with the tab.
  std::set<int>::iterator it = free_nodes_pool_.find(tab_node_id);
  DCHECK(it != free_nodes_pool_.end());
  free_nodes_pool_.erase(it);

  DCHECK(nodeid_tabid_map_.find(tab_node_id) == nodeid_tabid_map_.end());
  DVLOG(1) << "Associating tab node " << tab_node_id << " with tab "
           << tab_id.id();
  nodeid_tabid_map_.emplace(tab_node_id, tab_id);
  tabid_nodeid_map_[tab_id] = tab_node_id;
}

bool TabNodePool::GetTabNodeForTab(SessionID tab_id, int* tab_node_id) {
  if (tabid_nodeid_map_.find(tab_id) != tabid_nodeid_map_.end()) {
    *tab_node_id = tabid_nodeid_map_[tab_id];
    return true;
  }

  if (free_nodes_pool_.empty()) {
    // Tab pool has no free nodes, allocate new one.
    *tab_node_id = ++max_used_tab_node_id_;
    AddTabNode(*tab_node_id);

    AssociateTabNode(*tab_node_id, tab_id);
    return false;
  } else {
    // Return the next free node.
    *tab_node_id = *free_nodes_pool_.begin();
    AssociateTabNode(*tab_node_id, tab_id);
    return true;
  }
}

void TabNodePool::FreeTab(SessionID tab_id) {
  DCHECK(tab_id.is_valid());
  TabIDToTabNodeIDMap::iterator it = tabid_nodeid_map_.find(tab_id);
  if (it == tabid_nodeid_map_.end()) {
    return;  // Already freed.
  }

  int tab_node_id = it->second;
  DVLOG(1) << "Freeing tab " << tab_id.id() << " at node " << tab_node_id;
  nodeid_tabid_map_.erase(nodeid_tabid_map_.find(tab_node_id));
  tabid_nodeid_map_.erase(it);
  free_nodes_pool_.insert(tab_node_id);
}

void TabNodePool::ReassociateTabNode(int tab_node_id, SessionID tab_id) {
  DCHECK_GT(tab_node_id, kInvalidTabNodeID);
  DCHECK(tab_id.is_valid());

  auto tabid_it = tabid_nodeid_map_.find(tab_id);
  if (tabid_it != tabid_nodeid_map_.end()) {
    if (tabid_it->second == tab_node_id) {
      return;  // Already associated properly.
    } else {
      // Another node is already associated with this tab. Free it.
      FreeTab(tab_id);
    }
  }

  auto nodeid_it = nodeid_tabid_map_.find(tab_node_id);
  if (nodeid_it != nodeid_tabid_map_.end()) {
    // This node was already associated with another tab. Free it.
    FreeTab(nodeid_it->second);
  } else {
    // This is a new tab node. Add it before association.
    AddTabNode(tab_node_id);
  }

  AssociateTabNode(tab_node_id, tab_id);
}

SessionID TabNodePool::GetTabIdFromTabNodeId(int tab_node_id) const {
  TabNodeIDToTabIDMap::const_iterator it = nodeid_tabid_map_.find(tab_node_id);
  if (it != nodeid_tabid_map_.end()) {
    return it->second;
  }
  return SessionID::InvalidValue();
}

void TabNodePool::CleanupTabNodes(std::set<int>* deleted_node_ids) {
  // If number of free nodes exceed kFreeNodesHighWatermark,
  // delete sync nodes till number reaches kFreeNodesLowWatermark.
  // Note: This logic is to mitigate temporary disassociation issues with old
  // clients: https://crbug.com/259918. Newer versions do not need this.
  if (free_nodes_pool_.size() > kFreeNodesHighWatermark) {
    for (std::set<int>::iterator free_it = free_nodes_pool_.begin();
         free_it != free_nodes_pool_.end();) {
      deleted_node_ids->insert(*free_it);
      free_nodes_pool_.erase(free_it++);
      if (free_nodes_pool_.size() <= kFreeNodesLowWatermark) {
        return;
      }
    }
  }
}

// Clear tab pool.
void TabNodePool::Clear() {
  free_nodes_pool_.clear();
  nodeid_tabid_map_.clear();
  tabid_nodeid_map_.clear();
  max_used_tab_node_id_ = kInvalidTabNodeID;
}

size_t TabNodePool::Capacity() const {
  return nodeid_tabid_map_.size() + free_nodes_pool_.size();
}

bool TabNodePool::Empty() const {
  return free_nodes_pool_.empty();
}

bool TabNodePool::Full() {
  return nodeid_tabid_map_.empty();
}

}  // namespace sync_sessions
