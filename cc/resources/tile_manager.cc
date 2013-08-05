// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_manager.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/tile.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

namespace {

// Memory limit policy works by mapping some bin states to the NEVER bin.
const ManagedTileBin kBinPolicyMap[NUM_TILE_MEMORY_LIMIT_POLICIES][NUM_BINS] = {
  {  // [ALLOW_NOTHING]
    NEVER_BIN,      // [NOW_BIN]
    NEVER_BIN,      // [SOON_BIN]
    NEVER_BIN,      // [EVENTUALLY_BIN]
    NEVER_BIN       // [NEVER_BIN]
  }, {  // [ALLOW_ABSOLUTE_MINIMUM]
    NOW_BIN,        // [NOW_BIN]
    NEVER_BIN,      // [SOON_BIN]
    NEVER_BIN,      // [EVENTUALLY_BIN]
    NEVER_BIN       // [NEVER_BIN]
  }, {  // [ALLOW_PREPAINT_ONLY]
    NOW_BIN,         // [NOW_BIN]
    SOON_BIN,        // [SOON_BIN]
    NEVER_BIN,       // [EVENTUALLY_BIN]
    NEVER_BIN        // [NEVER_BIN]
  }, {  // [ALLOW_ANYTHING]
    NOW_BIN,         // [NOW_BIN]
    SOON_BIN,        // [SOON_BIN]
    EVENTUALLY_BIN,  // [EVENTUALLY_BIN]
    NEVER_BIN        // [NEVER_BIN]
  }
};

// Determine bin based on three categories of tiles: things we need now,
// things we need soon, and eventually.
inline ManagedTileBin BinFromTilePriority(const TilePriority& prio,
                                          TreePriority tree_priority) {
  // The amount of time for which we want to have prepainting coverage.
  const float kPrepaintingWindowTimeSeconds = 1.0f;
  const float kBackflingGuardDistancePixels = 314.0f;

  // Don't let low res tiles be in the now bin unless we're in a mode where
  // we're prioritizing checkerboard prevention.
  bool can_be_in_now_bin = tree_priority == SMOOTHNESS_TAKES_PRIORITY ||
                           prio.resolution != LOW_RESOLUTION;

  if (prio.distance_to_visible_in_pixels ==
      std::numeric_limits<float>::infinity())
    return NEVER_BIN;

  if (can_be_in_now_bin && prio.time_to_visible_in_seconds == 0)
    return NOW_BIN;

  if (prio.resolution == NON_IDEAL_RESOLUTION)
    return EVENTUALLY_BIN;

  if (prio.distance_to_visible_in_pixels < kBackflingGuardDistancePixels ||
      prio.time_to_visible_in_seconds < kPrepaintingWindowTimeSeconds)
    return SOON_BIN;

  return EVENTUALLY_BIN;
}

// Limit to the number of raster tasks that can be scheduled.
// This is high enough to not cause unnecessary scheduling but
// gives us an insurance that we're not spending a huge amount
// of time scheduling one enormous set of tasks.
const size_t kMaxRasterTasks = 256u;

}  // namespace

RasterTaskCompletionStats::RasterTaskCompletionStats()
    : completed_count(0u),
      canceled_count(0u) {
}

scoped_ptr<base::Value> RasterTaskCompletionStatsAsValue(
    const RasterTaskCompletionStats& stats) {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetInteger("completed_count", stats.completed_count);
  state->SetInteger("canceled_count", stats.canceled_count);
  return state.PassAs<base::Value>();
}

// static
scoped_ptr<TileManager> TileManager::Create(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    size_t num_raster_threads,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    bool use_map_image) {
  return make_scoped_ptr(
      new TileManager(client,
                      resource_provider,
                      use_map_image ?
                      ImageRasterWorkerPool::Create(
                          resource_provider, num_raster_threads) :
                      PixelBufferRasterWorkerPool::Create(
                          resource_provider, num_raster_threads),
                      num_raster_threads,
                      rendering_stats_instrumentation,
                      resource_provider->best_texture_format()));
}

