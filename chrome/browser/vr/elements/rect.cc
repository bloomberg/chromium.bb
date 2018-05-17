// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/rect.h"

#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

Rect::Rect() = default;
Rect::~Rect() = default;

void Rect::SetColor(SkColor color) {
  SetCenterColor(color);
  SetEdgeColor(color);
}

void Rect::SetCenterColor(SkColor color) {
  animation().TransitionColorTo(last_frame_time(), BACKGROUND_COLOR,
                                center_color_, color);
}

void Rect::SetEdgeColor(SkColor color) {
  animation().TransitionColorTo(last_frame_time(), FOREGROUND_COLOR,
                                edge_color_, color);
}

void Rect::NotifyClientColorAnimated(SkColor color,
                                     int target_property_id,
                                     cc::KeyframeModel* animation) {
  if (target_property_id == BACKGROUND_COLOR) {
    center_color_ = color;
  } else if (target_property_id == FOREGROUND_COLOR) {
    edge_color_ = color;
  } else {
    UiElement::NotifyClientColorAnimated(color, target_property_id, animation);
  }
}

void Rect::Render(UiElementRenderer* renderer, const CameraModel& model) const {
  float opacity = computed_opacity() * local_opacity_;
  if (opacity <= 0.f)
    return;
  renderer->DrawRadialGradientQuad(
      model.view_proj_matrix * world_space_transform(), edge_color_,
      center_color_, GetClipRect(), opacity, size(), corner_radii());
}

void Rect::SetLocalOpacity(float opacity) {
  animation().TransitionFloatTo(last_frame_time(), LOCAL_OPACITY,
                                local_opacity_, opacity);
}

void Rect::NotifyClientFloatAnimated(float value,
                                     int target_property_id,
                                     cc::KeyframeModel* keyframe_model) {
  if (target_property_id == LOCAL_OPACITY) {
    local_opacity_ = value;
  } else {
    UiElement::NotifyClientFloatAnimated(value, target_property_id,
                                         keyframe_model);
  }
}

float Rect::ComputedAndLocalOpacityForTest() const {
  return computed_opacity() * local_opacity_;
}

}  // namespace vr
