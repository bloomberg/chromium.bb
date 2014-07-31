// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/parent_compositor_draw_constraints.h"

namespace android_webview {

ParentCompositorDrawConstraints::ParentCompositorDrawConstraints()
    : is_layer(false) {
}

ParentCompositorDrawConstraints::ParentCompositorDrawConstraints(
    bool is_layer,
    const gfx::Transform& transform,
    const gfx::Rect& surface_rect)
    : is_layer(is_layer), transform(transform), surface_rect(surface_rect) {
}

bool ParentCompositorDrawConstraints::Equals(
    const ParentCompositorDrawConstraints& other) const {
  if (is_layer != other.is_layer || transform != other.transform)
    return false;

  // Don't care about the surface size when neither is on a layer.
  return !is_layer || surface_rect == other.surface_rect;
}

}  // namespace webview