TileManager::TileManager(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    scoped_ptr<RasterWorkerPool> raster_worker_pool,
    size_t num_raster_threads,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    GLenum texture_format)
    : client_(client),
      resource_pool_(ResourcePool::Create(resource_provider)),
      raster_worker_pool_(raster_worker_pool.Pass()),
      all_tiles_that_need_to_be_rasterized_have_memory_(true),
      all_tiles_required_for_activation_have_memory_(true),
      all_tiles_required_for_activation_have_been_initialized_(true),
      ever_exceeded_memory_budget_(false),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      did_initialize_visible_tile_(false),
      texture_format_(texture_format) {
  raster_worker_pool_->SetClient(this);
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();

  // Clear |sorted_tiles_| so that tiles kept alive by it can be freed.
  sorted_tiles_.clear();
  DCHECK_EQ(0u, tiles_.size());

  TileVector empty;
  ScheduleTasks(empty);

  // This should finish all pending tasks and release any uninitialized
  // resources.
  raster_worker_pool_->Shutdown();
  raster_worker_pool_->CheckForCompletedTasks();
}

void TileManager::SetGlobalState(
    const GlobalStateThatImpactsTilePriority& global_state) {
  global_state_ = global_state;
  resource_pool_->SetMemoryUsageLimits(
      global_state_.memory_limit_in_bytes,
      global_state_.unused_memory_limit_in_bytes,
      global_state_.num_resources_limit);
}

void TileManager::RegisterTile(Tile* tile) {
  DCHECK(!tile->required_for_activation());
  DCHECK(tiles_.find(tile->id()) == tiles_.end());

  tiles_[tile->id()] = tile;
}

void TileManager::UnregisterTile(Tile* tile) {
  FreeResourcesForTile(tile);

  DCHECK(tiles_.find(tile->id()) != tiles_.end());
  tiles_.erase(tile->id());
}

bool TileManager::ShouldForceTasksRequiredForActivationToComplete() const {
  return GlobalState().tree_priority != SMOOTHNESS_TAKES_PRIORITY;
}

void TileManager::DidFinishRunningTasks() {
  TRACE_EVENT0("cc", "TileManager::DidFinishRunningTasks");

  // When OOM, keep re-assigning memory until we reach a steady state
  // where top-priority tiles are initialized.
  if (all_tiles_that_need_to_be_rasterized_have_memory_)
    return;

  raster_worker_pool_->CheckForCompletedTasks();

  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(sorted_tiles_, &tiles_that_need_to_be_rasterized);

  // |tiles_that_need_to_be_rasterized| will be empty when we reach a
  // steady memory state. Keep scheduling tasks until we reach this state.
  if (!tiles_that_need_to_be_rasterized.empty()) {
    ScheduleTasks(tiles_that_need_to_be_rasterized);
    return;
  }

  // Use on-demand raster for any required-for-activation tiles that have not
  // been been assigned memory after reaching a steady memory state. This
  // ensures that we activate even when OOM.
  for (TileMap::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = it->second;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    if (tile->required_for_activation() && !tile_version.IsReadyToDraw())
      tile_version.set_rasterize_on_demand();
  }

  client_->NotifyReadyToActivate();
}

void TileManager::DidFinishRunningTasksRequiredForActivation() {
  // This is only a true indication that all tiles required for
  // activation are initialized when no tiles are OOM. We need to
  // wait for DidFinishRunningTasks() to be called, try to re-assign
  // memory and in worst case use on-demand raster when tiles
  // required for activation are OOM.
  if (!all_tiles_required_for_activation_have_memory_)
    return;

  client_->NotifyReadyToActivate();
}

class BinComparator {
 public:
  bool operator()(const scoped_refptr<Tile> a,
                  const scoped_refptr<Tile> b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();

    if (ams.visible_and_ready_to_draw != bms.visible_and_ready_to_draw)
      return ams.visible_and_ready_to_draw;

    if (ams.bin[HIGH_PRIORITY_BIN] != bms.bin[HIGH_PRIORITY_BIN])
      return ams.bin[HIGH_PRIORITY_BIN] < bms.bin[HIGH_PRIORITY_BIN];

    if (ams.bin[LOW_PRIORITY_BIN] != bms.bin[LOW_PRIORITY_BIN])
      return ams.bin[LOW_PRIORITY_BIN] < bms.bin[LOW_PRIORITY_BIN];

    if (ams.required_for_activation != bms.required_for_activation)
      return ams.required_for_activation;

    if (ams.resolution != bms.resolution)
      return ams.resolution < bms.resolution;

    if (ams.time_to_needed_in_seconds !=  bms.time_to_needed_in_seconds)
      return ams.time_to_needed_in_seconds < bms.time_to_needed_in_seconds;

    if (ams.distance_to_visible_in_pixels !=
        bms.distance_to_visible_in_pixels) {
      return ams.distance_to_visible_in_pixels <
             bms.distance_to_visible_in_pixels;
    }

    gfx::Rect a_rect = a->content_rect();
    gfx::Rect b_rect = b->content_rect();
    if (a_rect.y() != b_rect.y())
      return a_rect.y() < b_rect.y();
    return a_rect.x() < b_rect.x();
  }
};

