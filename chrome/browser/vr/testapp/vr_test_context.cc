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
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
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
#include "ui/gfx/geometry/angle_conversions.h"

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

void RotateToward(const gfx::Vector3dF& fwd, gfx::Transform* transform) {
  gfx::Quaternion quat(kForwardVector, fwd);
  transform->PreconcatTransform(gfx::Transform(quat));
}

}  // namespace

VrTestContext::VrTestContext() : view_scale_factor_(kDefaultViewScaleFactor) {
  base::FilePath pak_path;
  PathService::Get(base::DIR_MODULE, &pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_test.pak"));

  base::i18n::InitializeICU();

  ui_ = base::MakeUnique<Ui>(this, nullptr, UiInitialState());
  model_ = ui_->model_for_test();

  ToolbarState state(GURL("https://dangerous.com/dir/file.html"),
                     security_state::SecurityLevel::HTTP_SHOW_WARNING,
                     &toolbar::kHttpIcon, base::string16(), true, false);
  ui_->SetToolbarState(state);
  ui_->SetHistoryButtonsEnabled(true, true);
  ui_->SetLoading(true);
  ui_->SetLoadProgress(0.4);
  ui_->SetVideoCaptureEnabled(true);
  ui_->SetScreenCaptureEnabled(true);
  ui_->SetAudioCaptureEnabled(true);
  ui_->SetBluetoothConnected(true);
  ui_->SetLocationAccess(true);
  ui_->input_manager()->set_hit_test_strategy(
      UiInputManager::PROJECT_TO_LASER_ORIGIN_FOR_TEST);
}

VrTestContext::~VrTestContext() = default;

void VrTestContext::DrawFrame() {
  base::TimeTicks current_time = base::TimeTicks::Now();

  RenderInfo render_info;
  render_info.head_pose = head_pose_;
  render_info.surface_texture_size = window_size_;
  render_info.left_eye_model.viewport = gfx::Rect(window_size_);
  render_info.left_eye_model.view_matrix = head_pose_;
  render_info.left_eye_model.proj_matrix = ProjectionMatrix();
  render_info.left_eye_model.view_proj_matrix = ViewProjectionMatrix();

  UpdateController();

  // Update the render position of all UI elements (including desktop).
  ui_->scene()->OnBeginFrame(current_time, kForwardVector);
  ui_->OnProjMatrixChanged(render_info.left_eye_model.proj_matrix);
  ui_->ui_renderer()->Draw(render_info);

  // This is required in order to show the WebVR toasts.
  if (model_->web_vr_has_produced_frames()) {
    ui_->ui_renderer()->DrawWebVrOverlayForeground(render_info);
  }

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
      case ui::DomCode::US_O:
        CreateFakeOmniboxSuggestions();
        break;
      case ui::DomCode::US_D:
        ui_->Dump();
        break;
      case ui::DomCode::US_V:
        CreateFakeVoiceSearchResult();
        break;
      case ui::DomCode::US_W:
        CycleWebVrModes();
        break;
      case ui::DomCode::US_S:
        ToggleSplashScreen();
        break;
      case ui::DomCode::US_R:
        ui_->OnWebVrFrameAvailable();
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

  last_mouse_point_ = gfx::Point(mouse_event->x(), mouse_event->y());
  last_controller_model_ = UpdateController();
}

ControllerModel VrTestContext::UpdateController() {
  // We could map mouse position to controller position, and skip this logic,
  // but it will make targeting elements with a mouse feel strange and not
  // mouse-like. Instead, we make the reticle track the mouse position linearly
  // by working from reticle position backwards to compute controller position.
  // We also don't apply the elbow model (the controller pivots around its
  // centroid), so do not expect the positioning of the controller in the test
  // app to exactly match what will happen in production.
  //
  // We first set up a controller model that simulates firing the laser directly
  // through a screen pixel. We do this by computing two points behind the mouse
  // position in normalized device coordinates. The z components are arbitrary.
  gfx::Point3F mouse_point_far(
      2.0 * last_mouse_point_.x() / window_size_.width() - 1.0,
      -2.0 * last_mouse_point_.y() / window_size_.height() + 1.0, 0.8);
  gfx::Point3F mouse_point_near = mouse_point_far;
  mouse_point_near.set_z(-0.8);

  // We then convert these points into world space via the inverse view proj
  // matrix. These points are then used to construct a temporary controller
  // model that pretends to be shooting through the pixel at the mouse position.
  gfx::Transform inv_view_proj;
  CHECK(ViewProjectionMatrix().GetInverse(&inv_view_proj));
  inv_view_proj.TransformPoint(&mouse_point_near);
  inv_view_proj.TransformPoint(&mouse_point_far);

  ControllerModel controller_model;
  controller_model.touchpad_button_state =
      touchpad_pressed_ ? UiInputManager::DOWN : UiInputManager::UP;

  controller_model.laser_origin = mouse_point_near;
  controller_model.laser_direction = mouse_point_far - mouse_point_near;
  CHECK(controller_model.laser_direction.GetNormalized(
      &controller_model.laser_direction));

  controller_model.transform.Translate3d(kLaserOrigin.x(), kLaserOrigin.y(),
                                         kLaserOrigin.z());
  controller_model.transform.Scale3d(
      kControllerScaleFactor, kControllerScaleFactor, kControllerScaleFactor);
  RotateToward(controller_model.laser_direction, &controller_model.transform);

  // Hit testing is done in terms of this synthesized controller model.
  GestureList gesture_list;
  ReticleModel reticle_model;
  ui_->input_manager()->HandleInput(base::TimeTicks::Now(), controller_model,
                                    &reticle_model, &gesture_list);

  // Now that we have accurate hit information, we use this to construct a
  // controller model for display.
  controller_model.laser_direction = reticle_model.target_point - kLaserOrigin;

  controller_model.transform.MakeIdentity();
  controller_model.transform.Translate3d(kLaserOrigin.x(), kLaserOrigin.y(),
                                         kLaserOrigin.z());
  controller_model.transform.Scale3d(
      kControllerScaleFactor, kControllerScaleFactor, kControllerScaleFactor);
  RotateToward(controller_model.laser_direction, &controller_model.transform);

  gfx::Vector3dF local_offset = kLaserLocalOffset;
  controller_model.transform.TransformVector(&local_offset);
  controller_model.laser_origin = kLaserOrigin + local_offset;

  ui_->OnControllerUpdated(controller_model, reticle_model);

  return controller_model;
}

void VrTestContext::OnGlInitialized() {
  unsigned int content_texture_id = CreateFakeContentTexture();

  ui_->OnGlInitialized(content_texture_id,
                       UiElementRenderer::kTextureLocationLocal, false);

  ui_->ui_element_renderer()->SetUpController(
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
        AutocompleteMatch::Type::VOICE_SUGGEST, GURL("http://www.test.com/")));
  }
  ui_->SetOmniboxSuggestions(std::move(result));
}

