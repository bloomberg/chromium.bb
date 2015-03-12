// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"

#include <algorithm>

#include "base/trace_event/trace_event_argument.h"
#include "cc/base/math_util.h"
#include "cc/debug/traced_value.h"
#include "cc/resources/tile_manager.h"

namespace cc {

Tile::Id Tile::s_next_id_ = 0;

Tile::Tile(TileManager* tile_manager,
           RasterSource* raster_source,
           const gfx::Size& desired_texture_size,
           const gfx::Rect& content_rect,
           float contents_scale,
           int layer_id,
           int source_frame_number,
           int flags)
    : RefCountedManaged<Tile>(tile_manager),
      desired_texture_size_(desired_texture_size),
      content_rect_(content_rect),
      contents_scale_(contents_scale),
      layer_id_(layer_id),
      source_frame_number_(source_frame_number),
      flags_(flags),
      tiling_i_index_(-1),
      tiling_j_index_(-1),
      is_shared_(false),
      required_for_activation_(false),
      required_for_draw_(false),
      id_(s_next_id_++),
      scheduled_priority_(0) {
  set_raster_source(raster_source);
  for (int i = 0; i <= LAST_TREE; i++)
    is_occluded_[i] = false;
}

Tile::~Tile() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      "cc::Tile", this);
}

void Tile::AsValueInto(base::trace_event::TracedValue* res) const {
  TracedValue::MakeDictIntoImplicitSnapshotWithCategory(
      TRACE_DISABLED_BY_DEFAULT("cc.debug"), res, "cc::Tile", this);
  TracedValue::SetIDRef(raster_source_.get(), res, "picture_pile");
  res->SetDouble("contents_scale", contents_scale_);

  MathUtil::AddToTracedValue("content_rect", content_rect_, res);

  res->SetInteger("layer_id", layer_id_);

  res->BeginDictionary("active_priority");
  priority_[ACTIVE_TREE].AsValueInto(res);
  res->EndDictionary();

  res->BeginDictionary("pending_priority");
  priority_[PENDING_TREE].AsValueInto(res);
  res->EndDictionary();

  res->BeginDictionary("draw_info");
  draw_info_.AsValueInto(res);
  res->EndDictionary();

  res->SetBoolean("has_resource", HasResource());
  res->SetBoolean("is_using_gpu_memory", HasResource() || HasRasterTask());
  res->SetString("resolution",
                 TileResolutionToString(combined_priority().resolution));

  res->SetInteger("scheduled_priority", scheduled_priority_);

  res->SetBoolean("use_picture_analysis", use_picture_analysis());

  res->SetInteger("gpu_memory_usage", GPUMemoryUsageInBytes());
}

size_t Tile::GPUMemoryUsageInBytes() const {
  if (draw_info_.resource_)
    return draw_info_.resource_->bytes();
  return 0;
}

}  // namespace cc
