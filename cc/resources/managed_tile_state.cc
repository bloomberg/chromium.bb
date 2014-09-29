// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/managed_tile_state.h"

#include <limits>
#include <string>

#include "base/debug/trace_event_argument.h"
#include "cc/base/math_util.h"

namespace cc {

std::string ManagedTileBinToString(ManagedTileBin bin) {
  switch (bin) {
    case NOW_AND_READY_TO_DRAW_BIN:
      return "NOW_AND_READY_TO_DRAW_BIN";
    case NOW_BIN:
      return "NOW_BIN";
    case SOON_BIN:
      return "SOON_BIN";
    case EVENTUALLY_AND_ACTIVE_BIN:
      return "EVENTUALLY_AND_ACTIVE_BIN";
    case EVENTUALLY_BIN:
      return "EVENTUALLY_BIN";
    case AT_LAST_AND_ACTIVE_BIN:
      return "AT_LAST_AND_ACTIVE_BIN";
    case AT_LAST_BIN:
      return "AT_LAST_BIN";
    case NEVER_BIN:
      return "NEVER_BIN";
    case NUM_BINS:
      NOTREACHED();
      return "Invalid Bin (NUM_BINS)";
  }
  return "Invalid Bin (UNKNOWN)";
}

ManagedTileState::ManagedTileState()
    : bin(NEVER_BIN),
      resolution(NON_IDEAL_RESOLUTION),
      required_for_activation(false),
      priority_bin(TilePriority::EVENTUALLY),
      distance_to_visible(std::numeric_limits<float>::infinity()),
      visible_and_ready_to_draw(false),
      scheduled_priority(0) {
}

ManagedTileState::DrawInfo::DrawInfo()
    : mode_(RESOURCE_MODE), solid_color_(SK_ColorWHITE) {
}

ManagedTileState::DrawInfo::~DrawInfo() {
  DCHECK(!resource_);
}

bool ManagedTileState::DrawInfo::IsReadyToDraw() const {
  switch (mode_) {
    case RESOURCE_MODE:
      return !!resource_;
    case SOLID_COLOR_MODE:
    case PICTURE_PILE_MODE:
      return true;
  }
  NOTREACHED();
  return false;
}

ManagedTileState::~ManagedTileState() {}

void ManagedTileState::AsValueInto(base::debug::TracedValue* state) const {
  bool has_resource = (draw_info.resource_.get() != 0);
  bool has_active_task = (raster_task.get() != 0);

  bool is_using_gpu_memory = has_resource || has_active_task;

  state->SetBoolean("has_resource", has_resource);
  state->SetBoolean("is_using_gpu_memory", is_using_gpu_memory);
  state->SetString("bin", ManagedTileBinToString(bin));
  state->SetString("resolution", TileResolutionToString(resolution));
  state->SetString("priority_bin", TilePriorityBinToString(priority_bin));
  state->SetDouble("distance_to_visible",
                   MathUtil::AsFloatSafely(distance_to_visible));
  state->SetBoolean("required_for_activation", required_for_activation);
  state->SetBoolean("is_solid_color",
                    draw_info.mode_ == DrawInfo::SOLID_COLOR_MODE);
  state->SetBoolean("is_transparent",
                    draw_info.mode_ == DrawInfo::SOLID_COLOR_MODE &&
                        !SkColorGetA(draw_info.solid_color_));
  state->SetInteger("scheduled_priority", scheduled_priority);
}

}  // namespace cc
