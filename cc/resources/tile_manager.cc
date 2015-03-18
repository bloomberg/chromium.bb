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
#include "base/trace_event/trace_event_argument.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/debug/frame_viewer_instrumentation.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/raster_buffer.h"
#include "cc/resources/rasterizer.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_task_runner.h"
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

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {
    DCHECK(!raster_buffer_);
    raster_buffer_ = client->AcquireBufferForRaster(resource());
  }
  void CompleteOnOriginThread(TileTaskClient* client) override {
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
  const base::Callback<void(const RasterSource::SolidColorAnalysis&, bool)>
      reply_;
  scoped_ptr<RasterBuffer> raster_buffer_;

  DISALLOW_COPY_AND_ASSIGN(RasterTaskImpl);
};

class ImageDecodeTaskImpl : public ImageDecodeTask {
 public:
  ImageDecodeTaskImpl(SkPixelRef* pixel_ref,
                      const base::Callback<void(bool was_canceled)>& reply)
      : pixel_ref_(skia::SharePtr(pixel_ref)),
        reply_(reply) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT0("cc", "ImageDecodeTaskImpl::RunOnWorkerThread");

    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        pixel_ref_.get());
    // This will cause the image referred to by pixel ref to be decoded.
    pixel_ref_->lockPixels();
    pixel_ref_->unlockPixels();

    // Release the reference after decoding image to ensure that it is not
    // kept alive unless needed.
    pixel_ref_.clear();
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {}
  void CompleteOnOriginThread(TileTaskClient* client) override {}
  void RunReplyOnOriginThread() override { reply_.Run(!HasFinishedRunning()); }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  skia::RefPtr<SkPixelRef> pixel_ref_;
  const base::Callback<void(bool was_canceled)> reply_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

const char* TaskSetName(TaskSet task_set) {
  switch (task_set) {
    case TileManager::ALL:
      return "ALL";
    case TileManager::REQUIRED_FOR_ACTIVATION:
      return "REQUIRED_FOR_ACTIVATION";
    case TileManager::REQUIRED_FOR_DRAW:
      return "REQUIRED_FOR_DRAW";
  }

  NOTREACHED();
  return "Invalid TaskSet";
}

}  // namespace

RasterTaskCompletionStats::RasterTaskCompletionStats()
    : completed_count(0u), canceled_count(0u) {}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RasterTaskCompletionStatsAsValue(const RasterTaskCompletionStats& stats) {
  scoped_refptr<base::trace_event::TracedValue> state =
      new base::trace_event::TracedValue();
  state->SetInteger("completed_count", stats.completed_count);
  state->SetInteger("canceled_count", stats.canceled_count);
  return state;
}

// static
scoped_ptr<TileManager> TileManager::Create(
    TileManagerClient* client,
    base::SequencedTaskRunner* task_runner,
    ResourcePool* resource_pool,
    TileTaskRunner* tile_task_runner,
    Rasterizer* rasterizer,
    size_t scheduled_raster_task_limit) {
  return make_scoped_ptr(new TileManager(client, task_runner, resource_pool,
                                         tile_task_runner, rasterizer,
                                         scheduled_raster_task_limit));
}

