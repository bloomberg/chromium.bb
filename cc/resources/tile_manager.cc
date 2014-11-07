// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile_manager.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/bind.h"
#include "base/debug/trace_event_argument.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/tile.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {
namespace {

// Flag to indicate whether we should try and detect that
// a tile is of solid color.
const bool kUseColorEstimator = true;

class RasterTaskImpl : public RasterTask {
 public:
  RasterTaskImpl(
      const Resource* resource,
      RasterSource* raster_source,
      const gfx::Rect& content_rect,
      float contents_scale,
      TileResolution tile_resolution,
      int layer_id,
      const void* tile_id,
      int source_frame_number,
      bool analyze_picture,
      RenderingStatsInstrumentation* rendering_stats,
      const base::Callback<void(const RasterSource::SolidColorAnalysis&, bool)>&
          reply,
      ImageDecodeTask::Vector* dependencies)
      : RasterTask(resource, dependencies),
        raster_source_(raster_source),
        content_rect_(content_rect),
        contents_scale_(contents_scale),
        tile_resolution_(tile_resolution),
        layer_id_(layer_id),
        tile_id_(tile_id),
        source_frame_number_(source_frame_number),
        analyze_picture_(analyze_picture),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT0("cc", "RasterizerTaskImpl::RunOnWorkerThread");

    DCHECK(raster_source_.get());
    DCHECK(raster_buffer_);

    if (analyze_picture_) {
      Analyze(raster_source_.get());
      if (analysis_.is_solid_color)
        return;
    }

    Raster(raster_source_.get());
  }

  // Overridden from RasterizerTask:
  void ScheduleOnOriginThread(RasterizerTaskClient* client) override {
    DCHECK(!raster_buffer_);
    raster_buffer_ = client->AcquireBufferForRaster(resource());
  }
  void CompleteOnOriginThread(RasterizerTaskClient* client) override {
    client->ReleaseBufferForRaster(raster_buffer_.Pass());
  }
  void RunReplyOnOriginThread() override {
    DCHECK(!raster_buffer_);
    reply_.Run(analysis_, !HasFinishedRunning());
  }

 protected:
  ~RasterTaskImpl() override { DCHECK(!raster_buffer_); }

 private:
  void Analyze(const RasterSource* raster_source) {
    frame_viewer_instrumentation::ScopedAnalyzeTask analyze_task(
        tile_id_, tile_resolution_, source_frame_number_, layer_id_);

    DCHECK(raster_source);

    raster_source->PerformSolidColorAnalysis(content_rect_, contents_scale_,
                                             &analysis_);

    // Record the solid color prediction.
    UMA_HISTOGRAM_BOOLEAN("Renderer4.SolidColorTilesAnalyzed",
                          analysis_.is_solid_color);

    // Clear the flag if we're not using the estimator.
    analysis_.is_solid_color &= kUseColorEstimator;
  }

  void Raster(const RasterSource* raster_source) {
    frame_viewer_instrumentation::ScopedRasterTask raster_task(
        tile_id_, tile_resolution_, source_frame_number_, layer_id_);
    devtools_instrumentation::ScopedLayerTask layer_task(
        devtools_instrumentation::kRasterTask, layer_id_);

    DCHECK(raster_source);

    raster_buffer_->Playback(raster_source_.get(), content_rect_,
                             contents_scale_);
  }

  RasterSource::SolidColorAnalysis analysis_;
  scoped_refptr<RasterSource> raster_source_;
  gfx::Rect content_rect_;
  float contents_scale_;
  TileResolution tile_resolution_;
  int layer_id_;
  const void* tile_id_;
  int source_frame_number_;
  bool analyze_picture_;
  RenderingStatsInstrumentation* rendering_stats_;
  const base::Callback<void(const RasterSource::SolidColorAnalysis&, bool)>
      reply_;
  scoped_ptr<RasterBuffer> raster_buffer_;

  DISALLOW_COPY_AND_ASSIGN(RasterTaskImpl);
};

