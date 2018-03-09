// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/vr_test_context.h"

#include <memory>

#include "base/i18n/icu_util.h"
#include "base/numerics/ranges.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/testapp/assets_component_version.h"
#include "chrome/browser/vr/testapp/test_keyboard_delegate.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/grit/vr_testapp_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/security_state/core/security_state.h"
#include "components/toolbar/vector_icons.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/codec/png_codec.h"
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
constexpr float kPageLoadTimeMilliseconds = 500;

constexpr gfx::Point3F kDefaultLaserOrigin = {0.5f, -0.5f, 0.f};
constexpr gfx::Vector3dF kLaserLocalOffset = {0.f, -0.0075f, -0.05f};
constexpr float kControllerScaleFactor = 1.5f;

void RotateToward(const gfx::Vector3dF& fwd, gfx::Transform* transform) {
  gfx::Quaternion quat(kForwardVector, fwd);
  transform->PreconcatTransform(gfx::Transform(quat));
}

bool LoadPng(int resource_id, std::unique_ptr<SkBitmap>* out_image) {
  base::StringPiece data =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  *out_image = std::make_unique<SkBitmap>();
  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(data.data()), data.size(),
      out_image->get());
}

}  // namespace

VrTestContext::VrTestContext() : view_scale_factor_(kDefaultViewScaleFactor) {
  base::FilePath pak_path;
  PathService::Get(base::DIR_MODULE, &pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_testapp.pak"));

  base::i18n::InitializeICU();

  // TODO(cjgrant): Remove this when the keyboard is enabled by default.
  base::FeatureList::InitializeInstance("VrBrowserKeyboard", "");

  text_input_delegate_ = std::make_unique<TextInputDelegate>();
  keyboard_delegate_ = std::make_unique<TestKeyboardDelegate>();

  UiInitialState ui_initial_state;
  ui_ = std::make_unique<Ui>(this, nullptr, keyboard_delegate_.get(),
                             text_input_delegate_.get(), ui_initial_state);
  LoadAssets();

  text_input_delegate_->SetRequestFocusCallback(
      base::BindRepeating(&vr::Ui::RequestFocus, base::Unretained(ui_.get())));
  text_input_delegate_->SetRequestUnfocusCallback(base::BindRepeating(
      &vr::Ui::RequestUnfocus, base::Unretained(ui_.get())));
  text_input_delegate_->SetUpdateInputCallback(
      base::BindRepeating(&TestKeyboardDelegate::UpdateInput,
                          base::Unretained(keyboard_delegate_.get())));
  keyboard_delegate_->SetUiInterface(ui_.get());

  model_ = ui_->model_for_test();

  CycleOrigin();
  ui_->SetHistoryButtonsEnabled(true, true);
  ui_->SetLoading(true);
  ui_->SetLoadProgress(0.4);
  ui_->SetVideoCaptureEnabled(true);
  ui_->SetScreenCaptureEnabled(true);
  ui_->SetBluetoothConnected(true);
  ui_->SetLocationAccessEnabled(true);
  ui_->input_manager()->set_hit_test_strategy(
      UiInputManager::PROJECT_TO_LASER_ORIGIN_FOR_TEST);
}

VrTestContext::~VrTestContext() = default;

void VrTestContext::DrawFrame() {
  base::TimeTicks current_time = base::TimeTicks::Now();

  RenderInfo render_info = GetRenderInfo();

  UpdateController(render_info);

  // Update the render position of all UI elements (including desktop).
  ui_->scene()->OnBeginFrame(current_time, head_pose_);
  ui_->OnProjMatrixChanged(render_info.left_eye_model.proj_matrix);
  ui_->ui_renderer()->Draw(render_info);

  // This is required in order to show the WebVR toasts.
  if (model_->web_vr.has_produced_frames()) {
    ui_->ui_renderer()->DrawWebVrOverlayForeground(render_info);
  }

  auto load_progress = (current_time - page_load_start_).InMilliseconds() /
                       kPageLoadTimeMilliseconds;
  ui_->SetLoading(load_progress < 1.0f);
  ui_->SetLoadProgress(std::min(load_progress, 1.0f));
}

void VrTestContext::HandleInput(ui::Event* event) {
  if (event->IsKeyEvent()) {
    if (event->type() != ui::ET_KEY_PRESSED) {
      return;
    }
    if (keyboard_delegate_->HandleInput(event)) {
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
      case ui::DomCode::US_H:
        handedness_ = handedness_ == PlatformController::kRightHanded
                          ? PlatformController::kLeftHanded
                          : PlatformController::kRightHanded;
        break;
      case ui::DomCode::US_I:
        incognito_ = !incognito_;
        ui_->SetIncognito(incognito_);
        break;
      case ui::DomCode::US_D:
        ui_->Dump(false);
        break;
      case ui::DomCode::US_B:
        ui_->Dump(true);
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
      case ui::DomCode::US_E:
        model_->experimental_features_enabled =
            !model_->experimental_features_enabled;
        break;
      case ui::DomCode::US_O:
        CycleOrigin();
        model_->can_navigate_back = !model_->can_navigate_back;
        break;
      case ui::DomCode::US_P:
        model_->toggle_mode(kModeRepositionWindow);
        break;
      case ui::DomCode::US_X:
        ui_->OnAppButtonClicked();
        break;
      case ui::DomCode::US_Q:
        model_->active_modal_prompt_type =
            kModalPromptTypeGenericUnsupportedFeature;
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

  if (mouse_event->IsMiddleMouseButton()) {
    if (mouse_event->type() == ui::ET_MOUSE_RELEASED) {
      ui_->OnAppButtonClicked();
    }
  }

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
  last_controller_model_ = UpdateController(GetRenderInfo());
}

ControllerModel VrTestContext::UpdateController(const RenderInfo& render_info) {
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

  gfx::Point3F laser_origin = LaserOrigin();

  controller_model.transform.Translate3d(laser_origin.x(), laser_origin.y(),
                                         laser_origin.z());
  controller_model.transform.Scale3d(
      kControllerScaleFactor, kControllerScaleFactor, kControllerScaleFactor);
  RotateToward(controller_model.laser_direction, &controller_model.transform);

  // Hit testing is done in terms of this synthesized controller model.
  GestureList gesture_list;
  ReticleModel reticle_model;
  ui_->input_manager()->HandleInput(base::TimeTicks::Now(), render_info,
                                    controller_model, &reticle_model,
                                    &gesture_list);

  // Now that we have accurate hit information, we use this to construct a
  // controller model for display.
  controller_model.laser_direction = reticle_model.target_point - laser_origin;

  controller_model.transform.MakeIdentity();
  controller_model.transform.Translate3d(laser_origin.x(), laser_origin.y(),
                                         laser_origin.z());
  controller_model.transform.Scale3d(
      kControllerScaleFactor, kControllerScaleFactor, kControllerScaleFactor);
  RotateToward(controller_model.laser_direction, &controller_model.transform);

  gfx::Vector3dF local_offset = kLaserLocalOffset;
  controller_model.transform.TransformVector(&local_offset);
  controller_model.laser_origin = laser_origin + local_offset;
  controller_model.handedness = handedness_;

  ui_->OnControllerUpdated(controller_model, reticle_model);

  return controller_model;
}

void VrTestContext::OnGlInitialized() {
  unsigned int content_texture_id = CreateFakeContentTexture();

  ui_->OnGlInitialized(
      content_texture_id, UiElementRenderer::kTextureLocationLocal,
      content_texture_id, UiElementRenderer::kTextureLocationLocal, 0, false);

  keyboard_delegate_->Initialize(ui_->scene()->SurfaceProviderForTesting(),
                                 ui_->ui_element_renderer());
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

void VrTestContext::CreateFakeVoiceSearchResult() {
  if (!model_->voice_search_enabled())
    return;
  ui_->SetRecognitionResult(
      base::UTF8ToUTF16("I would like to see cat videos, please."));
  ui_->SetSpeechRecognitionEnabled(false);
}

void VrTestContext::CycleWebVrModes() {
  switch (model_->web_vr.state) {
    case kWebVrNoTimeoutPending:
      ui_->SetWebVrMode(true, false);
      break;
    case kWebVrAwaitingMinSplashScreenDuration:
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
    default:
      break;
  }
}

void VrTestContext::ToggleSplashScreen() {
  if (!show_web_vr_splash_screen_) {
    UiInitialState state;
    state.in_web_vr = true;
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
  page_load_start_ = base::TimeTicks::Now();
}

void VrTestContext::NavigateBack() {
  page_load_start_ = base::TimeTicks::Now();
}

void VrTestContext::ExitCct() {}

void VrTestContext::OnUnsupportedMode(vr::UiUnsupportedMode mode) {
  if (mode == UiUnsupportedMode::kUnhandledPageInfo ||
      mode == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission) {
    ui_->ShowExitVrPrompt(mode);
  }
}

void VrTestContext::CloseHostedDialog() {}

void VrTestContext::OnExitVrPromptResult(vr::ExitVrPromptChoice choice,
                                         vr::UiUnsupportedMode reason) {
  if (reason == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission &&
      choice == CHOICE_EXIT) {
    voice_search_enabled_ = true;
  }
}

void VrTestContext::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {}

void VrTestContext::StartAutocomplete(const AutocompleteRequest& request) {
  auto result = std::make_unique<OmniboxSuggestions>();

  // Supply an in-line match if the input matches a canned URL.
  base::string16 full_string = base::UTF8ToUTF16("wikipedia.org");
  if (!request.prevent_inline_autocomplete && request.text.size() >= 2 &&
      full_string.find(request.text) == 0) {
    result->suggestions.emplace_back(OmniboxSuggestion(
        full_string, base::string16(), ACMatchClassifications(),
        ACMatchClassifications(), AutocompleteMatch::Type::VOICE_SUGGEST,
        GURL(), request.text, full_string.substr(request.text.size())));
  }

  // Supply a verbatim search match.
  result->suggestions.emplace_back(OmniboxSuggestion(
      request.text, base::string16(), ACMatchClassifications(),
      ACMatchClassifications(), AutocompleteMatch::Type::VOICE_SUGGEST, GURL(),
      base::string16(), base::string16()));

  // Add a suggestion to exercise classification text styling.
  result->suggestions.emplace_back(OmniboxSuggestion(
      base::UTF8ToUTF16("Suggestion with classification"),
      base::UTF8ToUTF16("none url match dim invsible"),
      ACMatchClassifications(),
      {
          ACMatchClassification(0, ACMatchClassification::NONE),
          ACMatchClassification(5, ACMatchClassification::URL),
          ACMatchClassification(9, ACMatchClassification::MATCH),
          ACMatchClassification(15, ACMatchClassification::DIM),
          ACMatchClassification(19, ACMatchClassification::INVISIBLE),
      },
      AutocompleteMatch::Type::VOICE_SUGGEST, GURL("http://www.test.com/"),
      base::string16(), base::string16()));

  while (result->suggestions.size() < 4) {
    result->suggestions.emplace_back(OmniboxSuggestion(
        base::UTF8ToUTF16("Suggestion"),
        base::UTF8ToUTF16(
            "Very lengthy description of the suggestion that would wrap "
            "if not truncated through some other means."),
        ACMatchClassifications(), ACMatchClassifications(),
        AutocompleteMatch::Type::VOICE_SUGGEST, GURL("http://www.test.com/"),
        base::string16(), base::string16()));
  }

  ui_->SetOmniboxSuggestions(std::move(result));
}

void VrTestContext::StopAutocomplete() {
  ui_->SetOmniboxSuggestions(std::make_unique<OmniboxSuggestions>());
}

void VrTestContext::CycleOrigin() {
  const std::vector<ToolbarState> states = {
      {GURL("http://domain.com"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("https://www.domain.com/path/segment/directory/file.html"),
       security_state::SecurityLevel::SECURE, &toolbar::kHttpsValidIcon,
       base::UTF8ToUTF16("Secure"), true, false},
      {GURL("https://www.domain.com/path/segment/directory/file.html"),
       security_state::SecurityLevel::DANGEROUS, &toolbar::kHttpsInvalidIcon,
       base::UTF8ToUTF16("Dangerous"), true, false},
      // Do not show URL
      {GURL(), security_state::SecurityLevel::HTTP_SHOW_WARNING,
       &toolbar::kHttpIcon, base::string16(), false, false},
      {GURL("file:///C:/path/filename"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("file:///C:/path/path/path/path/path/path/path/path"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      // Elision-related cases.
      {GURL("http://domaaaaaaaaaaain.com"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://domaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaain.com"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://domain.com/a/"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://domain.com/aaaaaaa/"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://domain.com/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://domaaaaaaaaaaaaaaaaain.com/aaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://domaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaain.com/aaaaaaaaaa/"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://www.domain.com/path/segment/directory/file.html"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
      {GURL("http://subdomain.domain.com/"),
       security_state::SecurityLevel::HTTP_SHOW_WARNING, &toolbar::kHttpIcon,
       base::string16(), true, false},
  };

  static int state = 0;
  ui_->SetToolbarState(states[state]);
  state = (state + 1) % states.size();
}

RenderInfo VrTestContext::GetRenderInfo() const {
  RenderInfo render_info;
  render_info.head_pose = head_pose_;
  render_info.surface_texture_size = window_size_;
  render_info.left_eye_model.viewport = gfx::Rect(window_size_);
  render_info.left_eye_model.view_matrix = head_pose_;
  render_info.left_eye_model.proj_matrix = ProjectionMatrix();
  render_info.left_eye_model.view_proj_matrix = ViewProjectionMatrix();
  render_info.right_eye_model = render_info.left_eye_model;
  return render_info;
}

gfx::Point3F VrTestContext::LaserOrigin() const {
  gfx::Point3F origin = kDefaultLaserOrigin;
  if (handedness_ == PlatformController::kLeftHanded) {
    origin.set_x(-origin.x());
  }
  return origin;
}

void VrTestContext::LoadAssets() {
  base::Version assets_component_version(VR_ASSETS_COMPONENT_VERSION);
  auto assets = std::make_unique<Assets>();
  if (!(LoadPng(IDR_VR_BACKGROUND_IMAGE, &assets->background) &&
        LoadPng(IDR_VR_NORMAL_GRADIENT_IMAGE, &assets->normal_gradient) &&
        LoadPng(IDR_VR_INCOGNITO_GRADIENT_IMAGE, &assets->incognito_gradient) &&
        LoadPng(IDR_VR_FULLSCREEN_GRADIENT_IMAGE,
                &assets->fullscreen_gradient))) {
    ui_->OnAssetsLoaded(AssetsLoadStatus::kInvalidContent, nullptr,
                        assets_component_version);
    return;
  }
  ui_->OnAssetsLoaded(AssetsLoadStatus::kSuccess, std::move(assets),
                      assets_component_version);
}

}  // namespace vr
