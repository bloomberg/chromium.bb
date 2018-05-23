// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_GRID_H_
#define CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_GRID_H_

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/renderers/base_quad_renderer.h"
#include "third_party/skia/include/core/SkColor.h"

namespace vr {

// Draws a quad with a radial gradient and grid lines.
class Grid : public Rect {
 public:
  Grid();
  ~Grid() override;

  void Render(UiElementRenderer* renderer,
              const CameraModel& model) const override;

  SkColor grid_color() const { return grid_color_; }
  void SetGridColor(SkColor grid_color);

  void NotifyClientColorAnimated(SkColor color,
                                 int target_property_id,
                                 cc::KeyframeModel* keyframe_model) override;

  int gridline_count() const { return gridline_count_; }
  void set_gridline_count(int gridline_count) {
    gridline_count_ = gridline_count;
  }

  class Renderer : public BaseQuadRenderer {
   public:
    Renderer();
    ~Renderer() override;

    void Draw(const gfx::Transform& model_view_proj_matrix,
              SkColor grid_color,
              int gridline_count,
              float opacity);

    static void CreateBuffers();

   private:
    GLuint model_view_proj_matrix_handle_;
    GLuint grid_color_handle_;
    GLuint opacity_handle_;
    GLuint lines_count_handle_;

    DISALLOW_COPY_AND_ASSIGN(Renderer);
  };

 private:
  SkColor grid_color_ = SK_ColorWHITE;
  int gridline_count_ = 1;

  DISALLOW_COPY_AND_ASSIGN(Grid);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_ENVIRONMENT_GRID_H_
