// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_FAKE_UI_ELEMENT_RENDERER_H_
#define CHROME_BROWSER_VR_TEST_FAKE_UI_ELEMENT_RENDERER_H_

#include "base/macros.h"
#include "chrome/browser/vr/ui_element_renderer.h"

namespace vr {

class FakeUiElementRenderer : public UiElementRenderer {
 public:
  FakeUiElementRenderer();
  ~FakeUiElementRenderer() override;

  float texture_opacity() { return texture_opacity_; }
  float gradient_opacity() { return gradient_opacity_; }
  float grid_opacity() { return grid_opacity_; }

  void DrawTexturedQuad(int texture_data_handle,
                        TextureLocation texture_location,
                        const gfx::Transform& view_proj_matrix,
                        const gfx::RectF& copy_rect,
                        float opacity,
                        gfx::SizeF element_size,
                        float corner_radius) override;

  void DrawGradientQuad(const gfx::Transform& view_proj_matrix,
                        const SkColor edge_color,
                        const SkColor center_color,
                        float opacity,
                        gfx::SizeF element_size,
                        float corner_radius) override;

  void DrawGradientGridQuad(const gfx::Transform& view_proj_matrix,
                            const SkColor edge_color,
                            const SkColor center_color,
                            const SkColor grid_color,
                            int gridline_count,
                            float opacity) override;

 private:
  float texture_opacity_ = -1.f;
  float gradient_opacity_ = -1.f;
  float grid_opacity_ = -1.f;

  DISALLOW_COPY_AND_ASSIGN(FakeUiElementRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_FAKE_UI_ELEMENT_RENDERER_H_
