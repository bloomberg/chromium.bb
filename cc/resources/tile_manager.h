// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_MANAGER_H_
#define CC_RESOURCES_TILE_MANAGER_H_

#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/base/ref_counted_managed.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/resources/managed_tile_state.h"
#include "cc/resources/memory_history.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/prioritized_tile_set.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile.h"

namespace cc {
class RasterWorkerPoolDelegate;
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
  struct CC_EXPORT PairedPictureLayer {
    PairedPictureLayer();
    ~PairedPictureLayer();

    PictureLayerImpl* active_layer;
    PictureLayerImpl* pending_layer;
  };

  class CC_EXPORT RasterTileIterator {
   public:
    RasterTileIterator(TileManager* tile_manager, TreePriority tree_priority);
    ~RasterTileIterator();

    RasterTileIterator& operator++();
    operator bool() const;
    Tile* operator*();

   private:
    struct PairedPictureLayerIterator {
      PairedPictureLayerIterator();
      ~PairedPictureLayerIterator();

      Tile* PeekTile(TreePriority tree_priority);
      void PopTile(TreePriority tree_priority);

      std::pair<PictureLayerImpl::LayerRasterTileIterator*, WhichTree>
          NextTileIterator(TreePriority tree_priority);

      PictureLayerImpl::LayerRasterTileIterator active_iterator;
      PictureLayerImpl::LayerRasterTileIterator pending_iterator;

      std::vector<Tile*> returned_shared_tiles;
    };

    class RasterOrderComparator {
     public:
      explicit RasterOrderComparator(TreePriority tree_priority);

      bool operator()(PairedPictureLayerIterator* a,
                      PairedPictureLayerIterator* b) const;

     private:
      bool ComparePriorities(const TilePriority& a_priority,
                             const TilePriority& b_priority,
                             bool prioritize_low_res) const;
      TreePriority tree_priority_;
    };

    std::vector<PairedPictureLayerIterator> paired_iterators_;
    std::vector<PairedPictureLayerIterator*> iterator_heap_;
    TreePriority tree_priority_;
    RasterOrderComparator comparator_;
  };

  static scoped_ptr<TileManager> Create(
      TileManagerClient* client,
      base::SequencedTaskRunner* task_runner,
      ResourceProvider* resource_provider,
      ContextProvider* context_provider,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      bool use_map_image,
      bool use_rasterize_on_demand,
      size_t max_transfer_buffer_usage_bytes,
      size_t max_raster_usage_bytes,
      unsigned map_image_texture_target);
  virtual ~TileManager();

  void ManageTiles(const GlobalStateThatImpactsTilePriority& state);

  // Returns true when visible tiles have been initialized.
  bool UpdateVisibleTiles();

  scoped_refptr<Tile> CreateTile(PicturePileImpl* picture_pile,
                                 const gfx::Size& tile_size,
                                 const gfx::Rect& content_rect,
                                 const gfx::Rect& opaque_rect,
                                 float contents_scale,
                                 int layer_id,
                                 int source_frame_number,
                                 int flags);

  void RegisterPictureLayerImpl(PictureLayerImpl* layer);
  void UnregisterPictureLayerImpl(PictureLayerImpl* layer);

  scoped_ptr<base::Value> BasicStateAsValue() const;
  scoped_ptr<base::Value> AllTilesAsValue() const;
  void GetMemoryStats(size_t* memory_required_bytes,
                      size_t* memory_nice_to_have_bytes,
                      size_t* memory_allocated_bytes,
                      size_t* memory_used_bytes) const;

  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  void GetPairedPictureLayers(std::vector<PairedPictureLayer>* layers) const;

  ResourcePool* resource_pool() { return resource_pool_.get(); }

  void InitializeTilesWithResourcesForTesting(const std::vector<Tile*>& tiles) {
    for (size_t i = 0; i < tiles.size(); ++i) {
      ManagedTileState& mts = tiles[i]->managed_state();
      ManagedTileState::TileVersion& tile_version =
          mts.tile_versions[HIGH_QUALITY_NO_LCD_RASTER_MODE];

      tile_version.resource_ = resource_pool_->AcquireResource(gfx::Size(1, 1));

      bytes_releasable_ += BytesConsumedIfAllocated(tiles[i]);
      ++resources_releasable_;
    }
  }

