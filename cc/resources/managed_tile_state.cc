// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/managed_tile_state.h"

#include <limits>

#include "cc/base/math_util.h"

namespace cc {
namespace {

scoped_ptr<base::Value> MemoryStateAsValue(DrawingInfoMemoryState state) {
  switch (state) {
    case NOT_ALLOWED_TO_USE_MEMORY:
      return scoped_ptr<base::Value>(
          base::Value::CreateStringValue("NOT_ALLOWED_TO_USE_MEMORY"));
    case CAN_USE_MEMORY:
      return scoped_ptr<base::Value>(
          base::Value::CreateStringValue("CAN_USE_MEMORY"));
    case USING_UNRELEASABLE_MEMORY:
      return scoped_ptr<base::Value>(
          base::Value::CreateStringValue("USING_UNRELEASABLE_MEMORY"));
    case USING_RELEASABLE_MEMORY:
      return scoped_ptr<base::Value>(
          base::Value::CreateStringValue("USING_RELEASABLE_MEMORY"));
    default:
      NOTREACHED() << "Unrecognized DrawingInfoMemoryState value " << state;
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown DrawingInfoMemoryState value>"));
  }
}

}  // namespace

ManagedTileState::ManagedTileState()
    : picture_pile_analyzed(false),
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
      resource_format_(GL_RGBA),
      memory_state_(NOT_ALLOWED_TO_USE_MEMORY),
      forced_upload_(false) {
}

ManagedTileState::DrawingInfo::~DrawingInfo() {
  DCHECK(!resource_);
  DCHECK(memory_state_ == NOT_ALLOWED_TO_USE_MEMORY);
}

bool ManagedTileState::DrawingInfo::IsReadyToDraw() const {
  switch (mode_) {
    case RESOURCE_MODE:
      return resource_ &&
             (memory_state_ == USING_RELEASABLE_MEMORY ||
              (memory_state_ == USING_UNRELEASABLE_MEMORY && forced_upload_)) &&
             resource_->id();
    case SOLID_COLOR_MODE:
    case PICTURE_PILE_MODE:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

ManagedTileState::~ManagedTileState() {
}

scoped_ptr<base::Value> ManagedTileState::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetBoolean("has_resource", drawing_info.resource_.get() != 0);
  state->Set("memory_state",
      MemoryStateAsValue(drawing_info.memory_state_).release());
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
  state->SetBoolean("is_solid_color", picture_pile_analysis.is_solid_color);
  state->SetBoolean("is_transparent",
                    picture_pile_analysis.is_solid_color &&
                    !SkColorGetA(picture_pile_analysis.solid_color));
  return state.PassAs<base::Value>();
}

}  // namespace cc

