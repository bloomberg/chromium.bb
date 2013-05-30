// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_manager.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

namespace {

// If we raster too fast we become upload bound, and pending
// uploads consume memory. For maximum upload throughput, we would
// want to allow for upload_throughput * pipeline_time of pending
// uploads, after which we are just wasting memory. Since we don't
// know our upload throughput yet, this just caps our memory usage.
#if defined(OS_ANDROID)
// For reference, the Nexus10 can upload 1MB in about 2.5ms.
// Assuming a three frame deep pipeline this implies ~20MB.
const size_t kMaxPendingUploadBytes = 20 * 1024 * 1024;
// TODO(epenner): We should remove this upload limit (crbug.com/176197)
const size_t kMaxPendingUploads = 72;
#else
const size_t kMaxPendingUploadBytes = 100 * 1024 * 1024;
const size_t kMaxPendingUploads = 1000;
#endif

#if defined(OS_ANDROID)
const int kMaxNumPendingTasksPerThread = 8;
#else
const int kMaxNumPendingTasksPerThread = 40;
#endif

// Determine bin based on three categories of tiles: things we need now,
// things we need soon, and eventually.
inline TileManagerBin BinFromTilePriority(const TilePriority& prio) {
  // The amount of time for which we want to have prepainting coverage.
  const float kPrepaintingWindowTimeSeconds = 1.0f;
  const float kBackflingGuardDistancePixels = 314.0f;

  if (prio.time_to_visible_in_seconds == 0)
    return NOW_BIN;

  if (prio.resolution == NON_IDEAL_RESOLUTION)
    return EVENTUALLY_BIN;

  if (prio.distance_to_visible_in_pixels < kBackflingGuardDistancePixels ||
      prio.time_to_visible_in_seconds < kPrepaintingWindowTimeSeconds)
    return SOON_BIN;

  return EVENTUALLY_BIN;
}

}  // namespace

scoped_ptr<base::Value> TileManagerBinAsValue(TileManagerBin bin) {
  switch (bin) {
  case NOW_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NOW_BIN"));
  case SOON_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "SOON_BIN"));
  case EVENTUALLY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "EVENTUALLY_BIN"));
  case NEVER_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NEVER_BIN"));
  default:
      DCHECK(false) << "Unrecognized TileManagerBin value " << bin;
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown TileManagerBin value>"));
  }
}

scoped_ptr<base::Value> TileManagerBinPriorityAsValue(
    TileManagerBinPriority bin_priority) {
  switch (bin_priority) {
  case HIGH_PRIORITY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "HIGH_PRIORITY_BIN"));
  case LOW_PRIORITY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "LOW_PRIORITY_BIN"));
  default:
      DCHECK(false) << "Unrecognized TileManagerBinPriority value";
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown TileManagerBinPriority value>"));
  }
}

// static
scoped_ptr<TileManager> TileManager::Create(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    size_t num_raster_threads,
    bool use_color_estimator,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    bool use_map_image) {
  scoped_ptr<RasterWorkerPool> raster_worker_pool =
      RasterWorkerPool::Create(num_raster_threads);
  return make_scoped_ptr(new TileManager(client,
                                         resource_provider,
                                         raster_worker_pool.Pass(),
                                         num_raster_threads,
                                         use_color_estimator,
                                         rendering_stats_instrumentation,
                                         use_map_image));
}

TileManager::TileManager(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    scoped_ptr<RasterWorkerPool> raster_worker_pool,
    size_t num_raster_threads,
    bool use_color_estimator,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    bool use_map_image)
    : client_(client),
      resource_pool_(ResourcePool::Create(resource_provider)),
      raster_worker_pool_(raster_worker_pool.Pass()),
      manage_tiles_pending_(false),
      bytes_pending_upload_(0),
      has_performed_uploads_since_last_flush_(false),
      ever_exceeded_memory_budget_(false),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      use_color_estimator_(use_color_estimator),
      did_initialize_visible_tile_(false),
      max_pending_tasks_(kMaxNumPendingTasksPerThread * num_raster_threads) {
  raster_worker_pool_->SetClient(this);
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();
  AssignGpuMemoryToTiles();
  // This should finish all pending tasks and release any uninitialized
  // resources.
  raster_worker_pool_->Shutdown();
  AbortPendingTileUploads();
  DCHECK_EQ(0u, tiles_with_pending_upload_.size());
  DCHECK_EQ(0u, tiles_.size());
}