TileManager::TileManager(
    TileManagerClient* client,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    ResourcePool* resource_pool,
    TileTaskRunner* tile_task_runner,
    Rasterizer* rasterizer,
    size_t scheduled_raster_task_limit)
    : client_(client),
      task_runner_(task_runner),
      resource_pool_(resource_pool),
      tile_task_runner_(tile_task_runner),
      rasterizer_(rasterizer),
      scheduled_raster_task_limit_(scheduled_raster_task_limit),
      all_tiles_that_need_to_be_rasterized_are_scheduled_(true),
      did_check_for_completed_tasks_since_last_schedule_tasks_(true),
      did_oom_on_last_assign_(false),
      ready_to_activate_notifier_(
          task_runner_.get(),
          base::Bind(&TileManager::NotifyReadyToActivate,
                     base::Unretained(this))),
      ready_to_draw_notifier_(
          task_runner_.get(),
          base::Bind(&TileManager::NotifyReadyToDraw, base::Unretained(this))),
      ready_to_activate_check_notifier_(
          task_runner_.get(),
          base::Bind(&TileManager::CheckIfReadyToActivate,
                     base::Unretained(this))),
      ready_to_draw_check_notifier_(
          task_runner_.get(),
          base::Bind(&TileManager::CheckIfReadyToDraw, base::Unretained(this))),
      more_tiles_need_prepare_check_notifier_(
          task_runner_.get(),
          base::Bind(&TileManager::CheckIfMoreTilesNeedToBePrepared,
                     base::Unretained(this))),
      did_notify_ready_to_activate_(false),
      did_notify_ready_to_draw_(false) {
  tile_task_runner_->SetClient(this);
}

