// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile_manager.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "cc/platform_color.h"
#include "cc/rendering_stats.h"
#include "cc/resource_pool.h"
#include "cc/switches.h"
#include "cc/tile.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace {

const char* kRasterThreadNamePrefix = "CompositorRaster";

const int kMaxRasterThreads = 64;
const int kDefaultNumberOfRasterThreads = 1;

// Allow two pending raster tasks per thread. This keeps resource usage
// low while making sure raster threads aren't unnecessarily idle.
const int kNumPendingRasterTasksPerThread = 2;

// Determine bin based on three categories of tiles: things we need now,
// things we need soon, and eventually.
cc::TileManagerBin BinFromTilePriority(const cc::TilePriority& prio) {

  // The amount of time for which we want to have prepainting coverage.
  const double prepainting_window_time_seconds = 1.0;
  const double backfling_guard_distance_pixels = 314.0;

  // Explicitly limit how far ahead we will prepaint for low and non-low res.
  const double max_lores_paint_distance_pixels = 8192.0;
  const double max_hires_paint_distance_pixels = 4096.0;
  if (prio.resolution == cc::LOW_RESOLUTION) {
    if (prio.distance_to_visible_in_pixels > max_lores_paint_distance_pixels)
      return cc::NEVER_BIN;
  }
  else {
    if (prio.distance_to_visible_in_pixels > max_hires_paint_distance_pixels)
      return cc::NEVER_BIN;
  }

  if (prio.time_to_needed_in_seconds() == std::numeric_limits<float>::max())
    return cc::NEVER_BIN;

  if (prio.resolution == cc::NON_IDEAL_RESOLUTION)
    return cc::EVENTUALLY_BIN;

  if (prio.time_to_needed_in_seconds() == 0 ||
      prio.distance_to_visible_in_pixels < backfling_guard_distance_pixels)
    return cc::NOW_BIN;

  if (prio.time_to_needed_in_seconds() < prepainting_window_time_seconds)
    return cc::SOON_BIN;

  return cc::EVENTUALLY_BIN;
}

}  // namespace

namespace cc {

class RasterThread : public base::Thread {
 public:
  RasterThread(const std::string name)
      : base::Thread(name.c_str()),
        num_pending_tasks_(0) {
    Start();
  }
  virtual ~RasterThread() {
    Stop();
  }

  int num_pending_tasks() { return num_pending_tasks_; }

  void PostRasterTaskAndReply(const tracked_objects::Location& from_here,
                              PicturePileImpl* picture_pile,
                              uint8_t* mapped_buffer,
                              const gfx::Rect& rect,
                              float contents_scale,
                              RenderingStats* stats,
                              const base::Closure& reply) {
    ++num_pending_tasks_;
    message_loop_proxy()->PostTaskAndReply(
        from_here,
        base::Bind(&RunRasterTask,
                   base::Unretained(picture_pile),
                   mapped_buffer,
                   rect,
                   contents_scale,
                   stats),
        base::Bind(&RasterThread::RunReply, base::Unretained(this), reply));
  }

  void PostImageDecodingTaskAndReply(const tracked_objects::Location& from_here,
                                     skia::LazyPixelRef* pixel_ref,
                                     RenderingStats* stats,
                                     const base::Closure& reply) {
    ++num_pending_tasks_;
    message_loop_proxy()->PostTaskAndReply(
        from_here,
        base::Bind(&RunImageDecodeTask, pixel_ref, stats),
        base::Bind(&RasterThread::RunReply, base::Unretained(this), reply));
  }

 private:
  static void RunRasterTask(PicturePileImpl* picture_pile,
                            uint8_t* mapped_buffer,
                            const gfx::Rect& rect,
                            float contents_scale,
                            RenderingStats* stats) {
    TRACE_EVENT0("cc", "RasterThread::RunRasterTask");
    DCHECK(picture_pile);
    DCHECK(mapped_buffer);
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, rect.width(), rect.height());
    bitmap.setPixels(mapped_buffer);
    SkDevice device(bitmap);
    SkCanvas canvas(&device);
    picture_pile->Raster(
        &canvas,
        rect,
        contents_scale,
        stats);
  }

