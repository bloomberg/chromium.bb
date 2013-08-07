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
class CC_EXPORT TileManager : public RasterWorkerPoolClient {
 public:
  static scoped_ptr<TileManager> Create(
      TileManagerClient* client,
      ResourceProvider* resource_provider,
      size_t num_raster_threads,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      bool use_map_image);
  virtual ~TileManager();

  const GlobalStateThatImpactsTilePriority& GlobalState() const {
      return global_state_;
  }
  void SetGlobalState(const GlobalStateThatImpactsTilePriority& state);

  void ManageTiles();

  // Returns true when visible tiles have been initialized.
  bool UpdateVisibleTiles();

  scoped_ptr<base::Value> BasicStateAsValue() const;
  scoped_ptr<base::Value> AllTilesAsValue() const;
  void GetMemoryStats(size_t* memory_required_bytes,
                      size_t* memory_nice_to_have_bytes,
                      size_t* memory_used_bytes) const;

  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  bool AreTilesRequiredForActivationReady() const {
    return all_tiles_required_for_activation_have_been_initialized_;
  }

 protected:
  TileManager(TileManagerClient* client,
              ResourceProvider* resource_provider,
              scoped_ptr<RasterWorkerPool> raster_worker_pool,
              size_t num_raster_threads,
              RenderingStatsInstrumentation* rendering_stats_instrumentation,
              GLenum texture_format);

  // Methods called by Tile
  friend class Tile;
  void RegisterTile(Tile* tile);
  void UnregisterTile(Tile* tile);

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
  void GetPrioritizedTileSet(PrioritizedTileSet* tiles);

 private:
  void OnImageDecodeTaskCompleted(
      int layer_id,
      skia::LazyPixelRef* pixel_ref,
      bool was_canceled);
  void OnRasterTaskCompleted(
      Tile::Id tile,
      scoped_ptr<ResourcePool::Resource> resource,
      RasterMode raster_mode,
      const PicturePileImpl::Analysis& analysis,
      bool was_canceled);

  RasterMode DetermineRasterMode(const Tile* tile) const;
  void CleanUpUnusedImageDecodeTasks();
  void FreeResourceForTile(Tile* tile, RasterMode mode);
  void FreeResourcesForTile(Tile* tile);
  void FreeUnusedResourcesForTile(Tile* tile);
  RasterWorkerPool::Task CreateImageDecodeTask(
      Tile* tile, skia::LazyPixelRef* pixel_ref);
  RasterWorkerPool::RasterTask CreateRasterTask(Tile* tile);
  scoped_ptr<base::Value> GetMemoryRequirementsAsValue() const;

  TileManagerClient* client_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  GlobalStateThatImpactsTilePriority global_state_;

  typedef base::hash_map<Tile::Id, Tile*> TileMap;
  TileMap tiles_;

  PrioritizedTileSet prioritized_tiles_;

  bool all_tiles_that_need_to_be_rasterized_have_memory_;
  bool all_tiles_required_for_activation_have_memory_;
  bool all_tiles_required_for_activation_have_been_initialized_;

  bool ever_exceeded_memory_budget_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  bool did_initialize_visible_tile_;

  GLenum texture_format_;

  typedef base::hash_map<uint32_t, RasterWorkerPool::Task> PixelRefTaskMap;
  typedef base::hash_map<int, PixelRefTaskMap> LayerPixelRefTaskMap;
  LayerPixelRefTaskMap image_decode_tasks_;

  RasterTaskCompletionStats update_visible_tiles_stats_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
