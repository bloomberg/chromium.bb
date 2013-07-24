// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/tab_node_pool.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"

namespace browser_sync {

static const char kNoSessionsFolderError[] =
    "Server did not create the top-level sessions node. We "
    "might be running against an out-of-date server.";

const size_t TabNodePool::kFreeNodesLowWatermark = 25;
const size_t TabNodePool::kFreeNodesHighWatermark = 100;

TabNodePool::TabNodePool(ProfileSyncService* sync_service)
    : max_used_tab_node_id_(kInvalidTabNodeID),
      sync_service_(sync_service) {}

// static
// We start vending tab node IDs at 0.
const int TabNodePool::kInvalidTabNodeID = -1;

TabNodePool::~TabNodePool() {}

// Static
std::string TabNodePool::TabIdToTag(
    const std::string machine_tag, int tab_node_id) {
  return base::StringPrintf("%s %d", machine_tag.c_str(), tab_node_id);
}

void TabNodePool::AddTabNode(int64 sync_id,
                             const SessionID& tab_id,
                             int tab_node_id) {
  DCHECK_GT(sync_id, syncer::kInvalidId);
  DCHECK_GT(tab_id.id(), kInvalidTabID);
  DCHECK(syncid_tabid_map_.find(sync_id) == syncid_tabid_map_.end());
  unassociated_nodes_.insert(sync_id);
  if (max_used_tab_node_id_ < tab_node_id)
    max_used_tab_node_id_ = tab_node_id;
}

void TabNodePool::AssociateTabNode(int64 sync_id, SessionID::id_type tab_id) {
  DCHECK_GT(sync_id, 0);
  // Remove sync node if it is in unassociated nodes pool.
  std::set<int64>::iterator u_it = unassociated_nodes_.find(sync_id);
  if (u_it != unassociated_nodes_.end()) {
    unassociated_nodes_.erase(u_it);
  } else {
    // This is a new node association, the sync node should be free.
    // Remove node from free node pool and then associate it with the tab.
    std::set<int64>::iterator it = free_nodes_pool_.find(sync_id);
    DCHECK(it != free_nodes_pool_.end());
    free_nodes_pool_.erase(it);
  }
  DCHECK(syncid_tabid_map_.find(sync_id) == syncid_tabid_map_.end());
  syncid_tabid_map_[sync_id] = tab_id;
}

int64 TabNodePool::GetFreeTabNode() {
  DCHECK_GT(machine_tag_.length(), 0U);
  if (free_nodes_pool_.empty()) {
    // Tab pool has no free nodes, allocate new one.
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode root(&trans);
    if (root.InitByTagLookup(syncer::ModelTypeToRootTag(syncer::SESSIONS)) !=
                             syncer::BaseNode::INIT_OK) {
      LOG(ERROR) << kNoSessionsFolderError;
      return syncer::kInvalidId;
    }
    int tab_node_id = ++max_used_tab_node_id_;
    std::string tab_node_tag = TabIdToTag(machine_tag_, tab_node_id);
    syncer::WriteNode tab_node(&trans);
    syncer::WriteNode::InitUniqueByCreationResult result =
        tab_node.InitUniqueByCreation(syncer::SESSIONS, root, tab_node_tag);
    if (result != syncer::WriteNode::INIT_SUCCESS) {
      LOG(ERROR) << "Could not create new node with tag "
                 << tab_node_tag << "!";
      return syncer::kInvalidId;
    }
    // We fill the new node with just enough data so that in case of a crash/bug
    // we can identify the node as our own on re-association and reuse it.
    tab_node.SetTitle(UTF8ToWide(tab_node_tag));
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag(machine_tag_);
    specifics.set_tab_node_id(tab_node_id);
    tab_node.SetSessionSpecifics(specifics);

    // Grow the pool by 1 since we created a new node.
    DVLOG(1) << "Adding sync node " << tab_node.GetId()
             << " to tab syncid pool";
    free_nodes_pool_.insert(tab_node.GetId());
    return tab_node.GetId();
  } else {
    // Return the next free node.
    return *free_nodes_pool_.begin();
  }
}

void TabNodePool::FreeTabNode(int64 sync_id) {
  SyncIDToTabIDMap::iterator it = syncid_tabid_map_.find(sync_id);
  DCHECK(it != syncid_tabid_map_.end());
  syncid_tabid_map_.erase(it);
  FreeTabNodeInternal(sync_id);
}

void TabNodePool::FreeTabNodeInternal(int64 sync_id) {
  DCHECK(free_nodes_pool_.find(sync_id) == free_nodes_pool_.end());
  free_nodes_pool_.insert(sync_id);

  // If number of free nodes exceed kFreeNodesHighWatermark,
  // delete sync nodes till number reaches kFreeNodesLowWatermark.
  // Note: This logic is to mitigate temporary disassociation issues with old
  // clients: http://crbug.com/259918. Newer versions do not need this.
  if (free_nodes_pool_.size() > kFreeNodesHighWatermark) {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    for (std::set<int64>::iterator free_it = free_nodes_pool_.begin();
         free_it != free_nodes_pool_.end();) {
      syncer::WriteNode tab_node(&trans);
      if (tab_node.InitByIdLookup(*free_it) != syncer::BaseNode::INIT_OK) {
        LOG(ERROR) << "Could not find sync node with id: " << *free_it;
        return;
      }
      free_nodes_pool_.erase(free_it++);
      tab_node.Tombstone();
      if (free_nodes_pool_.size() <= kFreeNodesLowWatermark) {
        return;
      }
    }
  }
}

bool TabNodePool::IsUnassociatedTabNode(int64 sync_id) {
  return unassociated_nodes_.find(sync_id) != unassociated_nodes_.end();
}

void TabNodePool::ReassociateTabNode(int64 sync_id, SessionID::id_type tab_id) {
  // Remove from list of unassociated sync_nodes if present.
  std::set<int64>::iterator it = unassociated_nodes_.find(sync_id);
  if (it != unassociated_nodes_.end()) {
    unassociated_nodes_.erase(it);
  } else {
    // sync_id must be an already associated node.
    DCHECK(syncid_tabid_map_.find(sync_id) != syncid_tabid_map_.end());
  }
  syncid_tabid_map_[sync_id] = tab_id;
}

SessionID::id_type TabNodePool::GetTabIdFromSyncId(int64 sync_id) const {
  SyncIDToTabIDMap::const_iterator it = syncid_tabid_map_.find(sync_id);
  if (it != syncid_tabid_map_.end()) {
    return it->second;
  }
  return kInvalidTabID;
}

void TabNodePool::FreeUnassociatedTabNodes() {
  for (std::set<int64>::iterator it = unassociated_nodes_.begin();
       it != unassociated_nodes_.end();) {
    FreeTabNodeInternal(*it);
    unassociated_nodes_.erase(it++);
  }
  DCHECK(unassociated_nodes_.empty());
}

// Clear tab pool.
void TabNodePool::Clear() {
  unassociated_nodes_.clear();
  free_nodes_pool_.clear();
  syncid_tabid_map_.clear();
  max_used_tab_node_id_ = kInvalidTabNodeID;
}

size_t TabNodePool::Capacity() const {
  return syncid_tabid_map_.size() + unassociated_nodes_.size() +
         free_nodes_pool_.size();
}

bool TabNodePool::Empty() const { return free_nodes_pool_.empty(); }

bool TabNodePool::Full() { return syncid_tabid_map_.empty(); }

void TabNodePool::SetMachineTag(const std::string& machine_tag) {
  machine_tag_ = machine_tag;
}

}  // namespace browser_sync