void TileManager::GetTilesWithAssignedBins(TileRefVector* tiles) {
  TRACE_EVENT0("cc", "TileManager::GetTilesWithAssignedBins");

  DCHECK_EQ(0u, tiles->size());
  tiles->reserve(tiles_.size());

  const TileMemoryLimitPolicy memory_policy = global_state_.memory_limit_policy;
  const TreePriority tree_priority = global_state_.tree_priority;

  // For each tree, bin into different categories of tiles.
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = it->second;
    ManagedTileState& mts = tile->managed_state();

    TilePriority prio[NUM_BIN_PRIORITIES];
    switch (tree_priority) {
      case SAME_PRIORITY_FOR_BOTH_TREES:
        prio[HIGH_PRIORITY_BIN] = prio[LOW_PRIORITY_BIN] =
            tile->combined_priority();
        break;
      case SMOOTHNESS_TAKES_PRIORITY:
        prio[HIGH_PRIORITY_BIN] = tile->priority(ACTIVE_TREE);
        prio[LOW_PRIORITY_BIN] = tile->priority(PENDING_TREE);
        break;
      case NEW_CONTENT_TAKES_PRIORITY:
        prio[HIGH_PRIORITY_BIN] = tile->priority(PENDING_TREE);
        prio[LOW_PRIORITY_BIN] = tile->priority(ACTIVE_TREE);
        break;
    }

    mts.resolution = prio[HIGH_PRIORITY_BIN].resolution;
    mts.time_to_needed_in_seconds =
        prio[HIGH_PRIORITY_BIN].time_to_visible_in_seconds;
    mts.distance_to_visible_in_pixels =
        prio[HIGH_PRIORITY_BIN].distance_to_visible_in_pixels;
    mts.required_for_activation =
        prio[HIGH_PRIORITY_BIN].required_for_activation;
    mts.bin[HIGH_PRIORITY_BIN] =
        BinFromTilePriority(prio[HIGH_PRIORITY_BIN], tree_priority);
    mts.bin[LOW_PRIORITY_BIN] =
        BinFromTilePriority(prio[LOW_PRIORITY_BIN], tree_priority);
    mts.gpu_memmgr_stats_bin =
        BinFromTilePriority(tile->combined_priority(), tree_priority);

    mts.tree_bin[ACTIVE_TREE] = kBinPolicyMap[memory_policy][
        BinFromTilePriority(tile->priority(ACTIVE_TREE), tree_priority)];
    mts.tree_bin[PENDING_TREE] = kBinPolicyMap[memory_policy][
        BinFromTilePriority(tile->priority(PENDING_TREE), tree_priority)];

    for (int i = 0; i < NUM_BIN_PRIORITIES; ++i)
      mts.bin[i] = kBinPolicyMap[memory_policy][mts.bin[i]];

    mts.visible_and_ready_to_draw =
        mts.tree_bin[ACTIVE_TREE] == NOW_BIN && tile->IsReadyToDraw();

    // Skip and free resources for tiles in the NEVER_BIN on both trees.
    if (mts.is_in_never_bin_on_both_trees())
      FreeResourcesForTile(tile);
    else
      tiles->push_back(make_scoped_refptr(tile));
  }
}

void TileManager::SortTiles(TileRefVector* tiles) {
  TRACE_EVENT0("cc", "TileManager::SortTiles");

  // Sort by bin, resolution and time until needed.
  std::sort(tiles->begin(), tiles->end(), BinComparator());
}

void TileManager::GetSortedTilesWithAssignedBins(TileRefVector* tiles) {
  TRACE_EVENT0("cc", "TileManager::GetSortedTilesWithAssignedBins");

  GetTilesWithAssignedBins(tiles);
  SortTiles(tiles);
}

