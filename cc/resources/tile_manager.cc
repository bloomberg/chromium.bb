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
#include "cc/resources/image_raster_worker_pool.h"
#include "cc/resources/pixel_buffer_raster_worker_pool.h"
#include "cc/resources/tile.h"
#include "skia/ext/paint_simplifier.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

namespace {

class DisableLCDTextFilter : public SkDrawFilter {
 public:
  // SkDrawFilter interface.
  virtual bool filter(SkPaint* paint, SkDrawFilter::Type type) OVERRIDE {
    if (type != SkDrawFilter::kText_Type)
      return true;

    paint->setLCDRenderText(false);
    return true;
  }
};

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
  return make_scoped_ptr(
      new TileManager(client,
                      resource_provider,
                      use_map_image ?
                      ImageRasterWorkerPool::Create(
                          resource_provider, num_raster_threads) :
                      PixelBufferRasterWorkerPool::Create(
                          resource_provider, num_raster_threads),
                      num_raster_threads,
                      use_color_estimator,
                      rendering_stats_instrumentation));
}

TileManager::TileManager(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    scoped_ptr<RasterWorkerPool> raster_worker_pool,
    size_t num_raster_threads,
    bool use_color_estimator,
    RenderingStatsInstrumentation* rendering_stats_instrumentation)
    : client_(client),
      resource_pool_(ResourcePool::Create(resource_provider)),
      raster_worker_pool_(raster_worker_pool.Pass()),
      ever_exceeded_memory_budget_(false),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      use_color_estimator_(use_color_estimator),
      did_initialize_visible_tile_(false) {
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();
  AssignGpuMemoryToTiles();
  // This should finish all pending tasks and release any uninitialized
  // resources.
  raster_worker_pool_->Shutdown();
  raster_worker_pool_->CheckForCompletedTasks();
  DCHECK_EQ(0u, tiles_.size());
}

void TileManager::SetGlobalState(
    const GlobalStateThatImpactsTilePriority& global_state) {
  global_state_ = global_state;
  resource_pool_->SetMaxMemoryUsageBytes(
      global_state_.memory_limit_in_bytes,
      global_state_.unused_memory_limit_in_bytes);
}

void TileManager::RegisterTile(Tile* tile) {
  DCHECK(std::find(tiles_.begin(), tiles_.end(), tile) == tiles_.end());
  DCHECK(!tile->required_for_activation());
  tiles_.push_back(tile);
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
  AssignBinsToTiles();
  SortTiles();
  AssignGpuMemoryToTiles();

  // This could have changed after AssignGpuMemoryToTiles.
  if (AreTilesRequiredForActivationReady())
    client_->NotifyReadyToActivate();

  TRACE_EVENT_INSTANT1(
      "cc", "DidManage", TRACE_EVENT_SCOPE_THREAD,
      "state", TracedValue::FromValue(BasicStateAsValue().release()));

  // Finally, schedule rasterizer tasks.
  ScheduleTasks();
}

void TileManager::CheckForCompletedTileUploads() {
  raster_worker_pool_->CheckForCompletedTasks();

  if (!client_->ShouldForceTileUploadsRequiredForActivationToComplete())
    return;

  TileSet initialized_tiles;
  for (TileSet::iterator it =
           tiles_that_need_to_be_initialized_for_activation_.begin();
       it != tiles_that_need_to_be_initialized_for_activation_.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();

    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      ManagedTileState::TileVersion& pending_version =
          mts.tile_versions[mode];
      if (!pending_version.raster_task_.is_null() &&
          !pending_version.forced_upload_) {
        if (!raster_worker_pool_->ForceUploadToComplete(
                pending_version.raster_task_)) {
          continue;
        }
        // Setting |forced_upload_| to true makes this tile version
        // ready to draw.
        pending_version.forced_upload_ = true;
        initialized_tiles.insert(tile);
        break;
      }
    }
  }

  for (TileSet::iterator it = initialized_tiles.begin();
       it != initialized_tiles.end();
       ++it) {
    Tile* tile = *it;
    DidFinishTileInitialization(tile);
    DCHECK(tile->IsReadyToDraw(NULL));
  }
}

