// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile.h"

#include "cc/tile_manager.h"

namespace cc {

Tile::Tile(TileManager* tile_manager,
           gfx::Size tile_size,
           gfx::Rect rect_inside_picture,
           TileQuality quality)
  : tile_manager_(tile_manager),
    tile_size_(tile_size),
    rect_inside_picture_(rect_inside_picture),
    quality_(quality) {
  tile_manager_->RegisterTile(this);
}

Tile::~Tile() {
  tile_manager_->UnregisterTile(this);
}

}  // namespace cc
