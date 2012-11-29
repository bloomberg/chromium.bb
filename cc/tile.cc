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
           gfx::Rect rect_inside_picture)
  : tile_manager_(tile_manager),
    picture_pile_(picture_pile),
    tile_size_(tile_size),
    format_(format),
    rect_inside_picture_(rect_inside_picture) {
  tile_manager_->RegisterTile(this);
}

Tile::~Tile() {
  tile_manager_->UnregisterTile(this);
}

void Tile::set_priority(WhichTree tree, const TilePriority& priority) {
  tile_manager_->WillModifyTilePriority(this, tree, priority);
  priority_[tree] = priority;
}

size_t Tile::bytes_consumed_if_allocated() const {
  DCHECK(format_ == GL_RGBA);
  return 4 * tile_size_.width() * tile_size_.height();
}

}  // namespace cc