class ImageDecodeTaskImpl : public ImageDecodeTask {
 public:
  ImageDecodeTaskImpl(SkPixelRef* pixel_ref,
                      int layer_id,
                      RenderingStatsInstrumentation* rendering_stats,
                      const base::Callback<void(bool was_canceled)>& reply)
      : pixel_ref_(skia::SharePtr(pixel_ref)),
        layer_id_(layer_id),
        rendering_stats_(rendering_stats),
        reply_(reply) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT0("cc", "ImageDecodeTaskImpl::RunOnWorkerThread");

    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        pixel_ref_.get());
    // This will cause the image referred to by pixel ref to be decoded.
    pixel_ref_->lockPixels();
    pixel_ref_->unlockPixels();
  }

  // Overridden from RasterizerTask:
  void ScheduleOnOriginThread(RasterizerTaskClient* client) override {}
  void CompleteOnOriginThread(RasterizerTaskClient* client) override {}
  void RunReplyOnOriginThread() override { reply_.Run(!HasFinishedRunning()); }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  skia::RefPtr<SkPixelRef> pixel_ref_;
  int layer_id_;
  RenderingStatsInstrumentation* rendering_stats_;
  const base::Callback<void(bool was_canceled)> reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

}  // namespace

RasterTaskCompletionStats::RasterTaskCompletionStats()
    : completed_count(0u), canceled_count(0u) {}

scoped_refptr<base::debug::ConvertableToTraceFormat>
RasterTaskCompletionStatsAsValue(const RasterTaskCompletionStats& stats) {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();
  state->SetInteger("completed_count", stats.completed_count);
  state->SetInteger("canceled_count", stats.canceled_count);
  return state;
}

// static
scoped_ptr<TileManager> TileManager::Create(
    TileManagerClient* client,
    base::SequencedTaskRunner* task_runner,
    ResourcePool* resource_pool,
    Rasterizer* rasterizer,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    size_t scheduled_raster_task_limit) {
  return make_scoped_ptr(new TileManager(client,
                                         task_runner,
                                         resource_pool,
                                         rasterizer,
                                         rendering_stats_instrumentation,
                                         scheduled_raster_task_limit));
}

TileManager::TileManager(
    TileManagerClient* client,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ResourcePool* resource_pool,
    Rasterizer* rasterizer,
    RenderingStatsInstrumentation* rendering_stats_instrumentation,
    size_t scheduled_raster_task_limit)
    : client_(client),
      task_runner_(task_runner),
      resource_pool_(resource_pool),
      rasterizer_(rasterizer),
      scheduled_raster_task_limit_(scheduled_raster_task_limit),
      all_tiles_that_need_to_be_rasterized_are_scheduled_(true),
      rendering_stats_instrumentation_(rendering_stats_instrumentation),
      did_initialize_visible_tile_(false),
      did_check_for_completed_tasks_since_last_schedule_tasks_(true),
      did_oom_on_last_assign_(false),
      ready_to_activate_check_notifier_(
          task_runner_.get(),
          base::Bind(&TileManager::CheckIfReadyToActivate,
                     base::Unretained(this))) {
  rasterizer_->SetClient(this);
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();

  RasterTaskQueue empty;
  rasterizer_->ScheduleTasks(&empty);
  orphan_raster_tasks_.clear();

  // This should finish all pending tasks and release any uninitialized
  // resources.
  rasterizer_->Shutdown();
  rasterizer_->CheckForCompletedTasks();

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();
}

void TileManager::Release(Tile* tile) {
  released_tiles_.push_back(tile);
}

TaskSetCollection TileManager::TasksThatShouldBeForcedToComplete() const {
  TaskSetCollection tasks_that_should_be_forced_to_complete;
  if (global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY)
    tasks_that_should_be_forced_to_complete[REQUIRED_FOR_ACTIVATION] = true;
  return tasks_that_should_be_forced_to_complete;
}

void TileManager::FreeResourcesForReleasedTiles() {
  for (std::vector<Tile*>::iterator it = released_tiles_.begin();
       it != released_tiles_.end();
       ++it) {
    Tile* tile = *it;
    FreeResourcesForTile(tile);
  }
}

void TileManager::CleanUpReleasedTiles() {
  std::vector<Tile*>::iterator it = released_tiles_.begin();
  while (it != released_tiles_.end()) {
    Tile* tile = *it;

    if (tile->HasRasterTask()) {
      ++it;
      continue;
    }

    DCHECK(!tile->HasResources());
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
    it = released_tiles_.erase(it);
  }
}

