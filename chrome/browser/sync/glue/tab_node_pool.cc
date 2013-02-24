// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/tab_node_pool.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"

namespace browser_sync {

static const char kNoSessionsFolderError[] =
    "Server did not create the top-level sessions node. We "
    "might be running against an out-of-date server.";

TabNodePool::TabNodePool(
    ProfileSyncService* sync_service)
    : tab_pool_fp_(-1),
      sync_service_(sync_service) {
}

TabNodePool::~TabNodePool() {}

// Static
std::string TabNodePool::TabIdToTag(
    const std::string machine_tag,
    size_t tab_node_id) {
  return base::StringPrintf("%s %"PRIuS"", machine_tag.c_str(), tab_node_id);
}

void TabNodePool::AddTabNode(int64 sync_id) {
  tab_syncid_pool_.resize(tab_syncid_pool_.size() + 1);
  tab_syncid_pool_[static_cast<size_t>(++tab_pool_fp_)] = sync_id;
}

int64 TabNodePool::GetFreeTabNode() {
  DCHECK_GT(machine_tag_.length(), 0U);
  if (tab_pool_fp_ == -1) {
    // Tab pool has no free nodes, allocate new one.
    syncer::WriteTransaction trans(FROM_HERE, sync_service_->GetUserShare());
    syncer::ReadNode root(&trans);
    if (root.InitByTagLookup(syncer::ModelTypeToRootTag(syncer::SESSIONS)) !=
                             syncer::BaseNode::INIT_OK) {
      LOG(ERROR) << kNoSessionsFolderError;
      return syncer::kInvalidId;
    }
    size_t tab_node_id = tab_syncid_pool_.size();
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
    tab_node.SetTitle(base::UTF8ToWide(tab_node_tag));
    sync_pb::SessionSpecifics specifics;
    specifics.set_session_tag(machine_tag_);
    specifics.set_tab_node_id(tab_node_id);
    tab_node.SetSessionSpecifics(specifics);

    // Grow the pool by 1 since we created a new node. We don't actually need
    // to put the node's id in the pool now, since the pool is still empty.
    // The id will be added when that tab is closed and the node is freed.
    tab_syncid_pool_.resize(tab_node_id + 1);
    DVLOG(1) << "Adding sync node "
             << tab_node.GetId() << " to tab syncid pool";
    return tab_node.GetId();
  } else {
    // There are nodes available, grab next free and decrement free pointer.
    return tab_syncid_pool_[static_cast<size_t>(tab_pool_fp_--)];
  }
}

void TabNodePool::FreeTabNode(int64 sync_id) {
  // Pool size should always match # of free tab nodes.
  DCHECK_LT(tab_pool_fp_, static_cast<int64>(tab_syncid_pool_.size()));
  tab_syncid_pool_[static_cast<size_t>(++tab_pool_fp_)] = sync_id;
}

}  // namespace browser_sync