void TileManager::SetGlobalState(
    const GlobalStateThatImpactsTilePriority& global_state) {
  global_state_ = global_state;
  resource_pool_->SetMaxMemoryUsageBytes(
      global_state_.memory_limit_in_bytes,
      global_state_.unused_memory_limit_in_bytes);
  ScheduleManageTiles();
}

void TileManager::RegisterTile(Tile* tile) {
  DCHECK(std::find(tiles_.begin(), tiles_.end(), tile) == tiles_.end());
  DCHECK(!tile->required_for_activation());
  tiles_.push_back(tile);
  ScheduleManageTiles();
}

void TileManager::UnregisterTile(Tile* tile) {
  TileVector::iterator raster_iter =
      std::find(tiles_that_need_to_be_rasterized_.begin(),
                tiles_that_need_to_be_rasterized_.end(),
                tile);
  if (raster_iter != tiles_that_need_to_be_rasterized_.end())
    tiles_that_need_to_be_rasterized_.erase(raster_iter);

  tiles_that_need_to_be_initialized_for_activation_.erase(tile);

  DCHECK(std::find(tiles_.begin(), tiles_.end(), tile) != tiles_.end());
  FreeResourcesForTile(tile);
  tiles_.erase(std::remove(tiles_.begin(), tiles_.end(), tile));
}

