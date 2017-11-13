// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_renderer.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "ui/gl/gl_bindings.h"

namespace vr {

UiRenderer::UiRenderer(UiScene* scene, UiElementRenderer* ui_element_renderer)
    : scene_(scene), ui_element_renderer_(ui_element_renderer) {}

UiRenderer::~UiRenderer() = default;

// TODO(crbug.com/767515): UiRenderer must not care about the elements it's
// rendering and be platform agnostic, each element should know how to render
// itself correctly.
void UiRenderer::Draw(const RenderInfo& render_info) {
  Draw2dBrowsing(render_info);
  DrawSplashScreen(render_info);
}

void UiRenderer::Draw2dBrowsing(const RenderInfo& render_info) {
  const auto& elements = scene_->GetVisible2dBrowsingElements();
  const auto& elements_overlay = scene_->GetVisible2dBrowsingOverlayElements();
  const auto& controller_elements = scene_->GetVisibleControllerElements();
  if (elements.empty() && elements_overlay.empty())
    return;

  if (!elements.empty()) {
    // Note that we do not clear the color buffer. The scene's background
    // elements are responsible for drawing a complete background.
    glEnable(GL_CULL_FACE);
    DrawUiView(render_info, elements);
  }

  if (elements_overlay.empty() && controller_elements.empty())
    return;

  // The overlays do not make use of depth testing.
  glDisable(GL_CULL_FACE);
  DrawUiView(render_info, elements_overlay);

  // We do want to cull backfaces on the controller, however.
  glEnable(GL_CULL_FACE);
  DrawUiView(render_info, controller_elements);
}

void UiRenderer::DrawSplashScreen(const RenderInfo& render_info) {
  const auto& elements = scene_->GetVisibleSplashScreenElements();
  if (elements.empty())
    return;

  // WebVR is incompatible with 3D world compositing since the
  // depth buffer was already populated with unknown scaling - the
  // WebVR app has full control over zNear/zFar. Just leave the
  // existing content in place in the primary buffer without
  // clearing. Currently, there aren't any world elements in WebVR
  // mode, this will need further testing if those get added
  // later.
  glDisable(GL_CULL_FACE);

  DrawUiView(render_info, elements);

  // NB: we do not draw the viewport aware objects here. They get put into
  // another buffer that is size optimized.
}

void UiRenderer::DrawWebVrOverlayForeground(const RenderInfo& render_info) {
  // The WebVR overlay foreground is drawn as a separate pass, so we need to set
  // up our gl state before drawing.
  glEnable(GL_CULL_FACE);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  DrawUiView(render_info, scene_->GetVisibleWebVrOverlayForegroundElements());
}

void UiRenderer::DrawUiView(const RenderInfo& render_info,
                            const std::vector<const UiElement*>& elements) {
  TRACE_EVENT0("gpu", "UiRenderer::DrawUiView");

  auto sorted_elements = GetElementsInDrawOrder(elements);

  for (auto& eye_info :
       {render_info.left_eye_info, render_info.right_eye_info}) {
    glViewport(eye_info.viewport.x(), eye_info.viewport.y(),
               eye_info.viewport.width(), eye_info.viewport.height());

    DrawElements(eye_info.view_proj_matrix, sorted_elements, render_info);
  }
}

void UiRenderer::DrawElements(const gfx::Transform& view_proj_matrix,
                              const std::vector<const UiElement*>& elements,
                              const RenderInfo& render_info) {
  if (elements.empty()) {
    return;
  }
  for (const auto* element : elements) {
    DrawElement(view_proj_matrix, *element);
  }
  ui_element_renderer_->Flush();
}

void UiRenderer::DrawElement(const gfx::Transform& view_proj_matrix,
                             const UiElement& element) {
  DCHECK_GE(element.draw_phase(), 0);
  element.Render(ui_element_renderer_,
                 view_proj_matrix * element.world_space_transform());
}

std::vector<const UiElement*> UiRenderer::GetElementsInDrawOrder(
    const std::vector<const UiElement*>& elements) {
  std::vector<const UiElement*> sorted_elements = elements;

  // Sort elements primarily based on their draw phase (lower draw phase first)
  // and secondarily based on their tree order (as specified by the sorted
  // |elements| vector).
  // TODO(vollick): update the predicate to take into account some notion of "3d
  // rendering contexts" and the ordering of the reticle wrt to other elements.
  std::stable_sort(sorted_elements.begin(), sorted_elements.end(),
                   [](const UiElement* first, const UiElement* second) {
                     return first->draw_phase() < second->draw_phase();
                   });

  return sorted_elements;
}

}  // namespace vr