void TileManager::GetMemoryStats(
    size_t* memory_required_bytes,
    size_t* memory_nice_to_have_bytes,
    size_t* memory_used_bytes) const {
  *memory_required_bytes = 0;
  *memory_nice_to_have_bytes = 0;
  *memory_used_bytes = resource_pool_->acquired_memory_usage_bytes();
  for (TileVector::const_iterator it = tiles_.begin();
       it != tiles_.end();
       ++it) {
    const Tile* tile = *it;
    const ManagedTileState& mts = tile->managed_state();

    TileRasterMode mode = HIGH_QUALITY_RASTER_MODE;
    if (tile->IsReadyToDraw(&mode) &&
        !mts.tile_versions[mode].requires_resource())
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

TileRasterMode TileManager::DetermineRasterMode(const Tile* tile) const {
  DCHECK(tile);
  DCHECK(tile->picture_pile());

  TileRasterMode raster_mode;

  if (tile->managed_state().resolution == LOW_RESOLUTION)
    raster_mode = LOW_QUALITY_RASTER_MODE;
  else if (!tile->picture_pile()->can_use_lcd_text())
    raster_mode = HIGH_QUALITY_NO_LCD_RASTER_MODE;
  else
    raster_mode = HIGH_QUALITY_RASTER_MODE;

  return raster_mode;
}

void TileManager::AssignGpuMemoryToTiles() {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  tiles_that_need_to_be_rasterized_.clear();
  tiles_that_need_to_be_initialized_for_activation_.clear();

  size_t bytes_releasable = 0;
  for (TileVector::const_iterator it = tiles_.begin();
       it != tiles_.end();
       ++it) {
    const Tile* tile = *it;
    const ManagedTileState& mts = tile->managed_state();
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (mts.tile_versions[mode].resource_)
        bytes_releasable += tile->bytes_consumed_if_allocated();
    }
  }

  // Cast to prevent overflow.
  int64 bytes_available =
      static_cast<int64>(bytes_releasable) +
      static_cast<int64>(global_state_.memory_limit_in_bytes) -
      static_cast<int64>(resource_pool_->acquired_memory_usage_bytes());

  size_t bytes_allocatable =
      std::max(static_cast<int64>(0), bytes_available);

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

    // Pick the better version out of the one we already set,
    // and the one that is required.
    mts.raster_mode = std::min(mts.raster_mode, DetermineRasterMode(tile));

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

    // It costs to maintain a resource.
    for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
      if (mts.tile_versions[mode].resource_)
        tile_bytes += tile->bytes_consumed_if_allocated();
    }

    // If we don't have the required version, and it's not in flight
    // then we'll have to pay to create a new task.
    if (!tile_version.resource_ && tile_version.raster_task_.is_null())
      tile_bytes += tile->bytes_consumed_if_allocated();

    // Tile is OOM.
    if (tile_bytes > bytes_left) {
      mts.tile_versions[mts.raster_mode].set_rasterize_on_demand();
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

    if (!tile->IsReadyToDraw(NULL) &&
        tile->required_for_activation()) {
      AddRequiredTileForActivation(tile);
    }
  }

  // In OOM situation, we iterate tiles_, remove the memory for active tree
  // and not the now bin. And give them to bytes_oom_in_now_bin_on_pending_tree
  if (!tiles_requiring_memory_but_oomed.empty()) {
    size_t bytes_freed = 0;
    for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
      Tile* tile = *it;
      ManagedTileState& mts = tile->managed_state();
      if (mts.tree_bin[PENDING_TREE] == NEVER_BIN &&
          mts.tree_bin[ACTIVE_TREE] != NOW_BIN) {
        size_t bytes_that_can_be_freed = 0;
        for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
          ManagedTileState::TileVersion& tile_version =
              mts.tile_versions[mode];
          if (tile_version.resource_) {
            DCHECK(!tile->required_for_activation());
            bytes_that_can_be_freed += tile->bytes_consumed_if_allocated();
          }
        }

        if (bytes_that_can_be_freed > 0) {
          FreeResourcesForTile(tile);
          bytes_freed += bytes_that_can_be_freed;
          mts.tile_versions[mts.raster_mode].set_rasterize_on_demand();
          TileVector::iterator it = std::find(
                  tiles_that_need_to_be_rasterized_.begin(),
                  tiles_that_need_to_be_rasterized_.end(),
                  tile);
          if (it != tiles_that_need_to_be_rasterized_.end())
              tiles_that_need_to_be_rasterized_.erase(it);
        }
      }

      if (bytes_oom_in_now_bin_on_pending_tree <= bytes_freed)
        break;
    }

    for (TileVector::iterator it = tiles_requiring_memory_but_oomed.begin();
         it != tiles_requiring_memory_but_oomed.end() && bytes_freed > 0;
         ++it) {
      Tile* tile = *it;
      ManagedTileState& mts = tile->managed_state();
      size_t bytes_needed = tile->bytes_consumed_if_allocated();
      if (bytes_needed > bytes_freed)
        continue;
      mts.tile_versions[mts.raster_mode].set_use_resource();
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
  memory_stats_from_last_assign_.bytes_unreleasable =
      bytes_allocatable - bytes_releasable;
  memory_stats_from_last_assign_.bytes_over =
      bytes_that_exceeded_memory_budget_in_now_bin;
}

