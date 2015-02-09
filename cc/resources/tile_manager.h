// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_MANAGER_H_
#define CC_RESOURCES_TILE_MANAGER_H_

#include <deque>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/base/unique_notifier.h"
#include "cc/resources/eviction_tile_priority_queue.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/raster_source.h"
#include "cc/resources/raster_tile_priority_queue.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_draw_info.h"
#include "cc/resources/tile_task_runner.h"

namespace base {
namespace trace_event {
class ConvertableToTraceFormat;
class TracedValue;
}
}

namespace cc {
class PictureLayerImpl;
class Rasterizer;
class ResourceProvider;

class CC_EXPORT TileManagerClient {
 public:
  // Called when all tiles marked as required for activation are ready to draw.
  virtual void NotifyReadyToActivate() = 0;

  // Called when all tiles marked as required for draw are ready to draw.
  virtual void NotifyReadyToDraw() = 0;

  // Called when the visible representation of a tile might have changed. Some
  // examples are:
  // - Tile version initialized.
  // - Tile resources freed.
  // - Tile marked for on-demand raster.
  virtual void NotifyTileStateChanged(const Tile* tile) = 0;

  // Given an empty raster tile priority queue, this will build a priority queue
  // that will return tiles in order in which they should be rasterized.
  // Note if the queue was previous built, Reset must be called on it.
  virtual scoped_ptr<RasterTilePriorityQueue> BuildRasterQueue(
      TreePriority tree_priority,
      RasterTilePriorityQueue::Type type) = 0;

  // Given an empty eviction tile priority queue, this will build a priority
  // queue that will return tiles in order in which they should be evicted.
  // Note if the queue was previous built, Reset must be called on it.
  virtual scoped_ptr<EvictionTilePriorityQueue> BuildEvictionQueue(
      TreePriority tree_priority) = 0;

  // Informs the client that due to the currently rasterizing (or scheduled to
  // be rasterized) tiles, we will be in a position that will likely require a
  // draw. This can be used to preemptively start a frame.
  virtual void SetIsLikelyToRequireADraw(bool is_likely_to_require_a_draw) = 0;

 protected:
  virtual ~TileManagerClient() {}
};

struct RasterTaskCompletionStats {
  RasterTaskCompletionStats();