void VrTestContext::CreateFakeVoiceSearchResult() {
  if (!model_->speech.recognizing_speech)
    return;
  ui_->SetRecognitionResult(
      base::UTF8ToUTF16("I would like to see cat videos, please."));
  SetVoiceSearchActive(false);
}

void VrTestContext::CycleWebVrModes() {
  switch (model_->web_vr_timeout_state) {
    case kWebVrNoTimeoutPending:
      ui_->SetWebVrMode(true, false);
      break;
    case kWebVrAwaitingFirstFrame:
      ui_->OnWebVrTimeoutImminent();
      break;
    case kWebVrTimeoutImminent:
      ui_->OnWebVrTimedOut();
      break;
    case kWebVrTimedOut:
      ui_->SetWebVrMode(false, false);
      break;
  }
}

void VrTestContext::ToggleSplashScreen() {
  if (!show_web_vr_splash_screen_) {
    UiInitialState state;
    state.web_vr_autopresentation_expected = true;
    ui_->ReinitializeForTest(state);
  } else {
    ui_->ReinitializeForTest(UiInitialState());
  }
  show_web_vr_splash_screen_ = !show_web_vr_splash_screen_;
}

gfx::Transform VrTestContext::ProjectionMatrix() const {
  gfx::Transform transform(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0.5);
  if (window_size_.height() > 0) {
    transform.Scale(
        view_scale_factor_,
        view_scale_factor_ * window_size_.width() / window_size_.height());
  }
  return transform;
}

gfx::Transform VrTestContext::ViewProjectionMatrix() const {
  return ProjectionMatrix() * head_pose_;
}

void VrTestContext::SetVoiceSearchActive(bool active) {
  if (!voice_search_enabled_) {
    OnUnsupportedMode(
        UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
    return;
  }
  ui_->SetSpeechRecognitionEnabled(active);
  if (active)
    ui_->OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_RECOGNIZING);
}

void VrTestContext::ExitPresent() {}
void VrTestContext::ExitFullscreen() {}

void VrTestContext::Navigate(GURL gurl) {
  ToolbarState state(gurl, security_state::SecurityLevel::HTTP_SHOW_WARNING,
                     &toolbar::kHttpIcon, base::string16(), true, false);
  ui_->SetToolbarState(state);
}

void VrTestContext::NavigateBack() {}
void VrTestContext::ExitCct() {}

void VrTestContext::OnUnsupportedMode(vr::UiUnsupportedMode mode) {
  if (mode == UiUnsupportedMode::kUnhandledPageInfo ||
      mode == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission) {
    ui_->SetExitVrPromptEnabled(true, mode);
  }
}

void VrTestContext::OnExitVrPromptResult(vr::ExitVrPromptChoice choice,
                                         vr::UiUnsupportedMode reason) {
  LOG(ERROR) << "exit prompt result: " << choice;
  if (reason == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission &&
      choice == CHOICE_EXIT) {
    voice_search_enabled_ = true;
  }
  ui_->SetExitVrPromptEnabled(false, UiUnsupportedMode::kCount);
}

void VrTestContext::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {}
void VrTestContext::StartAutocomplete(const base::string16& string) {}
void VrTestContext::StopAutocomplete() {}

}  // namespace vr