void TileManager::DidFinishRunningTasks(TaskSet task_set) {
  if (task_set == ALL) {
    TRACE_EVENT1("cc", "TileManager::DidFinishRunningTasks", "task_set", "ALL");

    bool memory_usage_above_limit = resource_pool_->total_memory_usage_bytes() >
                                    global_state_.soft_memory_limit_in_bytes;

    // When OOM, keep re-assigning memory until we reach a steady state
    // where top-priority tiles are initialized.
    if (all_tiles_that_need_to_be_rasterized_are_scheduled_ &&
        !memory_usage_above_limit)
      return;

    rasterizer_->CheckForCompletedTasks();
    did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

    TileVector tiles_that_need_to_be_rasterized;
    AssignGpuMemoryToTiles(&tiles_that_need_to_be_rasterized);

    // |tiles_that_need_to_be_rasterized| will be empty when we reach a
    // steady memory state. Keep scheduling tasks until we reach this state.
    if (!tiles_that_need_to_be_rasterized.empty()) {
      ScheduleTasks(tiles_that_need_to_be_rasterized);
      return;
    }

    FreeResourcesForReleasedTiles();

    resource_pool_->ReduceResourceUsage();

    // We don't reserve memory for required-for-activation tiles during
    // accelerated gestures, so we just postpone activation when we don't
    // have these tiles, and activate after the accelerated gesture.
    // Likewise if we don't allow any tiles (as is the case when we're
    // invisible), if we have tiles that aren't ready, then we shouldn't
    // activate as activation can cause checkerboards.
    bool allow_rasterize_on_demand =
        global_state_.tree_priority != SMOOTHNESS_TAKES_PRIORITY &&
        global_state_.memory_limit_policy != ALLOW_NOTHING;

    // Use on-demand raster for any required-for-activation tiles that have not
    // been been assigned memory after reaching a steady memory state. This
    // ensures that we activate even when OOM. Note that we have to rebuilt the
    // queue in case the last AssignGpuMemoryToTiles evicted some tiles that
    // would otherwise not be picked up by the old raster queue.
    client_->BuildRasterQueue(&raster_priority_queue_,
                              global_state_.tree_priority);
    bool ready_to_activate = true;
    while (!raster_priority_queue_.IsEmpty()) {
      Tile* tile = raster_priority_queue_.Top();
      ManagedTileState& mts = tile->managed_state();

      if (tile->required_for_activation() && !mts.draw_info.IsReadyToDraw()) {
        // If we can't raster on demand, give up early (and don't activate).
        if (!allow_rasterize_on_demand) {
          ready_to_activate = false;
          break;
        }

        mts.draw_info.set_rasterize_on_demand();
        client_->NotifyTileStateChanged(tile);
      }
      raster_priority_queue_.Pop();
    }

    if (ready_to_activate) {
      DCHECK(IsReadyToActivate());
      ready_to_activate_check_notifier_.Schedule();
    }
    raster_priority_queue_.Reset();
    return;
  }

  if (task_set == REQUIRED_FOR_ACTIVATION) {
    TRACE_EVENT1("cc",
                 "TileManager::DidFinishRunningTasks",
                 "task_set",
                 "REQUIRED_FOR_ACTIVATION");
    ready_to_activate_check_notifier_.Schedule();
  }
}

void TileManager::ManageTiles(const GlobalStateThatImpactsTilePriority& state) {
  TRACE_EVENT0("cc", "TileManager::ManageTiles");

  global_state_ = state;

  // We need to call CheckForCompletedTasks() once in-between each call
  // to ScheduleTasks() to prevent canceled tasks from being scheduled.
  if (!did_check_for_completed_tasks_since_last_schedule_tasks_) {
    rasterizer_->CheckForCompletedTasks();
    did_check_for_completed_tasks_since_last_schedule_tasks_ = true;
  }

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();

  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(&tiles_that_need_to_be_rasterized);

  // Finally, schedule rasterizer tasks.
  ScheduleTasks(tiles_that_need_to_be_rasterized);

  TRACE_EVENT_INSTANT1("cc",
                       "DidManage",
                       TRACE_EVENT_SCOPE_THREAD,
                       "state",
                       BasicStateAsValue());

  TRACE_COUNTER_ID1("cc",
                    "unused_memory_bytes",
                    this,
                    resource_pool_->total_memory_usage_bytes() -
                        resource_pool_->acquired_memory_usage_bytes());
}

