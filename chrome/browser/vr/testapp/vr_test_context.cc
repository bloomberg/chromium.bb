// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/vr_test_context.h"

#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/test/constants.h"
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
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/font_list.h"

namespace vr {

namespace {

// Constants related to scaling of the UI for zooming, to let us look at
// elements up close. The adjustment factor determines how much the zoom changes
// at each adjustment step.
constexpr float kDefaultViewScaleFactor = 1.2f;
constexpr float kMinViewScaleFactor = 0.5f;
constexpr float kMaxViewScaleFactor = 5.0f;
constexpr float kViewScaleAdjustmentFactor = 0.2f;

constexpr gfx::Point3F kLaserOrigin = {0.5f, -0.5f, 0.f};

}  // namespace

VrTestContext::VrTestContext()
    : controller_info_(base::MakeUnique<ControllerInfo>()),
      view_scale_factor_(kDefaultViewScaleFactor) {
  controller_info_->reticle_render_target = nullptr;
  controller_info_->laser_origin = kLaserOrigin;

  base::FilePath pak_path;
  PathService::Get(base::DIR_MODULE, &pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_test.pak"));

  base::i18n::InitializeICU();

  ui_ = base::MakeUnique<Ui>(this, this, UiInitialState());

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

  const gfx::Transform proj_matrix(view_scale_factor_, 0, 0, 0, 0,
                                   view_scale_factor_, 0, 0, 0, 0, -1, 0, 0, 0,
                                   -1, 0);
  RenderInfo render_info;
  render_info.head_pose = head_pose_;
  render_info.surface_texture_size = window_size_;
  render_info.left_eye_info.viewport = gfx::Rect(window_size_);
  render_info.left_eye_info.view_matrix = head_pose_;
  render_info.left_eye_info.proj_matrix = proj_matrix;
  render_info.left_eye_info.view_proj_matrix = proj_matrix * head_pose_;

  // Update the render position of all UI elements (including desktop).
  ui_->scene()->OnBeginFrame(current_time, kForwardVector);
  ui_->OnProjMatrixChanged(render_info.left_eye_info.proj_matrix);
  ui_->ui_renderer()->Draw(render_info, *controller_info_);

  // TODO(cjgrant): Render viewport-aware elements.
}

void VrTestContext::HandleInput(ui::Event* event) {
  if (event->IsKeyEvent()) {
    if (event->type() != ui::ET_KEY_PRESSED) {
      return;
    }
    switch (event->AsKeyEvent()->code()) {
      case ui::DomCode::ESCAPE:
        view_scale_factor_ = kDefaultViewScaleFactor;
        head_angle_x_degrees_ = 0;
        head_angle_y_degrees_ = 0;
        head_pose_ = gfx::Transform();
        break;
      case ui::DomCode::US_F:
        fullscreen_ = !fullscreen_;
        ui_->SetFullscreen(fullscreen_);
        break;
      case ui::DomCode::US_I:
        incognito_ = !incognito_;
        ui_->SetIncognito(incognito_);
        break;
      default:
        break;
    }
    return;
  }

  if (event->IsMouseWheelEvent()) {
    int direction =
        base::ClampToRange(event->AsMouseWheelEvent()->y_offset(), -1, 1);
    view_scale_factor_ *= (1 + direction * kViewScaleAdjustmentFactor);
    view_scale_factor_ = base::ClampToRange(
        view_scale_factor_, kMinViewScaleFactor, kMaxViewScaleFactor);
    return;
  }

  if (!event->IsMouseEvent()) {
    return;
  }

  const ui::MouseEvent* mouse_event = event->AsMouseEvent();

  // Move the head pose if needed.
  if (mouse_event->IsRightMouseButton()) {
    if (last_drag_x_pixels_ != 0 || last_drag_y_pixels_ != 0) {
      float angle_y = 180.f *
                      ((mouse_event->x() - last_drag_x_pixels_) - 0.5f) /
                      window_size_.width();
      float angle_x = 180.f *
                      ((mouse_event->y() - last_drag_y_pixels_) - 0.5f) /
                      window_size_.height();
      head_angle_x_degrees_ += angle_x;
      head_angle_y_degrees_ += angle_y;
      head_angle_x_degrees_ =
          base::ClampToRange(head_angle_x_degrees_, -90.f, 90.f);
    }
    last_drag_x_pixels_ = mouse_event->x();
    last_drag_y_pixels_ = mouse_event->y();
  } else {
    last_drag_x_pixels_ = 0;
    last_drag_y_pixels_ = 0;
  }

  head_pose_ = gfx::Transform();
  head_pose_.RotateAboutXAxis(-head_angle_x_degrees_);
  head_pose_.RotateAboutYAxis(-head_angle_y_degrees_);

  // Determine a controller beam angle. Compute the beam angle relative to the
  // head pose so it's easier to hit something you're looking at.
  float delta_x = -180.f * ((mouse_event->y() / window_size_.height()) - 0.5f);
  float delta_y = -180.f * ((mouse_event->x() / window_size_.width()) - 0.5f);
  gfx::Transform beam_transform;
  beam_transform.RotateAboutYAxis(head_angle_y_degrees_ + delta_y);
  beam_transform.RotateAboutXAxis(head_angle_x_degrees_ + delta_x);
  gfx::Vector3dF controller_direction = {0, 0, -1.f};
  beam_transform.TransformVector(&controller_direction);

  GestureList gesture_list;
  ui_->input_manager()->HandleInput(
      controller_direction, controller_info_->laser_origin,
      controller_info_->touchpad_button_state, &gesture_list,
      &controller_info_->target_point,
      &controller_info_->reticle_render_target);
}

void VrTestContext::OnGlInitialized(const gfx::Size& window_size) {
  unsigned int content_texture_id = CreateFakeContentTexture();

  window_size_ = window_size;
  ui_->OnGlInitialized(content_texture_id,
                       UiElementRenderer::kTextureLocationLocal);
}

unsigned int VrTestContext::CreateFakeContentTexture() {
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(1, 1);
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(0xFF000080);

  SkPixmap pixmap;
  CHECK(surface->peekPixels(&pixmap));

  SkColorType type = pixmap.colorType();
  DCHECK(type == kRGBA_8888_SkColorType || type == kBGRA_8888_SkColorType);
  GLint format = (type == kRGBA_8888_SkColorType ? GL_RGBA : GL_BGRA);

  unsigned int texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, format, pixmap.width(), pixmap.height(), 0,
               format, GL_UNSIGNED_BYTE, pixmap.addr());

  return texture_id;
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
