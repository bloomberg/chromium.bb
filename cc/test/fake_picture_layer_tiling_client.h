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
#include "ui/gfx/rect.h"

namespace cc {

class FakePictureLayerTilingClient : public PictureLayerTilingClient {
 public:
  FakePictureLayerTilingClient();
  explicit FakePictureLayerTilingClient(ResourceProvider* resource_provider);
  virtual ~FakePictureLayerTilingClient();

  // PictureLayerTilingClient implementation.
  virtual scoped_refptr<Tile> CreateTile(
      PictureLayerTiling* tiling, const gfx::Rect& rect) OVERRIDE;
  virtual PicturePileImpl* GetPile() OVERRIDE;
  virtual gfx::Size CalculateTileSize(
      const gfx::Size& content_bounds) const OVERRIDE;
  virtual size_t GetMaxTilesForInterestArea() const OVERRIDE;
  virtual float GetSkewportTargetTimeInSeconds() const OVERRIDE;
  virtual int GetSkewportExtrapolationLimitInContentPixels() const OVERRIDE;

  void SetTileSize(const gfx::Size& tile_size);
  gfx::Size TileSize() const { return tile_size_; }

  virtual const Region* GetInvalidation() OVERRIDE;
  virtual const PictureLayerTiling* GetTwinTiling(
      const PictureLayerTiling* tiling) const OVERRIDE;
  virtual PictureLayerTiling* GetRecycledTwinTiling(
      const PictureLayerTiling* tiling) OVERRIDE;
  virtual WhichTree GetTree() const OVERRIDE;

  void set_twin_tiling(PictureLayerTiling* tiling) { twin_tiling_ = tiling; }
  void set_recycled_twin_tiling(PictureLayerTiling* tiling) {
    recycled_twin_tiling_ = tiling;
  }
  void set_text_rect(const gfx::Rect& rect) { text_rect_ = rect; }
  void set_allow_create_tile(bool allow) { allow_create_tile_ = allow; }
  void set_invalidation(const Region& region) { invalidation_ = region; }
  void set_max_tiles_for_interest_area(size_t area) {
    max_tiles_for_interest_area_ = area;
  }
  void set_skewport_target_time_in_seconds(float time) {
    skewport_target_time_in_seconds_ = time;
  }
  void set_skewport_extrapolation_limit_in_content_pixels(int limit) {
    skewport_extrapolation_limit_in_content_pixels_ = limit;
  }
  void set_tree(WhichTree tree) { tree_ = tree; }

  TileManager* tile_manager() const {
    return tile_manager_.get();
  }

 protected:
  FakeTileManagerClient tile_manager_client_;
  scoped_ptr<ResourcePool> resource_pool_;
  scoped_ptr<TileManager> tile_manager_;
  scoped_refptr<PicturePileImpl> pile_;
  gfx::Size tile_size_;
  PictureLayerTiling* twin_tiling_;
  PictureLayerTiling* recycled_twin_tiling_;
  gfx::Rect text_rect_;
  bool allow_create_tile_;
  Region invalidation_;
  size_t max_tiles_for_interest_area_;
  float skewport_target_time_in_seconds_;
  int skewport_extrapolation_limit_in_content_pixels_;
  WhichTree tree_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_