  static void RunImageDecodeTask(skia::LazyPixelRef* pixel_ref,
                                 RenderingStats* stats) {
    TRACE_EVENT0("cc", "RasterThread::RunImageDecodeTask");
    base::TimeTicks decodeBeginTime = base::TimeTicks::Now();
    pixel_ref->Decode();
    stats->totalDeferredImageDecodeTimeInSeconds +=
        (base::TimeTicks::Now() - decodeBeginTime).InSecondsF();
  }

  void RunReply(const base::Closure& reply) {
    --num_pending_tasks_;
    reply.Run();
  }

  int num_pending_tasks_;

  DISALLOW_COPY_AND_ASSIGN(RasterThread);
};

ManagedTileState::ManagedTileState()
    : can_use_gpu_memory(false),
      can_be_freed(true),
      resource_is_being_initialized(false),
      contents_swizzled(false),
      need_to_gather_pixel_refs(true),
      gpu_memmgr_stats_bin(NEVER_BIN) {
}

ManagedTileState::~ManagedTileState() {
  DCHECK(!resource);
  DCHECK(!resource_is_being_initialized);
}

TileManager::TileManager(
    TileManagerClient* client,
    ResourceProvider* resource_provider,
    size_t num_raster_threads)
    : client_(client),
      resource_pool_(ResourcePool::Create(resource_provider)),
      manage_tiles_pending_(false),
      manage_tiles_call_count_(0),
      check_for_completed_set_pixels_pending_(false) {
  // Initialize all threads.
  const std::string thread_name_prefix = kRasterThreadNamePrefix;
  while (raster_threads_.size() < num_raster_threads) {
    int thread_number = raster_threads_.size() + 1;
    scoped_ptr<RasterThread> thread = make_scoped_ptr(
        new RasterThread(thread_name_prefix +
                         StringPrintf("Worker%d", thread_number).c_str()));
    raster_threads_.push_back(thread.Pass());
  }

  ResetBinCounts();
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();
  AssignGpuMemoryToTiles();
  // This should finish all pending raster tasks and release any
  // uninitialized resources.
  raster_threads_.clear();
  ManageTiles();
  DCHECK(tiles_.size() == 0);
}

void TileManager::SetGlobalState(const GlobalStateThatImpactsTilePriority& global_state) {
  global_state_ = global_state;
  resource_pool_->SetMaxMemoryUsageBytes(global_state_.memory_limit_in_bytes);
  ScheduleManageTiles();
}

void TileManager::RegisterTile(Tile* tile) {
  tiles_.push_back(tile);
  ScheduleManageTiles();
}

