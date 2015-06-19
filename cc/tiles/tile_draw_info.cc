// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/tile_draw_info.h"

#include "cc/base/math_util.h"

namespace cc {

TileDrawInfo::TileDrawInfo()
    : mode_(RESOURCE_MODE),
      solid_color_(SK_ColorWHITE),
      contents_swizzled_(false) {
}

TileDrawInfo::~TileDrawInfo() {
  DCHECK(!resource_);
}

void TileDrawInfo::AsValueInto(base::trace_event::TracedValue* state) const {
  state->SetBoolean("is_solid_color", mode_ == SOLID_COLOR_MODE);
  state->SetBoolean("is_transparent",
                    mode_ == SOLID_COLOR_MODE && !SkColorGetA(solid_color_));
}

}  // namespace cc
