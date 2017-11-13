// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_RENDERER_H_
#define CHROME_BROWSER_VR_UI_RENDERER_H_

#include "chrome/browser/vr/ui_input_manager.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace vr {

class UiElement;
class UiScene;
class UiElement;
class UiElementRenderer;

// Provides information for rendering such as the viewport and view/projection
// matrix.
struct RenderInfo {
  struct EyeInfo {
    gfx::Rect viewport;
    gfx::Transform view_matrix;
    gfx::Transform proj_matrix;
    gfx::Transform view_proj_matrix;
  };

  gfx::Transform head_pose;
  EyeInfo left_eye_info;
  EyeInfo right_eye_info;

  gfx::Size surface_texture_size;
};

// Renders a UI scene.
class UiRenderer {
 public:
  UiRenderer(UiScene* scene, UiElementRenderer* ui_element_renderer);
  ~UiRenderer();

  void Draw(const RenderInfo& render_info);

  // This is exposed separately because we do a separate pass to render this
  // content into an optimized viewport.
  void DrawWebVrOverlayForeground(const RenderInfo& render_info);

  static std::vector<const UiElement*> GetElementsInDrawOrder(
      const std::vector<const UiElement*>& elements);

 private:
  void Draw2dBrowsing(const RenderInfo& render_info);
  void DrawSplashScreen(const RenderInfo& render_info);

  void DrawUiView(const RenderInfo& render_info,
                  const std::vector<const UiElement*>& elements);
  void DrawElements(const gfx::Transform& view_proj_matrix,
                    const std::vector<const UiElement*>& elements,
                    const RenderInfo& render_info);
  void DrawElement(const gfx::Transform& view_proj_matrix,
                   const UiElement& element);

  UiScene* scene_ = nullptr;
  UiElementRenderer* ui_element_renderer_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_RENDERER_H_