void TileManager::FreeResourceForTile(Tile* tile, TileRasterMode mode) {
  ManagedTileState& mts = tile->managed_state();
  if (mts.tile_versions[mode].resource_) {
    resource_pool_->ReleaseResource(
        mts.tile_versions[mode].resource_.Pass());
  }
  mts.tile_versions[mode].resource_id_ = 0;
  mts.tile_versions[mode].forced_upload_ = false;
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    FreeResourceForTile(tile, static_cast<TileRasterMode>(mode));
  }
}

void TileManager::FreeUnusedResourcesForTile(Tile* tile) {
  TileRasterMode used_mode = HIGH_QUALITY_RASTER_MODE;
  bool version_is_used = tile->IsReadyToDraw(&used_mode);
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode) {
    if (!version_is_used || mode != used_mode)
      FreeResourceForTile(tile, static_cast<TileRasterMode>(mode));
  }
}

void TileManager::ScheduleTasks() {
  TRACE_EVENT0("cc", "TileManager::ScheduleTasks");
  RasterWorkerPool::RasterTask::Queue tasks;
  PixelRefSet decoded_images;

  // Build a new task queue containing all task currently needed. Tasks
  // are added in order of priority, highest priority task first.
  for (TileVector::iterator it = tiles_that_need_to_be_rasterized_.begin();
       it != tiles_that_need_to_be_rasterized_.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    ManagedTileState::TileVersion& tile_version =
        mts.tile_versions[mts.raster_mode];

    DCHECK(tile_version.requires_resource());
    DCHECK(!tile_version.resource_);

    if (tile_version.raster_task_.is_null())
      tile_version.raster_task_ = CreateRasterTask(tile, &decoded_images);

    tasks.Append(tile_version.raster_task_);
  }

  // Schedule running of |tasks|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |tasks|.
  raster_worker_pool_->ScheduleTasks(&tasks);
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
                                             uint32_t pixel_ref_id) {
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
  metadata.raster_mode = mts.raster_mode;
  return metadata;
}

RasterWorkerPool::RasterTask TileManager::CreateRasterTask(
    Tile* tile,
    PixelRefSet* decoded_images) {
  TRACE_EVENT0("cc", "TileManager::CreateRasterTask");

  ManagedTileState& mts = tile->managed_state();

  scoped_ptr<ResourcePool::Resource> resource =
      resource_pool_->AcquireResource(
          tile->tile_size_.size(),
          mts.tile_versions[mts.raster_mode].resource_format_);
  const Resource* const_resource = resource.get();

  DCHECK(!mts.tile_versions[mts.raster_mode].resource_id_);
  DCHECK(!mts.tile_versions[mts.raster_mode].forced_upload_);
  mts.tile_versions[mts.raster_mode].resource_id_ = resource->id();

  PicturePileImpl::Analysis* analysis = new PicturePileImpl::Analysis;

  // Create and queue all image decode tasks that this tile depends on.
  RasterWorkerPool::Task::Set decode_tasks;
  for (PicturePileImpl::PixelRefIterator iter(tile->content_rect(),
                                              tile->contents_scale(),
                                              tile->picture_pile());
       iter; ++iter) {
    skia::LazyPixelRef* pixel_ref = *iter;
    uint32_t id = pixel_ref->getGenerationID();

    // Append existing image decode task if available.
    PixelRefMap::iterator decode_task_it = pending_decode_tasks_.find(id);
    if (decode_task_it != pending_decode_tasks_.end()) {
      decode_tasks.Insert(decode_task_it->second);
      continue;
    }

    if (decoded_images->find(id) != decoded_images->end())
      continue;

    // TODO(qinmin): passing correct image size to PrepareToDecode().
    if (pixel_ref->PrepareToDecode(skia::LazyPixelRef::PrepareParams())) {
      decoded_images->insert(id);
      rendering_stats_instrumentation_->IncrementDeferredImageCacheHitCount();
      continue;
    }

    // Create and append new image decode task for this pixel ref.
    RasterWorkerPool::Task decode_task = CreateImageDecodeTask(
        tile, pixel_ref);
    decode_tasks.Insert(decode_task);
    pending_decode_tasks_[id] = decode_task;
  }

  RasterTaskMetadata metadata = GetRasterTaskMetadata(*tile);
  return RasterWorkerPool::RasterTask(
      tile->picture_pile(),
      const_resource,
      base::Bind(&TileManager::RunAnalyzeAndRasterTask,
                 base::Bind(&TileManager::RunAnalyzeTask,
                            analysis,
                            tile->content_rect(),
                            tile->contents_scale(),
                            use_color_estimator_,
                            metadata,
                            rendering_stats_instrumentation_),
                 base::Bind(&TileManager::RunRasterTask,
                            analysis,
                            tile->content_rect(),
                            tile->contents_scale(),
                            metadata,
                            rendering_stats_instrumentation_)),
      base::Bind(&TileManager::OnRasterTaskCompleted,
                 base::Unretained(this),
                 make_scoped_refptr(tile),
                 base::Passed(&resource),
                 base::Owned(analysis),
                 metadata.raster_mode),
      &decode_tasks);
}

