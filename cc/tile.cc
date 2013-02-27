// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tile.h"

#include "base/stringprintf.h"
#include "cc/tile_manager.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

Tile::Tile(TileManager* tile_manager,
           PicturePileImpl* picture_pile,
           gfx::Size tile_size,
           GLenum format,
           gfx::Rect content_rect,
           gfx::Rect opaque_rect,
           float contents_scale)
  : tile_manager_(tile_manager),
    tile_size_(tile_size),
    format_(format),
    content_rect_(content_rect),
    opaque_rect_(opaque_rect),
    contents_scale_(contents_scale) {
  set_picture_pile(picture_pile);
  tile_manager_->RegisterTile(this);
}

Tile::~Tile() {
  tile_manager_->UnregisterTile(this);
}

scoped_ptr<base::Value> Tile::AsValue() const {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->SetString("id", base::StringPrintf("%p", this));
  res->SetString("picture_pile", base::StringPrintf("%p",
                                                    picture_pile_.get()));
  res->SetDouble("contents_scale", contents_scale_);
  res->Set("priority.0", priority_[ACTIVE_TREE].AsValue().release());
  res->Set("priority.1", priority_[PENDING_TREE].AsValue().release());
  res->Set("managed_state", managed_state_.AsValue().release());
  return res.PassAs<base::Value>();
}

}  // namespace cc
