// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_MANAGED_TILE_STATE_H_
#define CC_RESOURCES_MANAGED_TILE_STATE_H_

#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/tile_manager.h"

namespace cc {

enum DrawingInfoMemoryState {
  NOT_ALLOWED_TO_USE_MEMORY,
  CAN_USE_MEMORY,
  USING_UNRELEASABLE_MEMORY,
  USING_RELEASABLE_MEMORY
};

// This is state that is specific to a tile that is
// managed by the TileManager.
class CC_EXPORT ManagedTileState {
 public:
  class CC_EXPORT DrawingInfo {
    public:
      enum Mode {
        RESOURCE_MODE,
        SOLID_COLOR_MODE,
        PICTURE_PILE_MODE,
        NUM_MODES
      };

      DrawingInfo();
      ~DrawingInfo();

      Mode mode() const {
        return mode_;
      }

      bool IsReadyToDraw() const;

      ResourceProvider::ResourceId get_resource_id() const {
        DCHECK(mode_ == RESOURCE_MODE);
        DCHECK(resource_);
        DCHECK(memory_state_ == USING_RELEASABLE_MEMORY || forced_upload_);
        return resource_->id();
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

      scoped_ptr<ResourcePool::Resource>& GetResourceForTesting() {
        return resource_;
      }

      void SetMemoryStateForTesting(DrawingInfoMemoryState state) {
        memory_state_ = state;
      }

    private:
      friend class TileManager;
      friend class Tile;
      friend class ManagedTileState;

      void set_use_resource() {
        mode_ = RESOURCE_MODE;
        if (memory_state_ == NOT_ALLOWED_TO_USE_MEMORY)
          memory_state_ = CAN_USE_MEMORY;
      }

      void set_solid_color(const SkColor& color) {
        mode_ = SOLID_COLOR_MODE;
        solid_color_ = color;
        memory_state_ = NOT_ALLOWED_TO_USE_MEMORY;
      }

      void set_rasterize_on_demand() {
        mode_ = PICTURE_PILE_MODE;
        memory_state_ = NOT_ALLOWED_TO_USE_MEMORY;
      }

      Mode mode_;
      SkColor solid_color_;

      scoped_ptr<ResourcePool::Resource> resource_;
      GLenum resource_format_;
      DrawingInfoMemoryState memory_state_;
      bool forced_upload_;
  };


  ManagedTileState();
  ~ManagedTileState();
  scoped_ptr<base::Value> AsValue() const;

  // Persisted state: valid all the time.
  typedef base::hash_set<uint32_t> PixelRefSet;
  PixelRefSet decoded_pixel_refs;
  DrawingInfo drawing_info;
  PicturePileImpl::Analysis picture_pile_analysis;
  bool picture_pile_analyzed;

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
  float time_to_needed_in_seconds;
  float distance_to_visible_in_pixels;
};

}  // namespace cc

#endif  // CC_RESOURCES_MANAGED_TILE_STATE_H_