void TileManager::ManageTiles() {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");

  // Clear |sorted_tiles_| so that tiles kept alive by it can be freed.
  sorted_tiles_.clear();

  GetSortedTilesWithAssignedBins(&sorted_tiles_);

  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(sorted_tiles_, &tiles_that_need_to_be_rasterized);
  CleanUpUnusedImageDecodeTasks();

  TRACE_EVENT_INSTANT1(
      "cc", "DidManage", TRACE_EVENT_SCOPE_THREAD,
      "state", TracedValue::FromValue(BasicStateAsValue().release()));

  // Finally, schedule rasterizer tasks.
  ScheduleTasks(tiles_that_need_to_be_rasterized);
}

bool TileManager::UpdateVisibleTiles() {
  TRACE_EVENT0("cc", "TileManager::UpdateVisibleTiles");

  raster_worker_pool_->CheckForCompletedTasks();

  TRACE_EVENT_INSTANT1(
      "cc", "DidUpdateVisibleTiles", TRACE_EVENT_SCOPE_THREAD,
      "stats", TracedValue::FromValue(
          RasterTaskCompletionStatsAsValue(
              update_visible_tiles_stats_).release()));
  update_visible_tiles_stats_ = RasterTaskCompletionStats();

  bool did_initialize_visible_tile = did_initialize_visible_tile_;
  did_initialize_visible_tile_ = false;
  return did_initialize_visible_tile;
}

void TileManager::GetMemoryStats(
    size_t* memory_required_bytes,
    size_t* memory_nice_to_have_bytes,
    size_t* memory_used_bytes) const {
  *memory_required_bytes = 0;
  *memory_nice_to_have_bytes = 0;
  *memory_used_bytes = resource_pool_->acquired_memory_usage_bytes();
  for (TileMap::const_iterator it = tiles_.begin();
       it != tiles_.end();
       ++it) {
    const Tile* tile = it->second;
    const ManagedTileState& mts = tile->managed_state();

    const ManagedTileState::TileVersion& tile_version =
        tile->GetTileVersionForDrawing();
    if (tile_version.IsReadyToDraw() &&
        !tile_version.requires_resource())
      continue;

    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    if (mts.gpu_memmgr_stats_bin == NOW_BIN)
      *memory_required_bytes += tile_bytes;
    if (mts.gpu_memmgr_stats_bin != NEVER_BIN)
      *memory_nice_to_have_bytes += tile_bytes;
  }
}