class BinComparator {
 public:
  bool operator() (const Tile* a, const Tile* b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();
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

void TileManager::AssignBinsToTiles() {
  const TreePriority tree_priority = global_state_.tree_priority;

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
    bin_map[EVENTUALLY_BIN] = EVENTUALLY_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  }

  // For each tree, bin into different categories of tiles.
  for (TileVector::iterator it = tiles_.begin();
       it != tiles_.end();
       ++it) {
    Tile* tile = *it;
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
    mts.bin[HIGH_PRIORITY_BIN] = BinFromTilePriority(prio[HIGH_PRIORITY_BIN]);
    mts.bin[LOW_PRIORITY_BIN] = BinFromTilePriority(prio[LOW_PRIORITY_BIN]);
    mts.gpu_memmgr_stats_bin = BinFromTilePriority(tile->combined_priority());

    DidTileTreeBinChange(tile,
        bin_map[BinFromTilePriority(tile->priority(ACTIVE_TREE))],
        ACTIVE_TREE);
    DidTileTreeBinChange(tile,
        bin_map[BinFromTilePriority(tile->priority(PENDING_TREE))],
        PENDING_TREE);

    for (int i = 0; i < NUM_BIN_PRIORITIES; ++i)
      mts.bin[i] = bin_map[mts.bin[i]];
  }
}

void TileManager::SortTiles() {
  TRACE_EVENT0("cc", "TileManager::SortTiles");

  // Sort by bin, resolution and time until needed.
  std::sort(tiles_.begin(), tiles_.end(), BinComparator());
}

void TileManager::ManageTiles() {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");

  manage_tiles_pending_ = false;

  AssignBinsToTiles();
  SortTiles();
  AssignGpuMemoryToTiles();

  TRACE_EVENT_INSTANT1(
      "cc", "DidManage", TRACE_EVENT_SCOPE_THREAD,
      "state", TracedValue::FromValue(BasicStateAsValue().release()));

  // Finally, schedule rasterizer tasks.
  ScheduleTasks();
}

void TileManager::CheckForCompletedTileUploads() {
  while (!tiles_with_pending_upload_.empty()) {
    Tile* tile = tiles_with_pending_upload_.front();
    DCHECK(tile->tile_version().resource_);

    // Set pixel tasks complete in the order they are posted.
    if (!resource_pool_->resource_provider()->DidSetPixelsComplete(
          tile->tile_version().resource_->id())) {
      break;
    }

    // It's now safe to release the pixel buffer.
    resource_pool_->resource_provider()->ReleasePixelBuffer(
        tile->tile_version().resource_->id());

    bytes_pending_upload_ -= tile->bytes_consumed_if_allocated();
    bool was_forced = tile->tile_version().forced_upload_;
    // Reset forced_upload_ since we now got the upload completed notification.
    tile->tile_version().forced_upload_ = false;
    tile->tile_version().memory_state_ = USING_RELEASABLE_MEMORY;
    if (!was_forced)
      DidFinishTileInitialization(tile);

    tiles_with_pending_upload_.pop();
  }

  ScheduleTasks();
}

void TileManager::AbortPendingTileUploads() {
  while (!tiles_with_pending_upload_.empty()) {
    Tile* tile = tiles_with_pending_upload_.front();
    DCHECK(tile->tile_version().resource_);

    resource_pool_->resource_provider()->AbortSetPixels(
        tile->tile_version().resource_->id());
    resource_pool_->resource_provider()->ReleasePixelBuffer(
        tile->tile_version().resource_->id());
    tile->tile_version().memory_state_ = USING_RELEASABLE_MEMORY;

    FreeResourcesForTile(tile);

    bytes_pending_upload_ -= tile->bytes_consumed_if_allocated();
    tiles_with_pending_upload_.pop();
  }
}

void TileManager::ForceTileUploadToComplete(Tile* tile) {
  DCHECK(tile);
  if (tile->tile_version().resource_ &&
      tile->tile_version().memory_state_ == USING_UNRELEASABLE_MEMORY &&
      !tile->tile_version().forced_upload_) {
    resource_pool_->resource_provider()->
        ForceSetPixelsToComplete(tile->tile_version().resource_->id());

    // We have to set the memory state to be unreleasable, to ensure
    // that the tile will not be freed until we get the upload finished
    // notification. However, setting |forced_upload_| to true makes
    // this tile ready to draw.
    tile->tile_version().memory_state_ = USING_UNRELEASABLE_MEMORY;
    tile->tile_version().forced_upload_ = true;
    DidFinishTileInitialization(tile);
    DCHECK(tile->tile_version().IsReadyToDraw());
  }
}

void TileManager::GetMemoryStats(
    size_t* memory_required_bytes,
    size_t* memory_nice_to_have_bytes,
    size_t* memory_used_bytes) const {
  *memory_required_bytes = 0;
  *memory_nice_to_have_bytes = 0;
  *memory_used_bytes = 0;
  for (TileVector::const_iterator it = tiles_.begin();
       it != tiles_.end();
       ++it) {
    const Tile* tile = *it;
    if (!tile->tile_version().requires_resource())
      continue;

    const ManagedTileState& mts = tile->managed_state();
    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    if (mts.gpu_memmgr_stats_bin == NOW_BIN)
      *memory_required_bytes += tile_bytes;
    if (mts.gpu_memmgr_stats_bin != NEVER_BIN)
      *memory_nice_to_have_bytes += tile_bytes;
    if (tile->tile_version().memory_state_ != NOT_ALLOWED_TO_USE_MEMORY)
      *memory_used_bytes += tile_bytes;
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
  for (TileVector::const_iterator it = tiles_.begin();
       it != tiles_.end();
       it++) {
    state->Append((*it)->AsValue().release());
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

void TileManager::AddRequiredTileForActivation(Tile* tile) {
  DCHECK(std::find(tiles_that_need_to_be_initialized_for_activation_.begin(),
                   tiles_that_need_to_be_initialized_for_activation_.end(),
                   tile) ==
         tiles_that_need_to_be_initialized_for_activation_.end());
  tiles_that_need_to_be_initialized_for_activation_.insert(tile);
}

void TileManager::DidFinishDispatchingWorkerPoolCompletionCallbacks() {
  // If a flush is needed, do it now before starting to dispatch more tasks.
  if (has_performed_uploads_since_last_flush_) {
    resource_pool_->resource_provider()->ShallowFlushIfSupported();
    has_performed_uploads_since_last_flush_ = false;
  }

  ScheduleTasks();
}

void TileManager::AssignGpuMemoryToTiles() {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");
  size_t unreleasable_bytes = 0;

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  tiles_that_need_to_be_rasterized_.clear();
  tiles_that_need_to_be_initialized_for_activation_.clear();

  // By clearing the tiles_that_need_to_be_rasterized_ vector list
  // above we move all tiles currently waiting for raster to idle state.
  // Some memory cannot be released. We figure out how much in this
  // loop.
  for (TileVector::const_iterator it = tiles_.begin();
       it != tiles_.end();
       ++it) {
    const Tile* tile = *it;
    if (tile->tile_version().memory_state_ == USING_UNRELEASABLE_MEMORY)
      unreleasable_bytes += tile->bytes_consumed_if_allocated();
  }

  // Global state's memory limit can decrease, causing
  // it to be less than unreleasable_bytes
  size_t bytes_allocatable =
      global_state_.memory_limit_in_bytes > unreleasable_bytes ?
      global_state_.memory_limit_in_bytes - unreleasable_bytes :
      0;
  size_t bytes_that_exceeded_memory_budget_in_now_bin = 0;
  size_t bytes_left = bytes_allocatable;
  size_t bytes_oom_in_now_bin_on_pending_tree = 0;
  TileVector tiles_requiring_memory_but_oomed;
  bool higher_priority_tile_oomed = false;
  for (TileVector::iterator it = tiles_.begin();
       it != tiles_.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::TileVersion& tile_version = tile->tile_version();

    // If this tile doesn't need a resource, then nothing to do.
    if (!tile_version.requires_resource())
      continue;

    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    // Memory is already reserved for tile with unreleasable memory
    // so adding it to |tiles_that_need_to_be_rasterized_| doesn't
    // affect bytes_allocatable.
    if (tile_version.memory_state_ == USING_UNRELEASABLE_MEMORY)
      tile_bytes = 0;

    // If the tile is not needed, free it up.
    if (mts.is_in_never_bin_on_both_trees()) {
      if (tile_version.memory_state_ != USING_UNRELEASABLE_MEMORY) {
        FreeResourcesForTile(tile);
        tile_version.memory_state_ = NOT_ALLOWED_TO_USE_MEMORY;
      }
      continue;
    }

    // Tile is OOM.
    if (tile_bytes > bytes_left) {
      tile->tile_version().set_rasterize_on_demand();
      if (mts.tree_bin[PENDING_TREE] == NOW_BIN) {
        tiles_requiring_memory_but_oomed.push_back(tile);
        bytes_oom_in_now_bin_on_pending_tree += tile_bytes;
      }
      FreeResourcesForTile(tile);
      higher_priority_tile_oomed = true;
      continue;
    }

    tile_version.set_use_resource();
    bytes_left -= tile_bytes;

    // Tile shouldn't be rasterized if we've failed to assign
    // gpu memory to a higher priority tile. This is important for
    // two reasons:
    // 1. Tile size should not impact raster priority.
    // 2. Tile with unreleasable memory could otherwise incorrectly
    //    be added as it's not affected by |bytes_allocatable|.
    if (higher_priority_tile_oomed)
      continue;

    if (!tile_version.resource_)
      tiles_that_need_to_be_rasterized_.push_back(tile);

    if (!tile_version.resource_ && tile->required_for_activation())
      AddRequiredTileForActivation(tile);

    if (tile_version.memory_state_ == USING_UNRELEASABLE_MEMORY &&
        tile->required_for_activation()) {
      // If after rasterizing, this tile has become required or the client has
      // changed its mind about forcing tiles, do that now.
      if (!tile->tile_version().forced_upload_ &&
          client_->ShouldForceTileUploadsRequiredForActivationToComplete()) {
        ForceTileUploadToComplete(tile);
      }
    }
  }

  // In OOM situation, we iterate tiles_, remove the memory for active tree
  // and not the now bin. And give them to bytes_oom_in_now_bin_on_pending_tree
  if (!tiles_requiring_memory_but_oomed.empty()) {
    size_t bytes_freed = 0;
    for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
      Tile* tile = *it;
      ManagedTileState& mts = tile->managed_state();
      ManagedTileState::TileVersion& tile_version = tile->tile_version();
      if ((tile_version.memory_state_ == CAN_USE_MEMORY ||
           tile_version.memory_state_ == USING_RELEASABLE_MEMORY) &&
          mts.tree_bin[PENDING_TREE] == NEVER_BIN &&
          mts.tree_bin[ACTIVE_TREE] != NOW_BIN) {
        DCHECK(!tile->required_for_activation());
        FreeResourcesForTile(tile);
        tile_version.set_rasterize_on_demand();
        bytes_freed += tile->bytes_consumed_if_allocated();
        TileVector::iterator it = std::find(
                tiles_that_need_to_be_rasterized_.begin(),
                tiles_that_need_to_be_rasterized_.end(),
                tile);
        if (it != tiles_that_need_to_be_rasterized_.end())
            tiles_that_need_to_be_rasterized_.erase(it);
        if (bytes_oom_in_now_bin_on_pending_tree <= bytes_freed)
          break;
      }
    }

    for (TileVector::iterator it = tiles_requiring_memory_but_oomed.begin();
         it != tiles_requiring_memory_but_oomed.end() && bytes_freed > 0;
         ++it) {
      Tile* tile = *it;
      size_t bytes_needed = tile->bytes_consumed_if_allocated();
      if (bytes_needed > bytes_freed)
        continue;
      tile->tile_version().set_use_resource();
      bytes_freed -= bytes_needed;
      tiles_that_need_to_be_rasterized_.push_back(tile);
      if (tile->required_for_activation())
        AddRequiredTileForActivation(tile);
    }
  }

  ever_exceeded_memory_budget_ |=
      bytes_that_exceeded_memory_budget_in_now_bin > 0;
  if (ever_exceeded_memory_budget_) {
      TRACE_COUNTER_ID2("cc", "over_memory_budget", this,
                        "budget", global_state_.memory_limit_in_bytes,
                        "over", bytes_that_exceeded_memory_budget_in_now_bin);
  }
  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.memory_limit_in_bytes;
  memory_stats_from_last_assign_.bytes_allocated =
      bytes_allocatable - bytes_left;
  memory_stats_from_last_assign_.bytes_unreleasable = unreleasable_bytes;
  memory_stats_from_last_assign_.bytes_over =
      bytes_that_exceeded_memory_budget_in_now_bin;
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  DCHECK_NE(USING_UNRELEASABLE_MEMORY, tile->tile_version().memory_state_);
  if (tile->tile_version().resource_) {
    resource_pool_->ReleaseResource(
        tile->tile_version().resource_.Pass());
  }
  tile->tile_version().memory_state_ = NOT_ALLOWED_TO_USE_MEMORY;
}

void TileManager::ScheduleTasks() {
  TRACE_EVENT0("cc", "TileManager::ScheduleTasks");
  RasterWorkerPool::Task::Queue tasks;

  size_t bytes_pending_upload = bytes_pending_upload_;
  unsigned pending_tasks = 0;

  // Build a new task queue containing all task currently needed. Tasks
  // are added in order of priority, highest priority task first.
  for (TileVector::iterator it = tiles_that_need_to_be_rasterized_.begin();
       it != tiles_that_need_to_be_rasterized_.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();

    // Skip tile if determined to not require resource.
    if (!tile->tile_version().requires_resource())
      continue;

    // Skip tile if already rasterized.
    if (tile->tile_version().resource_)
      continue;

    // TODO(reveman): Remove throttling based on max pending tasks.
    if (pending_tasks >= max_pending_tasks_)
      break;

    // TODO(reveman): Remove throttling based on max pending uploads.
    if (tiles_with_pending_upload_.size() >= kMaxPendingUploads)
      break;

    // TODO(reveman): Throttle based on shared memory usage rather
    // than bytes pending upload.
    size_t new_bytes_pending = bytes_pending_upload;
    new_bytes_pending += tile->bytes_consumed_if_allocated();
    if (new_bytes_pending > kMaxPendingUploadBytes)
      break;
    bytes_pending_upload = new_bytes_pending;

    // Create raster task for this tile if necessary.
    if (mts.raster_task.is_null())
      mts.raster_task = CreateRasterTask(tile);

    // Finally append raster task.
    tasks.Append(mts.raster_task);
    pending_tasks++;
  }

  if (!tasks.empty()) {
    RasterWorkerPool::Task root(&tasks);

    // Schedule running of |tasks|. This replaces any previously
    // scheduled tasks and effectively cancels all tasks not present
    // in |tasks|.
    raster_worker_pool_->ScheduleTasks(&root);
  } else {
    raster_worker_pool_->ScheduleTasks(NULL);
  }

  if (did_initialize_visible_tile_) {
    did_initialize_visible_tile_ = false;
    client_->DidInitializeVisibleTile();
  }
}

RasterWorkerPool::Task TileManager::CreateImageDecodeTask(
    Tile* tile, skia::LazyPixelRef* pixel_ref) {
  TRACE_EVENT0("cc", "TileManager::CreateImageDecodeTask");

  return RasterWorkerPool::Task(
      base::Bind(&TileManager::RunImageDecodeTask,
                 pixel_ref,
                 tile->layer_id(),
                 rendering_stats_instrumentation_),
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 make_scoped_refptr(tile),
                 pixel_ref->getGenerationID()));
}

void TileManager::OnImageDecodeTaskCompleted(scoped_refptr<Tile> tile,
                                             uint32_t pixel_ref_id,
                                             bool was_canceled) {
  TRACE_EVENT0("cc", "TileManager::OnImageDecodeTaskCompleted");
  DCHECK(pending_decode_tasks_.find(pixel_ref_id) !=
         pending_decode_tasks_.end());
  pending_decode_tasks_.erase(pixel_ref_id);
}

TileManager::RasterTaskMetadata TileManager::GetRasterTaskMetadata(
    const Tile& tile) const {
  RasterTaskMetadata metadata;
  const ManagedTileState& mts = tile.managed_state();
  metadata.is_tile_in_pending_tree_now_bin =
      mts.tree_bin[PENDING_TREE] == NOW_BIN;
  metadata.tile_resolution = mts.resolution;
  metadata.layer_id = tile.layer_id();
  metadata.tile_id = &tile;
  metadata.source_frame_number = tile.source_frame_number();
  return metadata;
}

RasterWorkerPool::Task TileManager::CreateRasterTask(Tile* tile) {
  TRACE_EVENT0("cc", "TileManager::CreateRasterTask");

  scoped_ptr<ResourcePool::Resource> resource =
      resource_pool_->AcquireResource(
          tile->tile_size_.size(),
          tile->tile_version().resource_format_);
  resource_pool_->resource_provider()->AcquirePixelBuffer(resource->id());

  DCHECK_EQ(CAN_USE_MEMORY, tile->tile_version().memory_state_);
  tile->tile_version().memory_state_ = USING_UNRELEASABLE_MEMORY;

  PicturePileImpl::Analysis* analysis = new PicturePileImpl::Analysis;

  // MapPixelBuffer() returns NULL if context was lost at the time
  // AcquirePixelBuffer() was called. For simplicity we still create
  // a raster task that is essentially a noop in these situations.
  uint8* buffer = resource_pool_->resource_provider()->MapPixelBuffer(
      resource->id());

  // Create and queue all image decode tasks that this tile depends on.
  RasterWorkerPool::Task::Queue decode_tasks;
  for (PicturePileImpl::PixelRefIterator iter(tile->content_rect(),
                                              tile->contents_scale(),
                                              tile->picture_pile());
       iter; ++iter) {
    skia::LazyPixelRef* pixel_ref = *iter;
    uint32_t id = pixel_ref->getGenerationID();

    // Append existing image decode task if available.
    PixelRefMap::iterator decode_task_it = pending_decode_tasks_.find(id);
    if (decode_task_it != pending_decode_tasks_.end()) {
      decode_tasks.Append(decode_task_it->second);
      continue;
    }

    // TODO(qinmin): passing correct image size to PrepareToDecode().
    if (pixel_ref->PrepareToDecode(skia::LazyPixelRef::PrepareParams())) {
      rendering_stats_instrumentation_->IncrementDeferredImageCacheHitCount();
      continue;
    }

    // Create and append new image decode task for this pixel ref.
    RasterWorkerPool::Task decode_task = CreateImageDecodeTask(
        tile, pixel_ref);
    decode_tasks.Append(decode_task);
    pending_decode_tasks_[id] = decode_task;
  }

  return RasterWorkerPool::PictureTask(
      tile->picture_pile(),
      base::Bind(&TileManager::RunAnalyzeAndRasterTask,
                 base::Bind(&TileManager::RunAnalyzeTask,
                            analysis,
                            tile->content_rect(),
                            tile->contents_scale(),
                            use_color_estimator_,
                            GetRasterTaskMetadata(*tile),
                            rendering_stats_instrumentation_),
                 base::Bind(&TileManager::RunRasterTask,
                            buffer,
                            analysis,
                            tile->content_rect(),
                            tile->contents_scale(),
                            GetRasterTaskMetadata(*tile),
                            rendering_stats_instrumentation_)),
      base::Bind(&TileManager::OnRasterTaskCompleted,
                 base::Unretained(this),
                 make_scoped_refptr(tile),
                 base::Passed(&resource),
                 base::Owned(analysis)),
      &decode_tasks);
}

void TileManager::OnRasterTaskCompleted(
    scoped_refptr<Tile> tile,
    scoped_ptr<ResourcePool::Resource> resource,
    PicturePileImpl::Analysis* analysis,
    bool was_canceled) {
  TRACE_EVENT0("cc", "TileManager::OnRasterTaskCompleted");

  ManagedTileState& mts = tile->managed_state();
  DCHECK(!mts.raster_task.is_null());
  mts.raster_task.Reset();

  // Tile resources can't be freed until upload has completed.
  DCHECK_EQ(USING_UNRELEASABLE_MEMORY, tile->tile_version().memory_state_);

  // Release raster resources.
  resource_pool_->resource_provider()->UnmapPixelBuffer(resource->id());

  if (was_canceled) {
    tile->tile_version().memory_state_ = CAN_USE_MEMORY;
    resource_pool_->resource_provider()->ReleasePixelBuffer(resource->id());
    resource_pool_->ReleaseResource(resource.Pass());
    return;
  }

  mts.picture_pile_analysis = *analysis;
  mts.picture_pile_analyzed = true;

  if (analysis->is_solid_color) {
    tile->tile_version().set_solid_color(analysis->solid_color);
    resource_pool_->resource_provider()->ReleasePixelBuffer(resource->id());
    resource_pool_->ReleaseResource(resource.Pass());
    DidFinishTileInitialization(tile);
    return;
  }

  resource_pool_->resource_provider()->BeginSetPixels(resource->id());
  has_performed_uploads_since_last_flush_ = true;

  tile->tile_version().resource_ = resource.Pass();

  bytes_pending_upload_ += tile->bytes_consumed_if_allocated();
  tiles_with_pending_upload_.push(tile);

  if (tile->required_for_activation() &&
      client_->ShouldForceTileUploadsRequiredForActivationToComplete())
    ForceTileUploadToComplete(tile);
}

void TileManager::DidFinishTileInitialization(Tile* tile) {
  if (tile->priority(ACTIVE_TREE).distance_to_visible_in_pixels == 0)
    did_initialize_visible_tile_ = true;
  if (tile->required_for_activation()) {
    // It's possible that a tile required for activation is not in this list
    // if it was marked as being required after being dispatched for
    // rasterization but before AssignGPUMemory was called again.
    tiles_that_need_to_be_initialized_for_activation_.erase(tile);
  }
}

void TileManager::DidTileTreeBinChange(Tile* tile,
                                       TileManagerBin new_tree_bin,
                                       WhichTree tree) {
  ManagedTileState& mts = tile->managed_state();
  mts.tree_bin[tree] = new_tree_bin;
}

// static
void TileManager::RunImageDecodeTask(
    skia::LazyPixelRef* pixel_ref,
    int layer_id,
    RenderingStatsInstrumentation* stats_instrumentation) {
  TRACE_EVENT0("cc", "TileManager::RunImageDecodeTask");
  devtools_instrumentation::ScopedLayerTask image_decode_task(
      devtools_instrumentation::kImageDecodeTask, layer_id);
  base::TimeTicks start_time = stats_instrumentation->StartRecording();
  pixel_ref->Decode();
  base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
  stats_instrumentation->AddDeferredImageDecode(duration);
}

// static
void TileManager::RunAnalyzeAndRasterTask(
    const RasterWorkerPool::PictureTask::Callback& analyze_task,
    const RasterWorkerPool::PictureTask::Callback& raster_task,
    PicturePileImpl* picture_pile) {
  analyze_task.Run(picture_pile);
  raster_task.Run(picture_pile);
}

// static
void TileManager::RunAnalyzeTask(
    PicturePileImpl::Analysis* analysis,
    gfx::Rect rect,
    float contents_scale,
    bool use_color_estimator,
    const RasterTaskMetadata& metadata,
    RenderingStatsInstrumentation* stats_instrumentation,
    PicturePileImpl* picture_pile) {
  TRACE_EVENT0("cc", "TileManager::RunAnalyzeTask");

  DCHECK(picture_pile);
  DCHECK(analysis);
  DCHECK(stats_instrumentation);

  base::TimeTicks start_time = stats_instrumentation->StartRecording();
  picture_pile->AnalyzeInRect(rect, contents_scale, analysis);
  base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);

  // Record the solid color prediction.
  UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                        analysis->is_solid_color);
  stats_instrumentation->AddTileAnalysisResult(duration,
                                               analysis->is_solid_color);