bool TileManager::UpdateVisibleTiles() {
  TRACE_EVENT0("cc", "TileManager::UpdateVisibleTiles");

  rasterizer_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TRACE_EVENT_INSTANT1(
      "cc",
      "DidUpdateVisibleTiles",
      TRACE_EVENT_SCOPE_THREAD,
      "stats",
      RasterTaskCompletionStatsAsValue(update_visible_tiles_stats_));
  update_visible_tiles_stats_ = RasterTaskCompletionStats();

  bool did_initialize_visible_tile = did_initialize_visible_tile_;
  did_initialize_visible_tile_ = false;
  return did_initialize_visible_tile;
}

scoped_refptr<base::debug::ConvertableToTraceFormat>
TileManager::BasicStateAsValue() const {
  scoped_refptr<base::debug::TracedValue> value =
      new base::debug::TracedValue();
  BasicStateAsValueInto(value.get());
  return value;
}

void TileManager::BasicStateAsValueInto(base::debug::TracedValue* state) const {
  state->SetInteger("tile_count", tiles_.size());
  state->SetBoolean("did_oom_on_last_assign", did_oom_on_last_assign_);
  state->BeginDictionary("global_state");
  global_state_.AsValueInto(state);
  state->EndDictionary();
}

void TileManager::RebuildEvictionQueueIfNeeded() {
  TRACE_EVENT1("cc",
               "TileManager::RebuildEvictionQueueIfNeeded",
               "eviction_priority_queue_is_up_to_date",
               eviction_priority_queue_is_up_to_date_);
  if (eviction_priority_queue_is_up_to_date_)
    return;

  eviction_priority_queue_.Reset();
  client_->BuildEvictionQueue(&eviction_priority_queue_,
                              global_state_.tree_priority);
  eviction_priority_queue_is_up_to_date_ = true;
}

bool TileManager::FreeTileResourcesUntilUsageIsWithinLimit(
    const MemoryUsage& limit,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    RebuildEvictionQueueIfNeeded();
    if (eviction_priority_queue_.IsEmpty())
      return false;

    Tile* tile = eviction_priority_queue_.Top();
    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue_.Pop();
  }
  return true;
}

bool TileManager::FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
    const MemoryUsage& limit,
    const TilePriority& other_priority,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    RebuildEvictionQueueIfNeeded();
    if (eviction_priority_queue_.IsEmpty())
      return false;

    Tile* tile = eviction_priority_queue_.Top();
    if (!other_priority.IsHigherPriorityThan(tile->combined_priority()))
      return false;

    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue_.Pop();
  }
  return true;
}

bool TileManager::TilePriorityViolatesMemoryPolicy(
    const TilePriority& priority) {
  switch (global_state_.memory_limit_policy) {
    case ALLOW_NOTHING:
      return true;
    case ALLOW_ABSOLUTE_MINIMUM:
      return priority.priority_bin > TilePriority::NOW;
    case ALLOW_PREPAINT_ONLY:
      return priority.priority_bin > TilePriority::SOON;
    case ALLOW_ANYTHING:
      return priority.distance_to_visible ==
             std::numeric_limits<float>::infinity();
  }
  NOTREACHED();
  return true;
}

