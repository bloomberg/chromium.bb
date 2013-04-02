// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/managed_tile_state.h"

#include <limits>

#include "cc/base/math_util.h"

namespace cc {

ManagedTileState::ManagedTileState()
    : can_use_gpu_memory(false),
      need_to_gather_pixel_refs(true),
      picture_pile_analyzed(false),
      gpu_memmgr_stats_bin(NEVER_BIN),
      resolution(NON_IDEAL_RESOLUTION),
      time_to_needed_in_seconds(std::numeric_limits<float>::infinity()),
      distance_to_visible_in_pixels(std::numeric_limits<float>::infinity()) {
  for (int i = 0; i < NUM_TREES; ++i) {
    tree_bin[i] = NEVER_BIN;
    bin[i] = NEVER_BIN;
  }
}

ManagedTileState::DrawingInfo::DrawingInfo()
    : mode_(RESOURCE_MODE),
      resource_is_being_initialized_(false),
      can_be_freed_(true),
      contents_swizzled_(false) {
}

ManagedTileState::DrawingInfo::~DrawingInfo() {
}

bool ManagedTileState::DrawingInfo::IsReadyToDraw() const {
  switch (mode_) {
    case RESOURCE_MODE:
      return resource_ &&
             !resource_is_being_initialized_ &&
             resource_->id();
    case SOLID_COLOR_MODE:
    case TRANSPARENT_MODE:
    case PICTURE_PILE_MODE:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

ManagedTileState::~ManagedTileState() {
  DCHECK(!drawing_info.resource_);
  DCHECK(!drawing_info.resource_is_being_initialized_);
}

scoped_ptr<base::Value> ManagedTileState::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetBoolean("can_use_gpu_memory", can_use_gpu_memory);
  state->SetBoolean("can_be_freed", drawing_info.can_be_freed_);
  state->SetBoolean("has_resource", drawing_info.resource_.get() != 0);
  state->SetBoolean("resource_is_being_initialized",
      drawing_info.resource_is_being_initialized_);
  state->Set("bin.0", TileManagerBinAsValue(bin[ACTIVE_TREE]).release());
  state->Set("bin.1", TileManagerBinAsValue(bin[PENDING_TREE]).release());
  state->Set("gpu_memmgr_stats_bin",
      TileManagerBinAsValue(bin[ACTIVE_TREE]).release());
  state->Set("resolution", TileResolutionAsValue(resolution).release());
  state->Set("time_to_needed_in_seconds",
      MathUtil::AsValueSafely(time_to_needed_in_seconds).release());
  state->Set("distance_to_visible_in_pixels",
      MathUtil::AsValueSafely(distance_to_visible_in_pixels).release());
  state->SetBoolean("is_picture_pile_analyzed", picture_pile_analyzed);
  state->SetBoolean("is_cheap_to_raster",
      picture_pile_analysis.is_cheap_to_raster);
  state->SetBoolean("is_transparent", picture_pile_analysis.is_transparent);
  state->SetBoolean("is_solid_color", picture_pile_analysis.is_solid_color);
  return state.PassAs<base::Value>();
}

}  // namespace cc

