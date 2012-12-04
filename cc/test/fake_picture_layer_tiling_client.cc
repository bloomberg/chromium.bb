// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer_tiling_client.h"

namespace cc {

FakePictureLayerTilingClient::FakePictureLayerTilingClient()
    : tile_manager_(&tile_manager_client_, NULL, 1),
      pile_(PicturePileImpl::Create()) {
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
                                     rect));
}

void FakePictureLayerTilingClient::SetTileSize(gfx::Size tile_size) {
    tile_size_ = tile_size;
}

}
