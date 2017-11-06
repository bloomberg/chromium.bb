// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/vr_test_context.h"

#include "base/i18n/icu_util.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/vr/controller_mesh.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/toolbar_state.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "chrome/browser/vr/vr_shell_renderer.h"
#include "components/omnibox/browser/vector_icons.h"
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
constexpr gfx::Vector3dF kLaserLocalOffset = {0.f, -0.0075f, -0.05f};
constexpr float kControllerScaleFactor = 1.5f;

}  // namespace

VrTestContext::VrTestContext() : view_scale_factor_(kDefaultViewScaleFactor) {
  base::FilePath pak_path;
  PathService::Get(base::DIR_MODULE, &pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_test.pak"));

  base::i18n::InitializeICU();

  ui_ = base::MakeUnique<Ui>(this, nullptr, UiInitialState());

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

  GestureList gesture_list;
  ReticleModel reticle_model;
  ui_->input_manager()->HandleInput(base::TimeTicks::Now(),
                                    last_controller_model_, &reticle_model,
                                    &gesture_list);
  ui_->OnControllerUpdated(last_controller_model_, reticle_model);

  // Update the render position of all UI elements (including desktop).
  ui_->scene()->OnBeginFrame(current_time, kForwardVector);
  ui_->OnProjMatrixChanged(render_info.left_eye_info.proj_matrix);
  ui_->ui_renderer()->Draw(render_info);

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
      case ui::DomCode::US_S: {
        CreateFakeOmniboxSuggestions();
        break;
      }
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

  // TODO(cjgrant): Figure out why, quite regularly, mouse click events do not
  // make it into this method and are missed.
  if (mouse_event->IsLeftMouseButton()) {
    if (mouse_event->type() == ui::ET_MOUSE_PRESSED) {
      touchpad_pressed_ = true;
    } else if (mouse_event->type() == ui::ET_MOUSE_RELEASED) {
      touchpad_pressed_ = false;
    }
  }

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

  gfx::Vector3dF local_offset = kLaserLocalOffset;
  beam_transform.TransformVector(&local_offset);
  local_offset.Scale(kControllerScaleFactor);

  ControllerModel controller_model;
  controller_model.touchpad_button_state =
      touchpad_pressed_ ? UiInputManager::DOWN : UiInputManager::UP;
  controller_model.laser_direction = controller_direction;
  controller_model.laser_origin = kLaserOrigin + local_offset;

  controller_model.transform.Translate3d(kLaserOrigin.x(), kLaserOrigin.y(),
                                         kLaserOrigin.z());

  controller_model.transform.Scale3d(
      kControllerScaleFactor, kControllerScaleFactor, kControllerScaleFactor);
  controller_model.transform.RotateAboutYAxis(head_angle_y_degrees_ + delta_y);
  controller_model.transform.RotateAboutXAxis(head_angle_x_degrees_ + delta_x);

  GestureList gesture_list;
  ReticleModel reticle_model;
  ui_->input_manager()->HandleInput(base::TimeTicks::Now(), controller_model,
                                    &reticle_model, &gesture_list);
  last_controller_model_ = controller_model;
  ui_->OnControllerUpdated(controller_model, reticle_model);
}

void VrTestContext::OnGlInitialized(const gfx::Size& window_size) {
  unsigned int content_texture_id = CreateFakeContentTexture();

  window_size_ = window_size;
  ui_->OnGlInitialized(content_texture_id,
                       UiElementRenderer::kTextureLocationLocal);

  ui_->vr_shell_renderer()->GetControllerRenderer()->SetUp(
      ControllerMesh::LoadFromResources());
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

void VrTestContext::CreateFakeOmniboxSuggestions() {
  // Every time this method is called, change the number of suggestions shown.
  static int num_suggestions = 0;
  num_suggestions = (num_suggestions + 1) % 4;

  auto result = base::MakeUnique<OmniboxSuggestions>();
  for (int i = 0; i < num_suggestions; i++) {
    result->suggestions.emplace_back(OmniboxSuggestion(
        base::UTF8ToUTF16("Suggestion ") + base::IntToString16(i + 1),
        base::UTF8ToUTF16("Description text"),
        AutocompleteMatch::Type::VOICE_SUGGEST));
  }
  ui_->SetOmniboxSuggestions(std::move(result));
}

void VrTestContext::SetVoiceSearchActive(bool active) {}
void VrTestContext::ExitPresent() {}
void VrTestContext::ExitFullscreen() {}
void VrTestContext::NavigateBack() {}
void VrTestContext::ExitCct() {}
void VrTestContext::OnUnsupportedMode(vr::UiUnsupportedMode mode) {}
void VrTestContext::OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                                         vr::ExitVrPromptChoice choice) {}
void VrTestContext::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {}

}  // namespace vr
