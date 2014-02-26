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
#include "cc/debug/traced_value.h"
#include "cc/resources/direct_raster_worker_pool.h"
#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/raster_worker_pool_delegate.h"
#include "cc/resources/tile.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {
namespace {

// Memory limit policy works by mapping some bin states to the NEVER bin.
const ManagedTileBin kBinPolicyMap[NUM_TILE_MEMORY_LIMIT_POLICIES][NUM_BINS] = {
    // [ALLOW_NOTHING]
    {NEVER_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NEVER_BIN,  // [NOW_BIN]
     NEVER_BIN,  // [SOON_BIN]
     NEVER_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     NEVER_BIN,  // [EVENTUALLY_BIN]
     NEVER_BIN,  // [AT_LAST_AND_ACTIVE_BIN]
     NEVER_BIN,  // [AT_LAST_BIN]
     NEVER_BIN   // [NEVER_BIN]
    },
    // [ALLOW_ABSOLUTE_MINIMUM]
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     NEVER_BIN,                  // [SOON_BIN]
     NEVER_BIN,                  // [EVENTUALLY_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [EVENTUALLY_BIN]
     NEVER_BIN,                  // [AT_LAST_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // [ALLOW_PREPAINT_ONLY]
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     NEVER_BIN,                  // [EVENTUALLY_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [EVENTUALLY_BIN]
     NEVER_BIN,                  // [AT_LAST_AND_ACTIVE_BIN]
     NEVER_BIN,                  // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // [ALLOW_ANYTHING]
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    }};

// Ready to draw works by mapping NOW_BIN to NOW_AND_READY_TO_DRAW_BIN.
const ManagedTileBin kBinReadyToDrawMap[2][NUM_BINS] = {
    // Not ready
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // Ready
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_AND_READY_TO_DRAW_BIN,  // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    }};

// Active works by mapping some bin stats to equivalent _ACTIVE_BIN state.
const ManagedTileBin kBinIsActiveMap[2][NUM_BINS] = {
    // Inactive
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_BIN,             // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_BIN,                // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    },
    // Active
    {NOW_AND_READY_TO_DRAW_BIN,  // [NOW_AND_READY_TO_DRAW_BIN]
     NOW_BIN,                    // [NOW_BIN]
     SOON_BIN,                   // [SOON_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_AND_ACTIVE_BIN]
     EVENTUALLY_AND_ACTIVE_BIN,  // [EVENTUALLY_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_AND_ACTIVE_BIN]
     AT_LAST_AND_ACTIVE_BIN,     // [AT_LAST_BIN]
     NEVER_BIN                   // [NEVER_BIN]
    }};

// Determine bin based on three categories of tiles: things we need now,
// things we need soon, and eventually.
inline ManagedTileBin BinFromTilePriority(const TilePriority& prio) {
  const float kBackflingGuardDistancePixels = 314.0f;

  if (prio.priority_bin == TilePriority::NOW)
    return NOW_BIN;

  if (prio.priority_bin == TilePriority::SOON ||
      prio.distance_to_visible < kBackflingGuardDistancePixels)
    return SOON_BIN;

  if (prio.distance_to_visible == std::numeric_limits<float>::infinity())
    return NEVER_BIN;

  return EVENTUALLY_BIN;
}

}  // namespace

RasterTaskCompletionStats::RasterTaskCompletionStats()
    : completed_count(0u), canceled_count(0u) {}

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
    ContextProvider* context_provider,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    bool use_map_image,
    bool use_rasterize_on_demand,
    size_t max_transfer_buffer_usage_bytes,
    size_t max_raster_usage_bytes,
    unsigned map_image_texture_target) {
  return make_scoped_ptr(new TileManager(
      client,
      resource_provider,
      context_provider,
      use_map_image ? ImageRasterWorkerPool::Create(resource_provider,
                                                    map_image_texture_target)
                    : PixelBufferRasterWorkerPool::Create(
                          resource_provider, max_transfer_buffer_usage_bytes),
      DirectRasterWorkerPool::Create(resource_provider, context_provider),
      max_raster_usage_bytes,
      rendering_stats_instrumentation,
      use_rasterize_on_demand));
}

