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
#include "cc/resources/memory_history.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/tile_priority.h"

namespace cc {
class ResourceProvider;
class Tile;
class TileVersion;

class CC_EXPORT TileManagerClient {
 public:
  virtual void DidInitializeVisibleTile() = 0;
  virtual void NotifyReadyToActivate() = 0;

 protected:
  virtual ~TileManagerClient() {}
};

// Tile manager classifying tiles into a few basic
// bins:
enum TileManagerBin {
  NOW_BIN = 0,  // Needed ASAP.
  SOON_BIN = 1,  // Impl-side version of prepainting.
  EVENTUALLY_BIN = 2,  // Nice to have, if we've got memory and time.
  NEVER_BIN = 3,  // Dont bother.
  NUM_BINS = 4
  // Be sure to update TileManagerBinAsValue when adding new fields.
};
scoped_ptr<base::Value> TileManagerBinAsValue(
    TileManagerBin bin);

enum TileManagerBinPriority {
  HIGH_PRIORITY_BIN = 0,
  LOW_PRIORITY_BIN = 1,
  NUM_BIN_PRIORITIES = 2
};
scoped_ptr<base::Value> TileManagerBinPriorityAsValue(
    TileManagerBinPriority bin);

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
      bool use_color_estimator,
      RenderingStatsInstrumentation* rendering_stats_instrumentation,
      bool use_map_image);
  virtual ~TileManager();

  const GlobalStateThatImpactsTilePriority& GlobalState() const {
      return global_state_;
  }
  void SetGlobalState(const GlobalStateThatImpactsTilePriority& state);

  void ManageTiles();
  void CheckForCompletedTileUploads();

  scoped_ptr<base::Value> BasicStateAsValue() const;
  scoped_ptr<base::Value> AllTilesAsValue() const;
  void GetMemoryStats(size_t* memory_required_bytes,
                      size_t* memory_nice_to_have_bytes,
                      size_t* memory_used_bytes) const;

  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  bool AreTilesRequiredForActivationReady() const {
    return tiles_that_need_to_be_initialized_for_activation_.empty();
  }

 protected:
  TileManager(TileManagerClient* client,
              ResourceProvider* resource_provider,
              scoped_ptr<RasterWorkerPool> raster_worker_pool,
              size_t num_raster_threads,
              bool use_color_estimator,
              RenderingStatsInstrumentation* rendering_stats_instrumentation,
              GLenum texture_format);

  // Methods called by Tile
  friend class Tile;
  void RegisterTile(Tile* tile);
  void UnregisterTile(Tile* tile);

  // Overriden from RasterWorkerPoolClient:
  virtual bool ShouldForceTasksRequiredForActivationToComplete() const
      OVERRIDE;

  // Virtual for test
  virtual void ScheduleTasks();

 private:
  void OnImageDecodeTaskCompleted(
      int layer_id,
      skia::LazyPixelRef* pixel_ref,
      bool was_canceled);
  void OnRasterTaskCompleted(
      scoped_refptr<Tile> tile,
      scoped_ptr<ResourcePool::Resource> resource,
      RasterMode raster_mode,
      const PicturePileImpl::Analysis& analysis,
      bool was_canceled);

  void AssignBinsToTiles();
  void SortTiles();
  RasterMode DetermineRasterMode(const Tile* tile) const;
  void CleanUpUnusedImageDecodeTasks();
  void AssignGpuMemoryToTiles();
  void FreeResourceForTile(Tile* tile, RasterMode mode);
  void FreeResourcesForTile(Tile* tile);
  void FreeUnusedResourcesForTile(Tile* tile);
  RasterWorkerPool::Task CreateImageDecodeTask(
      Tile* tile, skia::LazyPixelRef* pixel_ref);
  RasterTaskMetadata GetRasterTaskMetadata(const Tile& tile) const;
  RasterWorkerPool::RasterTask CreateRasterTask(Tile* tile);
  void DidFinishTileInitialization(Tile* tile);
  void DidTileTreeBinChange(Tile* tile,
                            TileManagerBin new_tree_bin,
                            WhichTree tree);
  scoped_ptr<base::Value> GetMemoryRequirementsAsValue() const;
  void AddRequiredTileForActivation(Tile* tile);

  TileManagerClient* client_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  GlobalStateThatImpactsTilePriority global_state_;

  typedef std::vector<Tile*> TileVector;
  TileVector tiles_;
  TileVector tiles_that_need_to_be_rasterized_;
  typedef std::set<Tile*> TileSet;
  TileSet tiles_that_need_to_be_initialized_for_activation_;

  bool ever_exceeded_memory_budget_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  RenderingStatsInstrumentation* rendering_stats_instrumentation_;

  bool use_color_estimator_;
  bool did_initialize_visible_tile_;

  GLenum texture_format_;

  typedef base::hash_map<uint32_t, RasterWorkerPool::Task> PixelRefTaskMap;
  typedef base::hash_map<int, PixelRefTaskMap> LayerPixelRefTaskMap;
  LayerPixelRefTaskMap image_decode_tasks_;

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_MANAGER_H_
