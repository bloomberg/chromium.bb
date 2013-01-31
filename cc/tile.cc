// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile.h"

#include "cc/tile_manager.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

Tile::Tile(TileManager* tile_manager,
           PicturePileImpl* picture_pile,
           gfx::Size tile_size,
           GLenum format,
           gfx::Rect content_rect,
           float contents_scale)
  : tile_manager_(tile_manager),
    picture_pile_(picture_pile),
    tile_size_(tile_size),
    format_(format),
    content_rect_(content_rect),
    contents_scale_(contents_scale) {
  tile_manager_->RegisterTile(this);
}

Tile::~Tile() {
  tile_manager_->UnregisterTile(this);
}

}  // namespace cc
