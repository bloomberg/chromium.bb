// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_draw_info.h"

#include "base/metrics/histogram_macros.h"
#include "cc/base/math_util.h"

namespace cc {

TileDrawInfo::TileDrawInfo()
    : was_ever_ready_to_draw_(false),
      was_ever_used_to_draw_(false),
      was_a_prepaint_tile_(false) {}

TileDrawInfo::~TileDrawInfo() {
  DCHECK(!resource_);
  if (was_ever_ready_to_draw_ && was_a_prepaint_tile_) {
    UMA_HISTOGRAM_BOOLEAN("Renderer4.ReadyToDrawTileDrawStatus",
                          was_ever_used_to_draw_);
  }
}

void TileDrawInfo::AsValueInto(base::trace_event::TracedValue* state) const {
  state->SetBoolean("is_solid_color", mode_ == SOLID_COLOR_MODE);
  state->SetBoolean("is_transparent",
                    mode_ == SOLID_COLOR_MODE && !SkColorGetA(solid_color_));
}

Resource* TileDrawInfo::TakeResource() {
  Resource* resource = resource_;
  set_resource(nullptr);
  return resource;
}

}  // namespace cc
