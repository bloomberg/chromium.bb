// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_manager.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"
#include "third_party/skia/include/core/SkDevice.h"

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
  if (!prio.is_live)
    return NEVER_BIN;

  // The amount of time for which we want to have prepainting coverage.
  const float kPrepaintingWindowTimeSeconds = 1.0f;
  const float kBackflingGuardDistancePixels = 314.0f;

  if (prio.time_to_visible_in_seconds == 0 ||
      prio.distance_to_visible_in_pixels < kBackflingGuardDistancePixels)
    return NOW_BIN;

  if (prio.resolution == NON_IDEAL_RESOLUTION)
    return EVENTUALLY_BIN;

  if (prio.time_to_visible_in_seconds < kPrepaintingWindowTimeSeconds)
    return SOON_BIN;

  return EVENTUALLY_BIN;
}

std::string ValueToString(scoped_ptr<base::Value> value) {
  std::string str;
  base::JSONWriter::Write(value.get(), &str);
  return str;
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

TileManager::TileManager(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    size_t num_raster_threads,
    bool use_color_estimator,
    bool prediction_benchmarking,
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : client_(client),
      resource_pool_(ResourcePool::Create(resource_provider)),
      raster_worker_pool_(RasterWorkerPool::Create(this, num_raster_threads)),
      manage_tiles_pending_(false),
      manage_tiles_call_count_(0),
      bytes_pending_upload_(0),
      has_performed_uploads_since_last_flush_(false),
      ever_exceeded_memory_budget_(false),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      use_color_estimator_(use_color_estimator),
      prediction_benchmarking_(prediction_benchmarking),
      did_initialize_visible_tile_(false),
      pending_tasks_(0),
      max_pending_tasks_(kMaxNumPendingTasksPerThread * num_raster_threads) {
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();
  AssignGpuMemoryToTiles();
  // This should finish all pending tasks and release any uninitialized
  // resources.
  raster_worker_pool_.reset();
  AbortPendingTileUploads();
  DCHECK_EQ(0u, tiles_with_pending_upload_.size());
  DCHECK_EQ(0u, all_tiles_.size());
  DCHECK_EQ(0u, live_or_allocated_tiles_.size());
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
  all_tiles_.insert(tile);
  ScheduleManageTiles();
}

void TileManager::UnregisterTile(Tile* tile) {
  for (TileVector::iterator it = tiles_that_need_to_be_rasterized_.begin();
       it != tiles_that_need_to_be_rasterized_.end(); it++) {
    if (*it == tile) {
      tiles_that_need_to_be_rasterized_.erase(it);
      break;
    }
  }
  for (TileVector::iterator it = live_or_allocated_tiles_.begin();
       it != live_or_allocated_tiles_.end(); it++) {
    if (*it == tile) {
      live_or_allocated_tiles_.erase(it);
      break;
    }
  }
  TileSet::iterator it = all_tiles_.find(tile);
  DCHECK(it != all_tiles_.end());
  FreeResourcesForTile(tile);
  all_tiles_.erase(it);
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

  live_or_allocated_tiles_.clear();
  // For each tree, bin into different categories of tiles.
  for (TileSet::iterator it = all_tiles_.begin();
       it != all_tiles_.end(); ++it) {
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

    if ((tile->drawing_info().memory_state_ == CAN_USE_MEMORY ||
        tile->drawing_info().memory_state_ == NOT_ALLOWED_TO_USE_MEMORY) &&
        !tile->priority(ACTIVE_TREE).is_live &&
        !tile->priority(PENDING_TREE).is_live)
      continue;

    live_or_allocated_tiles_.push_back(tile);
  }

  TRACE_COUNTER_ID1("cc", "LiveOrAllocatedTileCount", this,
                    live_or_allocated_tiles_.size());
}

void TileManager::SortTiles() {
  TRACE_EVENT0("cc", "TileManager::SortTiles");
  TRACE_COUNTER_ID1(
      "cc", "LiveTileCount", this, live_or_allocated_tiles_.size());

  // Sort by bin, resolution and time until needed.
  std::sort(live_or_allocated_tiles_.begin(),
            live_or_allocated_tiles_.end(), BinComparator());
}

void TileManager::ManageTiles() {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");
  TRACE_COUNTER_ID1("cc", "TileCount", this, all_tiles_.size());

  manage_tiles_pending_ = false;
  ++manage_tiles_call_count_;

  AssignBinsToTiles();
  SortTiles();
  AssignGpuMemoryToTiles();

  TRACE_EVENT_INSTANT1("cc", "DidManage", TRACE_EVENT_SCOPE_THREAD,
                       "state", ValueToString(BasicStateAsValue()));

  // Finally, kick the rasterizer.
  DispatchMoreTasks();
}

void TileManager::CheckForCompletedTileUploads() {
  while (!tiles_with_pending_upload_.empty()) {
    Tile* tile = tiles_with_pending_upload_.front();
    DCHECK(tile->drawing_info().resource_);

    // Set pixel tasks complete in the order they are posted.
    if (!resource_pool_->resource_provider()->DidSetPixelsComplete(
          tile->drawing_info().resource_->id())) {
      break;
    }

    // It's now safe to release the pixel buffer.
    resource_pool_->resource_provider()->ReleasePixelBuffer(
        tile->drawing_info().resource_->id());

    bytes_pending_upload_ -= tile->bytes_consumed_if_allocated();
    // Reset forced_upload_ since we now got the upload completed notification.
    tile->drawing_info().forced_upload_ = false;
    tile->drawing_info().memory_state_ = USING_RELEASABLE_MEMORY;
    DidFinishTileInitialization(tile);

    tiles_with_pending_upload_.pop();
  }

  DispatchMoreTasks();
}

void TileManager::AbortPendingTileUploads() {
  while (!tiles_with_pending_upload_.empty()) {
    Tile* tile = tiles_with_pending_upload_.front();
    DCHECK(tile->drawing_info().resource_);

    resource_pool_->resource_provider()->AbortSetPixels(
        tile->drawing_info().resource_->id());
    resource_pool_->resource_provider()->ReleasePixelBuffer(
        tile->drawing_info().resource_->id());

    FreeResourcesForTile(tile);

    bytes_pending_upload_ -= tile->bytes_consumed_if_allocated();
    tiles_with_pending_upload_.pop();
  }
}

void TileManager::ForceTileUploadToComplete(Tile* tile) {
  DCHECK(tile);
  if (tile->drawing_info().resource_ &&
      tile->drawing_info().memory_state_ == USING_UNRELEASABLE_MEMORY &&
      !tile->drawing_info().forced_upload_) {
    resource_pool_->resource_provider()->
        ForceSetPixelsToComplete(tile->drawing_info().resource_->id());

    // We have to set the memory state to be unreleasable, to ensure
    // that the tile will not be freed until we get the upload finished
    // notification. However, setting |forced_upload_| to true makes
    // this tile ready to draw.
    tile->drawing_info().memory_state_ = USING_UNRELEASABLE_MEMORY;
    tile->drawing_info().forced_upload_ = true;
    DidFinishTileInitialization(tile);
  }

  if (did_initialize_visible_tile_) {
    did_initialize_visible_tile_ = false;
    client_->DidInitializeVisibleTile();
  }
}

void TileManager::GetMemoryStats(
    size_t* memory_required_bytes,
    size_t* memory_nice_to_have_bytes,
    size_t* memory_used_bytes) const {
  *memory_required_bytes = 0;
  *memory_nice_to_have_bytes = 0;
  *memory_used_bytes = 0;
  for (size_t i = 0; i < live_or_allocated_tiles_.size(); i++) {
    const Tile* tile = live_or_allocated_tiles_[i];
    if (!tile->drawing_info().requires_resource())
      continue;

    const ManagedTileState& mts = tile->managed_state();
    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    if (mts.gpu_memmgr_stats_bin == NOW_BIN)
      *memory_required_bytes += tile_bytes;
    if (mts.gpu_memmgr_stats_bin != NEVER_BIN)
      *memory_nice_to_have_bytes += tile_bytes;
    if (tile->drawing_info().memory_state_ != NOT_ALLOWED_TO_USE_MEMORY)
      *memory_used_bytes += tile_bytes;
  }
}

scoped_ptr<base::Value> TileManager::BasicStateAsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetInteger("tile_count", all_tiles_.size());
  state->Set("global_state", global_state_.AsValue().release());
  state->Set("memory_requirements", GetMemoryRequirementsAsValue().release());
  return state.PassAs<base::Value>();
}

scoped_ptr<base::Value> TileManager::AllTilesAsValue() const {
  scoped_ptr<base::ListValue> state(new base::ListValue());
  for (TileSet::const_iterator it = all_tiles_.begin();
       it != all_tiles_.end();
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

void TileManager::DidFinishDispatchingWorkerPoolCompletionCallbacks() {
  // If a flush is needed, do it now before starting to dispatch more tasks.
  if (has_performed_uploads_since_last_flush_) {
    resource_pool_->resource_provider()->ShallowFlushIfSupported();
    has_performed_uploads_since_last_flush_ = false;
  }

  DispatchMoreTasks();
}

void TileManager::AssignGpuMemoryToTiles() {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");
  size_t unreleasable_bytes = 0;

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  tiles_that_need_to_be_rasterized_.clear();

  // By clearing the tiles_that_need_to_be_rasterized_ vector list
  // above we move all tiles currently waiting for raster to idle state.
  // Some memory cannot be released. We figure out how much in this
  // loop as well.
  for (TileVector::iterator it = live_or_allocated_tiles_.begin();
       it != live_or_allocated_tiles_.end(); ++it) {
    Tile* tile = *it;
    if (tile->drawing_info().memory_state_ == USING_UNRELEASABLE_MEMORY)
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
  for (TileVector::iterator it = live_or_allocated_tiles_.begin();
       it != live_or_allocated_tiles_.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::DrawingInfo& drawing_info = tile->drawing_info();

    // If this tile doesn't need a resource, or if the memory
    // is unreleasable, then we do not need to do anything.
    if (!drawing_info.requires_resource() ||
        drawing_info.memory_state_ == USING_UNRELEASABLE_MEMORY) {
      continue;
    }

    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    // If the tile is not needed, free it up.
    if (mts.is_in_never_bin_on_both_trees()) {
      FreeResourcesForTile(tile);
      drawing_info.memory_state_ = NOT_ALLOWED_TO_USE_MEMORY;
      continue;
    }
    // Tile is OOM.
    if (tile_bytes > bytes_left) {
      FreeResourcesForTile(tile);
      tile->drawing_info().set_rasterize_on_demand();
      if (mts.tree_bin[PENDING_TREE] == NOW_BIN) {
        tiles_requiring_memory_but_oomed.push_back(tile);
        bytes_oom_in_now_bin_on_pending_tree += tile_bytes;
      }
      continue;
    }
    drawing_info.set_use_resource();
    bytes_left -= tile_bytes;
    if (!drawing_info.resource_ &&
        drawing_info.memory_state_ == CAN_USE_MEMORY) {
      tiles_that_need_to_be_rasterized_.push_back(tile);
    }
  }

  // In OOM situation, we iterate all_tiles_, remove the memory for active tree
  // and not the now bin. And give them to bytes_oom_in_now_bin_on_pending_tree
  if (!tiles_requiring_memory_but_oomed.empty()) {
    size_t bytes_freed = 0;
    for (TileSet::iterator it = all_tiles_.begin();
         it != all_tiles_.end(); ++it) {
      Tile* tile = *it;
      ManagedTileState& mts = tile->managed_state();
      ManagedTileState::DrawingInfo& drawing_info = tile->drawing_info();
      if ((drawing_info.memory_state_ == CAN_USE_MEMORY ||
           drawing_info.memory_state_ == USING_RELEASABLE_MEMORY) &&
          mts.tree_bin[PENDING_TREE] == NEVER_BIN &&
          mts.tree_bin[ACTIVE_TREE] != NOW_BIN) {
        FreeResourcesForTile(tile);
        drawing_info.set_rasterize_on_demand();
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
      tile->drawing_info().set_use_resource();
      bytes_freed -= bytes_needed;
      tiles_that_need_to_be_rasterized_.push_back(tile);
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

  // Reverse two tiles_that_need_* vectors such that pop_back gets
  // the highest priority tile.
  std::reverse(
      tiles_that_need_to_be_rasterized_.begin(),
      tiles_that_need_to_be_rasterized_.end());
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  DCHECK(tile->drawing_info().memory_state_ != USING_UNRELEASABLE_MEMORY);
  if (tile->drawing_info().resource_) {
    resource_pool_->ReleaseResource(
        tile->drawing_info().resource_.Pass());
  }
  tile->drawing_info().memory_state_ = NOT_ALLOWED_TO_USE_MEMORY;
}

bool TileManager::CanDispatchRasterTask(Tile* tile) const {
  if (pending_tasks_ >= max_pending_tasks_)
    return false;
  size_t new_bytes_pending = bytes_pending_upload_;
  new_bytes_pending += tile->bytes_consumed_if_allocated();
  return new_bytes_pending <= kMaxPendingUploadBytes &&
         tiles_with_pending_upload_.size() < kMaxPendingUploads;
}

void TileManager::DispatchMoreTasks() {
  TileVector tiles_with_image_decoding_tasks;

  // Process all tiles in the need_to_be_rasterized queue:
  // 1. Analyze the tile and early out if it's solid color or transparent.
  // 2. Dispatch image decode tasks.
  // 3. If the image decode isn't done, save the tile for later processing.
  // 4. Attempt to dispatch a raster task, or break out of the loop.
  while (!tiles_that_need_to_be_rasterized_.empty()) {
    Tile* tile = tiles_that_need_to_be_rasterized_.back();

    AnalyzeTile(tile);
    if (!tile->drawing_info().requires_resource()) {
      tiles_that_need_to_be_rasterized_.pop_back();
      continue;
    }

    DispatchImageDecodeTasksForTile(tile);
    if (!tile->managed_state().pending_pixel_refs.empty()) {
      tiles_with_image_decoding_tasks.push_back(tile);
    } else if (!CanDispatchRasterTask(tile)) {
        break;
    } else {
      DispatchOneRasterTask(tile);
    }
    tiles_that_need_to_be_rasterized_.pop_back();
  }

  // Put the saved tiles back into the queue. The order is reversed
  // to preserve original ordering.
  tiles_that_need_to_be_rasterized_.insert(
      tiles_that_need_to_be_rasterized_.end(),
      tiles_with_image_decoding_tasks.rbegin(),
      tiles_with_image_decoding_tasks.rend());

  if (did_initialize_visible_tile_) {
    did_initialize_visible_tile_ = false;
    client_->DidInitializeVisibleTile();
  }
}

void TileManager::AnalyzeTile(Tile* tile) {
  ManagedTileState& managed_tile_state = tile->managed_state();
  if (use_color_estimator_ && !managed_tile_state.picture_pile_analyzed) {
    tile->picture_pile()->AnalyzeInRect(
        tile->content_rect(),
        tile->contents_scale(),
        &managed_tile_state.picture_pile_analysis);
    managed_tile_state.picture_pile_analysis.is_solid_color &=
        use_color_estimator_;
    managed_tile_state.picture_pile_analysis.is_transparent &=
        use_color_estimator_;
    managed_tile_state.picture_pile_analyzed = true;
    managed_tile_state.need_to_gather_pixel_refs = false;
    managed_tile_state.pending_pixel_refs.swap(
        managed_tile_state.picture_pile_analysis.lazy_pixel_refs);

    if (managed_tile_state.picture_pile_analysis.is_solid_color) {
      tile->drawing_info().set_solid_color(
        managed_tile_state.picture_pile_analysis.solid_color);
      DidFinishTileInitialization(tile);
    } else if (managed_tile_state.picture_pile_analysis.is_transparent) {
      tile->drawing_info().set_transparent();
      DidFinishTileInitialization(tile);
    }
  }
}

void TileManager::GatherPixelRefsForTile(Tile* tile) {
  // TODO(vmpstr): Remove this function and pending_pixel_refs
  // when reveman's improvements to worker pool go in.
  TRACE_EVENT0("cc", "TileManager::GatherPixelRefsForTile");
  ManagedTileState& managed_tile_state = tile->managed_state();
  if (managed_tile_state.need_to_gather_pixel_refs) {
    base::TimeTicks start_time =
        rendering_stats_instrumentation_->StartRecording();
    for (PicturePileImpl::PixelRefIterator pixel_ref_iter(
            tile->content_rect(),
            tile->contents_scale(),
            tile->picture_pile());
        pixel_ref_iter;
        ++pixel_ref_iter) {
      managed_tile_state.pending_pixel_refs.push_back(*pixel_ref_iter);
    }
    managed_tile_state.need_to_gather_pixel_refs = false;
    base::TimeDelta duration =
        rendering_stats_instrumentation_->EndRecording(start_time);
    rendering_stats_instrumentation_->AddImageGathering(duration);
  }
}

void TileManager::DispatchImageDecodeTasksForTile(Tile* tile) {
  GatherPixelRefsForTile(tile);
  std::list<skia::LazyPixelRef*>& pending_pixel_refs =
      tile->managed_state().pending_pixel_refs;
  std::list<skia::LazyPixelRef*>::iterator it = pending_pixel_refs.begin();
  while (it != pending_pixel_refs.end()) {
    if (pending_decode_tasks_.end() != pending_decode_tasks_.find(
        (*it)->getGenerationID())) {
      ++it;
      continue;
    }
    // TODO(qinmin): passing correct image size to PrepareToDecode().
    if ((*it)->PrepareToDecode(skia::LazyPixelRef::PrepareParams())) {
      rendering_stats_instrumentation_->IncrementDeferredImageCacheHitCount();
      pending_pixel_refs.erase(it++);
    } else {
      if (pending_tasks_ >= max_pending_tasks_)
        return;
      DispatchOneImageDecodeTask(tile, *it);
      ++it;
    }
  }
}

void TileManager::DispatchOneImageDecodeTask(
    scoped_refptr<Tile> tile, skia::LazyPixelRef* pixel_ref) {
  TRACE_EVENT0("cc", "TileManager::DispatchOneImageDecodeTask");
  uint32_t pixel_ref_id = pixel_ref->getGenerationID();
  DCHECK(pending_decode_tasks_.end() ==
      pending_decode_tasks_.find(pixel_ref_id));
  pending_decode_tasks_[pixel_ref_id] = pixel_ref;

  raster_worker_pool_->PostTaskAndReply(
      base::Bind(&TileManager::RunImageDecodeTask,
                 pixel_ref,
                 rendering_stats_instrumentation_),
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 tile,
                 pixel_ref_id));
  pending_tasks_++;
}

void TileManager::OnImageDecodeTaskCompleted(
    scoped_refptr<Tile> tile, uint32_t pixel_ref_id) {
  TRACE_EVENT0("cc", "TileManager::OnImageDecodeTaskCompleted");
  pending_decode_tasks_.erase(pixel_ref_id);
  pending_tasks_--;

  for (TileVector::iterator it = tiles_that_need_to_be_rasterized_.begin();
       it != tiles_that_need_to_be_rasterized_.end(); ++it) {
    std::list<skia::LazyPixelRef*>& pixel_refs =
        (*it)->managed_state().pending_pixel_refs;
    for (std::list<skia::LazyPixelRef*>::iterator pixel_it =
        pixel_refs.begin(); pixel_it != pixel_refs.end(); ++pixel_it) {
      if (pixel_ref_id == (*pixel_it)->getGenerationID()) {
        pixel_refs.erase(pixel_it);
        break;
      }
    }
  }
}

scoped_ptr<ResourcePool::Resource> TileManager::PrepareTileForRaster(
    Tile* tile) {
  scoped_ptr<ResourcePool::Resource> resource = resource_pool_->AcquireResource(
      tile->tile_size_.size(),
      tile->drawing_info().resource_format_);
  resource_pool_->resource_provider()->AcquirePixelBuffer(resource->id());

  tile->drawing_info().memory_state_ = USING_UNRELEASABLE_MEMORY;

  return resource.Pass();
}

void TileManager::DispatchOneRasterTask(scoped_refptr<Tile> tile) {
  TRACE_EVENT0("cc", "TileManager::DispatchOneRasterTask");
  scoped_ptr<ResourcePool::Resource> resource = PrepareTileForRaster(tile);
  ResourceProvider::ResourceId resource_id = resource->id();
  uint8* buffer =
      resource_pool_->resource_provider()->MapPixelBuffer(resource_id);

  CHECK(buffer);
  // skia requires that our buffer be 4-byte aligned
  CHECK(!(reinterpret_cast<intptr_t>(buffer) & 3));

  raster_worker_pool_->PostRasterTaskAndReply(
      tile->picture_pile(),
      base::Bind(&TileManager::RunRasterTask,
                 buffer,
                 tile->content_rect(),
                 tile->contents_scale(),
                 GetRasterTaskMetadata(*tile),
                 rendering_stats_instrumentation_),
      base::Bind(&TileManager::OnRasterTaskCompleted,
                 base::Unretained(this),
                 tile,
                 base::Passed(&resource),
                 manage_tiles_call_count_));
  pending_tasks_++;
}

TileManager::RasterTaskMetadata TileManager::GetRasterTaskMetadata(
    const Tile& tile) const {
  RasterTaskMetadata metadata;
  const ManagedTileState& mts = tile.managed_state();
  metadata.prediction_benchmarking = prediction_benchmarking_;
  metadata.is_tile_in_pending_tree_now_bin =
      mts.tree_bin[PENDING_TREE] == NOW_BIN;
  metadata.tile_resolution = mts.resolution;
  metadata.layer_id = tile.layer_id();
  return metadata;
}

void TileManager::OnRasterTaskCompleted(
    scoped_refptr<Tile> tile,
    scoped_ptr<ResourcePool::Resource> resource,
    int manage_tiles_call_count_when_dispatched) {
  TRACE_EVENT0("cc", "TileManager::OnRasterTaskCompleted");

  pending_tasks_--;

  // Release raster resources.
  resource_pool_->resource_provider()->UnmapPixelBuffer(resource->id());

  tile->drawing_info().memory_state_ = USING_RELEASABLE_MEMORY;

  // Tile can be freed after the completion of the raster task. Call
  // AssignGpuMemoryToTiles() to re-assign gpu memory to highest priority
  // tiles if ManageTiles() was called since task was dispatched. The result
  // of this could be that this tile is no longer allowed to use gpu
  // memory and in that case we need to abort initialization and free all
  // associated resources before calling DispatchMoreTasks().
  if (manage_tiles_call_count_when_dispatched != manage_tiles_call_count_)
    AssignGpuMemoryToTiles();

  // Finish resource initialization we're still using memory.
  if (tile->drawing_info().memory_state_ == USING_RELEASABLE_MEMORY) {
    // Tile resources can't be freed until upload has completed.
    tile->drawing_info().memory_state_ = USING_UNRELEASABLE_MEMORY;

    resource_pool_->resource_provider()->BeginSetPixels(resource->id());
    has_performed_uploads_since_last_flush_ = true;

    tile->drawing_info().resource_ = resource.Pass();

    bytes_pending_upload_ += tile->bytes_consumed_if_allocated();
    tiles_with_pending_upload_.push(tile);
  } else {
    resource_pool_->resource_provider()->ReleasePixelBuffer(resource->id());
    resource_pool_->ReleaseResource(resource.Pass());
  }
}

void TileManager::DidFinishTileInitialization(Tile* tile) {
  if (tile->priority(ACTIVE_TREE).distance_to_visible_in_pixels == 0)
    did_initialize_visible_tile_ = true;
}

void TileManager::DidTileTreeBinChange(Tile* tile,
                                       TileManagerBin new_tree_bin,
                                       WhichTree tree) {
  ManagedTileState& mts = tile->managed_state();
  mts.tree_bin[tree] = new_tree_bin;
}

// static
void TileManager::RunRasterTask(
    uint8* buffer,
    gfx::Rect rect,
    float contents_scale,
    const RasterTaskMetadata& metadata,
    RenderingStatsInstrumentation* stats_instrumentation,
    PicturePileImpl* picture_pile) {
  TRACE_EVENT2(
      "cc", "TileManager::RunRasterTask",
      "is_on_pending_tree",
      metadata.is_tile_in_pending_tree_now_bin,
      "is_low_res",
      metadata.tile_resolution == LOW_RESOLUTION);
  devtools_instrumentation::ScopedRasterTask raster_task(metadata.layer_id);

  DCHECK(picture_pile);
  DCHECK(buffer);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
  bitmap.setPixels(buffer);
  SkDevice device(bitmap);
  SkCanvas canvas(&device);

  if (stats_instrumentation->record_rendering_stats()) {
    PicturePileImpl::RasterStats raster_stats;
    picture_pile->Raster(&canvas, rect, contents_scale, &raster_stats);
    stats_instrumentation->AddRaster(raster_stats.total_rasterize_time,
                                     raster_stats.best_rasterize_time,
                                     raster_stats.total_pixels_rasterized,
                                     metadata.is_tile_in_pending_tree_now_bin);

    HISTOGRAM_CUSTOM_COUNTS("Renderer4.PictureRasterTimeUS",
                            raster_stats.total_rasterize_time.InMicroseconds(),
                            0,
                            100000,
                            100);

    if (metadata.prediction_benchmarking) {
      PicturePileImpl::Analysis analysis;
      picture_pile->AnalyzeInRect(rect, contents_scale, &analysis);

      DCHECK_EQ(bitmap.rowBytes(),
                static_cast<size_t>(bitmap.width() * bitmap.bytesPerPixel()));

      RecordSolidColorPredictorResults(
          reinterpret_cast<SkColor*>(bitmap.getPixels()),
          bitmap.getSize() / bitmap.bytesPerPixel(),
          analysis.is_solid_color,
          analysis.solid_color,
          analysis.is_transparent);
    }
  } else {
    picture_pile->Raster(&canvas, rect, contents_scale, NULL);
  }
}

// static
void TileManager::RecordSolidColorPredictorResults(
    const SkColor* actual_colors,
    size_t color_count,
    bool is_predicted_solid,
    SkColor predicted_color,
    bool is_predicted_transparent) {
  DCHECK_GT(color_count, 0u);

  bool is_actually_solid = true;
  bool is_transparent = true;

  SkColor actual_color = *actual_colors;
  for (unsigned int i = 0; i < color_count; ++i) {
    SkColor current_color = actual_colors[i];
    if (current_color != actual_color ||
        SkColorGetA(current_color) != 255)
      is_actually_solid = false;

    if (SkColorGetA(current_color) != 0)
      is_transparent = false;

    if (!is_actually_solid && !is_transparent)
      break;
  }

  if (is_predicted_solid && !is_actually_solid)
    HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.WrongActualNotSolid", true);
  else if (is_predicted_solid &&
           is_actually_solid &&
           predicted_color != actual_color)
    HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.WrongColor", true);
  else if (!is_predicted_solid && is_actually_solid)
    HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.WrongActualSolid", true);

  bool correct_guess = (is_predicted_solid && is_actually_solid &&
                        predicted_color == actual_color) ||
                       (!is_predicted_solid && !is_actually_solid);
  HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.Accuracy", correct_guess);

  if (correct_guess)
    HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.IsCorrectSolid",
                          is_predicted_solid);

  if (is_predicted_transparent)
    HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.PredictedTransparentIsActually",
                      is_transparent);
  HISTOGRAM_BOOLEAN("Renderer4.ColorPredictor.IsActuallyTransparent",
                    is_transparent);
}

// static
void TileManager::RunImageDecodeTask(
    skia::LazyPixelRef* pixel_ref,
    RenderingStatsInstrumentation* stats_instrumentation) {
  TRACE_EVENT0("cc", "TileManager::RunImageDecodeTask");
  base::TimeTicks start_time = stats_instrumentation->StartRecording();
  pixel_ref->Decode();
  base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
  stats_instrumentation->AddDeferredImageDecode(duration);
}

}  // namespace cc
