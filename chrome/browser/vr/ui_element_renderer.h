// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
#define CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_

#include "chrome/browser/vr/controller_mesh.h"
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
                                float opacity,
                                gfx::SizeF element_size,
                                float corner_radius) = 0;

  virtual void DrawGradientGridQuad(const gfx::Transform& view_proj_matrix,
                                    const SkColor edge_color,
                                    const SkColor center_color,
                                    const SkColor grid_color,
                                    int gridline_count,
                                    float opacity) = 0;

  // TODO(crbug/779108) This presumes a Daydream controller.
  virtual void DrawController(ControllerMesh::State state,
                              float opacity,
                              const gfx::Transform& view_proj_matrix) = 0;

  virtual void DrawLaser(float opacity,
                         const gfx::Transform& view_proj_matrix) = 0;

  virtual void DrawReticle(float opacity,
                           const gfx::Transform& view_proj_matrix) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_ELEMENT_RENDERER_H_
