// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/managed_tile_state.h"

#include <limits>
#include <string>

#include "base/debug/trace_event_argument.h"
#include "cc/base/math_util.h"

namespace cc {

ManagedTileState::ManagedTileState()
    : resolution(NON_IDEAL_RESOLUTION),
      priority_bin(TilePriority::EVENTUALLY),
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
  state->SetString("resolution", TileResolutionToString(resolution));
  state->SetString("priority_bin", TilePriorityBinToString(priority_bin));
  state->SetBoolean("is_solid_color",
                    draw_info.mode_ == DrawInfo::SOLID_COLOR_MODE);
  state->SetBoolean("is_transparent",
                    draw_info.mode_ == DrawInfo::SOLID_COLOR_MODE &&
                        !SkColorGetA(draw_info.solid_color_));
  state->SetInteger("scheduled_priority", scheduled_priority);
}

}  // namespace cc
