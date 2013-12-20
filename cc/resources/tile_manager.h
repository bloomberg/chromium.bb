// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_MANAGER_H_
#define CC_RESOURCES_TILE_MANAGER_H_

#include <queue>
#include <set>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/resources/managed_tile_state.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/prioritized_tile_set.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"

namespace cc {
class ResourceProvider;

class CC_EXPORT TileManagerClient {
 public:
  virtual void NotifyReadyToActivate() = 0;

 protected:
  virtual ~TileManagerClient() {}
};

struct RasterTaskCompletionStats {
  RasterTaskCompletionStats();

  size_t completed_count;
  size_t canceled_count;
};
scoped_ptr<base::Value> RasterTaskCompletionStatsAsValue(
    const RasterTaskCompletionStats& stats);

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class CC_EXPORT TileManager : public RasterWorkerPoolClient,
                              public RefCountedManager<Tile> {
 public:
  static scoped_ptr<TileManager> Create(
      TileManagerClient* client,
      ResourceProvider* resource_provider,
      size_t num_raster_threads,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      bool use_map_image,
      size_t max_transfer_buffer_usage_bytes,
      size_t max_raster_usage_bytes,
      GLenum map_image_texture_target);
  virtual ~TileManager();

  void ManageTiles(const GlobalStateThatImpactsTilePriority& state);

  // Returns true when visible tiles have been initialized.
  bool UpdateVisibleTiles();

  scoped_refptr<Tile> CreateTile(PicturePileImpl* picture_pile,
                                 gfx::Size tile_size,
                                 gfx::Rect content_rect,
                                 gfx::Rect opaque_rect,
                                 float contents_scale,
                                 int layer_id,
                                 int source_frame_number,
                                 int flags);

  scoped_ptr<base::Value> BasicStateAsValue() const;
  scoped_ptr<base::Value> AllTilesAsValue() const;
  void GetMemoryStats(size_t* memory_required_bytes,
                      size_t* memory_nice_to_have_bytes,
                      size_t* memory_allocated_bytes,
                      size_t* memory_used_bytes) const;

  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  void InitializeTilesWithResourcesForTesting(
      const std::vector<Tile*>& tiles,
      ResourceProvider* resource_provider) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      ManagedTileState& mts = tiles[i]->managed_state();
      ManagedTileState::TileVersion& tile_version =
          mts.tile_versions[HIGH_QUALITY_NO_LCD_RASTER_MODE];

      tile_version.resource_ = resource_pool_->AcquireResource(
          gfx::Size(1, 1));

      bytes_releasable_ += BytesConsumedIfAllocated(tiles[i]);
      ++resources_releasable_;
    }
  }
  RasterWorkerPool* RasterWorkerPoolForTesting() {
    return raster_worker_pool_.get();
  }

  void SetGlobalStateForTesting(
      const GlobalStateThatImpactsTilePriority& state) {
    if (state != global_state_) {
      global_state_ = state;
      prioritized_tiles_dirty_ = true;
      resource_pool_->SetResourceUsageLimits(
          global_state_.memory_limit_in_bytes,
          global_state_.unused_memory_limit_in_bytes,
          global_state_.num_resources_limit);
    }
  }

 protected:
  TileManager(TileManagerClient* client,
              ResourceProvider* resource_provider,
              scoped_ptr<RasterWorkerPool> raster_worker_pool,
              size_t num_raster_threads,
              size_t max_raster_usage_bytes,
              RenderingStatsInstrumentation* rendering_stats_instrumentation);

  // Methods called by Tile
  friend class Tile;
  void DidChangeTilePriority(Tile* tile);

  void CleanUpReleasedTiles();

  // Overriden from RefCountedManager<Tile>:
  virtual void Release(Tile* tile) OVERRIDE;

  // Overriden from RasterWorkerPoolClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const
      OVERRIDE;
  virtual void DidFinishRunningTasks() OVERRIDE;
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE;

  typedef std::vector<Tile*> TileVector;
  typedef std::set<Tile*> TileSet;

  // Virtual for test
  virtual void ScheduleTasks(
      const TileVector& tiles_that_need_to_be_rasterized);

  void AssignGpuMemoryToTiles(
      PrioritizedTileSet* tiles,
      TileVector* tiles_that_need_to_be_rasterized);
  void GetTilesWithAssignedBins(PrioritizedTileSet* tiles);

 private:
  void OnImageDecodeTaskCompleted(
      int layer_id,
      skia::LazyPixelRef* pixel_ref,
      bool was_canceled);
  void OnRasterTaskCompleted(Tile::Id tile,
                             scoped_ptr<ScopedResource> resource,
                             RasterMode raster_mode,
                             const PicturePileImpl::Analysis& analysis,
                             bool was_canceled);

  inline size_t BytesConsumedIfAllocated(const Tile* tile) const {
    return Resource::MemorySizeBytes(tile->size(),
                                     raster_worker_pool_->GetResourceFormat());
  }

  RasterMode DetermineRasterMode(const Tile* tile) const;
  void FreeResourceForTile(Tile* tile, RasterMode mode);
  void FreeResourcesForTile(Tile* tile);
  void FreeUnusedResourcesForTile(Tile* tile);
  RasterWorkerPool::Task CreateImageDecodeTask(
      Tile* tile, skia::LazyPixelRef* pixel_ref);
  RasterWorkerPool::RasterTask CreateRasterTask(Tile* tile);
  scoped_ptr<base::Value> GetMemoryRequirementsAsValue() const;
  void UpdatePrioritizedTileSetIfNeeded();

  TileManagerClient* client_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  GlobalStateThatImpactsTilePriority global_state_;

  typedef base::hash_map<Tile::Id, Tile*> TileMap;
  TileMap tiles_;

  PrioritizedTileSet prioritized_tiles_;
  bool prioritized_tiles_dirty_;

  bool all_tiles_that_need_to_be_rasterized_have_memory_;
  bool all_tiles_required_for_activation_have_memory_;

  size_t memory_required_bytes_;
  size_t memory_nice_to_have_bytes_;

  size_t bytes_releasable_;
  size_t resources_releasable_;
  size_t max_raster_usage_bytes_;

  bool ever_exceeded_memory_budget_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  bool did_initialize_visible_tile_;
  bool did_check_for_completed_tasks_since_last_schedule_tasks_;

  typedef base::hash_map<uint32_t, RasterWorkerPool::Task> PixelRefTaskMap;
  typedef base::hash_map<int, PixelRefTaskMap> LayerPixelRefTaskMap;
  LayerPixelRefTaskMap image_decode_tasks_;

  typedef base::hash_map<int, int> LayerCountMap;
  LayerCountMap used_layer_counts_;

  RasterTaskCompletionStats update_visible_tiles_stats_;

  std::vector<Tile*> released_tiles_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
