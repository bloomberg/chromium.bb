// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PARENT_COMPOSITOR_DRAW_CONSTRAINTS_H_
#define ANDROID_WEBVIEW_BROWSER_PARENT_COMPOSITOR_DRAW_CONSTRAINTS_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace android_webview {

struct ParentCompositorDrawConstraints {
  bool is_layer;
  gfx::Transform transform;
  gfx::Rect surface_rect;

  ParentCompositorDrawConstraints();
  ParentCompositorDrawConstraints(bool is_layer,
                                  const gfx::Transform& transform,
                                  const gfx::Rect& surface_rect);
  bool Equals(const ParentCompositorDrawConstraints& other) const;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PARENT_COMPOSITOR_DRAW_CONSTRAINTS_H_