void TileManager::UnregisterTile(Tile* tile) {
  for (TileList::iterator it = tiles_with_image_decoding_tasks_.begin();
       it != tiles_with_image_decoding_tasks_.end(); it++) {
    if (*it == tile) {
      tiles_with_image_decoding_tasks_.erase(it);
      break;;
    }
  }
  for (TileVector::iterator it = tiles_that_need_to_be_rasterized_.begin();
       it != tiles_that_need_to_be_rasterized_.end(); it++) {
    if (*it == tile) {
      tiles_that_need_to_be_rasterized_.erase(it);
      break;
    }
  }
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); it++) {
    if (*it == tile) {
      FreeResourcesForTile(tile);
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
  client_->ScheduleManageTiles();
  manage_tiles_pending_ = true;
}

void TileManager::ScheduleCheckForCompletedSetPixels() {
  if (check_for_completed_set_pixels_pending_)
    return;
  client_->ScheduleCheckForCompletedSetPixels();
  check_for_completed_set_pixels_pending_ = true;
}

class BinComparator {
public:
  bool operator() (const Tile* a, const Tile* b) const {
    const ManagedTileState& ams = a->managed_state();
    const ManagedTileState& bms = b->managed_state();
    if (ams.raster_bin != bms.raster_bin)
      return ams.raster_bin < bms.raster_bin;

    if (ams.resolution != bms.resolution)
      return ams.resolution < bms.resolution;

    return
      ams.time_to_needed_in_seconds <
      bms.time_to_needed_in_seconds;
  }
};

void TileManager::ManageTiles() {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");
  manage_tiles_pending_ = false;
  ++manage_tiles_call_count_;

  const bool smoothness_takes_priority =
      global_state_.smoothness_takes_priority;

  // For each tree, bin into different categories of tiles.
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    mts.bin[ACTIVE_TREE] = BinFromTilePriority(tile->priority(ACTIVE_TREE));
    mts.bin[PENDING_TREE] = BinFromTilePriority(tile->priority(PENDING_TREE));

    TilePriority prio;
    if (smoothness_takes_priority)
      prio = tile->priority(ACTIVE_TREE);
    else
      prio = tile->combined_priority();

    mts.resolution = prio.resolution;
    mts.time_to_needed_in_seconds = prio.time_to_needed_in_seconds();
    mts.raster_bin = BinFromTilePriority(prio);
    mts.gpu_memmgr_stats_bin = BinFromTilePriority(tile->combined_priority());
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
    bin_map[EVENTUALLY_BIN] = EVENTUALLY_BIN;
    bin_map[NEVER_BIN] = NEVER_BIN;
  }
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    mts.bin[ACTIVE_TREE] = bin_map[mts.bin[ACTIVE_TREE]];
    mts.bin[PENDING_TREE] = bin_map[mts.bin[PENDING_TREE]];
    mts.raster_bin = bin_map[mts.raster_bin];
  }

  // Update bin counts.
  ResetBinCounts();
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    for (int i = 0; i < NUM_TREES; ++i)
      tiles_in_bin_count_[mts.bin[i]][i]++;

    // Increment drawable count if GetResourceId() doesn't return 0.
    if (tile->GetResourceId()) {
      for (int i = 0; i < NUM_TREES; ++i)
        drawable_tiles_in_bin_count_[mts.bin[i]][i]++;
    }
  }

  // Sort by bin.
  std::sort(tiles_.begin(), tiles_.end(), BinComparator());

  // Assign gpu memory and determine what tiles need to be rasterized.
  AssignGpuMemoryToTiles();

  // Finally, kick the rasterizer.
  DispatchMoreTasks();
}

void TileManager::CheckForCompletedSetPixels() {
  check_for_completed_set_pixels_pending_ = false;

  while (!tiles_with_pending_set_pixels_.empty()) {
    Tile* tile = tiles_with_pending_set_pixels_.front();
    DCHECK(tile->managed_state().resource);

    // Set pixel tasks complete in the order they are posted.
    if (!resource_pool_->resource_provider()->didSetPixelsComplete(
          tile->managed_state().resource->id())) {
      ScheduleCheckForCompletedSetPixels();
      break;
    }

    // It's now safe to release the pixel buffer.
    resource_pool_->resource_provider()->releasePixelBuffer(
        tile->managed_state().resource->id());

    DidFinishTileInitialization(tile);
    tiles_with_pending_set_pixels_.pop();
  }
}

void TileManager::GetRenderingStats(RenderingStats* stats) {
  stats->totalRasterizeTimeInSeconds =
      rendering_stats_.totalRasterizeTimeInSeconds;
  stats->totalPixelsRasterized = rendering_stats_.totalPixelsRasterized;
  stats->totalDeferredImageDecodeCount =
      rendering_stats_.totalDeferredImageDecodeCount;
  stats->totalDeferredImageCacheHitCount =
      rendering_stats_.totalDeferredImageCacheHitCount;
  stats->totalImageGatheringCount = rendering_stats_.totalImageGatheringCount;
  stats->totalDeferredImageDecodeTimeInSeconds =
      rendering_stats_.totalDeferredImageDecodeTimeInSeconds;
  stats->totalImageGatheringTimeInSeconds =
      rendering_stats_.totalImageGatheringTimeInSeconds;
}

int TileManager::GetTilesInBinCount(TileManagerBin bin, WhichTree tree) {
  DCHECK(bin >= 0);
  DCHECK(bin < NUM_BINS);
  DCHECK(tree >= 0);
  DCHECK(tree < NUM_TREES);
  return tiles_in_bin_count_[bin][tree];
}

