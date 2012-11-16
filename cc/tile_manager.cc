// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_manager.h"

#include <algorithm>

#include "base/logging.h"
#include "cc/tile.h"

namespace cc {

TileManager::TileManager(TileManagerClient* client)
  : client_(client)
  , manage_tiles_pending_(false)
{
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();
  ManageTiles();
  DCHECK(tiles_.size() == 0);
}

void TileManager::SetGlobalState(const GlobalStateThatImpactsTilePriority& global_state) {
  global_state_ = global_state;
  ScheduleManageTiles();
}

void TileManager::RegisterTile(Tile* tile) {
  tiles_.push_back(tile);
  ScheduleManageTiles();
}

void TileManager::UnregisterTile(Tile* tile) {
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); it++) {
    if (*it == tile) {
      tiles_.erase(it);
      return;
    }
  }
  DCHECK(false) << "Could not find tile version.";
}

void TileManager::WillModifyTilePriority(Tile*, WhichTree tree, const TilePriority& new_priority) {
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

class BinComparator {
public:
  bool operator() (const Tile* a, const Tile* b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();
    if (ams.bin != bms.bin)
      return ams.bin < bms.bin;

    if (ams.resolution != bms.resolution)
      return ams.resolution < ams.resolution;

    return
      ams.time_to_needed_in_seconds <
      bms.time_to_needed_in_seconds;
  }
};

void TileManager::ManageTiles() {
  // The amount of time for which we want to have prepainting coverage.
  const double prepainting_window_time_seconds = 1.0;
  const double backfling_guard_distance_pixels = 314.0;

  const bool smoothness_takes_priority = global_state_.smoothness_takes_priority;

  // Bin into three categories of tiles: things we need now, things we need soon, and eventually
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    TilePriority prio;
    if (smoothness_takes_priority)
      prio = tile->priority(ACTIVE_TREE);
    else
      prio = tile->combined_priority();

    mts.resolution = prio.resolution;
    mts.time_to_needed_in_seconds = prio.time_to_needed_in_seconds();

    if (mts.time_to_needed_in_seconds ==
        std::numeric_limits<float>::max()) {
      mts.bin = NEVER_BIN;
      continue;
    }

    if (mts.resolution == NON_IDEAL_RESOLUTION) {
      mts.bin = EVENTUALLY_BIN;
      continue;
    }

    if (mts.time_to_needed_in_seconds == 0 ||
        prio.distance_to_visible_in_pixels < backfling_guard_distance_pixels) {
      mts.bin = NOW_BIN;
      continue;
    }

    if (prio.time_to_needed_in_seconds() < prepainting_window_time_seconds) {
      mts.bin = SOON_BIN;
      continue;
    }

    mts.bin = EVENTUALLY_BIN;
  }

  // Memory limit policy works by mapping some bin states to the NEVER bin.
  TileManagerBin bin_map[NUM_BINS];
  if (global_state_.memory_limit_policy == ALLOW_NOTHING) {
    bin_map[NOW_BIN] = NEVER_BIN;
    bin_map[SOON_BIN] = NEVER_BIN;
    bin_map[EVENTUALLY_BIN] = NEVER_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  } else if (global_state_.memory_limit_policy == ALLOW_ABSOLUTE_MINIMUM) {
    bin_map[NOW_BIN] = NOW_BIN;
    bin_map[SOON_BIN] = NEVER_BIN;
    bin_map[EVENTUALLY_BIN] = NEVER_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  } else if (global_state_.memory_limit_policy == ALLOW_PREPAINT_ONLY) {
    bin_map[NOW_BIN] = NOW_BIN;
    bin_map[SOON_BIN] = SOON_BIN;
    bin_map[EVENTUALLY_BIN] = NEVER_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  } else {
    bin_map[NOW_BIN] = NOW_BIN;
    bin_map[SOON_BIN] = SOON_BIN;
    bin_map[EVENTUALLY_BIN] = NEVER_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  }
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    TileManagerBin bin = bin_map[tile->managed_state().bin];
    tile->managed_state().bin = bin;
  }

  // Sort by bin.
  std::sort(tiles_.begin(), tiles_.end(), BinComparator());

  // Some memory cannot be released. Figure out which.
  size_t unreleasable_bytes = 0;
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    if (tile->managed_state().resource_id_can_be_freed)
      unreleasable_bytes += tile->bytes_consumed_if_allocated();
  }

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-painted and needs-to-be-freed queues.
  tiles_that_need_to_be_painted_.erase(
      tiles_that_need_to_be_painted_.begin(),
      tiles_that_need_to_be_painted_.end());

  size_t bytes_left = global_state_.memory_limit_in_bytes - unreleasable_bytes;
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    ManagedTileState& managed_tile_state = tile->managed_state();
    if (managed_tile_state.resource_id_can_be_freed)
      continue;
    if (tile_bytes > bytes_left) {
      managed_tile_state.can_use_gpu_memory = false;
      if (managed_tile_state.resource_id && managed_tile_state.resource_id_can_be_freed)
        FreeResourcesForTile(tile);
      continue;
    }
    bytes_left -= tile_bytes;
    managed_tile_state.can_use_gpu_memory = true;
    if (!managed_tile_state.resource_id)
      tiles_that_need_to_be_painted_.push_back(tile);
  }

  // Reverse two tiles_that_need_* vectors such that pop_back gets
  // the highest priority tile.
  std::reverse(
      tiles_that_need_to_be_painted_.begin(),
      tiles_that_need_to_be_painted_.end());

  // Finally, kick the rasterizer.
  ScheduleMorePaintingJobs();
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  DCHECK(!tile->managed_state().can_use_gpu_memory &&
         tile->managed_state().resource_id_can_be_freed);
  // TODO(nduca): Do something intelligent here.
}

void TileManager::ScheduleMorePaintingJobs() {
  // TODO(nduca): The next big thing.
}


}
