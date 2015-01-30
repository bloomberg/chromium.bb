// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_
#define CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_

#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class FakePictureLayerTilingClient : public PictureLayerTilingClient {
 public:
  FakePictureLayerTilingClient();
  explicit FakePictureLayerTilingClient(ResourceProvider* resource_provider);
  ~FakePictureLayerTilingClient() override;

  // PictureLayerTilingClient implementation.
  scoped_refptr<Tile> CreateTile(float contents_scale,
                                 const gfx::Rect& rect) override;
  gfx::Size CalculateTileSize(const gfx::Size& content_bounds) const override;
  TilePriority::PriorityBin GetMaxTilePriorityBin() const override;

  void SetTileSize(const gfx::Size& tile_size);
  gfx::Size TileSize() const { return tile_size_; }

  const Region* GetPendingInvalidation() override;
  const PictureLayerTiling* GetPendingOrActiveTwinTiling(
      const PictureLayerTiling* tiling) const override;
  PictureLayerTiling* GetRecycledTwinTiling(
      const PictureLayerTiling* tiling) override;
  bool RequiresHighResToDraw() const override;
  WhichTree GetTree() const override;

  void set_twin_tiling_set(PictureLayerTilingSet* set) {
    twin_set_ = set;
    twin_tiling_ = nullptr;
  }
  void set_twin_tiling(PictureLayerTiling* tiling) {
    twin_tiling_ = tiling;
    twin_set_ = nullptr;
  }
  void set_recycled_twin_tiling(PictureLayerTiling* tiling) {
    recycled_twin_tiling_ = tiling;
  }
  void set_text_rect(const gfx::Rect& rect) { text_rect_ = rect; }
  void set_invalidation(const Region& region) { invalidation_ = region; }
  void set_max_tile_priority_bin(TilePriority::PriorityBin bin) {
    max_tile_priority_bin_ = bin;
  }
  void set_tree(WhichTree tree) { tree_ = tree; }
  RasterSource* raster_source() { return pile_.get(); }

  TileManager* tile_manager() const {
    return tile_manager_.get();
  }

 protected:
  FakeTileManagerClient tile_manager_client_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<TileManager> tile_manager_;
  scoped_refptr<PicturePileImpl> pile_;
  gfx::Size tile_size_;
  PictureLayerTilingSet* twin_set_;
  PictureLayerTiling* twin_tiling_;
  PictureLayerTiling* recycled_twin_tiling_;
  gfx::Rect text_rect_;
  Region invalidation_;
  TilePriority::PriorityBin max_tile_priority_bin_;
  WhichTree tree_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_