int TileManager::GetDrawableTilesInBinCount(
    TileManagerBin bin, WhichTree tree) {
  DCHECK(bin >= 0);
  DCHECK(bin < NUM_BINS);
  DCHECK(tree >= 0);
  DCHECK(tree < NUM_TREES);
  return drawable_tiles_in_bin_count_[bin][tree];
}

void TileManager::GetMemoryStats(
    size_t* memoryRequiredBytes,
    size_t* memoryNiceToHaveBytes,
    size_t* memoryUsedBytes) {
  *memoryRequiredBytes = 0;
  *memoryNiceToHaveBytes = 0;
  *memoryUsedBytes = 0;
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();
    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    if (mts.gpu_memmgr_stats_bin == NOW_BIN)
      *memoryRequiredBytes += tile_bytes;
    if (mts.gpu_memmgr_stats_bin != NEVER_BIN)
      *memoryNiceToHaveBytes += tile_bytes;
    if (mts.can_use_gpu_memory)
      *memoryUsedBytes += tile_bytes;
  }
}

void TileManager::ResetBinCounts() {
  for (int i = 0; i < NUM_BINS; ++i)
    for (int j = 0; j < NUM_TREES; ++j)
      tiles_in_bin_count_[i][j] = drawable_tiles_in_bin_count_[i][j] = 0;
}

void TileManager::AssignGpuMemoryToTiles() {
  TRACE_EVENT0("cc", "TileManager::AssignGpuMemoryToTiles");
  // Some memory cannot be released. Figure out which.
  size_t unreleasable_bytes = 0;
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    if (!tile->managed_state().can_be_freed)
      unreleasable_bytes += tile->bytes_consumed_if_allocated();
  }

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  tiles_that_need_to_be_rasterized_.erase(
      tiles_that_need_to_be_rasterized_.begin(),
      tiles_that_need_to_be_rasterized_.end());

  // Reset the image decoding list so that we don't mess up with tile
  // priorities. Tiles will be added to the image decoding list again
  // when DispatchMoreTasks() is called.
  tiles_with_image_decoding_tasks_.clear();

  size_t bytes_left = global_state_.memory_limit_in_bytes - unreleasable_bytes;
  for (TileVector::iterator it = tiles_.begin(); it != tiles_.end(); ++it) {
    Tile* tile = *it;
    size_t tile_bytes = tile->bytes_consumed_if_allocated();
    ManagedTileState& managed_tile_state = tile->managed_state();
    if (!managed_tile_state.can_be_freed)
      continue;
    if (managed_tile_state.raster_bin == NEVER_BIN) {
      managed_tile_state.can_use_gpu_memory = false;
      FreeResourcesForTile(tile);
      continue;
    }
    if (tile_bytes > bytes_left) {
      managed_tile_state.can_use_gpu_memory = false;
      FreeResourcesForTile(tile);
      continue;
    }
    bytes_left -= tile_bytes;
    managed_tile_state.can_use_gpu_memory = true;
    if (!managed_tile_state.resource &&
        !managed_tile_state.resource_is_being_initialized)
      tiles_that_need_to_be_rasterized_.push_back(tile);
  }

  // Reverse two tiles_that_need_* vectors such that pop_back gets
  // the highest priority tile.
  std::reverse(
      tiles_that_need_to_be_rasterized_.begin(),
      tiles_that_need_to_be_rasterized_.end());
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  ManagedTileState& managed_tile_state = tile->managed_state();
  DCHECK(managed_tile_state.can_be_freed);
  if (managed_tile_state.resource)
    resource_pool_->ReleaseResource(managed_tile_state.resource.Pass());
}

RasterThread* TileManager::GetFreeRasterThread() {
  RasterThread* thread = 0;
  for (RasterThreadVector::iterator it = raster_threads_.begin();
       it != raster_threads_.end(); ++it) {
    if ((*it)->num_pending_tasks() == kNumPendingRasterTasksPerThread)
      continue;
    // Check if this is the best thread we've found so far.
    if (!thread || (*it)->num_pending_tasks() < thread->num_pending_tasks())
      thread = *it;
  }
  return thread;
}

