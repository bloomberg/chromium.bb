// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/rect.h"

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

Rect::Rect() {}
Rect::~Rect() {}

void Rect::SetCenterColor(SkColor color) {
  animation_player().TransitionColorTo(last_frame_time(), BACKGROUND_COLOR,
                                       center_color_, color);
}

void Rect::SetEdgeColor(SkColor color) {
  animation_player().TransitionColorTo(last_frame_time(), FOREGROUND_COLOR,
                                       center_color_, color);
}

void Rect::NotifyClientColorAnimated(SkColor color,
                                     int target_property_id,
                                     cc::Animation* animation) {
  if (target_property_id == BACKGROUND_COLOR) {
    center_color_ = color;
  } else if (target_property_id == FOREGROUND_COLOR) {
    edge_color_ = color;
  } else {
    UiElement::NotifyClientColorAnimated(color, target_property_id, animation);
  }
}

void Rect::Render(UiElementRenderer* renderer,
                  const gfx::Transform& model_view_proj_matrix) const {
  renderer->DrawGradientQuad(model_view_proj_matrix, edge_color_, center_color_,
                             computed_opacity(), size(), corner_radius());
}

}  // namespace vr