  size_t completed_count;
  size_t canceled_count;
};
scoped_refptr<base::trace_event::ConvertableToTraceFormat>
RasterTaskCompletionStatsAsValue(const RasterTaskCompletionStats& stats);

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class CC_EXPORT TileManager : public TileTaskRunnerClient,
                              public RefCountedManager<Tile> {
 public:
  enum NamedTaskSet {
    REQUIRED_FOR_ACTIVATION,
    REQUIRED_FOR_DRAW,
    // PixelBufferTileTaskWorkerPool depends on ALL being last.
    ALL
    // Adding additional values requires increasing kNumberOfTaskSets in
    // rasterizer.h
  };

  static_assert(NamedTaskSet::ALL == (kNumberOfTaskSets - 1),
                "NamedTaskSet::ALL should be equal to kNumberOfTaskSets"
                "minus 1");

  static scoped_ptr<TileManager> Create(TileManagerClient* client,
                                        base::SequencedTaskRunner* task_runner,
                                        ResourcePool* resource_pool,
                                        TileTaskRunner* tile_task_runner,
                                        Rasterizer* rasterizer,
                                        size_t scheduled_raster_task_limit);
  ~TileManager() override;

  // Assigns tile memory and schedules work to prepare tiles for drawing.
  // - Runs client_->NotifyReadyToActivate() when all tiles required for
  // activation are prepared, or failed to prepare due to OOM.
  // - Runs client_->NotifyReadyToDraw() when all tiles required draw are
  // prepared, or failed to prepare due to OOM.
  void PrepareTiles(const GlobalStateThatImpactsTilePriority& state);

  void UpdateVisibleTiles(const GlobalStateThatImpactsTilePriority& state);

  scoped_refptr<Tile> CreateTile(RasterSource* raster_source,
                                 const gfx::Size& desired_texture_size,
                                 const gfx::Rect& content_rect,
                                 float contents_scale,
                                 int layer_id,
                                 int source_frame_number,
                                 int flags);

  scoped_refptr<base::trace_event::ConvertableToTraceFormat> BasicStateAsValue()
      const;
  void BasicStateAsValueInto(base::trace_event::TracedValue* dict) const;
  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  void InitializeTilesWithResourcesForTesting(const std::vector<Tile*>& tiles) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      TileDrawInfo& draw_info = tiles[i]->draw_info();
      draw_info.resource_ = resource_pool_->AcquireResource(
          tiles[i]->desired_texture_size(),
          tile_task_runner_->GetResourceFormat());
    }
  }

  void ReleaseTileResourcesForTesting(const std::vector<Tile*>& tiles) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      Tile* tile = tiles[i];
      FreeResourcesForTile(tile);
    }
  }

  void SetGlobalStateForTesting(
      const GlobalStateThatImpactsTilePriority& state) {
    global_state_ = state;
  }

  void SetTileTaskRunnerForTesting(TileTaskRunner* tile_task_runner);

  void FreeResourcesAndCleanUpReleasedTilesForTesting() {
    FreeResourcesForReleasedTiles();
    CleanUpReleasedTiles();
  }

  std::vector<Tile*> AllTilesForTesting() const {
    std::vector<Tile*> tiles;
    for (TileMap::const_iterator it = tiles_.begin(); it != tiles_.end();
         ++it) {
      tiles.push_back(it->second);
    }
    return tiles;
  }

  void SetScheduledRasterTaskLimitForTesting(size_t limit) {
    scheduled_raster_task_limit_ = limit;
  }

  bool IsReadyToActivate() const;
  bool IsReadyToDraw() const;

 protected:
  TileManager(TileManagerClient* client,
              const scoped_refptr<base::SequencedTaskRunner>& task_runner,
              ResourcePool* resource_pool,
              TileTaskRunner* tile_task_runner,
              Rasterizer* rasterizer,
              size_t scheduled_raster_task_limit);

  void FreeResourcesForReleasedTiles();
  void CleanUpReleasedTiles();

  // Overriden from RefCountedManager<Tile>:
  friend class Tile;
  void Release(Tile* tile) override;

  // Overriden from TileTaskRunnerClient:
  void DidFinishRunningTileTasks(TaskSet task_set) override;
  TaskSetCollection TasksThatShouldBeForcedToComplete() const override;

  typedef std::vector<Tile*> TileVector;
  typedef std::set<Tile*> TileSet;

  // Virtual for test
  virtual void ScheduleTasks(
      const TileVector& tiles_that_need_to_be_rasterized);

  void AssignGpuMemoryToTiles(RasterTilePriorityQueue* raster_priority_queue,
                              size_t scheduled_raser_task_limit,
                              TileVector* tiles_that_need_to_be_rasterized);

  void SynchronouslyRasterizeTiles(
      const GlobalStateThatImpactsTilePriority& state);

 private:
  class MemoryUsage {
   public:
    MemoryUsage();
    MemoryUsage(int64 memory_bytes, int resource_count);

    static MemoryUsage FromConfig(const gfx::Size& size, ResourceFormat format);
    static MemoryUsage FromTile(const Tile* tile);

    MemoryUsage& operator+=(const MemoryUsage& other);
    MemoryUsage& operator-=(const MemoryUsage& other);
    MemoryUsage operator-(const MemoryUsage& other);

    bool Exceeds(const MemoryUsage& limit) const;
    int64 memory_bytes() const { return memory_bytes_; }

   private:
    int64 memory_bytes_;
    int resource_count_;
  };

  void OnImageDecodeTaskCompleted(int layer_id,
                                  SkPixelRef* pixel_ref,
                                  bool was_canceled);
  void OnRasterTaskCompleted(Tile::Id tile,
                             scoped_ptr<ScopedResource> resource,
                             const RasterSource::SolidColorAnalysis& analysis,
                             bool was_canceled);
  void UpdateTileDrawInfo(Tile* tile,
                          scoped_ptr<ScopedResource> resource,
                          const RasterSource::SolidColorAnalysis& analysis);

  void FreeResourcesForTile(Tile* tile);
  void FreeResourcesForTileAndNotifyClientIfTileWasReadyToDraw(Tile* tile);
  scoped_refptr<ImageDecodeTask> CreateImageDecodeTask(Tile* tile,
                                                       SkPixelRef* pixel_ref);
  scoped_refptr<RasterTask> CreateRasterTask(Tile* tile);

  scoped_ptr<EvictionTilePriorityQueue>
  FreeTileResourcesUntilUsageIsWithinLimit(
      scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
      const MemoryUsage& limit,
      MemoryUsage* usage);
  scoped_ptr<EvictionTilePriorityQueue>
  FreeTileResourcesWithLowerPriorityUntilUsageIsWithinLimit(
      scoped_ptr<EvictionTilePriorityQueue> eviction_priority_queue,
      const MemoryUsage& limit,
      const TilePriority& oother_priority,
      MemoryUsage* usage);
  bool TilePriorityViolatesMemoryPolicy(const TilePriority& priority);
  bool AreRequiredTilesReadyToDraw(RasterTilePriorityQueue::Type type) const;
  void NotifyReadyToActivate();
  void NotifyReadyToDraw();
  void CheckIfReadyToActivate();
  void CheckIfReadyToDraw();
  void CheckIfMoreTilesNeedToBePrepared();

  TileManagerClient* client_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ResourcePool* resource_pool_;
  TileTaskRunner* tile_task_runner_;
  Rasterizer* rasterizer_;
  GlobalStateThatImpactsTilePriority global_state_;
  size_t scheduled_raster_task_limit_;

  typedef base::hash_map<Tile::Id, Tile*> TileMap;
  TileMap tiles_;

  bool all_tiles_that_need_to_be_rasterized_are_scheduled_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  bool did_check_for_completed_tasks_since_last_schedule_tasks_;
  bool did_oom_on_last_assign_;

  typedef base::hash_map<uint32_t, scoped_refptr<ImageDecodeTask>>
      PixelRefTaskMap;
  typedef base::hash_map<int, PixelRefTaskMap> LayerPixelRefTaskMap;
  LayerPixelRefTaskMap image_decode_tasks_;

  typedef base::hash_map<int, int> LayerCountMap;
  LayerCountMap used_layer_counts_;

  RasterTaskCompletionStats update_visible_tiles_stats_;

  std::vector<Tile*> released_tiles_;

  // Queue used when scheduling raster tasks.
  TileTaskQueue raster_queue_;

  std::vector<scoped_refptr<RasterTask>> orphan_raster_tasks_;

  UniqueNotifier ready_to_activate_notifier_;
  UniqueNotifier ready_to_draw_notifier_;
  UniqueNotifier ready_to_activate_check_notifier_;
  UniqueNotifier ready_to_draw_check_notifier_;
  UniqueNotifier more_tiles_need_prepare_check_notifier_;

  bool did_notify_ready_to_activate_;
  bool did_notify_ready_to_draw_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
