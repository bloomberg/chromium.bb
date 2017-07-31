// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
#define CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class RectF;
class SizeF;
class Transform;
}  // namespace gfx

namespace vr {

// This is the interface offered by VrShell's GL system to UI elements.
class UiElementRenderer {
 public:
  enum TextureLocation {
    kTextureLocationLocal,
    kTextureLocationExternal,
  };

  virtual ~UiElementRenderer() {}

  virtual void DrawTexturedQuad(int texture_data_handle,
                                TextureLocation texture_location,
                                const gfx::Transform& view_proj_matrix,
                                const gfx::RectF& copy_rect,
                                float opacity,
                                gfx::SizeF element_size,
                                float corner_radius) = 0;

  virtual void DrawGradientQuad(const gfx::Transform& view_proj_matrix,
                                const SkColor edge_color,
                                const SkColor center_color,
                                float opacity) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