TileManager::TileManager(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    ContextProvider* context_provider,
    scoped_ptr<RasterWorkerPool> raster_worker_pool,
    scoped_ptr<RasterWorkerPool> direct_raster_worker_pool,
    size_t max_raster_usage_bytes,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    bool use_rasterize_on_demand)
    : client_(client),
      context_provider_(context_provider),
      resource_pool_(
          ResourcePool::Create(resource_provider,
                               raster_worker_pool->GetResourceTarget(),
                               raster_worker_pool->GetResourceFormat())),
      raster_worker_pool_(raster_worker_pool.Pass()),
      direct_raster_worker_pool_(direct_raster_worker_pool.Pass()),
      prioritized_tiles_dirty_(false),
      all_tiles_that_need_to_be_rasterized_have_memory_(true),
      all_tiles_required_for_activation_have_memory_(true),
      memory_required_bytes_(0),
      memory_nice_to_have_bytes_(0),
      bytes_releasable_(0),
      resources_releasable_(0),
      max_raster_usage_bytes_(max_raster_usage_bytes),
      ever_exceeded_memory_budget_(false),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      did_initialize_visible_tile_(false),
      did_check_for_completed_tasks_since_last_schedule_tasks_(true),
      use_rasterize_on_demand_(use_rasterize_on_demand) {
  RasterWorkerPool* raster_worker_pools[NUM_RASTER_WORKER_POOL_TYPES] = {
      raster_worker_pool_.get(),        // RASTER_WORKER_POOL_TYPE_DEFAULT
      direct_raster_worker_pool_.get()  // RASTER_WORKER_POOL_TYPE_DIRECT
  };
  raster_worker_pool_delegate_ = RasterWorkerPoolDelegate::Create(
      this, raster_worker_pools, arraysize(raster_worker_pools));
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();

  CleanUpReleasedTiles();
  DCHECK_EQ(0u, tiles_.size());

  RasterTaskQueue empty[NUM_RASTER_WORKER_POOL_TYPES];
  raster_worker_pool_delegate_->ScheduleTasks(empty);

  // This should finish all pending tasks and release any uninitialized
  // resources.
  raster_worker_pool_delegate_->Shutdown();
  raster_worker_pool_delegate_->CheckForCompletedTasks();

  DCHECK_EQ(0u, bytes_releasable_);
  DCHECK_EQ(0u, resources_releasable_);
}

void TileManager::Release(Tile* tile) {
  prioritized_tiles_dirty_ = true;
  released_tiles_.push_back(tile);
}

void TileManager::DidChangeTilePriority(Tile* tile) {
  prioritized_tiles_dirty_ = true;
}

bool TileManager::ShouldForceTasksRequiredForActivationToComplete() const {
  return global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY;
}

void TileManager::CleanUpReleasedTiles() {
  for (std::vector<Tile*>::iterator it = released_tiles_.begin();
       it != released_tiles_.end();
       ++it) {
    Tile* tile = *it;

    FreeResourcesForTile(tile);

    DCHECK(tiles_.find(tile->id()) != tiles_.end());
    tiles_.erase(tile->id());

    LayerCountMap::iterator layer_it =
        used_layer_counts_.find(tile->layer_id());
    DCHECK_GT(layer_it->second, 0);
    if (--layer_it->second == 0) {
      used_layer_counts_.erase(layer_it);
      image_decode_tasks_.erase(tile->layer_id());
    }

    delete tile;
  }

  released_tiles_.clear();
}

void TileManager::UpdatePrioritizedTileSetIfNeeded() {
  if (!prioritized_tiles_dirty_)
    return;

  CleanUpReleasedTiles();

  prioritized_tiles_.Clear();
  GetTilesWithAssignedBins(&prioritized_tiles_);
  prioritized_tiles_dirty_ = false;
}

