// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"

#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/tile_manager.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

Tile::Id Tile::s_next_id_ = 0;

Tile::Tile(TileManager* tile_manager,
           PicturePileImpl* picture_pile,
           gfx::Size tile_size,
           gfx::Rect content_rect,
           gfx::Rect opaque_rect,
           float contents_scale,
           int layer_id,
           int source_frame_number,
           bool can_use_lcd_text)
  : tile_manager_(tile_manager),
    tile_size_(tile_size),
    content_rect_(content_rect),
    contents_scale_(contents_scale),
    opaque_rect_(opaque_rect),
    layer_id_(layer_id),
    source_frame_number_(source_frame_number),
    can_use_lcd_text_(can_use_lcd_text),
    id_(s_next_id_++) {
  set_picture_pile(picture_pile);
  tile_manager_->RegisterTile(this);
}

Tile::~Tile() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::Tile", this);
  tile_manager_->UnregisterTile(this);
}

scoped_ptr<base::Value> Tile::AsValue() const {
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  TracedValue::MakeDictIntoImplicitSnapshot(res.get(), "cc::Tile", this);
  res->Set("picture_pile",
           TracedValue::CreateIDRef(picture_pile_.get()).release());
  res->SetDouble("contents_scale", contents_scale_);
  res->Set("content_rect", MathUtil::AsValue(content_rect_).release());
  res->SetInteger("layer_id", layer_id_);
  res->Set("active_priority", priority_[ACTIVE_TREE].AsValue().release());
  res->Set("pending_priority", priority_[PENDING_TREE].AsValue().release());
  res->Set("managed_state", managed_state_.AsValue().release());
  return res.PassAs<base::Value>();
}

size_t Tile::GPUMemoryUsageInBytes() const {
  size_t total_size = 0;
  for (int mode = 0; mode < NUM_RASTER_MODES; ++mode)
    total_size += managed_state_.tile_versions[mode].GPUMemoryUsageInBytes();
  return total_size;
}

}  // namespace cc
