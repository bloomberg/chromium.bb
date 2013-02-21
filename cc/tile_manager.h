// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILE_MANAGER_H_
#define CC_TILE_MANAGER_H_

#include <list>
#include <queue>
#include <set>
#include <vector>

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "cc/memory_history.h"
#include "cc/rendering_stats.h"
#include "cc/resource_pool.h"
#include "cc/tile_priority.h"
#include "cc/worker_pool.h"

namespace cc {
class RasterWorkerPool;
class ResourceProvider;
class Tile;
class TileVersion;

class CC_EXPORT TileManagerClient {
 public:
  virtual void ScheduleManageTiles() = 0;
  virtual void DidUploadVisibleHighResolutionTile() = 0;

 protected:
  virtual ~TileManagerClient() {}
};

// Tile manager classifying tiles into a few basic
// bins:
enum TileManagerBin {
  NOW_BIN = 0, // Needed ASAP.
  SOON_BIN = 1, // Impl-side version of prepainting.
  EVENTUALLY_BIN = 2, // Nice to have, if we've got memory and time.
  NEVER_BIN = 3, // Dont bother.
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

enum TileRasterState {
  IDLE_STATE = 0,
  WAITING_FOR_RASTER_STATE = 1,
  RASTER_STATE = 2,
  SET_PIXELS_STATE = 3,
  NUM_STATES = 4
};
scoped_ptr<base::Value> TileRasterStateAsValue(
    TileRasterState bin);

// This is state that is specific to a tile that is
// managed by the TileManager.
class CC_EXPORT ManagedTileState {
 public:
  ManagedTileState();
  ~ManagedTileState();
  scoped_ptr<base::Value> AsValue() const;

  // Persisted state: valid all the time.
  bool can_use_gpu_memory;
  bool can_be_freed;
  scoped_ptr<ResourcePool::Resource> resource;
  bool resource_is_being_initialized;
  bool contents_swizzled;
  bool need_to_gather_pixel_refs;
  std::list<skia::LazyPixelRef*> pending_pixel_refs;
  TileRasterState raster_state;

  // Ephemeral state, valid only during Manage.
  TileManagerBin bin[NUM_BIN_PRIORITIES];
  TileManagerBin tree_bin[NUM_TREES];
  // The bin that the tile would have if the GPU memory manager had a maximally permissive policy,
  // send to the GPU memory manager to determine policy.
  TileManagerBin gpu_memmgr_stats_bin;
  TileResolution resolution;
  float time_to_needed_in_seconds;
  float distance_to_visible_in_pixels;
};

// This class manages tiles, deciding which should get rasterized and which
// should no longer have any memory assigned to them. Tile objects are "owned"
// by layers; they automatically register with the manager when they are
// created, and unregister from the manager when they are deleted.
class CC_EXPORT TileManager : public WorkerPoolClient {
 public:
  TileManager(TileManagerClient* client,
              ResourceProvider *resource_provider,
              size_t num_raster_threads,
              bool use_cheapess_estimator);
  virtual ~TileManager();

  const GlobalStateThatImpactsTilePriority& GlobalState() const {
      return global_state_;
  }
  void SetGlobalState(const GlobalStateThatImpactsTilePriority& state);

  void ManageTiles();
  void CheckForCompletedTileUploads();
  void AbortPendingTileUploads();
  void DidCompleteFrame();

  scoped_ptr<base::Value> BasicStateAsValue() const;
  scoped_ptr<base::Value> AllTilesAsValue() const;
  void GetMemoryStats(size_t* memoryRequiredBytes,
                      size_t* memoryNiceToHaveBytes,
                      size_t* memoryUsedBytes) const;
  void SetRecordRenderingStats(bool record_rendering_stats);
  void GetRenderingStats(RenderingStats* stats);
  bool HasPendingWorkScheduled(WhichTree tree) const;

  const MemoryHistory::Entry& memory_stats_from_last_assign() const {
    return memory_stats_from_last_assign_;
  }

  // Overridden from WorkerPoolClient:
  virtual void DidFinishDispatchingWorkerPoolCompletionCallbacks() OVERRIDE;