void TileManager::DidFinishRunningTasks() {
  TRACE_EVENT0("cc", "TileManager::DidFinishRunningTasks");

  // When OOM, keep re-assigning memory until we reach a steady state
  // where top-priority tiles are initialized.
  if (all_tiles_that_need_to_be_rasterized_have_memory_)
    return;

  raster_worker_pool_delegate_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(&prioritized_tiles_,
                         &tiles_that_need_to_be_rasterized);

  // |tiles_that_need_to_be_rasterized| will be empty when we reach a
  // steady memory state. Keep scheduling tasks until we reach this state.
  if (!tiles_that_need_to_be_rasterized.empty()) {
    ScheduleTasks(tiles_that_need_to_be_rasterized);
    return;
  }

  // We don't reserve memory for required-for-activation tiles during
  // accelerated gestures, so we just postpone activation when we don't
  // have these tiles, and activate after the accelerated gesture.
  bool allow_rasterize_on_demand =
      global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY;

  // Use on-demand raster for any required-for-activation tiles that have not
  // been been assigned memory after reaching a steady memory state. This
  // ensures that we activate even when OOM.
  for (TileMap::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = it->second;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    if (tile->required_for_activation() && !tile_version.IsReadyToDraw()) {
      // If we can't raster on demand, give up early (and don't activate).
      if (!allow_rasterize_on_demand)
        return;
      if (use_rasterize_on_demand_)
        tile_version.set_rasterize_on_demand();
    }
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

void TileManager::GetTilesWithAssignedBins(PrioritizedTileSet* tiles) {
  TRACE_EVENT0("cc", "TileManager::GetTilesWithAssignedBins");

  // Compute new stats to be return by GetMemoryStats().
  memory_required_bytes_ = 0;
  memory_nice_to_have_bytes_ = 0;

  const TileMemoryLimitPolicy memory_policy = global_state_.memory_limit_policy;
  const TreePriority tree_priority = global_state_.tree_priority;

  // For each tree, bin into different categories of tiles.
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = it->second;
    ManagedTileState& mts = tile->managed_state();

    const ManagedTileState::TileVersion& tile_version =
        tile->GetTileVersionForDrawing();
    bool tile_is_ready_to_draw = tile_version.IsReadyToDraw();
    bool tile_is_active = tile_is_ready_to_draw ||
                          mts.tile_versions[mts.raster_mode].raster_task_;

    // Get the active priority and bin.
    TilePriority active_priority = tile->priority(ACTIVE_TREE);
    ManagedTileBin active_bin = BinFromTilePriority(active_priority);

    // Get the pending priority and bin.
    TilePriority pending_priority = tile->priority(PENDING_TREE);
    ManagedTileBin pending_bin = BinFromTilePriority(pending_priority);

    bool pending_is_low_res = pending_priority.resolution == LOW_RESOLUTION;
    bool pending_is_non_ideal =
        pending_priority.resolution == NON_IDEAL_RESOLUTION;
    bool active_is_non_ideal =
        active_priority.resolution == NON_IDEAL_RESOLUTION;

    // Adjust pending bin state for low res tiles. This prevents
    // pending tree low-res tiles from being initialized before
    // high-res tiles.
    if (pending_is_low_res)
      pending_bin = std::max(pending_bin, EVENTUALLY_BIN);

    // Adjust bin state based on if ready to draw.
    active_bin = kBinReadyToDrawMap[tile_is_ready_to_draw][active_bin];
    pending_bin = kBinReadyToDrawMap[tile_is_ready_to_draw][pending_bin];

    // Adjust bin state based on if active.
    active_bin = kBinIsActiveMap[tile_is_active][active_bin];
    pending_bin = kBinIsActiveMap[tile_is_active][pending_bin];

    // We never want to paint new non-ideal tiles, as we always have
    // a high-res tile covering that content (paint that instead).
    if (!tile_is_ready_to_draw && active_is_non_ideal)
      active_bin = NEVER_BIN;
    if (!tile_is_ready_to_draw && pending_is_non_ideal)
      pending_bin = NEVER_BIN;

    // Compute combined bin.
    ManagedTileBin combined_bin = std::min(active_bin, pending_bin);

    if (!tile_is_ready_to_draw || tile_version.requires_resource()) {
      // The bin that the tile would have if the GPU memory manager had
      // a maximally permissive policy, send to the GPU memory manager
      // to determine policy.
      ManagedTileBin gpu_memmgr_stats_bin = combined_bin;
      if ((gpu_memmgr_stats_bin == NOW_BIN) ||
          (gpu_memmgr_stats_bin == NOW_AND_READY_TO_DRAW_BIN))
        memory_required_bytes_ += BytesConsumedIfAllocated(tile);
      if (gpu_memmgr_stats_bin != NEVER_BIN)
        memory_nice_to_have_bytes_ += BytesConsumedIfAllocated(tile);
    }

    ManagedTileBin tree_bin[NUM_TREES];
    tree_bin[ACTIVE_TREE] = kBinPolicyMap[memory_policy][active_bin];
    tree_bin[PENDING_TREE] = kBinPolicyMap[memory_policy][pending_bin];

    TilePriority tile_priority;
    switch (tree_priority) {
      case SAME_PRIORITY_FOR_BOTH_TREES:
        mts.bin = kBinPolicyMap[memory_policy][combined_bin];
        tile_priority = tile->combined_priority();
        break;
      case SMOOTHNESS_TAKES_PRIORITY:
        mts.bin = tree_bin[ACTIVE_TREE];
        tile_priority = active_priority;
        break;
      case NEW_CONTENT_TAKES_PRIORITY:
        mts.bin = tree_bin[PENDING_TREE];
        tile_priority = pending_priority;
        break;
    }

    // Bump up the priority if we determined it's NEVER_BIN on one tree,
    // but is still required on the other tree.
    bool is_in_never_bin_on_both_trees = tree_bin[ACTIVE_TREE] == NEVER_BIN &&
                                         tree_bin[PENDING_TREE] == NEVER_BIN;

    if (mts.bin == NEVER_BIN && !is_in_never_bin_on_both_trees)
      mts.bin = tile_is_active ? AT_LAST_AND_ACTIVE_BIN : AT_LAST_BIN;

    mts.resolution = tile_priority.resolution;
    mts.priority_bin = tile_priority.priority_bin;
    mts.distance_to_visible = tile_priority.distance_to_visible;
    mts.required_for_activation = tile_priority.required_for_activation;

    mts.visible_and_ready_to_draw =
        tree_bin[ACTIVE_TREE] == NOW_AND_READY_TO_DRAW_BIN;

    if (mts.bin == NEVER_BIN) {
      FreeResourcesForTile(tile);
      continue;
    }

    // In almost all concievable cases (all in practice right now), we can't get
    // memory back from visible-active-tree tiles. Further, active-tree tiles
    // can still be used for animations. Further, ready-to-draw tiles won't
    // delay activation since they are already rastered. So we should keep
    // these tiles around in all cases.
    if (mts.visible_and_ready_to_draw)
      mts.bin = NOW_AND_READY_TO_DRAW_BIN;

    // Insert the tile into a priority set.
    tiles->InsertTile(tile, mts.bin);
  }
}

void TileManager::ManageTiles(const GlobalStateThatImpactsTilePriority& state) {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");

  // Update internal state.
  if (state != global_state_) {
    global_state_ = state;
    prioritized_tiles_dirty_ = true;
    // Soft limit is used for resource pool such that
    // memory returns to soft limit after going over.
    resource_pool_->SetResourceUsageLimits(
        global_state_.soft_memory_limit_in_bytes,
        global_state_.unused_memory_limit_in_bytes,
        global_state_.num_resources_limit);
  }

  // We need to call CheckForCompletedTasks() once in-between each call
  // to ScheduleTasks() to prevent canceled tasks from being scheduled.
  if (!did_check_for_completed_tasks_since_last_schedule_tasks_) {
    raster_worker_pool_delegate_->CheckForCompletedTasks();
    did_check_for_completed_tasks_since_last_schedule_tasks_ = true;
  }

  UpdatePrioritizedTileSetIfNeeded();

  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(&prioritized_tiles_,
                         &tiles_that_need_to_be_rasterized);

  // Finally, schedule rasterizer tasks.
  ScheduleTasks(tiles_that_need_to_be_rasterized);

  TRACE_EVENT_INSTANT1("cc",
                       "DidManage",
                       TRACE_EVENT_SCOPE_THREAD,
                       "state",
                       TracedValue::FromValue(BasicStateAsValue().release()));

  TRACE_COUNTER_ID1("cc",
                    "unused_memory_bytes",
                    this,
                    resource_pool_->total_memory_usage_bytes() -
                        resource_pool_->acquired_memory_usage_bytes());
}

bool TileManager::UpdateVisibleTiles() {
  TRACE_EVENT0("cc", "TileManager::UpdateVisibleTiles");

  raster_worker_pool_delegate_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TRACE_EVENT_INSTANT1(
      "cc",
      "DidUpdateVisibleTiles",
      TRACE_EVENT_SCOPE_THREAD,
      "stats",
      TracedValue::FromValue(RasterTaskCompletionStatsAsValue(
                                 update_visible_tiles_stats_).release()));
  update_visible_tiles_stats_ = RasterTaskCompletionStats();

  bool did_initialize_visible_tile = did_initialize_visible_tile_;
  did_initialize_visible_tile_ = false;
  return did_initialize_visible_tile;
}

void TileManager::GetMemoryStats(size_t* memory_required_bytes,
                                 size_t* memory_nice_to_have_bytes,
                                 size_t* memory_allocated_bytes,
                                 size_t* memory_used_bytes) const {
  *memory_required_bytes = memory_required_bytes_;
  *memory_nice_to_have_bytes = memory_nice_to_have_bytes_;
  *memory_allocated_bytes = resource_pool_->total_memory_usage_bytes();
  *memory_used_bytes = resource_pool_->acquired_memory_usage_bytes();
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
  for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end(); ++it)
    state->Append(it->second->AsValue().release());

  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileManager::GetMemoryRequirementsAsValue() const {
  scoped_ptr<base::DictionaryValue> requirements(new base::DictionaryValue());

  size_t memory_required_bytes;
  size_t memory_nice_to_have_bytes;
  size_t memory_allocated_bytes;
  size_t memory_used_bytes;
  GetMemoryStats(&memory_required_bytes,
                 &memory_nice_to_have_bytes,
                 &memory_allocated_bytes,
                 &memory_used_bytes);
  requirements->SetInteger("memory_required_bytes", memory_required_bytes);
  requirements->SetInteger("memory_nice_to_have_bytes",
                           memory_nice_to_have_bytes);
  requirements->SetInteger("memory_allocated_bytes", memory_allocated_bytes);
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
    PrioritizedTileSet* tiles,
    TileVector* tiles_that_need_to_be_rasterized) {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");

  // Maintain the list of released resources that can potentially be re-used
  // or deleted.
  // If this operation becomes expensive too, only do this after some
  // resource(s) was returned. Note that in that case, one also need to
  // invalidate when releasing some resource from the pool.
  resource_pool_->CheckBusyResources();

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  all_tiles_that_need_to_be_rasterized_have_memory_ = true;
  all_tiles_required_for_activation_have_memory_ = true;

  // Cast to prevent overflow.
  int64 soft_bytes_available =
      static_cast<int64>(bytes_releasable_) +
      static_cast<int64>(global_state_.soft_memory_limit_in_bytes) -
      static_cast<int64>(resource_pool_->acquired_memory_usage_bytes());
  int64 hard_bytes_available =
      static_cast<int64>(bytes_releasable_) +
      static_cast<int64>(global_state_.hard_memory_limit_in_bytes) -
      static_cast<int64>(resource_pool_->acquired_memory_usage_bytes());
  int resources_available = resources_releasable_ +
                            global_state_.num_resources_limit -
                            resource_pool_->acquired_resource_count();
  size_t soft_bytes_allocatable =
      std::max(static_cast<int64>(0), soft_bytes_available);
  size_t hard_bytes_allocatable =
      std::max(static_cast<int64>(0), hard_bytes_available);
  size_t resources_allocatable = std::max(0, resources_available);

  size_t bytes_that_exceeded_memory_budget = 0;
  size_t soft_bytes_left = soft_bytes_allocatable;
  size_t hard_bytes_left = hard_bytes_allocatable;

  size_t resources_left = resources_allocatable;
  bool oomed_soft = false;
  bool oomed_hard = false;
  bool have_hit_soft_memory = false;  // Soft memory comes after hard.

  // Memory we assign to raster tasks now will be deducted from our memory
  // in future iterations if priorities change. By assigning at most half
  // the raster limit, we will always have another 50% left even if priorities
  // change completely (assuming we check for completed/cancelled rasters
  // between each call to this function).
  size_t max_raster_bytes = max_raster_usage_bytes_ / 2;
  size_t raster_bytes = 0;

  unsigned schedule_priority = 1u;
  for (PrioritizedTileSet::Iterator it(tiles, true); it; ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();

    mts.scheduled_priority = schedule_priority++;

    mts.raster_mode = DetermineRasterMode(tile);

    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    // If this tile doesn't need a resource, then nothing to do.
    if (!tile_version.requires_resource())
      continue;

    // If the tile is not needed, free it up.
    if (mts.bin == NEVER_BIN) {
      FreeResourcesForTile(tile);
      continue;
    }

    const bool tile_uses_hard_limit = mts.bin <= NOW_BIN;
    const size_t bytes_if_allocated = BytesConsumedIfAllocated(tile);
    const size_t raster_bytes_if_rastered = raster_bytes + bytes_if_allocated;
    const size_t tile_bytes_left =
        (tile_uses_hard_limit) ? hard_bytes_left : soft_bytes_left;

    // Hard-limit is reserved for tiles that would cause a calamity
    // if they were to go away, so by definition they are the highest
    // priority memory, and must be at the front of the list.
    DCHECK(!(have_hit_soft_memory && tile_uses_hard_limit));
    have_hit_soft_memory |= !tile_uses_hard_limit;

    size_t tile_bytes = 0;
    size_t tile_resources = 0;

    // It costs to maintain a resource.
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (mts.tile_versions[mode].resource_) {
        tile_bytes += bytes_if_allocated;
        tile_resources++;
      }
    }

    // Allow lower priority tiles with initialized resources to keep
    // their memory by only assigning memory to new raster tasks if
    // they can be scheduled.
    if (raster_bytes_if_rastered <= max_raster_bytes) {
      // If we don't have the required version, and it's not in flight
      // then we'll have to pay to create a new task.
      if (!tile_version.resource_ && !tile_version.raster_task_) {
        tile_bytes += bytes_if_allocated;
        tile_resources++;
      }
    }

    // Tile is OOM.
    if (tile_bytes > tile_bytes_left || tile_resources > resources_left) {
      FreeResourcesForTile(tile);

      // This tile was already on screen and now its resources have been
      // released. In order to prevent checkerboarding, set this tile as
      // rasterize on demand immediately.
      if (mts.visible_and_ready_to_draw && use_rasterize_on_demand_)
        tile_version.set_rasterize_on_demand();

      oomed_soft = true;
      if (tile_uses_hard_limit) {
        oomed_hard = true;
        bytes_that_exceeded_memory_budget += tile_bytes;
      }
    } else {
      resources_left -= tile_resources;
      hard_bytes_left -= tile_bytes;
      soft_bytes_left =
          (soft_bytes_left > tile_bytes) ? soft_bytes_left - tile_bytes : 0;
      if (tile_version.resource_)
        continue;
    }

    DCHECK(!tile_version.resource_);

    // Tile shouldn't be rasterized if |tiles_that_need_to_be_rasterized|
    // has reached it's limit or we've failed to assign gpu memory to this
    // or any higher priority tile. Preventing tiles that fit into memory
    // budget to be rasterized when higher priority tile is oom is
    // important for two reasons:
    // 1. Tile size should not impact raster priority.
    // 2. Tiles with existing raster task could otherwise incorrectly
    //    be added as they are not affected by |bytes_allocatable|.
    if (oomed_soft || raster_bytes_if_rastered > max_raster_bytes) {
      all_tiles_that_need_to_be_rasterized_have_memory_ = false;
      if (tile->required_for_activation())
        all_tiles_required_for_activation_have_memory_ = false;
      it.DisablePriorityOrdering();
      continue;
    }

    raster_bytes = raster_bytes_if_rastered;
    tiles_that_need_to_be_rasterized->push_back(tile);
  }

  // OOM reporting uses hard-limit, soft-OOM is normal depending on limit.
  ever_exceeded_memory_budget_ |= oomed_hard;
  if (ever_exceeded_memory_budget_) {
    TRACE_COUNTER_ID2("cc",
                      "over_memory_budget",
                      this,
                      "budget",
                      global_state_.hard_memory_limit_in_bytes,
                      "over",
                      bytes_that_exceeded_memory_budget);
  }
  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.hard_memory_limit_in_bytes;
  memory_stats_from_last_assign_.bytes_allocated =
      hard_bytes_allocatable - hard_bytes_left;
  memory_stats_from_last_assign_.bytes_unreleasable =
      hard_bytes_allocatable - bytes_releasable_;
  memory_stats_from_last_assign_.bytes_over = bytes_that_exceeded_memory_budget;
}