void TileManager::OnRasterTaskCompleted(
    scoped_refptr<Tile> tile,
    scoped_ptr<ResourcePool::Resource> resource,
    PicturePileImpl::Analysis* analysis,
    TileRasterMode raster_mode,
    bool was_canceled) {
  TRACE_EVENT1("cc", "TileManager::OnRasterTaskCompleted",
               "was_canceled", was_canceled);

  ManagedTileState& mts = tile->managed_state();
  ManagedTileState::TileVersion& tile_version =
      mts.tile_versions[raster_mode];
  DCHECK(!tile_version.raster_task_.is_null());
  tile_version.raster_task_.Reset();

  if (was_canceled) {
    resource_pool_->ReleaseResource(resource.Pass());
    tile_version.resource_id_ = 0;
    return;
  }

  mts.picture_pile_analysis = *analysis;
  mts.picture_pile_analyzed = true;

  if (analysis->is_solid_color) {
    tile_version.set_solid_color(analysis->solid_color);
    resource_pool_->ReleaseResource(resource.Pass());
  } else {
    tile_version.resource_ = resource.Pass();
    tile_version.forced_upload_ = false;
  }

  FreeUnusedResourcesForTile(tile.get());

  DidFinishTileInitialization(tile.get());
}

void TileManager::DidFinishTileInitialization(Tile* tile) {
  if (tile->priority(ACTIVE_TREE).distance_to_visible_in_pixels == 0)
    did_initialize_visible_tile_ = true;
  if (tile->required_for_activation()) {
    // It's possible that a tile required for activation is not in this list
    // if it was marked as being required after being dispatched for
    // rasterization but before AssignGPUMemory was called again.
    tiles_that_need_to_be_initialized_for_activation_.erase(tile);

    if (AreTilesRequiredForActivationReady())
      client_->NotifyReadyToActivate();
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
bool TileManager::RunAnalyzeAndRasterTask(
    const base::Callback<void(PicturePileImpl* picture_pile)>& analyze_task,
    const RasterWorkerPool::RasterTask::Callback& raster_task,
    SkDevice* device,
    PicturePileImpl* picture_pile) {
  analyze_task.Run(picture_pile);
  return raster_task.Run(device, picture_pile);
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
  TRACE_EVENT1(
      "cc", "TileManager::RunAnalyzeTask",
      "metadata", TracedValue::FromValue(metadata.AsValue().release()));

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
  res->SetInteger("raster_mode", raster_mode);
  return res.PassAs<base::Value>();
}

// static
bool TileManager::RunRasterTask(
    PicturePileImpl::Analysis* analysis,
    gfx::Rect rect,
    float contents_scale,
    const RasterTaskMetadata& metadata,
    RenderingStatsInstrumentation* stats_instrumentation,
    SkDevice* device,
    PicturePileImpl* picture_pile) {
  TRACE_EVENT1(
      "cc", "TileManager::RunRasterTask",
      "metadata", TracedValue::FromValue(metadata.AsValue().release()));
  devtools_instrumentation::ScopedLayerTask raster_task(
      devtools_instrumentation::kRasterTask, metadata.layer_id);

  DCHECK(picture_pile);
  DCHECK(analysis);
  DCHECK(device);

  if (analysis->is_solid_color)
    return false;

  SkCanvas canvas(device);

  skia::RefPtr<SkDrawFilter> draw_filter;
  switch (metadata.raster_mode) {
    case LOW_QUALITY_RASTER_MODE:
      draw_filter = skia::AdoptRef(new skia::PaintSimplifier);
      break;
    case HIGH_QUALITY_NO_LCD_RASTER_MODE:
      draw_filter = skia::AdoptRef(new DisableLCDTextFilter);
      break;
    case HIGH_QUALITY_RASTER_MODE:
      break;
    case NUM_RASTER_MODES:
    default:
      NOTREACHED();
  }

  canvas.setDrawFilter(draw_filter.get());

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

  return true;
}

}  // namespace cc
