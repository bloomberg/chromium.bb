// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_tiling_client.h"

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

FakePictureLayerTilingClient::FakePictureLayerTilingClient()
    : tile_manager_(&tile_manager_client_,
                    NULL,
                    1,
                    false,
                    false,
                    false,
                    &stats_instrumentation_),
      pile_(new FakeInfinitePicturePileImpl()) {
}

FakePictureLayerTilingClient::~FakePictureLayerTilingClient() {
}

scoped_refptr<Tile> FakePictureLayerTilingClient::CreateTile(
    PictureLayerTiling*,
    gfx::Rect rect) {
  return make_scoped_refptr(new Tile(&tile_manager_,
                                     pile_.get(),
                                     tile_size_,
                                     GL_RGBA,
                                     rect,
                                     gfx::Rect(),
                                     1,
                                     0));
}

void FakePictureLayerTilingClient::SetTileSize(gfx::Size tile_size) {
  tile_size_ = tile_size;
}

gfx::Size FakePictureLayerTilingClient::CalculateTileSize(
    gfx::Size /* current_tile_size */,
    gfx::Size /* content_bounds */) {
  return tile_size_;
}

}  // namespace cc
