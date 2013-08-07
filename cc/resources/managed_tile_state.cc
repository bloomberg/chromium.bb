// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/managed_tile_state.h"

#include <limits>

#include "cc/base/math_util.h"

namespace cc {

scoped_ptr<base::Value> ManagedTileBinAsValue(ManagedTileBin bin) {
  switch (bin) {
  case NOW_AND_READY_TO_DRAW_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NOW_AND_READY_TO_DRAW_BIN"));
  case NOW_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NOW_BIN"));
  case SOON_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "SOON_BIN"));
  case EVENTUALLY_AND_ACTIVE_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "EVENTUALLY_AND_ACTIVE_BIN"));
  case EVENTUALLY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "EVENTUALLY_BIN"));
  case NEVER_AND_ACTIVE_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NEVER_AND_ACTIVE_BIN"));
  case NEVER_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "NEVER_BIN"));
  default:
      DCHECK(false) << "Unrecognized ManagedTileBin value " << bin;
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown ManagedTileBin value>"));
  }
}

scoped_ptr<base::Value> ManagedTileBinPriorityAsValue(
    ManagedTileBinPriority bin_priority) {
  switch (bin_priority) {
  case HIGH_PRIORITY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "HIGH_PRIORITY_BIN"));
  case LOW_PRIORITY_BIN:
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "LOW_PRIORITY_BIN"));
  default:
      DCHECK(false) << "Unrecognized ManagedTileBinPriority value";
      return scoped_ptr<base::Value>(base::Value::CreateStringValue(
          "<unknown ManagedTileBinPriority value>"));
  }
}

ManagedTileState::ManagedTileState()
    : raster_mode(LOW_QUALITY_RASTER_MODE),
      gpu_memmgr_stats_bin(NEVER_BIN),
      resolution(NON_IDEAL_RESOLUTION),
      required_for_activation(false),
      time_to_needed_in_seconds(std::numeric_limits<float>::infinity()),
      distance_to_visible_in_pixels(std::numeric_limits<float>::infinity()),
      visible_and_ready_to_draw(false),
      scheduled_priority(0) {
  for (int i = 0; i < NUM_TREES; ++i) {
    tree_bin[i] = NEVER_BIN;
    bin[i] = NEVER_BIN;
  }
}

ManagedTileState::TileVersion::TileVersion()
    : mode_(RESOURCE_MODE),
      has_text_(false) {
}

ManagedTileState::TileVersion::~TileVersion() {
  DCHECK(!resource_);
}

bool ManagedTileState::TileVersion::IsReadyToDraw() const {
  switch (mode_) {
    case RESOURCE_MODE:
      return !!resource_;
    case SOLID_COLOR_MODE:
    case PICTURE_PILE_MODE:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

size_t ManagedTileState::TileVersion::GPUMemoryUsageInBytes() const {
  if (!resource_)
    return 0;
  return resource_->bytes();
}

ManagedTileState::~ManagedTileState() {
}

scoped_ptr<base::Value> ManagedTileState::AsValue() const {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue());
  state->SetBoolean("has_resource",
                    tile_versions[raster_mode].resource_.get() != 0);
  state->Set("bin.0", ManagedTileBinAsValue(bin[ACTIVE_TREE]).release());
  state->Set("bin.1", ManagedTileBinAsValue(bin[PENDING_TREE]).release());
  state->Set("gpu_memmgr_stats_bin",
      ManagedTileBinAsValue(bin[ACTIVE_TREE]).release());
  state->Set("resolution", TileResolutionAsValue(resolution).release());
  state->Set("time_to_needed_in_seconds",
      MathUtil::AsValueSafely(time_to_needed_in_seconds).release());
  state->Set("distance_to_visible_in_pixels",
      MathUtil::AsValueSafely(distance_to_visible_in_pixels).release());
  state->SetBoolean("required_for_activation", required_for_activation);
  state->SetBoolean(
      "is_solid_color",
      tile_versions[raster_mode].mode_ == TileVersion::SOLID_COLOR_MODE);
  state->SetBoolean(
      "is_transparent",
      tile_versions[raster_mode].mode_ == TileVersion::SOLID_COLOR_MODE &&
          !SkColorGetA(tile_versions[raster_mode].solid_color_));
  state->SetInteger("scheduled_priority", scheduled_priority);
  return state.PassAs<base::Value>();
}

}  // namespace cc