 protected:
  // Methods called by Tile
  friend class Tile;
  void RegisterTile(Tile* tile);
  void UnregisterTile(Tile* tile);
  void WillModifyTilePriority(
      Tile* tile, WhichTree tree, const TilePriority& new_priority) {
    // TODO(nduca): Do something smarter if reprioritization turns out to be
    // costly.
    ScheduleManageTiles();
  }

 private:

  // Data that is passed to raster tasks.
  struct RasterTaskMetadata {
      bool use_cheapness_estimator;
      bool is_tile_in_pending_tree_now_bin;
      TileResolution tile_resolution;
  };

  RasterTaskMetadata GetRasterTaskMetadata(const Tile& tile) const;
  void SortTiles();
  void AssignGpuMemoryToTiles();
  void FreeResourcesForTile(Tile* tile);
  void ScheduleManageTiles() {
    if (manage_tiles_pending_)
      return;
    client_->ScheduleManageTiles();
    manage_tiles_pending_ = true;
  }
  void DispatchMoreTasks();
  void GatherPixelRefsForTile(Tile* tile);
  void DispatchImageDecodeTasksForTile(Tile* tile);
  void DispatchOneImageDecodeTask(
      scoped_refptr<Tile> tile, skia::LazyPixelRef* pixel_ref);
  void OnImageDecodeTaskCompleted(
      scoped_refptr<Tile> tile,
      uint32_t pixel_ref_id);
  bool CanDispatchRasterTask(Tile* tile) const;
  scoped_ptr<ResourcePool::Resource> PrepareTileForRaster(Tile* tile);
  void DispatchOneRasterTask(scoped_refptr<Tile> tile);
  void OnRasterTaskCompleted(
      scoped_refptr<Tile> tile,
      scoped_ptr<ResourcePool::Resource> resource,
      int manage_tiles_call_count_when_dispatched);
  void DidFinishTileInitialization(Tile* tile);
  void DidTileRasterStateChange(Tile* tile, TileRasterState state);
  void DidTileTreeBinChange(Tile* tile,
                            TileManagerBin new_tree_bin,
                            WhichTree tree);
  scoped_ptr<Value> GetMemoryRequirementsAsValue() const;

  static void RunRasterTask(uint8* buffer,
                            const gfx::Rect& rect,
                            float contents_scale,
                            const RasterTaskMetadata& metadata,
                            PicturePileImpl* picture_pile,
                            RenderingStats* stats);
  static void RunImageDecodeTask(skia::LazyPixelRef* pixel_ref,
                                 RenderingStats* stats);

  static void RecordCheapnessPredictorResults(bool is_predicted_cheap,
                                              bool is_actually_cheap);

  TileManagerClient* client_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<RasterWorkerPool> raster_worker_pool_;
  bool manage_tiles_pending_;
  int manage_tiles_call_count_;

  GlobalStateThatImpactsTilePriority global_state_;

  typedef std::vector<Tile*> TileVector;
  typedef std::set<Tile*> TileSet;
  TileSet all_tiles_;
  TileVector live_or_allocated_tiles_;
  TileVector tiles_that_need_to_be_rasterized_;

  typedef std::list<Tile*> TileList;
  // Tiles with image decoding tasks. These tiles need to be rasterized
  // when all the image decoding tasks finish.
  TileList tiles_with_image_decoding_tasks_;

  typedef base::hash_map<uint32_t, skia::LazyPixelRef*> PixelRefMap;
  PixelRefMap pending_decode_tasks_;

  typedef std::queue<scoped_refptr<Tile> > TileQueue;
  TileQueue tiles_with_pending_set_pixels_;
  size_t bytes_pending_set_pixels_;
  bool has_performed_uploads_since_last_flush_;
  bool ever_exceeded_memory_budget_;
  MemoryHistory::Entry memory_stats_from_last_assign_;

  bool record_rendering_stats_;
  RenderingStats rendering_stats_;

  bool use_cheapness_estimator_;
  bool did_schedule_cheap_tasks_;
  bool allow_cheap_tasks_;
  int raster_state_count_[NUM_STATES][NUM_TREES][NUM_BINS];

  DISALLOW_COPY_AND_ASSIGN(TileManager);
};

}  // namespace cc

#endif  // CC_TILE_MANAGER_H_
