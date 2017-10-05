// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_renderer.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/vr_controller_model.h"
#include "chrome/browser/vr/vr_shell_renderer.h"
#include "ui/gl/gl_bindings.h"

namespace vr {

namespace {

static constexpr gfx::Point3F kOrigin = {0.0f, 0.0f, 0.0f};

// Fraction of the distance to the object the reticle is drawn at to avoid
// rounding errors drawing the reticle behind the object.
// TODO(mthiesse): Find a better approach for drawing the reticle on an object.
// Right now we have to wedge it very precisely between the content window and
// backplane to avoid rendering artifacts. We should stop using the depth buffer
// since the back-to-front order of our elements is well defined. This would,
// among other things, prevent z-fighting when we draw content in the same
// plane.
static constexpr float kReticleOffset = 0.999f;

static constexpr float kLaserWidth = 0.01f;

static constexpr float kReticleWidth = 0.025f;
static constexpr float kReticleHeight = 0.025f;

}  // namespace

UiRenderer::UiRenderer(UiScene* scene, VrShellRenderer* vr_shell_renderer)
    : scene_(scene), vr_shell_renderer_(vr_shell_renderer) {}

UiRenderer::~UiRenderer() = default;

// TODO(crbug.com/767515): UiRenderer must not care about the elements its
// rendering and be platform agnostic, each element should know how to render
// itself correctly.
void UiRenderer::Draw(const RenderInfo& render_info,
                      const ControllerInfo& controller_info) {
  Draw2dBrowsing(render_info, controller_info);
  DrawSplashScreen(render_info, controller_info);
}

void UiRenderer::Draw2dBrowsing(const RenderInfo& render_info,
                                const ControllerInfo& controller_info) {
  const auto& elements = scene_->GetVisible2dBrowsingElements();
  const auto& elements_overlay = scene_->GetVisible2dBrowsingOverlayElements();
  if (elements.empty() && elements_overlay.empty())
    return;

  // TODO(crbug.com/767583): The controller is drawn as part of a call to
  // DrawUiView for 2d browsing elements. This means that the controller is not
  // supported with overlay elements.
  if (!elements.empty()) {
    // Enable depth testing. Note that we do not clear the color buffer. The
    // scene's background elements are responsible for drawing a complete
    // background.
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    DrawUiView(render_info, controller_info, elements, kReticleVisible);
  }

  if (elements_overlay.empty())
    return;

  // The overlays do not make use of depth testing.
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  DrawUiView(render_info, controller_info, elements_overlay, kReticleHidden);
}

void UiRenderer::DrawSplashScreen(const RenderInfo& render_info,
                                  const ControllerInfo& controller_info) {
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
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  DrawUiView(render_info, controller_info, elements, kReticleHidden);
  // NB: we do not draw the viewport aware objects here. They get put into
  // another buffer that is size optimized.
}

void UiRenderer::DrawWebVrOverlayForeground(
    const RenderInfo& render_info,
    const ControllerInfo& controller_info) {
  // The WebVR overlay foreground is drawn as a separate pass, so we need to set
  // up our gl state before drawing.
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  DrawUiView(render_info, controller_info,
             scene_->GetVisibleWebVrOverlayForegroundElements(),
             kReticleHidden);
}

void UiRenderer::DrawUiView(const RenderInfo& render_info,
                            const ControllerInfo& controller_info,
                            const std::vector<const UiElement*>& elements,
                            UiRenderer::ReticleMode reticle_mode) {
  TRACE_EVENT0("gpu", "VrShellGl::DrawUiView");

  auto sorted_elements =
      GetElementsInDrawOrder(render_info.head_pose, elements);

  for (auto& eye_info :
       {render_info.left_eye_info, render_info.right_eye_info}) {
    glViewport(eye_info.viewport.x(), eye_info.viewport.y(),
               eye_info.viewport.width(), eye_info.viewport.height());

    DrawElements(eye_info.view_proj_matrix, sorted_elements, render_info,
                 controller_info, reticle_mode);
    if (reticle_mode == kReticleVisible) {
      DrawController(eye_info.view_proj_matrix, render_info, controller_info);
      DrawLaser(eye_info.view_proj_matrix, render_info, controller_info);
    }
  }
}

void UiRenderer::DrawElements(const gfx::Transform& view_proj_matrix,
                              const std::vector<const UiElement*>& elements,
                              const RenderInfo& render_info,
                              const ControllerInfo& controller_info,
                              UiRenderer::ReticleMode reticle_mode) {
  if (elements.empty()) {
    return;
  }
  vr_shell_renderer_->set_surface_texture_size(
      render_info.surface_texture_size);
  bool drawn_reticle = false;
  for (const auto* element : elements) {
    // If we have no element to draw the reticle on, draw it after the
    // background (the initial draw phase).
    if (!controller_info.reticle_render_target &&
        reticle_mode == kReticleVisible && !drawn_reticle &&
        element->draw_phase() >= scene_->first_foreground_draw_phase()) {
      DrawReticle(view_proj_matrix, render_info, controller_info);
      drawn_reticle = true;
    }

    DrawElement(view_proj_matrix, *element);

    if (reticle_mode == kReticleVisible &&
        (controller_info.reticle_render_target == element)) {
      DrawReticle(view_proj_matrix, render_info, controller_info);
    }
  }
  vr_shell_renderer_->Flush();
}

void UiRenderer::DrawElement(const gfx::Transform& view_proj_matrix,
                             const UiElement& element) {
  DCHECK_GE(element.draw_phase(), 0);
  element.Render(vr_shell_renderer_,
                 view_proj_matrix * element.world_space_transform());
}

std::vector<const UiElement*> UiRenderer::GetElementsInDrawOrder(
    const gfx::Transform& view_matrix,
    const std::vector<const UiElement*>& elements) {
  std::vector<const UiElement*> sorted_elements = elements;

  // Sort elements primarily based on their draw phase (lower draw phase first)
  // and secondarily based on their z-axis distance (more distant first).
  // TODO(mthiesse, crbug.com/721356): This will not work well for elements not
  // directly in front of the user, but works well enough for our initial
  // release, and provides a consistent ordering that we can easily design
  // around.
  std::sort(sorted_elements.begin(), sorted_elements.end(),
            [](const UiElement* first, const UiElement* second) {
              if (first->draw_phase() != second->draw_phase()) {
                return first->draw_phase() < second->draw_phase();
              } else {
                return first->world_space_transform().matrix().get(2, 3) <
                       second->world_space_transform().matrix().get(2, 3);
              }
            });

  return sorted_elements;
}

void UiRenderer::DrawReticle(const gfx::Transform& render_matrix,
                             const RenderInfo& render_info,
                             const ControllerInfo& controller_info) {
  // Scale the reticle to have a fixed FOV size at any distance.
  const float eye_to_target =
      std::sqrt(controller_info.target_point.SquaredDistanceTo(kOrigin));

  gfx::Transform mat;
  mat.Scale3d(kReticleWidth * eye_to_target, kReticleHeight * eye_to_target, 1);

  gfx::Quaternion rotation;

  if (controller_info.reticle_render_target != nullptr) {
    // Make the reticle planar to the element it's hitting.
    rotation =
        gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                        -controller_info.reticle_render_target->GetNormal());
  } else {
    // Rotate the reticle to directly face the eyes.
    rotation = gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                               controller_info.target_point - kOrigin);
  }
  gfx::Transform rotation_mat(rotation);
  mat = rotation_mat * mat;

  gfx::Point3F target_point =
      ScalePoint(controller_info.target_point, kReticleOffset);
  // Place the pointer slightly in front of the plane intersection point.
  mat.matrix().postTranslate(target_point.x(), target_point.y(),
                             target_point.z());

  gfx::Transform transform = render_matrix * mat;
  vr_shell_renderer_->GetReticleRenderer()->Draw(transform);
}

