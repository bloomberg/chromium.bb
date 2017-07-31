// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_RENDERER_H_
#define CHROME_BROWSER_VR_UI_RENDERER_H_

#include "chrome/browser/vr/ui_input_manager.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace vr {

class UiScene;
class UiElement;
class VrShellRenderer;

// Provides information needed to render the controller.
struct ControllerInfo {
  gfx::Point3F target_point;
  gfx::Point3F laser_origin;
  UiInputManager::ButtonState touchpad_button_state;
  UiInputManager::ButtonState app_button_state;
  UiInputManager::ButtonState home_button_state;
  gfx::Transform transform;
  float opacity;
  UiElement* reticle_render_target;
};

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
  UiRenderer(UiScene* scene,
             VrShellRenderer* vr_shell_renderer);
  ~UiRenderer();

  void Draw(const RenderInfo& render_info,
            const ControllerInfo& controller_info,
            bool web_vr_mode);
  void DrawHeadLocked(const RenderInfo& render_info,
                      const ControllerInfo& controller_info);

 private:
  void DrawWorldElements(const RenderInfo& render_info,
                         const ControllerInfo& controller_info,
                         bool web_vr_mode);
  void DrawOverlayElements(const RenderInfo& render_info,
                           const ControllerInfo& controller_info);
  void DrawUiView(const RenderInfo& render_info,
                  const ControllerInfo& controller_info,
                  const std::vector<const UiElement*>& elements,
                  bool draw_reticle);
  void DrawElements(const gfx::Transform& view_proj_matrix,
                    const std::vector<const UiElement*>& elements,
                    const RenderInfo& render_info,
                    const ControllerInfo& controller_info,
                    bool draw_reticle);
  void DrawElement(const gfx::Transform& view_proj_matrix,
                   const UiElement& element);
  std::vector<const UiElement*> GetElementsInDrawOrder(
      const gfx::Transform& view_matrix,
      const std::vector<const UiElement*>& elements);
  void DrawReticle(const gfx::Transform& render_matrix,
                   const RenderInfo& render_info,
                   const ControllerInfo& controller_info);
  void DrawLaser(const gfx::Transform& render_matrix,
                 const RenderInfo& render_info,
                 const ControllerInfo& controller_info);
  void DrawController(const gfx::Transform& view_proj_matrix,
                      const RenderInfo& render_info,
                      const ControllerInfo& controller_info);

  UiScene* scene_ = nullptr;
  VrShellRenderer* vr_shell_renderer_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_RENDERER_H_
