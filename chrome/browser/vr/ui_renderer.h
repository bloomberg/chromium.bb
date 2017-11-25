// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_RENDERER_H_
#define CHROME_BROWSER_VR_UI_RENDERER_H_

#include "chrome/browser/vr/model/camera_model.h"
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
  gfx::Transform head_pose;
  gfx::Size surface_texture_size;
  CameraModel left_eye_model;
  CameraModel right_eye_model;
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
  void DrawController(const RenderInfo& render_info);

  void DrawUiView(const RenderInfo& render_info,
                  const std::vector<const UiElement*>& elements);
  void DrawElements(const CameraModel& camera_model,
                    const std::vector<const UiElement*>& elements,
                    const RenderInfo& render_info);
  void DrawElement(const CameraModel& camera_model, const UiElement& element);

  UiScene* scene_ = nullptr;
  UiElementRenderer* ui_element_renderer_ = nullptr;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_RENDERER_H_