scoped_ptr<base::Value> TileManager::BasicStateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetInteger("tile_count", tiles_.size());
  state->Set("global_state", global_state_.AsValue().release());
  state->Set("memory_requirements", GetMemoryRequirementsAsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileManager::AllTilesAsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  for (TileMap::const_iterator it = tiles_.begin();
       it != tiles_.end();
       it++) {
    state->Append(it->second->AsValue().release());
  }
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileManager::GetMemoryRequirementsAsValue() const {
  scoped_ptr<base::DictionaryValue> requirements(
      new base::DictionaryValue());

  size_t memory_required_bytes;
  size_t memory_nice_to_have_bytes;
  size_t memory_used_bytes;
  GetMemoryStats(&memory_required_bytes,
                 &memory_nice_to_have_bytes,
                 &memory_used_bytes);
  requirements->SetInteger("memory_required_bytes", memory_required_bytes);
  requirements->SetInteger("memory_nice_to_have_bytes",
                           memory_nice_to_have_bytes);
  requirements->SetInteger("memory_used_bytes", memory_used_bytes);
  return requirements.PassAs<base::Value>();
}

RasterMode TileManager::DetermineRasterMode(const Tile* tile) const {
  DCHECK(tile);
  DCHECK(tile->picture_pile());

  const ManagedTileState& mts = tile->managed_state();
  RasterMode current_mode = mts.raster_mode;

  RasterMode raster_mode = HIGH_QUALITY_RASTER_MODE;
  if (tile->managed_state().resolution == LOW_RESOLUTION)
    raster_mode = LOW_QUALITY_RASTER_MODE;
  else if (tile->can_use_lcd_text())
    raster_mode = HIGH_QUALITY_RASTER_MODE;
  else if (mts.tile_versions[current_mode].has_text_ ||
           !mts.tile_versions[current_mode].IsReadyToDraw())
    raster_mode = HIGH_QUALITY_NO_LCD_RASTER_MODE;

  return std::min(raster_mode, current_mode);
}

void TileManager::AssignGpuMemoryToTiles(
    const TileRefVector& sorted_tiles,
    TileVector* tiles_that_need_to_be_rasterized) {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  size_t bytes_releasable = 0;
  size_t resources_releasable = 0;
  for (TileRefVector::const_iterator it = sorted_tiles.begin();
       it != sorted_tiles.end();
       ++it) {
    const Tile* tile = it->get();
    const ManagedTileState& mts = tile->managed_state();
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (mts.tile_versions[mode].resource_) {
        bytes_releasable += tile->bytes_consumed_if_allocated();
        resources_releasable++;
      }
    }
  }

  all_tiles_that_need_to_be_rasterized_have_memory_ = true;
  all_tiles_required_for_activation_have_memory_ = true;
  all_tiles_required_for_activation_have_been_initialized_ = true;

  // Cast to prevent overflow.
  int64 bytes_available =
      static_cast<int64>(bytes_releasable) +
      static_cast<int64>(global_state_.memory_limit_in_bytes) -
      static_cast<int64>(resource_pool_->acquired_memory_usage_bytes());
  int resources_available = resources_releasable +
                            global_state_.num_resources_limit -
                            resource_pool_->NumResources();

  size_t bytes_allocatable =
      std::max(static_cast<int64>(0), bytes_available);
  size_t resources_allocatable = std::max(0, resources_available);

  size_t bytes_that_exceeded_memory_budget = 0;
  size_t bytes_left = bytes_allocatable;
  size_t resources_left = resources_allocatable;
  bool oomed = false;

  unsigned schedule_priority = 1u;
  for (TileRefVector::const_iterator it = sorted_tiles.begin();
       it != sorted_tiles.end();
       ++it) {
    Tile* tile = it->get();
    ManagedTileState& mts = tile->managed_state();

    mts.scheduled_priority = schedule_priority++;

    mts.raster_mode = DetermineRasterMode(tile);

    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    // If this tile doesn't need a resource, then nothing to do.
    if (!tile_version.requires_resource())
      continue;

    // If the tile is not needed, free it up.
    if (mts.is_in_never_bin_on_both_trees()) {
      FreeResourcesForTile(tile);
      continue;
    }

    size_t tile_bytes = 0;
    size_t tile_resources = 0;

    // It costs to maintain a resource.
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (mts.tile_versions[mode].resource_) {
        tile_bytes += tile->bytes_consumed_if_allocated();
        tile_resources++;
      }
    }

    // Allow lower priority tiles with initialized resources to keep
    // their memory by only assigning memory to new raster tasks if
    // they can be scheduled.
    if (tiles_that_need_to_be_rasterized->size() < kMaxRasterTasks) {
      // If we don't have the required version, and it's not in flight
      // then we'll have to pay to create a new task.
      if (!tile_version.resource_ && tile_version.raster_task_.is_null()) {
        tile_bytes += tile->bytes_consumed_if_allocated();
        tile_resources++;
      }
    }

    // Tile is OOM.
    if (tile_bytes > bytes_left || tile_resources > resources_left) {
      FreeResourcesForTile(tile);

      // This tile was already on screen and now its resources have been
      // released. In order to prevent checkerboarding, set this tile as
      // rasterize on demand immediately.
      if (mts.visible_and_ready_to_draw)
        tile_version.set_rasterize_on_demand();

      oomed = true;
      bytes_that_exceeded_memory_budget += tile_bytes;
    } else {
      bytes_left -= tile_bytes;
      resources_left -= tile_resources;

      if (tile_version.resource_)
        continue;
    }

    DCHECK(!tile_version.resource_);

    if (tile->required_for_activation())
      all_tiles_required_for_activation_have_been_initialized_ = false;

    // Tile shouldn't be rasterized if |tiles_that_need_to_be_rasterized|
    // has reached it's limit or we've failed to assign gpu memory to this
    // or any higher priority tile. Preventing tiles that fit into memory
    // budget to be rasterized when higher priority tile is oom is
    // important for two reasons:
    // 1. Tile size should not impact raster priority.
    // 2. Tiles with existing raster task could otherwise incorrectly
    //    be added as they are not affected by |bytes_allocatable|.
    if (oomed || tiles_that_need_to_be_rasterized->size() >= kMaxRasterTasks) {
      all_tiles_that_need_to_be_rasterized_have_memory_ = false;
      if (tile->required_for_activation())
        all_tiles_required_for_activation_have_memory_ = false;
      continue;
    }

    tiles_that_need_to_be_rasterized->push_back(tile);
  }

  ever_exceeded_memory_budget_ |= bytes_that_exceeded_memory_budget > 0;
  if (ever_exceeded_memory_budget_) {
      TRACE_COUNTER_ID2("cc", "over_memory_budget", this,
                        "budget", global_state_.memory_limit_in_bytes,
                        "over", bytes_that_exceeded_memory_budget);
  }
  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.memory_limit_in_bytes;
  memory_stats_from_last_assign_.bytes_allocated =
      bytes_allocatable - bytes_left;
  memory_stats_from_last_assign_.bytes_unreleasable =
      bytes_allocatable - bytes_releasable;
  memory_stats_from_last_assign_.bytes_over =
      bytes_that_exceeded_memory_budget;
}

