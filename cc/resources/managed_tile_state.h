// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_MANAGED_TILE_STATE_H_
#define CC_RESOURCES_MANAGED_TILE_STATE_H_

#include <list>

#include "base/memory/scoped_ptr.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/tile_manager.h"

namespace cc {

// This is state that is specific to a tile that is
// managed by the TileManager.
class CC_EXPORT ManagedTileState {
 public:
  class CC_EXPORT DrawingInfo {
    public:
      enum Mode {
        RESOURCE_MODE,
        SOLID_COLOR_MODE,
        TRANSPARENT_MODE,
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
        DCHECK(!resource_is_being_initialized_);
        return resource_->id();
      }

      SkColor get_solid_color() const {
        DCHECK(mode_ == SOLID_COLOR_MODE);

        return solid_color_;
      }

      bool contents_swizzled() const {
        return contents_swizzled_;
      }

      bool requires_resource() const {
        return mode_ == RESOURCE_MODE ||
               mode_ == PICTURE_PILE_MODE;
      }

      scoped_ptr<ResourcePool::Resource>& GetResourceForTesting() {
        return resource_;
      }

    private:
      friend class TileManager;
      friend class ManagedTileState;

      void set_use_resource() {
        mode_ = RESOURCE_MODE;
      }

      void set_transparent() {
        mode_ = TRANSPARENT_MODE;
      }

      void set_solid_color(const SkColor& color) {
        mode_ = SOLID_COLOR_MODE;
        solid_color_ = color;
      }

      void set_rasterize_on_demand() {
        mode_ = PICTURE_PILE_MODE;
      }

      void set_contents_swizzled(bool contents_swizzled) {
        contents_swizzled_ = contents_swizzled;
      }

      Mode mode_;
      SkColor solid_color_;

      scoped_ptr<ResourcePool::Resource> resource_;
      bool resource_is_being_initialized_;
      bool can_be_freed_;
      bool contents_swizzled_;
  };


  ManagedTileState();
  ~ManagedTileState();
  scoped_ptr<base::Value> AsValue() const;

  // Persisted state: valid all the time.
  bool can_use_gpu_memory;
  bool need_to_gather_pixel_refs;
  std::list<skia::LazyPixelRef*> pending_pixel_refs;
  DrawingInfo drawing_info;
  PicturePileImpl::Analysis picture_pile_analysis;
  bool picture_pile_analyzed;

  // Ephemeral state, valid only during TileManager::ManageTiles.
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