  void SetGlobalStateForTesting(
      const GlobalStateThatImpactsTilePriority& state) {
    // Soft limit is used for resource pool such that
    // memory returns to soft limit after going over.
    if (state != global_state_) {
      global_state_ = state;
      prioritized_tiles_dirty_ = true;
      resource_pool_->SetResourceUsageLimits(
          global_state_.soft_memory_limit_in_bytes,
          global_state_.unused_memory_limit_in_bytes,
          global_state_.num_resources_limit);
    }
  }

 protected:
  TileManager(TileManagerClient* client,
              base::SequencedTaskRunner* task_runner,
              ResourceProvider* resource_provider,
              ContextProvider* context_provider,
              scoped_ptr<RasterWorkerPool> raster_worker_pool,
              scoped_ptr<RasterWorkerPool> direct_raster_worker_pool,
              size_t max_raster_usage_bytes,
              RenderingStatsInstrumentation* rendering_stats_instrumentation,
              bool use_rasterize_on_demand);

  // Methods called by Tile
  friend class Tile;
  void DidChangeTilePriority(Tile* tile);

  void CleanUpReleasedTiles();

  // Overriden from RefCountedManager<Tile>:
  virtual void Release(Tile* tile) OVERRIDE;

  // Overriden from RasterWorkerPoolClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const OVERRIDE;
  virtual void DidFinishRunningTasks() OVERRIDE;
  virtual void DidFinishRunningTasksRequiredForActivation() OVERRIDE;

  typedef std::vector<Tile*> TileVector;
  typedef std::set<Tile*> TileSet;

  // Virtual for test
  virtual void ScheduleTasks(
      const TileVector& tiles_that_need_to_be_rasterized);

  void AssignGpuMemoryToTiles(PrioritizedTileSet* tiles,
                              TileVector* tiles_that_need_to_be_rasterized);
  void GetTilesWithAssignedBins(PrioritizedTileSet* tiles);

 private:
  enum RasterWorkerPoolType {
    RASTER_WORKER_POOL_TYPE_DEFAULT,
    RASTER_WORKER_POOL_TYPE_DIRECT,
    NUM_RASTER_WORKER_POOL_TYPES
  };

  void OnImageDecodeTaskCompleted(int layer_id,
                                  SkPixelRef* pixel_ref,
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

  void FreeResourceForTile(Tile* tile, RasterMode mode);
  void FreeResourcesForTile(Tile* tile);
  void FreeUnusedResourcesForTile(Tile* tile);
  scoped_refptr<internal::WorkerPoolTask> CreateImageDecodeTask(
      Tile* tile,
      SkPixelRef* pixel_ref);
  scoped_refptr<internal::RasterWorkerPoolTask> CreateRasterTask(Tile* tile);
  scoped_ptr<base::Value> GetMemoryRequirementsAsValue() const;
  void UpdatePrioritizedTileSetIfNeeded();

  TileManagerClient* client_;
  ContextProvider* context_provider_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  scoped_ptr<RasterWorkerPool> direct_raster_worker_pool_;
  scoped_ptr<RasterWorkerPoolDelegate> raster_worker_pool_delegate_;
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

  typedef base::hash_map<uint32_t, scoped_refptr<internal::WorkerPoolTask> >
      PixelRefTaskMap;
  typedef base::hash_map<int, PixelRefTaskMap> LayerPixelRefTaskMap;
  LayerPixelRefTaskMap image_decode_tasks_;

  typedef base::hash_map<int, int> LayerCountMap;
  LayerCountMap used_layer_counts_;

  RasterTaskCompletionStats update_visible_tiles_stats_;

  std::vector<Tile*> released_tiles_;

  bool use_rasterize_on_demand_;

  // Queues used when scheduling raster tasks.
  RasterTaskQueue raster_queue_[NUM_RASTER_WORKER_POOL_TYPES];

  std::vector<scoped_refptr<internal::Task> > orphan_raster_tasks_;

  std::vector<PictureLayerImpl*> layers_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