void TileManager::CleanUpUnusedImageDecodeTasks() {
  // Calculate a set of layers that are used by at least one tile.
  base::hash_set<int> used_layers;
  for (TileMap::iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    used_layers.insert(it->second->layer_id());

  // Now calculate the set of layers in |image_decode_tasks_| that are not used
  // by any tile.
  std::vector<int> unused_layers;
  for (LayerPixelRefTaskMap::iterator it = image_decode_tasks_.begin();
       it != image_decode_tasks_.end();
       ++it) {
    if (used_layers.find(it->first) == used_layers.end())
      unused_layers.push_back(it->first);
  }

  // Erase unused layers from |image_decode_tasks_|.
  for (std::vector<int>::iterator it = unused_layers.begin();
       it != unused_layers.end();
       ++it) {
    image_decode_tasks_.erase(*it);
  }
}

void TileManager::FreeResourceForTile(Tile* tile, RasterMode mode) {
  ManagedTileState& mts = tile->managed_state();
  if (mts.tile_versions[mode].resource_) {
    resource_pool_->ReleaseResource(
        mts.tile_versions[mode].resource_.Pass());
  }
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    FreeResourceForTile(tile, static_cast<RasterMode>(mode));
  }
}

void TileManager::FreeUnusedResourcesForTile(Tile* tile) {
  DCHECK(tile->IsReadyToDraw());
  ManagedTileState& mts = tile->managed_state();
  RasterMode used_mode = HIGH_QUALITY_NO_LCD_RASTER_MODE;
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    if (mts.tile_versions[mode].IsReadyToDraw()) {
      used_mode = static_cast<RasterMode>(mode);
      break;
    }
  }

  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    if (mode != used_mode)
      FreeResourceForTile(tile, static_cast<RasterMode>(mode));
  }
}

void TileManager::ScheduleTasks(
    const TileVector& tiles_that_need_to_be_rasterized) {
  TRACE_EVENT1("cc", "TileManager::ScheduleTasks",
               "count", tiles_that_need_to_be_rasterized.size());
  RasterWorkerPool::RasterTask::Queue tasks;

  // Build a new task queue containing all task currently needed. Tasks
  // are added in order of priority, highest priority task first.
  for (TileVector::const_iterator it = tiles_that_need_to_be_rasterized.begin();
       it != tiles_that_need_to_be_rasterized.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    DCHECK(tile_version.requires_resource());
    DCHECK(!tile_version.resource_);

    if (tile_version.raster_task_.is_null())
      tile_version.raster_task_ = CreateRasterTask(tile);

    tasks.Append(tile_version.raster_task_, tile->required_for_activation());
  }

  // Schedule running of |tasks|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |tasks|.
  raster_worker_pool_->ScheduleTasks(&tasks);
}

RasterWorkerPool::Task TileManager::CreateImageDecodeTask(
    Tile* tile, skia::LazyPixelRef* pixel_ref) {
  return RasterWorkerPool::CreateImageDecodeTask(
      pixel_ref,
      tile->layer_id(),
      rendering_stats_instrumentation_,
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 tile->layer_id(),
                 base::Unretained(pixel_ref)));
}