TileManager::~TileManager() {
  // Reset global state and manage. This should cause
  // our memory usage to drop to zero.
  global_state_ = GlobalStateThatImpactsTilePriority();

  TileTaskQueue empty;
  tile_task_runner_->ScheduleTasks(&empty);
  orphan_raster_tasks_.clear();

  // This should finish all pending tasks and release any uninitialized
  // resources.
  tile_task_runner_->Shutdown();
  tile_task_runner_->CheckForCompletedTasks();

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

    DCHECK(!tile->HasResource());
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

void TileManager::DidFinishRunningTileTasks(TaskSet task_set) {
  TRACE_EVENT1("cc", "TileManager::DidFinishRunningTileTasks", "task_set",
               TaskSetName(task_set));

  switch (task_set) {
    case ALL: {
      bool memory_usage_above_limit =
          resource_pool_->total_memory_usage_bytes() >
          global_state_.soft_memory_limit_in_bytes;

      if (all_tiles_that_need_to_be_rasterized_are_scheduled_ &&
          !memory_usage_above_limit)
        return;

      more_tiles_need_prepare_check_notifier_.Schedule();
      return;
    }
    case REQUIRED_FOR_ACTIVATION:
      ready_to_activate_check_notifier_.Schedule();
      return;
    case REQUIRED_FOR_DRAW:
      ready_to_draw_check_notifier_.Schedule();
      return;
  }

  NOTREACHED();
}

void TileManager::PrepareTiles(
    const GlobalStateThatImpactsTilePriority& state) {
  TRACE_EVENT0("cc", "TileManager::PrepareTiles");

  global_state_ = state;

  PrepareTilesMode prepare_tiles_mode = rasterizer_->GetPrepareTilesMode();

  // TODO(hendrikw): Consider moving some of this code to the rasterizer.
  if (prepare_tiles_mode != PrepareTilesMode::PREPARE_NONE) {
    // We need to call CheckForCompletedTasks() once in-between each call
    // to ScheduleTasks() to prevent canceled tasks from being scheduled.
    if (!did_check_for_completed_tasks_since_last_schedule_tasks_) {
      tile_task_runner_->CheckForCompletedTasks();
      did_check_for_completed_tasks_since_last_schedule_tasks_ = true;
    }

    FreeResourcesForReleasedTiles();
    CleanUpReleasedTiles();

    TileVector tiles_that_need_to_be_rasterized;
    scoped_ptr<RasterTilePriorityQueue> raster_priority_queue(
        client_->BuildRasterQueue(global_state_.tree_priority,
                                  RasterTilePriorityQueue::Type::ALL));
    AssignGpuMemoryToTiles(raster_priority_queue.get(),
                           scheduled_raster_task_limit_,
                           &tiles_that_need_to_be_rasterized);

    // Inform the client that will likely require a draw if the highest priority
    // tile that will be rasterized is required for draw.
    client_->SetIsLikelyToRequireADraw(
        !tiles_that_need_to_be_rasterized.empty() &&
        (*tiles_that_need_to_be_rasterized.begin())->required_for_draw());

    // Schedule tile tasks.
    ScheduleTasks(tiles_that_need_to_be_rasterized);

    did_notify_ready_to_activate_ = false;
    did_notify_ready_to_draw_ = false;
  } else {
    if (global_state_.hard_memory_limit_in_bytes == 0) {
      resource_pool_->CheckBusyResources(false);
      MemoryUsage memory_limit(0, 0);
      MemoryUsage memory_usage(resource_pool_->acquired_memory_usage_bytes(),
                               resource_pool_->acquired_resource_count());
      FreeTileResourcesUntilUsageIsWithinLimit(nullptr, memory_limit,
                                               &memory_usage);
    }

    did_notify_ready_to_activate_ = false;
    did_notify_ready_to_draw_ = false;
    ready_to_activate_notifier_.Schedule();
    ready_to_draw_notifier_.Schedule();
  }

  TRACE_EVENT_INSTANT1("cc", "DidPrepareTiles", TRACE_EVENT_SCOPE_THREAD,
                       "state", BasicStateAsValue());

  TRACE_COUNTER_ID1("cc", "unused_memory_bytes", this,
                    resource_pool_->total_memory_usage_bytes() -
                        resource_pool_->acquired_memory_usage_bytes());
}

void TileManager::SynchronouslyRasterizeTiles(
    const GlobalStateThatImpactsTilePriority& state) {
  TRACE_EVENT0("cc", "TileManager::SynchronouslyRasterizeTiles");

  DCHECK(rasterizer_->GetPrepareTilesMode() !=
         PrepareTilesMode::RASTERIZE_PRIORITIZED_TILES);

  global_state_ = state;

  FreeResourcesForReleasedTiles();
  CleanUpReleasedTiles();

  scoped_ptr<RasterTilePriorityQueue> required_for_draw_queue(
      client_->BuildRasterQueue(
          global_state_.tree_priority,
          RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW));
  TileVector tiles_that_need_to_be_rasterized;
  AssignGpuMemoryToTiles(required_for_draw_queue.get(),
                         std::numeric_limits<size_t>::max(),
                         &tiles_that_need_to_be_rasterized);

  // We must reduce the amount of unused resources before calling
  // RunTasks to prevent usage from rising above limits.
  resource_pool_->ReduceResourceUsage();

  // Run and complete all raster task synchronously.
  rasterizer_->RasterizeTiles(
      tiles_that_need_to_be_rasterized, resource_pool_,
      tile_task_runner_->GetResourceFormat(),
      base::Bind(&TileManager::UpdateTileDrawInfo, base::Unretained(this)));

  // Use on-demand raster for any required-for-draw tiles that have not been
  // assigned memory after reaching a steady memory state.
  // TODO(hendrikw): Figure out why this would improve jank on some tests - See
  // crbug.com/449288
  required_for_draw_queue = client_->BuildRasterQueue(
      global_state_.tree_priority,
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);

  // Change to OOM mode for any tiles that have not been been assigned memory.
  // This ensures that we draw even when OOM.
  for (; !required_for_draw_queue->IsEmpty(); required_for_draw_queue->Pop()) {
    Tile* tile = required_for_draw_queue->Top();
    tile->draw_info().set_oom();
    client_->NotifyTileStateChanged(tile);
  }

  TRACE_EVENT_INSTANT1("cc", "DidRasterize", TRACE_EVENT_SCOPE_THREAD, "state",
                       BasicStateAsValue());

  TRACE_COUNTER_ID1("cc", "unused_memory_bytes", this,
                    resource_pool_->total_memory_usage_bytes() -
                        resource_pool_->acquired_memory_usage_bytes());
}

void TileManager::UpdateVisibleTiles(
    const GlobalStateThatImpactsTilePriority& state) {
  TRACE_EVENT0("cc", "TileManager::UpdateVisibleTiles");

  tile_task_runner_->CheckForCompletedTasks();

  DCHECK(rasterizer_);
  PrepareTilesMode prepare_tiles_mode = rasterizer_->GetPrepareTilesMode();
  if (prepare_tiles_mode != PrepareTilesMode::RASTERIZE_PRIORITIZED_TILES)
    SynchronouslyRasterizeTiles(state);

  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  TRACE_EVENT_INSTANT1(
      "cc",
      "DidUpdateVisibleTiles",
      TRACE_EVENT_SCOPE_THREAD,
      "stats",
      RasterTaskCompletionStatsAsValue(update_visible_tiles_stats_));
  update_visible_tiles_stats_ = RasterTaskCompletionStats();
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
TileManager::BasicStateAsValue() const {
  scoped_refptr<base::trace_event::TracedValue> value =
      new base::trace_event::TracedValue();
  BasicStateAsValueInto(value.get());
  return value;
}

void TileManager::BasicStateAsValueInto(
    base::trace_event::TracedValue* state) const {
  state->SetInteger("tile_count", tiles_.size());
  state->SetBoolean("did_oom_on_last_assign", did_oom_on_last_assign_);
  state->BeginDictionary("global_state");
  global_state_.AsValueInto(state);
  state->EndDictionary();
}

scoped_ptr<EvictionTilePriorityQueue>
TileManager::FreeTileResourcesUntilUsageIsWithinLimit(
    scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
    const MemoryUsage& limit,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    if (!eviction_priority_queue) {
      eviction_priority_queue =
          client_->BuildEvictionQueue(global_state_.tree_priority);
    }
    if (eviction_priority_queue->IsEmpty())
      break;

    Tile* tile = eviction_priority_queue->Top();
    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue->Pop();
  }
  return eviction_priority_queue;
}

scoped_ptr<EvictionTilePriorityQueue>
TileManager::FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
    scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
    const MemoryUsage& limit,
    const TilePriority& other_priority,
    MemoryUsage* usage) {
  while (usage->Exceeds(limit)) {
    if (!eviction_priority_queue) {
      eviction_priority_queue =
          client_->BuildEvictionQueue(global_state_.tree_priority);
    }
    if (eviction_priority_queue->IsEmpty())
      break;

    Tile* tile = eviction_priority_queue->Top();
    if (!other_priority.IsHigherPriorityThan(tile->combined_priority()))
      break;

    *usage -= MemoryUsage::FromTile(tile);
    FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(tile);
    eviction_priority_queue->Pop();
  }
  return eviction_priority_queue;
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
    RasterTilePriorityQueue* raster_priority_queue,
    size_t scheduled_raster_task_limit,
    TileVector* tiles_that_need_to_be_rasterized) {
  TRACE_EVENT_BEGIN0("cc", "TileManager::AssignGpuMemoryToTiles");

  // Maintain the list of released resources that can potentially be re-used
  // or deleted. If this operation becomes expensive too, only do this after
  // some resource(s) was returned. Note that in that case, one also need to
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

  scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue;
  for (; !raster_priority_queue->IsEmpty(); raster_priority_queue->Pop()) {
    Tile* tile = raster_priority_queue->Top();
    TilePriority priority = tile->combined_priority();

    if (TilePriorityViolatesMemoryPolicy(priority)) {
      TRACE_EVENT_INSTANT0(
          "cc", "TileManager::AssignGpuMemory tile violates memory policy",
          TRACE_EVENT_SCOPE_THREAD);
      break;
    }

    // We won't be able to schedule this tile, so break out early.
    if (tiles_that_need_to_be_rasterized->size() >=
        scheduled_raster_task_limit) {
      all_tiles_that_need_to_be_rasterized_are_scheduled_ = false;
      break;
    }

    TileDrawInfo& draw_info = tile->draw_info();
    tile->scheduled_priority_ = schedule_priority++;

    DCHECK_IMPLIES(draw_info.mode() != TileDrawInfo::OOM_MODE,
                   !draw_info.IsReadyToDraw());

    // If the tile already has a raster_task, then the memory used by it is
    // already accounted for in memory_usage. Otherwise, we'll have to acquire
    // more memory to create a raster task.
    MemoryUsage memory_required_by_tile_to_be_scheduled;
    if (!tile->raster_task_.get()) {
      memory_required_by_tile_to_be_scheduled = MemoryUsage::FromConfig(
          tile->desired_texture_size(), tile_task_runner_->GetResourceFormat());
    }

    bool tile_is_needed_now = priority.priority_bin == TilePriority::NOW;

    // This is the memory limit that will be used by this tile. Depending on
    // the tile priority, it will be one of hard_memory_limit or
    // soft_memory_limit.
    MemoryUsage& tile_memory_limit =
        tile_is_needed_now ? hard_memory_limit : soft_memory_limit;

    const MemoryUsage& scheduled_tile_memory_limit =
        tile_memory_limit - memory_required_by_tile_to_be_scheduled;
    eviction_priority_queue =
        FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
            eviction_priority_queue.Pass(), scheduled_tile_memory_limit,
            priority, &memory_usage);
    bool memory_usage_is_within_limit =
        !memory_usage.Exceeds(scheduled_tile_memory_limit);

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
  }

  // Note that we should try and further reduce memory in case the above loop
  // didn't reduce memory. This ensures that we always release as many resources
  // as possible to stay within the memory limit.
  eviction_priority_queue = FreeTileResourcesUntilUsageIsWithinLimit(
      eviction_priority_queue.Pass(), hard_memory_limit, &memory_usage);

  UMA_HISTOGRAM_BOOLEAN("TileManager.ExceededMemoryBudget",
                        !had_enough_memory_to_schedule_tiles_needed_now);
  did_oom_on_last_assign_ = !had_enough_memory_to_schedule_tiles_needed_now;

  memory_stats_from_last_assign_.total_budget_in_bytes =
      global_state_.hard_memory_limit_in_bytes;
  memory_stats_from_last_assign_.total_bytes_used = memory_usage.memory_bytes();
  memory_stats_from_last_assign_.had_enough_memory =
      had_enough_memory_to_schedule_tiles_needed_now;

  TRACE_EVENT_END2("cc", "TileManager::AssignGpuMemoryToTiles",
                   "all_tiles_that_need_to_be_rasterized_are_scheduled",
                   all_tiles_that_need_to_be_rasterized_are_scheduled_,
                   "had_enough_memory_to_schedule_tiles_needed_now",
                   had_enough_memory_to_schedule_tiles_needed_now);
}