void TileManager::FreeResourceForTile(Tile* tile, RasterMode mode) {
  ManagedTileState& mts = tile->managed_state();
  if (mts.tile_versions[mode].resource_) {
    resource_pool_->ReleaseResource(mts.tile_versions[mode].resource_.Pass());

    DCHECK_GE(bytes_releasable_, BytesConsumedIfAllocated(tile));
    DCHECK_GE(resources_releasable_, 1u);

    bytes_releasable_ -= BytesConsumedIfAllocated(tile);
    --resources_releasable_;
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
  TRACE_EVENT1("cc",
               "TileManager::ScheduleTasks",
               "count",
               tiles_that_need_to_be_rasterized.size());

  DCHECK(did_check_for_completed_tasks_since_last_schedule_tasks_);

  for (size_t i = 0; i < NUM_RASTER_WORKER_POOL_TYPES; ++i)
    raster_queue_[i].Reset();

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

    if (!tile_version.raster_task_)
      tile_version.raster_task_ = CreateRasterTask(tile);

    size_t pool_type = tile->use_gpu_rasterization()
                           ? RASTER_WORKER_POOL_TYPE_DIRECT
                           : RASTER_WORKER_POOL_TYPE_DEFAULT;

    raster_queue_[pool_type].items.push_back(RasterTaskQueue::Item(
        tile_version.raster_task_.get(), tile->required_for_activation()));
    raster_queue_[pool_type].required_for_activation_count +=
        tile->required_for_activation();
  }

  // We must reduce the amount of unused resoruces before calling
  // ScheduleTasks to prevent usage from rising above limits.
  resource_pool_->ReduceResourceUsage();

  // Schedule running of |raster_tasks_|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |raster_tasks_|.
  raster_worker_pool_delegate_->ScheduleTasks(raster_queue_);

  did_check_for_completed_tasks_since_last_schedule_tasks_ = false;
}

