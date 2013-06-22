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

static const size_t kMaxNumberOfFreeNodes = 25;

TabNodePool::TabNodePool(ProfileSyncService* sync_service)
    : max_used_tab_node_id_(0), sync_service_(sync_service) {}

TabNodePool::~TabNodePool() {}

// Static
std::string TabNodePool::TabIdToTag(
    const std::string machine_tag,
    size_t tab_node_id) {
  return base::StringPrintf("%s %" PRIuS "", machine_tag.c_str(), tab_node_id);
}

void TabNodePool::AddTabNode(int64 sync_id,
                             const SessionID& tab_id,
                             size_t tab_node_id) {
  DCHECK_GT(sync_id, syncer::kInvalidId);
  DCHECK_GT(tab_id.id(), kInvalidTabID);
  DCHECK(syncid_tabid_map_.find(sync_id) == syncid_tabid_map_.end());
  syncid_tabid_map_[sync_id] = tab_id.id();
  if (max_used_tab_node_id_ < tab_node_id)
    max_used_tab_node_id_ = tab_node_id;
}

void TabNodePool::AssociateTabNode(int64 sync_id, SessionID::id_type tab_id) {
  DCHECK_GT(sync_id, 0);
  // Remove node from free node pool and associate it with tab.
  std::set<int64>::iterator it = free_nodes_pool_.find(sync_id);
  DCHECK(it != free_nodes_pool_.end());
  free_nodes_pool_.erase(it);
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
    size_t tab_node_id = ++max_used_tab_node_id_;
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
  DCHECK(free_nodes_pool_.find(sync_id) == free_nodes_pool_.end());

  // If number of free nodes exceed number of maximum allowed free nodes,
  // delete the sync node.
  if (free_nodes_pool_.size() < kMaxNumberOfFreeNodes) {
    free_nodes_pool_.insert(sync_id);
  } else {
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::WriteNode tab_node(&trans);
    if (tab_node.InitByIdLookup(sync_id) != syncer::BaseNode::INIT_OK) {
      LOG(ERROR) << "Could not find sync node with id: " << sync_id;
      return;
    }
    tab_node.Tombstone();
  }
}

bool TabNodePool::ReassociateTabNode(int64 sync_id, SessionID::id_type tab_id) {
  SyncIDToTabIDMap::iterator it = syncid_tabid_map_.find(sync_id);
  if (it != syncid_tabid_map_.end()) {
    syncid_tabid_map_[sync_id] = tab_id;
    return true;
  }
  return false;
}

SessionID::id_type TabNodePool::GetTabIdFromSyncId(int64 sync_id) const {
  SyncIDToTabIDMap::const_iterator it = syncid_tabid_map_.find(sync_id);
  if (it != syncid_tabid_map_.end()) {
    return it->second;
  }
  return kInvalidTabID;
}

void TabNodePool::FreeUnusedTabNodes(const std::set<int64>& used_sync_ids) {
  for (SyncIDToTabIDMap::iterator it = syncid_tabid_map_.begin();
       it != syncid_tabid_map_.end();) {
    if (used_sync_ids.find(it->first) == used_sync_ids.end()) {
      // This sync node is not used, return it to free node pool.
      int64 sync_id = it->first;
      ++it;
      FreeTabNode(sync_id);
    } else {
      ++it;
    }
  }
}

// Clear tab pool.
void TabNodePool::Clear() {
  free_nodes_pool_.clear();
  syncid_tabid_map_.clear();
  max_used_tab_node_id_ = 0;
}

size_t TabNodePool::Capacity() const {
  return syncid_tabid_map_.size() + free_nodes_pool_.size();
}

bool TabNodePool::Empty() const { return free_nodes_pool_.empty(); }

bool TabNodePool::Full() { return syncid_tabid_map_.empty(); }

void TabNodePool::SetMachineTag(const std::string& machine_tag) {
  machine_tag_ = machine_tag;
}

}  // namespace browser_sync