void TileManager::DispatchMoreTasks() {
  // Because tiles in the image decoding list have higher priorities, we
  // need to process those tiles first before we start to handle the tiles
  // in the need_to_be_rasterized queue.
  std::list<Tile*>::iterator it = tiles_with_image_decoding_tasks_.begin();
  while (it != tiles_with_image_decoding_tasks_.end()) {
    DispatchImageDecodingTasksForTile(*it);
    ManagedTileState& managed_state = (*it)->managed_state();
    if (managed_state.pending_pixel_refs.empty()) {
      RasterThread* thread = GetFreeRasterThread();
      if (!thread)
        return;
      DispatchOneRasterTask(thread, *it);
      tiles_with_image_decoding_tasks_.erase(it++);
    } else {
      ++it;
    }
  }

  // Process all tiles in the need_to_be_rasterized queue. If a tile has
  // image decoding tasks, put it to the back of the image decoding list.
  while (!tiles_that_need_to_be_rasterized_.empty()) {
    Tile* tile = tiles_that_need_to_be_rasterized_.back();
    DispatchImageDecodingTasksForTile(tile);
    ManagedTileState& managed_state = tile->managed_state();
    if (!managed_state.pending_pixel_refs.empty()) {
      tiles_with_image_decoding_tasks_.push_back(tile);
    } else {
      RasterThread* thread = GetFreeRasterThread();
      if (!thread)
        return;
      DispatchOneRasterTask(thread, tile);
    }
    tiles_that_need_to_be_rasterized_.pop_back();
  }
}

void TileManager::GatherPixelRefsForTile(Tile* tile) {
  TRACE_EVENT0("cc", "TileManager::GatherPixelRefsForTile");
  ManagedTileState& managed_state = tile->managed_state();
  if (managed_state.need_to_gather_pixel_refs) {
    base::TimeTicks gather_begin_time = base::TimeTicks::Now();
    const_cast<PicturePileImpl *>(tile->picture_pile())->GatherPixelRefs(
        tile->content_rect_, managed_state.pending_pixel_refs);
    rendering_stats_.totalImageGatheringCount++;
    rendering_stats_.totalImageGatheringTimeInSeconds +=
        (base::TimeTicks::Now() - gather_begin_time).InSecondsF();
    managed_state.need_to_gather_pixel_refs = false;
  }
}

void TileManager::DispatchImageDecodingTasksForTile(Tile* tile) {
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
      rendering_stats_.totalDeferredImageCacheHitCount++;
      pending_pixel_refs.erase(it++);
    } else {
      RasterThread* thread = GetFreeRasterThread();
      if (!thread)
        return;
      DispatchOneImageDecodingTask(thread, tile, *it);
      ++it;
    }
  }
}

void TileManager::DispatchOneImageDecodingTask(RasterThread* thread,
                                               scoped_refptr<Tile> tile,
                                               skia::LazyPixelRef* pixel_ref) {
  TRACE_EVENT0("cc", "TileManager::DispatchOneImageDecodingTask");
  uint32_t pixel_ref_id = pixel_ref->getGenerationID();
  DCHECK(pending_decode_tasks_.end() ==
      pending_decode_tasks_.find(pixel_ref_id));
  pending_decode_tasks_[pixel_ref_id] = pixel_ref;
  RenderingStats* stats = new RenderingStats();

  thread->PostImageDecodingTaskAndReply(
          FROM_HERE,
          pixel_ref,
          stats,
          base::Bind(&TileManager::OnImageDecodingTaskCompleted,
                     base::Unretained(this),
                     tile,
                     pixel_ref_id,
                     stats));
}

