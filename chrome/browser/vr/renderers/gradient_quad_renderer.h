// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_GRADIENT_QUAD_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_GRADIENT_QUAD_RENDERER_H_

#include "base/macros.h"
#include "chrome/browser/vr/renderers/base_renderer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class SizeF;
class Transform;
}  // namespace gfx

namespace vr {

struct CornerRadii;

class GradientQuadRenderer : public BaseRenderer {
 public:
  GradientQuadRenderer();
  ~GradientQuadRenderer() override;

  void Draw(const gfx::Transform& model_view_proj_matrix,
            SkColor edge_color,
            SkColor center_color,
            float opacity,
            const gfx::SizeF& element_size,
            const CornerRadii& radii);

 private:
  GLuint model_view_proj_matrix_handle_;
  GLuint ul_corner_offset_handle_;
  GLuint ur_corner_offset_handle_;
  GLuint lr_corner_offset_handle_;
  GLuint ll_corner_offset_handle_;
  GLuint corner_position_handle_;
  GLuint offset_scale_handle_;
  GLuint opacity_handle_;
  GLuint center_color_handle_;
  GLuint edge_color_handle_;

  DISALLOW_COPY_AND_ASSIGN(GradientQuadRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_GRADIENT_QUAD_RENDERER_H_