void UiRenderer::DrawLaser(const gfx::Transform& render_matrix,
                           const RenderInfo& render_info,
                           const ControllerInfo& controller_info) {
  // Find the length of the beam (from hand to target).
  const float laser_length =
      std::sqrt(controller_info.laser_origin.SquaredDistanceTo(
          ScalePoint(controller_info.target_point, kReticleOffset)));

  // Build a beam, originating from the origin.
  gfx::Transform mat;

  // Move the beam half its height so that its end sits on the origin.
  mat.matrix().postTranslate(0.0f, 0.5f, 0.0f);
  mat.matrix().postScale(kLaserWidth, laser_length, 1);

  // Tip back 90 degrees to flat, pointing at the scene.
  const gfx::Quaternion quat(gfx::Vector3dF(1.0f, 0.0f, 0.0f), -M_PI / 2);
  gfx::Transform rotation_mat(quat);
  mat = rotation_mat * mat;

  const gfx::Vector3dF beam_direction =
      controller_info.target_point - controller_info.laser_origin;

  gfx::Transform beam_direction_mat(
      gfx::Quaternion(gfx::Vector3dF(0.0f, 0.0f, -1.0f), beam_direction));

  // Render multiple faces to make the laser appear cylindrical.
  const int faces = 4;
  gfx::Transform face_transform;
  gfx::Transform transform;
  for (int i = 0; i < faces; i++) {
    // Rotate around Z.
    const float angle = M_PI * 2 * i / faces;
    const gfx::Quaternion rot({0.0f, 0.0f, 1.0f}, angle);
    face_transform = beam_direction_mat * gfx::Transform(rot) * mat;

    // Move the beam origin to the hand.
    face_transform.matrix().postTranslate(controller_info.laser_origin.x(),
                                          controller_info.laser_origin.y(),
                                          controller_info.laser_origin.z());
    transform = render_matrix * face_transform;
    vr_shell_renderer_->GetLaserRenderer()->Draw(controller_info.opacity,
                                                 transform);
  }
}

void UiRenderer::DrawController(const gfx::Transform& view_proj_matrix,
                                const RenderInfo& render_info,
                                const ControllerInfo& controller_info) {
  if (!vr_shell_renderer_->GetControllerRenderer()->IsSetUp()) {
    return;
  }

  VrControllerModel::State state;
  if (controller_info.touchpad_button_state ==
      UiInputManager::ButtonState::DOWN) {
    state = VrControllerModel::TOUCHPAD;
  } else if (controller_info.app_button_state ==
             UiInputManager::ButtonState::DOWN) {
    state = VrControllerModel::APP;
  } else if (controller_info.home_button_state ==
             UiInputManager::ButtonState::DOWN) {
    state = VrControllerModel::SYSTEM;
  } else {
    state = VrControllerModel::IDLE;
  }

  gfx::Transform transform = view_proj_matrix * controller_info.transform;
  vr_shell_renderer_->GetControllerRenderer()->Draw(
      state, controller_info.opacity, transform);
}

}  // namespace vr