  // Clear the flag if we're not using the estimator.
  analysis->is_solid_color &= use_color_estimator;
}

scoped_ptr<base::Value> TileManager::RasterTaskMetadata::AsValue() const {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->Set("tile_id", TracedValue::CreateIDRef(tile_id).release());
  res->SetBoolean("is_tile_in_pending_tree_now_bin",
                  is_tile_in_pending_tree_now_bin);
  res->Set("resolution", TileResolutionAsValue(tile_resolution).release());
  res->SetInteger("source_frame_number", source_frame_number);
  return res.PassAs<base::Value>();
}

// static
void TileManager::RunRasterTask(
    uint8* buffer,
    PicturePileImpl::Analysis* analysis,
    gfx::Rect rect,
    float contents_scale,
    const RasterTaskMetadata& metadata,
    RenderingStatsInstrumentation* stats_instrumentation,
    PicturePileImpl* picture_pile) {
  TRACE_EVENT1(
      "cc", "TileManager::RunRasterTask",
      "metadata", TracedValue::FromValue(metadata.AsValue().release()));
  devtools_instrumentation::ScopedLayerTask raster_task(
      devtools_instrumentation::kRasterTask, metadata.layer_id);

  DCHECK(picture_pile);
  DCHECK(analysis);

  // |buffer| can be NULL in lost context situations.
  if (!buffer)
    return;

  if (analysis->is_solid_color)
    return;

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
  bitmap.setPixels(buffer);
  SkDevice device(bitmap);
  SkCanvas canvas(&device);

  if (stats_instrumentation->record_rendering_stats()) {
    PicturePileImpl::RasterStats raster_stats;
    picture_pile->RasterToBitmap(&canvas, rect, contents_scale, &raster_stats);
    stats_instrumentation->AddRaster(
        raster_stats.total_rasterize_time,
        raster_stats.best_rasterize_time,
        raster_stats.total_pixels_rasterized,
        metadata.is_tile_in_pending_tree_now_bin);

    HISTOGRAM_CUSTOM_COUNTS(
        "Renderer4.PictureRasterTimeUS",
        raster_stats.total_rasterize_time.InMicroseconds(),
        0,
        100000,
        100);
  } else {
    picture_pile->RasterToBitmap(&canvas, rect, contents_scale, NULL);
  }
}

}  // namespace cc
