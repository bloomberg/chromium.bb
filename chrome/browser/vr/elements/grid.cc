// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/grid.h"

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_element_renderer.h"

namespace vr {

Grid::Grid() {}
Grid::~Grid() {}

void Grid::SetGridColor(SkColor color) {
  animation_player().TransitionColorTo(last_frame_time(), GRID_COLOR,
                                       grid_color_, color);
}

void Grid::NotifyClientColorAnimated(SkColor color,
                                     int target_property_id,
                                     cc::Animation* animation) {
  if (target_property_id == GRID_COLOR) {
    grid_color_ = color;
  } else {
    Rect::NotifyClientColorAnimated(color, target_property_id, animation);
  }
}

void Grid::Render(UiElementRenderer* renderer,
                  const gfx::Transform& model_view_proj_matrix) const {
  renderer->DrawGradientGridQuad(model_view_proj_matrix, edge_color(),
                                 center_color(), grid_color_, gridline_count_,
                                 computed_opacity());
}

}  // namespace vr