void TileManager::AssignGpuMemoryToTiles(
    TileVector* tiles_that_need_to_be_rasterized) {
  TRACE_EVENT_BEGIN0("cc", "TileManager::AssignGpuMemoryToTiles");

  // Maintain the list of released resources that can potentially be re-used
  // or deleted.
  // If this operation becomes expensive too, only do this after some
  // resource(s) was returned. Note that in that case, one also need to
  // invalidate when releasing some resource from the pool.
  resource_pool_->CheckBusyResources(false);

  // Now give memory out to the tiles until we're out, and build
  // the needs-to-be-rasterized queue.
  unsigned schedule_priority = 1u;
  all_tiles_that_need_to_be_rasterized_are_scheduled_ = true;
  bool had_enough_memory_to_schedule_tiles_needed_now = true;

  MemoryUsage hard_memory_limit(global_state_.hard_memory_limit_in_bytes,
                                global_state_.num_resources_limit);
  MemoryUsage soft_memory_limit(global_state_.soft_memory_limit_in_bytes,
                                global_state_.num_resources_limit);
  MemoryUsage memory_usage(resource_pool_->acquired_memory_usage_bytes(),
                           resource_pool_->acquired_resource_count());

  eviction_priority_queue_is_up_to_date_ = false;
  client_->BuildRasterQueue(&raster_priority_queue_,
                            global_state_.tree_priority);

  while (!raster_priority_queue_.IsEmpty()) {
    Tile* tile = raster_priority_queue_.Top();
    TilePriority priority = tile->combined_priority();

    if (TilePriorityViolatesMemoryPolicy(priority)) {
      TRACE_EVENT_INSTANT0(
          "cc",
          "TileManager::AssignGpuMemory tile violates memory policy",
          TRACE_EVENT_SCOPE_THREAD);
      break;
    }

    // We won't be able to schedule this tile, so break out early.
    if (tiles_that_need_to_be_rasterized->size() >=
        scheduled_raster_task_limit_) {
      all_tiles_that_need_to_be_rasterized_are_scheduled_ = false;
      break;
    }

    ManagedTileState& mts = tile->managed_state();
    mts.scheduled_priority = schedule_priority++;
    mts.resolution = priority.resolution;

    DCHECK(mts.draw_info.mode() ==
               ManagedTileState::DrawInfo::PICTURE_PILE_MODE ||
           !mts.draw_info.IsReadyToDraw());

    // If the tile already has a raster_task, then the memory used by it is
    // already accounted for in memory_usage. Otherwise, we'll have to acquire
    // more memory to create a raster task.
    MemoryUsage memory_required_by_tile_to_be_scheduled;
    if (!mts.raster_task.get()) {
      memory_required_by_tile_to_be_scheduled = MemoryUsage::FromConfig(
          tile->size(), resource_pool_->resource_format());
    }

    bool tile_is_needed_now = priority.priority_bin == TilePriority::NOW;

    // This is the memory limit that will be used by this tile. Depending on
    // the tile priority, it will be one of hard_memory_limit or
    // soft_memory_limit.
    MemoryUsage& tile_memory_limit =
        tile_is_needed_now ? hard_memory_limit : soft_memory_limit;

    bool memory_usage_is_within_limit =
        FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
            tile_memory_limit - memory_required_by_tile_to_be_scheduled,
            priority,
            &memory_usage);

    // If we couldn't fit the tile into our current memory limit, then we're
    // done.
    if (!memory_usage_is_within_limit) {
      if (tile_is_needed_now)
        had_enough_memory_to_schedule_tiles_needed_now = false;
      all_tiles_that_need_to_be_rasterized_are_scheduled_ = false;
      break;
    }

    memory_usage += memory_required_by_tile_to_be_scheduled;
    tiles_that_need_to_be_rasterized->push_back(tile);
    raster_priority_queue_.Pop();
  }

  // Note that we should try and further reduce memory in case the above loop
  // didn't reduce memory. This ensures that we always release as many resources
  // as possible to stay within the memory limit.
  FreeTileResourcesUntilUsageIsWithinLimit(hard_memory_limit, &memory_usage);

  UMA_HISTOGRAM_BOOLEAN("TileManager.ExceededMemoryBudget",
                        !had_enough_memory_to_schedule_tiles_needed_now);
  did_oom_on_last_assign_ = !had_enough_memory_to_schedule_tiles_needed_now;

  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.hard_memory_limit_in_bytes;
  memory_stats_from_last_assign_.total_bytes_used = memory_usage.memory_bytes();
  memory_stats_from_last_assign_.had_enough_memory =
      had_enough_memory_to_schedule_tiles_needed_now;

  raster_priority_queue_.Reset();

  TRACE_EVENT_END2("cc",
                   "TileManager::AssignGpuMemoryToTiles",
                   "all_tiles_that_need_to_be_rasterized_are_scheduled",
                   all_tiles_that_need_to_be_rasterized_are_scheduled_,
                   "had_enough_memory_to_schedule_tiles_needed_now",
                   had_enough_memory_to_schedule_tiles_needed_now);
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  ManagedTileState& mts = tile->managed_state();
  if (mts.draw_info.resource_)
    resource_pool_->ReleaseResource(mts.draw_info.resource_.Pass());
}

void TileManager::FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(
    Tile* tile) {
  bool was_ready_to_draw = tile->IsReadyToDraw();
  FreeResourcesForTile(tile);
  if (was_ready_to_draw)
    client_->NotifyTileStateChanged(tile);
}