RasterWorkerPool::RasterTask TileManager::CreateRasterTask(Tile* tile) {
  ManagedTileState& mts = tile->managed_state();

  scoped_ptr<ResourcePool::Resource> resource =
      resource_pool_->AcquireResource(tile->tile_size_.size(),
                                      texture_format_);
  const Resource* const_resource = resource.get();

  // Create and queue all image decode tasks that this tile depends on.
  RasterWorkerPool::Task::Set decode_tasks;
  PixelRefTaskMap& existing_pixel_refs = image_decode_tasks_[tile->layer_id()];
  for (PicturePileImpl::PixelRefIterator iter(tile->content_rect(),
                                              tile->contents_scale(),
                                              tile->picture_pile());
       iter; ++iter) {
    skia::LazyPixelRef* pixel_ref = *iter;
    uint32_t id = pixel_ref->getGenerationID();

    // Append existing image decode task if available.
    PixelRefTaskMap::iterator decode_task_it = existing_pixel_refs.find(id);
    if (decode_task_it != existing_pixel_refs.end()) {
      decode_tasks.Insert(decode_task_it->second);
      continue;
    }

    // Create and append new image decode task for this pixel ref.
    RasterWorkerPool::Task decode_task = CreateImageDecodeTask(
        tile, pixel_ref);
    decode_tasks.Insert(decode_task);
    existing_pixel_refs[id] = decode_task;
  }

  return RasterWorkerPool::CreateRasterTask(
      const_resource,
      tile->picture_pile(),
      tile->content_rect(),
      tile->contents_scale(),
      mts.raster_mode,
      mts.tree_bin[PENDING_TREE] == NOW_BIN,
      mts.resolution,
      tile->layer_id(),
      static_cast<const void *>(tile),
      tile->source_frame_number(),
      rendering_stats_instrumentation_,
      base::Bind(&TileManager::OnRasterTaskCompleted,
                 base::Unretained(this),
                 tile->id(),
                 base::Passed(&resource),
                 mts.raster_mode),
      &decode_tasks);
}

void TileManager::OnImageDecodeTaskCompleted(
    int layer_id,
    skia::LazyPixelRef* pixel_ref,
    bool was_canceled) {
  // If the task was canceled, we need to clean it up
  // from |image_decode_tasks_|.
  if (!was_canceled)
    return;

  LayerPixelRefTaskMap::iterator layer_it =
      image_decode_tasks_.find(layer_id);

  if (layer_it == image_decode_tasks_.end())
    return;

  PixelRefTaskMap& pixel_ref_tasks = layer_it->second;
  PixelRefTaskMap::iterator task_it =
      pixel_ref_tasks.find(pixel_ref->getGenerationID());

  if (task_it != pixel_ref_tasks.end())
    pixel_ref_tasks.erase(task_it);
}

void TileManager::OnRasterTaskCompleted(
    Tile::Id tile_id,
    scoped_ptr<ResourcePool::Resource> resource,
    RasterMode raster_mode,
    const PicturePileImpl::Analysis& analysis,
    bool was_canceled) {
  TileMap::iterator it = tiles_.find(tile_id);
  if (it == tiles_.end()) {
    ++update_visible_tiles_stats_.canceled_count;
    resource_pool_->ReleaseResource(resource.Pass());
    return;
  }

  Tile* tile = it->second;
  ManagedTileState& mts = tile->managed_state();
  ManagedTileState::TileVersion& tile_version =
      mts.tile_versions[raster_mode];
  DCHECK(!tile_version.raster_task_.is_null());
  tile_version.raster_task_.Reset();

  if (was_canceled) {
    ++update_visible_tiles_stats_.canceled_count;
    resource_pool_->ReleaseResource(resource.Pass());
    return;
  }

  ++update_visible_tiles_stats_.completed_count;

  tile_version.set_has_text(analysis.has_text);
  if (analysis.is_solid_color) {
    tile_version.set_solid_color(analysis.solid_color);
    resource_pool_->ReleaseResource(resource.Pass());
  } else {
    tile_version.set_use_resource();
    tile_version.resource_ = resource.Pass();
  }

  FreeUnusedResourcesForTile(tile);
  if (tile->priority(ACTIVE_TREE).distance_to_visible_in_pixels == 0)
    did_initialize_visible_tile_ = true;
}

}  // namespace cc