void TileManager::FreeResourcesForTile(Tile* tile) {
  TileDrawInfo& draw_info = tile->draw_info();
  if (draw_info.resource_)
    resource_pool_->ReleaseResource(draw_info.resource_.Pass());
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
    TileDrawInfo& draw_info = tile->draw_info();

    DCHECK(draw_info.requires_resource());
    DCHECK(!draw_info.resource_);

    if (!tile->raster_task_.get())
      tile->raster_task_ = CreateRasterTask(tile);

    TaskSetCollection task_sets;
    if (tile->required_for_activation())
      task_sets.set(REQUIRED_FOR_ACTIVATION);
    if (tile->required_for_draw())
      task_sets.set(REQUIRED_FOR_DRAW);
    task_sets.set(ALL);
    raster_queue_.items.push_back(
        TileTaskQueue::Item(tile->raster_task_.get(), task_sets));
  }

  // We must reduce the amount of unused resoruces before calling
  // ScheduleTasks to prevent usage from rising above limits.
  resource_pool_->ReduceResourceUsage();

  // Schedule running of |raster_queue_|. This replaces any previously
  // scheduled tasks and effectively cancels all tasks not present
  // in |raster_queue_|.
  tile_task_runner_->ScheduleTasks(&raster_queue_);

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
      base::Bind(&TileManager::OnImageDecodeTaskCompleted,
                 base::Unretained(this),
                 tile->layer_id(),
                 base::Unretained(pixel_ref))));
}