scoped_refptr<internal::WorkerPoolTask> TileManager::CreateImageDecodeTask(
    Tile* tile,
    SkPixelRef* pixel_ref) {
  return RasterWorkerPool::CreateImageDecodeTask(
      pixel_ref,
      tile->layer_id(),
      rendering_stats_instrumentation_,
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 tile->layer_id(),
                 base::Unretained(pixel_ref)));
}

scoped_refptr<internal::RasterWorkerPoolTask> TileManager::CreateRasterTask(
    Tile* tile) {
  ManagedTileState& mts = tile->managed_state();

  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(tile->tile_size_.size());
  const ScopedResource* const_resource = resource.get();

  // Create and queue all image decode tasks that this tile depends on.
  internal::WorkerPoolTask::Vector decode_tasks;
  PixelRefTaskMap& existing_pixel_refs = image_decode_tasks_[tile->layer_id()];
  for (PicturePileImpl::PixelRefIterator iter(
           tile->content_rect(), tile->contents_scale(), tile->picture_pile());
       iter;
       ++iter) {
    SkPixelRef* pixel_ref = *iter;
    uint32_t id = pixel_ref->getGenerationID();

    // Append existing image decode task if available.
    PixelRefTaskMap::iterator decode_task_it = existing_pixel_refs.find(id);
    if (decode_task_it != existing_pixel_refs.end()) {
      decode_tasks.push_back(decode_task_it->second);
      continue;
    }

    // Create and append new image decode task for this pixel ref.
    scoped_refptr<internal::WorkerPoolTask> decode_task =
        CreateImageDecodeTask(tile, pixel_ref);
    decode_tasks.push_back(decode_task);
    existing_pixel_refs[id] = decode_task;
  }

  return RasterWorkerPool::CreateRasterTask(
      const_resource,
      tile->picture_pile(),
      tile->content_rect(),
      tile->contents_scale(),
      mts.raster_mode,
      mts.resolution,
      tile->layer_id(),
      static_cast<const void*>(tile),
      tile->source_frame_number(),
      rendering_stats_instrumentation_,
      base::Bind(&TileManager::OnRasterTaskCompleted,
                 base::Unretained(this),
                 tile->id(),
                 base::Passed(&resource),
                 mts.raster_mode),
      &decode_tasks);
}

