// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_
#define CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_

#include "cc/resources/picture_layer_tiling.h"
#include "cc/resources/picture_pile_impl.h"
#include "cc/resources/tile.h"
#include "cc/resources/tile_manager.h"
#include "cc/test/fake_rendering_stats_instrumentation.h"
#include "cc/test/fake_tile_manager_client.h"
#include "ui/gfx/rect.h"

namespace cc {

class FakePictureLayerTilingClient : public PictureLayerTilingClient {
 public:
  FakePictureLayerTilingClient();
  virtual ~FakePictureLayerTilingClient();

  // PictureLayerTilingClient implementation.
  virtual scoped_refptr<Tile> CreateTile(
      PictureLayerTiling* tiling, gfx::Rect rect) OVERRIDE;
  virtual void UpdatePile(Tile* tile) OVERRIDE {}
  virtual gfx::Size CalculateTileSize(
      gfx::Size content_bounds) OVERRIDE;

  void SetTileSize(gfx::Size tile_size);
  gfx::Size TileSize() const { return tile_size_; }
  scoped_refptr<PicturePileImpl> pile() { return pile_; }

  virtual const Region* GetInvalidation() OVERRIDE;
  virtual const PictureLayerTiling* GetTwinTiling(
      const PictureLayerTiling* tiling) OVERRIDE;
  virtual bool TileHasText(Tile* tile) OVERRIDE;

  void set_twin_tiling(PictureLayerTiling* tiling) { twin_tiling_ = tiling; }
  void set_text_rect(gfx::Rect rect) { text_rect_ = rect; }
  void set_allow_create_tile(bool allow) { allow_create_tile_ = allow; }
  void set_invalidation(const Region& region) { invalidation_ = region; }

 protected:
  FakeTileManagerClient tile_manager_client_;
  FakeRenderingStatsInstrumentation stats_instrumentation_;
  TileManager tile_manager_;
  scoped_refptr<PicturePileImpl> pile_;
  gfx::Size tile_size_;
  PictureLayerTiling* twin_tiling_;
  gfx::Rect text_rect_;
  bool allow_create_tile_;
  Region invalidation_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_TILING_CLIENT_H_
