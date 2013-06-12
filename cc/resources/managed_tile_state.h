// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_MANAGED_TILE_STATE_H_
#define CC_RESOURCES_MANAGED_TILE_STATE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/tile_manager.h"

namespace cc {

// This is state that is specific to a tile that is
// managed by the TileManager.
class CC_EXPORT ManagedTileState {
 public:
  class CC_EXPORT TileVersion {
    public:
      enum Mode {
        RESOURCE_MODE,
        SOLID_COLOR_MODE,
        PICTURE_PILE_MODE,
        NUM_MODES
      };

      TileVersion();
      ~TileVersion();

      Mode mode() const {
        return mode_;
      }

      bool IsReadyToDraw() const;

      ResourceProvider::ResourceId get_resource_id() const {
        DCHECK(mode_ == RESOURCE_MODE);

        // We have to have a resource ID here.
        DCHECK(resource_id_);
        // If we have a resource, it implies IDs are equal.
        DCHECK(!resource_ || (resource_id_ == resource_->id()));
        // If we don't have a resource, it implies that we're in forced upload.
        DCHECK(resource_ || (resource_id_ && forced_upload_));

        return resource_id_;
      }

      SkColor get_solid_color() const {
        DCHECK(mode_ == SOLID_COLOR_MODE);

        return solid_color_;
      }

      bool contents_swizzled() const {
        return !PlatformColor::SameComponentOrder(resource_format_);
      }

      bool requires_resource() const {
        return mode_ == RESOURCE_MODE ||
               mode_ == PICTURE_PILE_MODE;
      }

      size_t GPUMemoryUsageInBytes() const;

      void SetResourceForTesting(scoped_ptr<ResourcePool::Resource> resource) {
        resource_ = resource.Pass();
        resource_id_ = resource_->id();
      }

      scoped_ptr<ResourcePool::Resource>& GetResourceForTesting() {
        return resource_;
      }

    private:
      friend class TileManager;
      friend class Tile;
      friend class ManagedTileState;

      void set_use_resource() {
        mode_ = RESOURCE_MODE;
      }

      void set_solid_color(const SkColor& color) {
        mode_ = SOLID_COLOR_MODE;
        solid_color_ = color;
        resource_id_ = 0;
      }

      void set_rasterize_on_demand() {
        mode_ = PICTURE_PILE_MODE;
        resource_id_ = 0;
      }

      Mode mode_;
      SkColor solid_color_;

      // TODO(reveman): Eliminate the need for both |resource_id_|
      // and |resource| by re-factoring the "force upload"
      // mechanism. crbug.com/245767
      ResourceProvider::ResourceId resource_id_;
      scoped_ptr<ResourcePool::Resource> resource_;
      GLenum resource_format_;
      bool forced_upload_;
      RasterWorkerPool::RasterTask raster_task_;
  };


  ManagedTileState();
  ~ManagedTileState();

  scoped_ptr<base::Value> AsValue() const;

  // Persisted state: valid all the time.
  TileVersion tile_versions[NUM_RASTER_MODES];
  RasterMode raster_mode;

  bool picture_pile_analyzed;
  PicturePileImpl::Analysis picture_pile_analysis;

  // Ephemeral state, valid only during TileManager::ManageTiles.
  bool is_in_never_bin_on_both_trees() const {
    return bin[HIGH_PRIORITY_BIN] == NEVER_BIN &&
           bin[LOW_PRIORITY_BIN] == NEVER_BIN;
  }
  bool is_in_now_bin_on_either_tree() const {
    return bin[HIGH_PRIORITY_BIN] == NOW_BIN ||
           bin[LOW_PRIORITY_BIN] == NOW_BIN;
  }

  TileManagerBin bin[NUM_BIN_PRIORITIES];
  TileManagerBin tree_bin[NUM_TREES];

  // The bin that the tile would have if the GPU memory manager had
  // a maximally permissive policy, send to the GPU memory manager
  // to determine policy.
  TileManagerBin gpu_memmgr_stats_bin;
  TileResolution resolution;
  bool required_for_activation;
  float time_to_needed_in_seconds;
  float distance_to_visible_in_pixels;
};

}  // namespace cc

#endif  // CC_RESOURCES_MANAGED_TILE_STATE_H_
