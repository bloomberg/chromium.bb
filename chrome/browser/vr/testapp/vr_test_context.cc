// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/vr_test_context.h"

#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/toolbar_state.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "chrome/browser/vr/vr_shell_renderer.h"
#include "components/security_state/core/security_state.h"
#include "components/toolbar/vector_icons.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/font_list.h"

namespace vr {

namespace {

constexpr float kDefaultViewScale = 1.4f;
constexpr float kMousePaneWidth = 5.0f;
constexpr float kMousePaneDepth = 2.0f;

}  // namespace

VrTestContext::VrTestContext()
    : ui_(base::MakeUnique<Ui>(this, this, UiInitialState())),
      controller_info_(base::MakeUnique<ControllerInfo>()) {
  controller_info_->reticle_render_target = nullptr;

  base::FilePath pak_path;
  PathService::Get(base::DIR_MODULE, &pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_test.pak"));

  base::i18n::InitializeICU();

  GURL gurl("https://dangerous.com/dir/file.html");
  ToolbarState state(gurl, security_state::SecurityLevel::DANGEROUS,
                     &toolbar::kHttpsInvalidIcon,
                     base::UTF8ToUTF16("Not secure"), true, false);
  ui_->SetToolbarState(state);
  ui_->SetHistoryButtonsEnabled(true, true);
  ui_->SetLoading(true);
  ui_->SetLoadProgress(0.4);
  ui_->SetVideoCapturingIndicator(true);
  ui_->SetScreenCapturingIndicator(true);
  ui_->SetAudioCapturingIndicator(true);
  ui_->SetBluetoothConnectedIndicator(true);
  ui_->SetLocationAccessIndicator(true);
}

VrTestContext::~VrTestContext() = default;

void VrTestContext::DrawFrame() {
  base::TimeTicks current_time = base::TimeTicks::Now();

  const gfx::Transform proj_matrix(kDefaultViewScale, 0, 0, 0, 0,
                                   kDefaultViewScale, 0, 0, 0, 0, -1, 0, 0, 0,
                                   -1, 0);
  const gfx::Transform head_pose;
  RenderInfo render_info;
  render_info.head_pose = head_pose;
  render_info.surface_texture_size = window_size_;
  render_info.left_eye_info.viewport = gfx::Rect(window_size_);
  render_info.left_eye_info.view_matrix = head_pose;
  render_info.left_eye_info.proj_matrix = proj_matrix;
  render_info.left_eye_info.view_proj_matrix = proj_matrix * head_pose;

  // Update the render position of all UI elements (including desktop).
  ui_->scene()->OnBeginFrame(current_time, gfx::Vector3dF(0.f, 0.f, -1.0f));
  ui_->scene()->PrepareToDraw();
  ui_->OnProjMatrixChanged(render_info.left_eye_info.proj_matrix);
  ui_->ui_renderer()->Draw(render_info, *controller_info_);

  // TODO(cjgrant): Render viewport-aware elements.
}

void VrTestContext::HandleInput(ui::Event* event) {
  if (event->IsKeyEvent()) {
    const ui::KeyEvent* key_event = event->AsKeyEvent();
    if (key_event->code() == ui::DomCode::US_F) {
      ui_->SetFullscreen(true);
    }
    return;
  }

  if (!event->IsMouseEvent()) {
    return;
  }

  // Synthesize a controller direction based on mouse position in the window.
  const ui::MouseEvent* mouse_event = event->AsMouseEvent();
  gfx::Point3F laser_origin{0.5, -0.5, 0};
  gfx::Point3F target_point = {
      kMousePaneWidth * (mouse_event->x() / window_size_.width() - 0.5),
      kMousePaneWidth * window_size_.height() / window_size_.width() *
          (-mouse_event->y() / window_size_.height() + 0.5),
      -kMousePaneDepth};
  gfx::Vector3dF controller_direction = target_point - laser_origin;
  controller_info_->laser_origin = laser_origin;
  controller_info_->reticle_render_target = nullptr;

  GestureList gesture_list;
  ui_->input_manager()->HandleInput(
      controller_direction, controller_info_->laser_origin,
      controller_info_->touchpad_button_state, &gesture_list,
      &controller_info_->target_point,
      &controller_info_->reticle_render_target);
}

void VrTestContext::OnGlInitialized(const gfx::Size& window_size) {
  window_size_ = window_size;
  ui_->OnGlInitialized(0);
}

void VrTestContext::OnContentEnter(const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentLeave() {}

void VrTestContext::OnContentMove(const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentDown(const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentUp(const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentFlingStart(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentFlingCancel(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentScrollBegin(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentScrollUpdate(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}

void VrTestContext::OnContentScrollEnd(
    std::unique_ptr<blink::WebGestureEvent> gesture,
    const gfx::PointF& normalized_hit_point) {}

void VrTestContext::ExitPresent() {}
void VrTestContext::ExitFullscreen() {}
void VrTestContext::NavigateBack() {}
void VrTestContext::ExitCct() {}
void VrTestContext::OnUnsupportedMode(vr::UiUnsupportedMode mode) {}
void VrTestContext::OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                                         vr::ExitVrPromptChoice choice) {}
void VrTestContext::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {}

}  // namespace vr
