// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"

#include <algorithm>

#include "base/debug/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/tile_manager.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace cc {

Tile::Id Tile::s_next_id_ = 0;

Tile::Tile(TileManager* tile_manager,
           RasterSource* raster_source,
           const gfx::Size& tile_size,
           const gfx::Rect& content_rect,
           float contents_scale,
           int layer_id,
           int source_frame_number,
           int flags)
    : RefCountedManaged<Tile>(tile_manager),
      tile_manager_(tile_manager),
      size_(tile_size),
      content_rect_(content_rect),
      contents_scale_(contents_scale),
      layer_id_(layer_id),
      source_frame_number_(source_frame_number),
      flags_(flags),
      is_shared_(false),
      tiling_i_index_(-1),
      tiling_j_index_(-1),
      required_for_activation_(false),
      id_(s_next_id_++) {
  set_raster_source(raster_source);
  for (int i = 0; i < NUM_TREES; i++)
    is_occluded_[i] = false;
}

Tile::~Tile() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      "cc::Tile", this);
}

void Tile::AsValueInto(base::debug::TracedValue* res) const {
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), res, "cc::Tile", this);
  TracedValue::SetIDRef(raster_source_.get(), res, "picture_pile");
  res->SetDouble("contents_scale", contents_scale_);

  res->BeginArray("content_rect");
  MathUtil::AddToTracedValue(content_rect_, res);
  res->EndArray();

  res->SetInteger("layer_id", layer_id_);

  res->BeginDictionary("active_priority");
  priority_[ACTIVE_TREE].AsValueInto(res);
  res->EndDictionary();

  res->BeginDictionary("pending_priority");
  priority_[PENDING_TREE].AsValueInto(res);
  res->EndDictionary();

  res->BeginDictionary("managed_state");
  managed_state_.AsValueInto(res);
  res->EndDictionary();

  res->SetBoolean("use_picture_analysis", use_picture_analysis());

  res->SetInteger("gpu_memory_usage", GPUMemoryUsageInBytes());
}

size_t Tile::GPUMemoryUsageInBytes() const {
  if (managed_state_.draw_info.resource_)
    return managed_state_.draw_info.resource_->bytes();
  return 0;
}

bool Tile::HasRasterTask() const {
  return !!managed_state_.raster_task.get();
}

}  // namespace cc