void TileManager::OnImageDecodingTaskCompleted(scoped_refptr<Tile> tile,
                                               uint32_t pixel_ref_id,
                                               RenderingStats* stats) {
  TRACE_EVENT0("cc", "TileManager::OnImageDecoded");
  pending_decode_tasks_.erase(pixel_ref_id);
  rendering_stats_.totalDeferredImageDecodeTimeInSeconds +=
      stats->totalDeferredImageDecodeTimeInSeconds;
  rendering_stats_.totalDeferredImageDecodeCount++;
  delete stats;
  for (TileList::iterator it = tiles_with_image_decoding_tasks_.begin();
      it != tiles_with_image_decoding_tasks_.end(); ++it) {
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
  DispatchMoreTasks();
}

void TileManager::DispatchOneRasterTask(
    RasterThread* thread, scoped_refptr<Tile> tile) {
  TRACE_EVENT0("cc", "TileManager::DispatchOneRasterTask");
  ManagedTileState& managed_tile_state = tile->managed_state();
  DCHECK(managed_tile_state.can_use_gpu_memory);
  scoped_ptr<ResourcePool::Resource> resource =
      resource_pool_->AcquireResource(tile->tile_size_.size(), tile->format_);
  resource_pool_->resource_provider()->acquirePixelBuffer(resource->id());

  managed_tile_state.resource_is_being_initialized = true;
  managed_tile_state.can_be_freed = false;

  ResourceProvider::ResourceId resource_id = resource->id();
  scoped_refptr<PicturePileImpl> picture_pile_clone =
      tile->picture_pile()->GetCloneForDrawingOnThread(thread);
  RenderingStats* stats = new RenderingStats();

  thread->PostRasterTaskAndReply(
      FROM_HERE,
      picture_pile_clone.get(),
      resource_pool_->resource_provider()->mapPixelBuffer(resource_id),
      tile->content_rect_,
      tile->contents_scale(),
      stats,
      base::Bind(&TileManager::OnRasterTaskCompleted,
                 base::Unretained(this),
                 tile,
                 base::Passed(&resource),
                 picture_pile_clone,
                 manage_tiles_call_count_,
                 stats));
}

void TileManager::OnRasterTaskCompleted(
    scoped_refptr<Tile> tile,
    scoped_ptr<ResourcePool::Resource> resource,
    scoped_refptr<PicturePileImpl> picture_pile_clone,
    int manage_tiles_call_count_when_dispatched,
    RenderingStats* stats) {
  TRACE_EVENT0("cc", "TileManager::OnRasterTaskCompleted");
  rendering_stats_.totalRasterizeTimeInSeconds +=
    stats->totalRasterizeTimeInSeconds;
  rendering_stats_.totalPixelsRasterized += stats->totalPixelsRasterized;
  delete stats;

  // Release raster resources.
  resource_pool_->resource_provider()->unmapPixelBuffer(resource->id());

  ManagedTileState& managed_tile_state = tile->managed_state();
  managed_tile_state.can_be_freed = true;

  // Tile can be freed after the completion of the raster task. Call
  // AssignGpuMemoryToTiles() to re-assign gpu memory to highest priority
  // tiles if ManageTiles() was called since task was dispatched. The result
  // of this could be that this tile is no longer allowed to use gpu
  // memory and in that case we need to abort initialization and free all
  // associated resources before calling DispatchMoreTasks().
  if (manage_tiles_call_count_when_dispatched != manage_tiles_call_count_)
    AssignGpuMemoryToTiles();

  // Finish resource initialization if |can_use_gpu_memory| is true.
  if (managed_tile_state.can_use_gpu_memory) {
    // The component order may be bgra if we're uploading bgra pixels to rgba
    // texture. Mark contents as swizzled if image component order is
    // different than texture format.
    managed_tile_state.contents_swizzled =
        !PlatformColor::sameComponentOrder(tile->format_);

    // Tile resources can't be freed until upload has completed.
    managed_tile_state.can_be_freed = false;

    resource_pool_->resource_provider()->beginSetPixels(resource->id());
    managed_tile_state.resource = resource.Pass();
    tiles_with_pending_set_pixels_.push(tile);

    ScheduleCheckForCompletedSetPixels();
  } else {
    resource_pool_->resource_provider()->releasePixelBuffer(resource->id());
    resource_pool_->ReleaseResource(resource.Pass());
    managed_tile_state.resource_is_being_initialized = false;
  }
  DispatchMoreTasks();
}

void TileManager::DidFinishTileInitialization(Tile* tile) {
  ManagedTileState& managed_tile_state = tile->managed_state();
  DCHECK(managed_tile_state.resource);
  managed_tile_state.resource_is_being_initialized = false;
  managed_tile_state.can_be_freed = true;
  for (int i = 0; i < NUM_TREES; ++i)
    drawable_tiles_in_bin_count_[managed_tile_state.bin[i]][i]++;
}

}
