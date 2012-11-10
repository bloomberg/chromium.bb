// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_manager.h"

#include "base/logging.h"

namespace cc {

TileManager::TileManager(TileManagerClient* client)
  : client_(client)
  , manage_tiles_pending_(false)
{
}

TileManager::~TileManager() {
  ManageTiles();
  DCHECK(tile_versions_.size() == 0);
}

void TileManager::SetGlobalState(const GlobalStateThatImpactsTilePriority& global_state) {
  global_state_ = global_state;
  ScheduleManageTiles();
}

void TileManager::ManageTiles() {
  // Figure out how much memory we would be willing to give out.

  // Free up memory.

  // GC old versions.
}

void TileManager::DidCreateTileVersion(TileVersion* version) {
  tile_versions_.push_back(version);
  ScheduleManageTiles();
}

void TileManager::DidDeleteTileVersion(TileVersion* version) {
  for (size_t i = 0; i < tile_versions_.size(); i++) {
    if (tile_versions_[i] == version) {
      tile_versions_.erase(tile_versions_.begin() + i);
      return;
    }
  }
  DCHECK(false) << "Could not find tile version.";
}

void TileManager::WillModifyTileVersionPriority(TileVersion*, const TilePriority& new_priority) {
  // TODO(nduca): Do something smarter if reprioritization turns out to be
  // costly.
  ScheduleManageTiles();
}

void TileManager::ScheduleManageTiles() {
  if (manage_tiles_pending_)
    return;
  ScheduleManageTiles();
  manage_tiles_pending_ = true;
}

}
