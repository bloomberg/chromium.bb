// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_tiling_client.h"

#include <limits>

#include "cc/test/fake_tile_manager.h"

namespace cc {

class FakeInfinitePicturePileImpl : public PicturePileImpl {
 public:
  FakeInfinitePicturePileImpl() {
    gfx::Size size(std::numeric_limits<int>::max(),
                   std::numeric_limits<int>::max());
    Resize(size);
    recorded_region_ = Region(gfx::Rect(size));
  }

 protected:
  virtual ~FakeInfinitePicturePileImpl() {}
};

FakePictureLayerTilingClient::FakePictureLayerTilingClient(
    TileManager* tile_manager)
    : tile_manager_(tile_manager),
      pile_(new FakeInfinitePicturePileImpl()),
      twin_tiling_(NULL),
      allow_create_tile_(true),
      is_active_(false),
      is_pending_(false) {}

FakePictureLayerTilingClient::~FakePictureLayerTilingClient() {
}

scoped_refptr<Tile> FakePictureLayerTilingClient::CreateTile(
    PictureLayerTiling*,
    gfx::Rect rect) {
  if (!allow_create_tile_)
    return scoped_refptr<Tile>();
  return tile_manager()->CreateTile(
      pile_.get(), tile_size_, rect, gfx::Rect(), 1, 0, 0, Tile::USE_LCD_TEXT);
}

scoped_refptr<TileBundle> FakePictureLayerTilingClient::CreateTileBundle(
    int offset_x,
    int offset_y,
    int width,
    int height) {
  return tile_manager()->CreateTileBundle(offset_x, offset_y, width, height);
}

void FakePictureLayerTilingClient::SetTileSize(gfx::Size tile_size) {
  tile_size_ = tile_size;
}

gfx::Size FakePictureLayerTilingClient::CalculateTileSize(
    gfx::Size /* content_bounds */) const {
  return tile_size_;
}

const Region* FakePictureLayerTilingClient::GetInvalidation() {
  return &invalidation_;
}

const PictureLayerTiling* FakePictureLayerTilingClient::GetTwinTiling(
      const PictureLayerTiling* tiling) const {
  return twin_tiling_;
}

}  // namespace cc
