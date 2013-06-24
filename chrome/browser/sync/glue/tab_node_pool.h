// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_TAB_NODE_POOL_H_
#define CHROME_BROWSER_SYNC_GLUE_TAB_NODE_POOL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

class ProfileSyncService;

namespace browser_sync {

// A pool for managing free/used tab sync nodes. Performs lazy creation
// of sync nodes when necessary.
class TabNodePool {
 public:
  explicit TabNodePool(ProfileSyncService* sync_service);
  ~TabNodePool();

  // Build a sync tag from tab_node_id.
  static std::string TabIdToTag(const std::string machine_tag,
                                size_t tab_node_id);

  // Add a previously allocated tab sync node to our pool. Increases the size
  // of tab_syncid_pool_ by one and marks the new tab node as free.
  // Note: this should only be called when we discover tab sync nodes from
  // previous sessions, not for freeing tab nodes we created through
  // GetFreeTabNode (use FreeTabNode below for that).
  void AddTabNode(int64 sync_id);

  // Returns the sync_id for the next free tab node. If none are available,
  // creates a new tab node.
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
  int64 GetFreeTabNode();

  // Return a tab node to our free pool.
  // Note: the difference between FreeTabNode and AddTabNode is that
  // FreeTabNode does not modify the size of |tab_syncid_pool_|, while
  // AddTabNode increases it by one. In the case of FreeTabNode, the size of
  // the |tab_syncid_pool_| should always be equal to the amount of tab nodes
  // associated with this machine.
  void FreeTabNode(int64 sync_id);

  // Clear tab pool.
  void clear() {
    tab_syncid_pool_.clear();
    tab_pool_fp_ = -1;
  }

  // Return the number of tab nodes this client currently has allocated
  // (including both free and used nodes)
  size_t capacity() const { return tab_syncid_pool_.size(); }

  // Return empty status (all tab nodes are in use).
  bool empty() const { return tab_pool_fp_ == -1; }

  // Return full status (no tab nodes are in use).
  bool full() {
    return tab_pool_fp_ == static_cast<int64>(tab_syncid_pool_.size())-1;
  }

  void set_machine_tag(const std::string& machine_tag) {
    machine_tag_ = machine_tag;
  }

 private:
  // Pool of all available syncid's for tab's we have created.
  std::vector<int64> tab_syncid_pool_;

  // Free pointer for tab pool. Only those node id's, up to and including the
  // one indexed by the free pointer, are valid and free. The rest of the
  // |tab_syncid_pool_| is invalid because the nodes are in use.
  // To get the next free node, use tab_syncid_pool_[tab_pool_fp_--].
  int64 tab_pool_fp_;

  // The machiine tag associated with this tab pool. Used in the title of new
  // sync nodes.
  std::string machine_tag_;

  // Our sync service profile (for making changes to the sync db)
  ProfileSyncService* sync_service_;

  DISALLOW_COPY_AND_ASSIGN(TabNodePool);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_TAB_NODE_POOL_H_