scoped_refptr<RasterTask> TileManager::CreateRasterTask(Tile* tile) {
  scoped_ptr<ScopedResource> resource =
      resource_pool_->AcquireResource(tile->desired_texture_size(),
                                      tile_task_runner_->GetResourceFormat());
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

  return make_scoped_refptr(new RasterTaskImpl(
      const_resource, tile->raster_source(), tile->content_rect(),
      tile->contents_scale(), tile->combined_priority().resolution,
      tile->layer_id(), static_cast<const void*>(tile),
      tile->source_frame_number(), tile->use_picture_analysis(),
      base::Bind(&TileManager::OnRasterTaskCompleted, base::Unretained(this),
                 tile->id(), base::Passed(&resource)),
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
  DCHECK(tile->raster_task_.get());
  orphan_raster_tasks_.push_back(tile->raster_task_);
  tile->raster_task_ = nullptr;

  if (was_canceled) {
    ++update_visible_tiles_stats_.canceled_count;
    resource_pool_->ReleaseResource(resource.Pass());
    return;
  }

  UpdateTileDrawInfo(tile, resource.Pass(), analysis);
}

void TileManager::UpdateTileDrawInfo(
    Tile* tile,
    scoped_ptr<ScopedResource> resource,
    const RasterSource::SolidColorAnalysis& analysis) {
  TileDrawInfo& draw_info = tile->draw_info();

  ++update_visible_tiles_stats_.completed_count;

  if (analysis.is_solid_color) {
    draw_info.set_solid_color(analysis.solid_color);
    if (resource)
      resource_pool_->ReleaseResource(resource.Pass());
  } else {
    DCHECK(resource);
    draw_info.set_use_resource();
    draw_info.resource_ = resource.Pass();
  }

  client_->NotifyTileStateChanged(tile);
}

scoped_refptr<Tile> TileManager::CreateTile(
    RasterSource* raster_source,
    const gfx::Size& desired_texture_size,
    const gfx::Rect& content_rect,
    float contents_scale,
    int layer_id,
    int source_frame_number,
    int flags) {
  scoped_refptr<Tile> tile = make_scoped_refptr(
      new Tile(this, raster_source, desired_texture_size, content_rect,
               contents_scale, layer_id, source_frame_number, flags));
  DCHECK(tiles_.find(tile->id()) == tiles_.end());

  tiles_[tile->id()] = tile.get();
  used_layer_counts_[tile->layer_id()]++;
  return tile;
}

void TileManager::SetTileTaskRunnerForTesting(
    TileTaskRunner* tile_task_runner) {
  tile_task_runner_ = tile_task_runner;
  tile_task_runner_->SetClient(this);
}

bool TileManager::AreRequiredTilesReadyToDraw(
    RasterTilePriorityQueue::Type type) const {
  scoped_ptr<RasterTilePriorityQueue> raster_priority_queue(
      client_->BuildRasterQueue(global_state_.tree_priority, type));
  // It is insufficient to check whether the raster queue we constructed is
  // empty. The reason for this is that there are situations (rasterize on
  // demand) when the tile both needs raster and it's ready to draw. Hence, we
  // have to iterate the queue to check whether the required tiles are ready to
  // draw.
  for (; !raster_priority_queue->IsEmpty(); raster_priority_queue->Pop()) {
    if (!raster_priority_queue->Top()->IsReadyToDraw())
      return false;
  }
  return true;
}
bool TileManager::IsReadyToActivate() const {
  TRACE_EVENT0("cc", "TileManager::IsReadyToActivate");
  return AreRequiredTilesReadyToDraw(
      RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION);
}

bool TileManager::IsReadyToDraw() const {
  TRACE_EVENT0("cc", "TileManager::IsReadyToDraw");
  return AreRequiredTilesReadyToDraw(
      RasterTilePriorityQueue::Type::REQUIRED_FOR_DRAW);
}

void TileManager::NotifyReadyToActivate() {
  TRACE_EVENT0("cc", "TileManager::NotifyReadyToActivate");
  if (did_notify_ready_to_activate_)
    return;
  client_->NotifyReadyToActivate();
  did_notify_ready_to_activate_ = true;
}

void TileManager::NotifyReadyToDraw() {
  TRACE_EVENT0("cc", "TileManager::NotifyReadyToDraw");
  if (did_notify_ready_to_draw_)
    return;
  client_->NotifyReadyToDraw();
  did_notify_ready_to_draw_ = true;
}

void TileManager::CheckIfReadyToActivate() {
  TRACE_EVENT0("cc", "TileManager::CheckIfReadyToActivate");

  tile_task_runner_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  if (did_notify_ready_to_activate_)
    return;
  if (!IsReadyToActivate())
    return;

  NotifyReadyToActivate();
}

void TileManager::CheckIfReadyToDraw() {
  TRACE_EVENT0("cc", "TileManager::CheckIfReadyToDraw");

  tile_task_runner_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  if (did_notify_ready_to_draw_)
    return;
  if (!IsReadyToDraw())
    return;

  NotifyReadyToDraw();
}

void TileManager::CheckIfMoreTilesNeedToBePrepared() {
  tile_task_runner_->CheckForCompletedTasks();
  did_check_for_completed_tasks_since_last_schedule_tasks_ = true;

  // When OOM, keep re-assigning memory until we reach a steady state
  // where top-priority tiles are initialized.
  TileVector tiles_that_need_to_be_rasterized;
  scoped_ptr<RasterTilePriorityQueue> raster_priority_queue(
      client_->BuildRasterQueue(global_state_.tree_priority,
                                RasterTilePriorityQueue::Type::ALL));
  AssignGpuMemoryToTiles(raster_priority_queue.get(),
                         scheduled_raster_task_limit_,
                         &tiles_that_need_to_be_rasterized);

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
  bool wait_for_all_required_tiles =
      global_state_.tree_priority == SMOOTHNESS_TAKES_PRIORITY ||
      global_state_.memory_limit_policy == ALLOW_NOTHING;

  // Mark any required-for-activation tiles that have not been been assigned
  // memory after reaching a steady memory state as OOM. This ensures that we
  // activate even when OOM. Note that we can't reuse the queue we used for
  // AssignGpuMemoryToTiles, since the AssignGpuMemoryToTiles call could have
  // evicted some tiles that would not be picked up by the old raster queue.
  scoped_ptr<RasterTilePriorityQueue> required_for_activation_queue(
      client_->BuildRasterQueue(
          global_state_.tree_priority,
          RasterTilePriorityQueue::Type::REQUIRED_FOR_ACTIVATION));

  // If we have tiles left to raster for activation, and we don't allow
  // activating without them, then skip activation and return early.
  if (!required_for_activation_queue->IsEmpty() && wait_for_all_required_tiles)
    return;

  // Mark required tiles as OOM so that we can activate without them.
  for (; !required_for_activation_queue->IsEmpty();
       required_for_activation_queue->Pop()) {
    Tile* tile = required_for_activation_queue->Top();
    tile->draw_info().set_oom();
    client_->NotifyTileStateChanged(tile);
  }

  DCHECK(IsReadyToActivate());
  ready_to_activate_check_notifier_.Schedule();
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
  const TileDrawInfo& draw_info = tile->draw_info();
  if (draw_info.resource_) {
    return MemoryUsage::FromConfig(draw_info.resource_->size(),
                                   draw_info.resource_->format());
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