void TileManager::OnImageDecodeTaskCompleted(int layer_id,
                                             SkPixelRef* pixel_ref,
                                             bool was_canceled) {
  // If the task was canceled, we need to clean it up
  // from |image_decode_tasks_|.
  if (!was_canceled)
    return;

  LayerPixelRefTaskMap::iterator layer_it = image_decode_tasks_.find(layer_id);

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
    scoped_ptr<ScopedResource> resource,
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
  ManagedTileState::TileVersion& tile_version = mts.tile_versions[raster_mode];
  DCHECK(tile_version.raster_task_);
  tile_version.raster_task_ = NULL;

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

    bytes_releasable_ += BytesConsumedIfAllocated(tile);
    ++resources_releasable_;
  }

  FreeUnusedResourcesForTile(tile);
  if (tile->priority(ACTIVE_TREE).distance_to_visible == 0.f)
    did_initialize_visible_tile_ = true;
}

scoped_refptr<Tile> TileManager::CreateTile(PicturePileImpl* picture_pile,
                                            const gfx::Size& tile_size,
                                            const gfx::Rect& content_rect,
                                            const gfx::Rect& opaque_rect,
                                            float contents_scale,
                                            int layer_id,
                                            int source_frame_number,
                                            int flags) {
  scoped_refptr<Tile> tile = make_scoped_refptr(new Tile(this,
                                                         picture_pile,
                                                         tile_size,
                                                         content_rect,
                                                         opaque_rect,
                                                         contents_scale,
                                                         layer_id,
                                                         source_frame_number,
                                                         flags));
  DCHECK(tiles_.find(tile->id()) == tiles_.end());

  tiles_[tile->id()] = tile;
  used_layer_counts_[tile->layer_id()]++;
  prioritized_tiles_dirty_ = true;
  return tile;
}

}  // namespace cc
