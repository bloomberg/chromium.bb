// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_H_

#include <list>
#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"

namespace android_webview {

class GlobalTileManagerClient;

// A global tile manager that keeps track of the number of tile resources. Each
// tile needs file descriptors (typically 2) and there is a soft limit of 1024
// file descriptors per Android process. The GlobalTileManager does not keep
// track of how many tiles each individual view is actually using. The purpose
// of GlobalTileManager is to behave gracefully (as in not crashing) when the
// embedder of webview creates a lot of webviews and draw them at the same time.
class GlobalTileManager {
 private:
  typedef std::list<GlobalTileManagerClient*> ListType;

 public:
  typedef ListType::iterator Key;
  static GlobalTileManager* GetInstance();

  // Requests the |num_of_tiles| from the available global pool. Calls
  // GlobalTileManagerClient.SetNumTiles after the manager determines how many
  // tiles are available for the client. If the number of tiles left is not
  // enough to satisfy the request, the manager will evict tiles allocated to
  // other clients.
  void RequestTiles(size_t new_num_of_tiles, Key key);

  Key PushBack(GlobalTileManagerClient* client);

  // |key| must be already in manager. Move the tile manager client
  // corresponding to |key| to most recent. This function should be called after
  // RequestTiles.
  void DidUse(Key key);

  void Remove(Key key);

 private:
  friend struct base::DefaultLazyInstanceTraits<GlobalTileManager>;
  GlobalTileManager();
  ~GlobalTileManager();

  // Continues evicting the inactive views until freeing up at least amount of
  // tiles specified by |desired_num_tiles| to draw a view specified by |key|,
  // or until all inactive views have been evicted. Returns the amount of
  // memory that was actually evicted. This function is called when a
  // request cannot be satisfied.
  size_t Evict(size_t desired_num_tiles, Key key);

  // Check that the sum of all client's tiles is equal to
  // total_allocated_tiles_.
  bool IsConsistent() const;

  size_t total_allocated_tiles_;
  ListType mru_list_;
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(GlobalTileManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GLOBAL_TILE_MANAGER_H_