void TileManager::ScheduleTasks(
    const TileVector& tiles_that_need_to_be_rasterized) {
  TRACE_EVENT1("cc",
               "TileManager::ScheduleTasks",
               "count",
               tiles_that_need_to_be_rasterized.size());

  DCHECK(did_check_for_completed_tasks_since_last_schedule_tasks_);

  raster_queue_.Reset();

  // Build a new task queue containing all task currently needed. Tasks
  // are added in order of priority, highest priority task first.
  for (TileVector::const_iterator it = tiles_that_need_to_be_rasterized.begin();
       it != tiles_that_need_to_be_rasterized.end();
       ++it) {
    Tile* tile = *it;
    ManagedTileState& mts = tile->managed_state();

    DCHECK(mts.draw_info.requires_resource());
    DCHECK(!mts.draw_info.resource_);

    if (!mts.raster_task.get())
      mts.raster_task = CreateRasterTask(tile);

    TaskSetCollection task_sets;
    if (tile->required_for_activation())
      task_sets.set(REQUIRED_FOR_ACTIVATION);
    task_sets.set(ALL);
    raster_queue_.items.push_back(
        RasterTaskQueue::Item(mts.raster_task.get(), task_sets));
  }

  // We must reduce the amount of unused resoruces before calling
  // ScheduleTasks to prevent usage from rising above limits.
  resource_pool_->ReduceResourceUsage();

  // Schedule running of |raster_queue_|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |raster_queue_|.
  rasterizer_->ScheduleTasks(&raster_queue_);

  // It's now safe to clean up orphan tasks as raster worker pool is not
  // allowed to keep around unreferenced raster tasks after ScheduleTasks() has
  // been called.
  orphan_raster_tasks_.clear();

  did_check_for_completed_tasks_since_last_schedule_tasks_ = false;
}

scoped_refptr<ImageDecodeTask> TileManager::CreateImageDecodeTask(
    Tile* tile,
    SkPixelRef* pixel_ref) {
  return make_scoped_refptr(new ImageDecodeTaskImpl(
      pixel_ref,
      tile->layer_id(),
      rendering_stats_instrumentation_,
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 tile->layer_id(),
                 base::Unretained(pixel_ref))));
}

scoped_refptr<RasterTask> TileManager::CreateRasterTask(Tile* tile) {
  ManagedTileState& mts = tile->managed_state();

  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(tile->size());
  const ScopedResource* const_resource = resource.get();

  // Create and queue all image decode tasks that this tile depends on.
  ImageDecodeTask::Vector decode_tasks;
  PixelRefTaskMap& existing_pixel_refs = image_decode_tasks_[tile->layer_id()];
  std::vector<SkPixelRef*> pixel_refs;
  tile->raster_source()->GatherPixelRefs(
      tile->content_rect(), tile->contents_scale(), &pixel_refs);
  for (SkPixelRef* pixel_ref : pixel_refs) {
    uint32_t id = pixel_ref->getGenerationID();

    // Append existing image decode task if available.
    PixelRefTaskMap::iterator decode_task_it = existing_pixel_refs.find(id);
    if (decode_task_it != existing_pixel_refs.end()) {
      decode_tasks.push_back(decode_task_it->second);
      continue;
    }

    // Create and append new image decode task for this pixel ref.
    scoped_refptr<ImageDecodeTask> decode_task =
        CreateImageDecodeTask(tile, pixel_ref);
    decode_tasks.push_back(decode_task);
    existing_pixel_refs[id] = decode_task;
  }

  return make_scoped_refptr(
      new RasterTaskImpl(const_resource,
                         tile->raster_source(),
                         tile->content_rect(),
                         tile->contents_scale(),
                         mts.resolution,
                         tile->layer_id(),
                         static_cast<const void*>(tile),
                         tile->source_frame_number(),
                         tile->use_picture_analysis(),
                         rendering_stats_instrumentation_,
                         base::Bind(&TileManager::OnRasterTaskCompleted,
                                    base::Unretained(this),
                                    tile->id(),
                                    base::Passed(&resource)),
                         &decode_tasks));
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
    const RasterSource::SolidColorAnalysis& analysis,
    bool was_canceled) {
  DCHECK(tiles_.find(tile_id) != tiles_.end());

  Tile* tile = tiles_[tile_id];
  ManagedTileState& mts = tile->managed_state();
  DCHECK(mts.raster_task.get());
  orphan_raster_tasks_.push_back(mts.raster_task);
  mts.raster_task = NULL;

  if (was_canceled) {
    ++update_visible_tiles_stats_.canceled_count;
    resource_pool_->ReleaseResource(resource.Pass());
    return;
  }

  ++update_visible_tiles_stats_.completed_count;

  if (analysis.is_solid_color) {
    mts.draw_info.set_solid_color(analysis.solid_color);
    resource_pool_->ReleaseResource(resource.Pass());
  } else {
    mts.draw_info.set_use_resource();
    mts.draw_info.resource_ = resource.Pass();
  }

  if (tile->priority(ACTIVE_TREE).distance_to_visible == 0.f)
    did_initialize_visible_tile_ = true;

  client_->NotifyTileStateChanged(tile);
}

