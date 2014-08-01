// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/global_tile_manager.h"
#include "android_webview/browser/global_tile_manager_client.h"
#include "base/lazy_instance.h"

namespace android_webview {

namespace {

base::LazyInstance<GlobalTileManager>::Leaky g_tile_manager =
    LAZY_INSTANCE_INITIALIZER;

// The soft limit of the number of file descriptors per process is 1024 on
// Android and gralloc buffers may not be the only thing that uses file
// descriptors. For each tile, there is a gralloc buffer backing it, which
// uses 2 FDs.
const size_t kNumTilesLimit = 450;

}  // namespace

// static
GlobalTileManager* GlobalTileManager::GetInstance() {
  return g_tile_manager.Pointer();
}

void GlobalTileManager::Remove(Key key) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(mru_list_.end() != key);

  total_allocated_tiles_ -= (*key)->GetNumTiles();
  mru_list_.erase(key);
  DCHECK(IsConsistent());
}

size_t GlobalTileManager::Evict(size_t desired_num_tiles, Key key) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  size_t total_evicted_tiles = 0;

  // Evicts from the least recent drawn view, until the disired number of tiles
  // can be reclaimed, or until we've evicted all inactive views.
  ListType::reverse_iterator it;
  for (it = mru_list_.rbegin(); it != mru_list_.rend(); it++) {
    // key represents the view that requested the eviction, so we don't need to
    // evict the requester itself. And we only evict the inactive views,
    // which are all the views after the requester.
    if (*it == *key)
      break;

    size_t evicted_tiles = (*it)->GetNumTiles();
    (*it)->SetNumTiles(0, true);

    total_evicted_tiles += evicted_tiles;
    if (total_evicted_tiles >= desired_num_tiles)
      break;
  }

  return total_evicted_tiles;
}

void GlobalTileManager::SetTileLimit(size_t num_tiles_limit) {
  num_tiles_limit_ = num_tiles_limit;
}

void GlobalTileManager::RequestTiles(size_t new_num_of_tiles, Key key) {
  DCHECK(IsConsistent());
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  size_t old_num_of_tiles = (*key)->GetNumTiles();
  size_t num_of_active_views = std::distance(mru_list_.begin(), key) + 1;
  size_t tiles_per_view_limit;
  if (num_of_active_views == 0)
    tiles_per_view_limit = num_tiles_limit_;
  else
    tiles_per_view_limit = num_tiles_limit_ / num_of_active_views;
  new_num_of_tiles = std::min(new_num_of_tiles, tiles_per_view_limit);
  size_t new_total_allocated_tiles =
      total_allocated_tiles_ - old_num_of_tiles + new_num_of_tiles;
  // Has enough tiles to satisfy the request.
  if (new_total_allocated_tiles <= num_tiles_limit_) {
    total_allocated_tiles_ = new_total_allocated_tiles;
    (*key)->SetNumTiles(new_num_of_tiles, false);
    return;
  }

  // Does not have enough tiles. Now evict other clients' tiles.
  size_t tiles_left = num_tiles_limit_ - total_allocated_tiles_;

  size_t evicted_tiles =
      Evict(new_total_allocated_tiles - num_tiles_limit_, key);
  if (evicted_tiles >= new_total_allocated_tiles - num_tiles_limit_) {
    new_total_allocated_tiles -= evicted_tiles;
    total_allocated_tiles_ = new_total_allocated_tiles;
    (*key)->SetNumTiles(new_num_of_tiles, false);
    return;
  } else {
    total_allocated_tiles_ = num_tiles_limit_;
    (*key)->SetNumTiles(tiles_left + old_num_of_tiles + evicted_tiles, false);
    return;
  }
}

GlobalTileManager::Key GlobalTileManager::PushBack(
    GlobalTileManagerClient* client) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(mru_list_.end() ==
         std::find(mru_list_.begin(), mru_list_.end(), client));
  mru_list_.push_back(client);
  Key back = mru_list_.end();
  back--;
  return back;
}

void GlobalTileManager::DidUse(Key key) {
  DCHECK(sequence_checker_.CalledOnValidSequencedThread());
  DCHECK(mru_list_.end() != key);

  mru_list_.splice(mru_list_.begin(), mru_list_, key);
}

GlobalTileManager::GlobalTileManager()
    : num_tiles_limit_(kNumTilesLimit), total_allocated_tiles_(0) {
}

GlobalTileManager::~GlobalTileManager() {
}

bool GlobalTileManager::IsConsistent() const {
  size_t total_tiles = 0;
  ListType::const_iterator it;
  for (it = mru_list_.begin(); it != mru_list_.end(); it++) {
    total_tiles += (*it)->GetNumTiles();
  }

  bool is_consistent = (total_tiles <= num_tiles_limit_ &&
                        total_tiles == total_allocated_tiles_);

  return is_consistent;
}

}  // namespace webview
