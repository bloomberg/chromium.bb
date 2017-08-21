// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/render_widget_surface_properties.h"

namespace content {

// static
RenderWidgetSurfaceProperties
RenderWidgetSurfaceProperties::FromCompositorFrame(
    const cc::CompositorFrame& frame) {
  RenderWidgetSurfaceProperties properties;
  properties.size = frame.size_in_pixels();
  properties.device_scale_factor = frame.device_scale_factor();
#ifdef OS_ANDROID
  properties.top_controls_height = frame.metadata.top_controls_height;
  properties.top_controls_shown_ratio = frame.metadata.top_controls_shown_ratio;
  properties.bottom_controls_height = frame.metadata.bottom_controls_height;
  properties.bottom_controls_shown_ratio =
      frame.metadata.bottom_controls_shown_ratio;
  properties.selection = frame.metadata.selection;
  properties.has_transparent_background =
      frame.render_pass_list.back()->has_transparent_background;
#endif
  return properties;
}

RenderWidgetSurfaceProperties::RenderWidgetSurfaceProperties() = default;

RenderWidgetSurfaceProperties::RenderWidgetSurfaceProperties(
    const RenderWidgetSurfaceProperties& other) = default;

RenderWidgetSurfaceProperties::~RenderWidgetSurfaceProperties() = default;

RenderWidgetSurfaceProperties& RenderWidgetSurfaceProperties::operator=(
    const RenderWidgetSurfaceProperties& other) = default;

bool RenderWidgetSurfaceProperties::operator==(
    const RenderWidgetSurfaceProperties& other) const {
  return other.device_scale_factor == device_scale_factor &&
#ifdef OS_ANDROID
         other.top_controls_height == top_controls_height &&
         other.top_controls_shown_ratio == top_controls_shown_ratio &&
         other.bottom_controls_height == bottom_controls_height &&
         other.bottom_controls_shown_ratio == bottom_controls_shown_ratio &&
         other.selection == selection &&
         other.has_transparent_background == has_transparent_background &&
#endif
         other.size == size;
}

bool RenderWidgetSurfaceProperties::operator!=(
    const RenderWidgetSurfaceProperties& other) const {
  return !(*this == other);
}

}  // namespace content
