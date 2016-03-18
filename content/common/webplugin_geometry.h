// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WEBPLUGIN_GEOMETRY_H_
#define CONTENT_COMMON_WEBPLUGIN_GEOMETRY_H_

#include <vector>

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// Describes the new location for a plugin window.
struct WebPluginGeometry {
  WebPluginGeometry();
  WebPluginGeometry(const WebPluginGeometry& other);
  ~WebPluginGeometry();

  bool Equals(const WebPluginGeometry& rhs) const;

  gfx::Rect window_rect;
  // Clip rect (include) and cutouts (excludes), relative to
  // window_rect origin.
  gfx::Rect clip_rect;
  std::vector<gfx::Rect> cutout_rects;
  bool rects_valid;
  bool visible;
};

}  // namespace content

#endif  // CONTENT_COMMON_WEBPLUGIN_GEOMETRY_H_
