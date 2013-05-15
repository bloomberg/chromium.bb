// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"

#include "cc/debug/traced_value.h"
#include "cc/resources/tile_manager.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

Tile::Tile(TileManager* tile_manager,
           PicturePileImpl* picture_pile,
           gfx::Size tile_size,
           gfx::Rect content_rect,
           gfx::Rect opaque_rect,
           float contents_scale,
           int layer_id)
  : tile_manager_(tile_manager),
    tile_size_(tile_size),
    content_rect_(content_rect),
    contents_scale_(contents_scale),
    opaque_rect_(opaque_rect),
    layer_id_(layer_id) {
  set_picture_pile(picture_pile);
  tile_manager_->RegisterTile(this);
}

Tile::~Tile() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID("cc.debug", "cc::Tile", this);
  tile_manager_->UnregisterTile(this);
}

scoped_ptr<base::Value> Tile::AsValue() const {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  TracedValue::MakeDictIntoImplicitSnapshot(res.get(), "cc::Tile", this);
  res->Set("picture_pile",
           TracedValue::CreateIDRef(picture_pile_.get()).release());
  res->SetDouble("contents_scale", contents_scale_);
  res->Set("active_priority", priority_[ACTIVE_TREE].AsValue().release());
  res->Set("pending_priority", priority_[PENDING_TREE].AsValue().release());
  res->Set("managed_state", managed_state_.AsValue().release());
  return res.PassAs<base::Value>();
}

void Tile::SetPriority(WhichTree tree, const TilePriority& priority) {
  if (priority_[tree] == priority)
    return;

  tile_manager_->WillModifyTilePriority(this, tree, priority);
  priority_[tree] = priority;
}

}  // namespace cc