scoped_refptr<Tile> TileManager::CreateTile(RasterSource* raster_source,
                                            const gfx::Size& tile_size,
                                            const gfx::Rect& content_rect,
                                            float contents_scale,
                                            int layer_id,
                                            int source_frame_number,
                                            int flags) {
  scoped_refptr<Tile> tile = make_scoped_refptr(new Tile(this,
                                                         raster_source,
                                                         tile_size,
                                                         content_rect,
                                                         contents_scale,
                                                         layer_id,
                                                         source_frame_number,
                                                         flags));
  DCHECK(tiles_.find(tile->id()) == tiles_.end());

  tiles_[tile->id()] = tile.get();
  used_layer_counts_[tile->layer_id()]++;
  return tile;
}

void TileManager::SetRasterizerForTesting(Rasterizer* rasterizer) {
  rasterizer_ = rasterizer;
  rasterizer_->SetClient(this);
}

bool TileManager::IsReadyToActivate() const {
  TRACE_EVENT0("cc", "TileManager::IsReadyToActivate");
  const std::vector<PictureLayerImpl*>& layers = client_->GetPictureLayers();

  for (std::vector<PictureLayerImpl*>::const_iterator it = layers.begin();
       it != layers.end();
       ++it) {
    if (!(*it)->AllTilesRequiredForActivationAreReadyToDraw())
      return false;
  }

  return true;
}

void TileManager::CheckIfReadyToActivate() {
  TRACE_EVENT0("cc", "TileManager::CheckIfReadyToActivate");

  rasterizer_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  if (IsReadyToActivate())
    client_->NotifyReadyToActivate();
}

TileManager::MemoryUsage::MemoryUsage() : memory_bytes_(0), resource_count_(0) {
}

TileManager::MemoryUsage::MemoryUsage(int64 memory_bytes, int resource_count)
    : memory_bytes_(memory_bytes), resource_count_(resource_count) {
}

// static
TileManager::MemoryUsage TileManager::MemoryUsage::FromConfig(
    const gfx::Size& size,
    ResourceFormat format) {
  return MemoryUsage(Resource::MemorySizeBytes(size, format), 1);
}

// static
TileManager::MemoryUsage TileManager::MemoryUsage::FromTile(const Tile* tile) {
  const ManagedTileState& mts = tile->managed_state();
  if (mts.draw_info.resource_) {
    return MemoryUsage::FromConfig(tile->size(),
                                   mts.draw_info.resource_->format());
  }
  return MemoryUsage();
}

TileManager::MemoryUsage& TileManager::MemoryUsage::operator+=(
    const MemoryUsage& other) {
  memory_bytes_ += other.memory_bytes_;
  resource_count_ += other.resource_count_;
  return *this;
}

TileManager::MemoryUsage& TileManager::MemoryUsage::operator-=(
    const MemoryUsage& other) {
  memory_bytes_ -= other.memory_bytes_;
  resource_count_ -= other.resource_count_;
  return *this;
}

TileManager::MemoryUsage TileManager::MemoryUsage::operator-(
    const MemoryUsage& other) {
  MemoryUsage result = *this;
  result -= other;
  return result;
}

bool TileManager::MemoryUsage::Exceeds(const MemoryUsage& limit) const {
  return memory_bytes_ > limit.memory_bytes_ ||
         resource_count_ > limit.resource_count_;
}

}  // namespace cc
