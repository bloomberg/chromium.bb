// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <sstream>

#include "chrome/browser/vr/ui.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/cpu_surface_provider.h"
#include "chrome/browser/vr/ganesh_surface_provider.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "chrome/common/chrome_features.h"

namespace vr {

Ui::Ui(UiBrowserInterface* browser,
       ContentInputForwarder* content_input_forwarder,
       const UiInitialState& ui_initial_state)
    : Ui(browser,
         base::MakeUnique<ContentInputDelegate>(content_input_forwarder),
         ui_initial_state) {}

Ui::Ui(UiBrowserInterface* browser,
       std::unique_ptr<ContentInputDelegate> content_input_delegate,
       const UiInitialState& ui_initial_state)
    : browser_(browser),
      scene_(base::MakeUnique<UiScene>()),
      model_(base::MakeUnique<Model>()),
      content_input_delegate_(std::move(content_input_delegate)),
      input_manager_(base::MakeUnique<UiInputManager>(scene_.get())),
      weak_ptr_factory_(this) {
  model_->web_vr_started_for_autopresentation =
      ui_initial_state.web_vr_autopresentation_expected;
  model_->web_vr_show_splash_screen =
      ui_initial_state.web_vr_autopresentation_expected;
  model_->experimental_features_enabled =
      base::FeatureList::IsEnabled(features::kVrBrowsingExperimentalFeatures);
  model_->speech.has_or_can_request_audio_permission =
      ui_initial_state.has_or_can_request_audio_permission;
  model_->web_vr_mode = ui_initial_state.in_web_vr;
  model_->in_cct = ui_initial_state.in_cct;
  model_->browsing_disabled = ui_initial_state.browsing_disabled;

  UiSceneManager(browser, scene_.get(), content_input_delegate_.get(),
                 model_.get())
      .CreateScene();
}

Ui::~Ui() = default;

base::WeakPtr<BrowserUiInterface> Ui::GetBrowserUiWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void Ui::SetWebVrMode(bool enabled, bool show_toast) {
  model_->web_vr_timeout_state =
      enabled ? kWebVrAwaitingFirstFrame : kWebVrNoTimeoutPending;
  model_->web_vr_mode = enabled;
  model_->web_vr_show_toast = show_toast;
  if (!enabled) {
    model_->web_vr_show_splash_screen = false;
    model_->web_vr_started_for_autopresentation = false;
  }
}

void Ui::SetFullscreen(bool enabled) {
  model_->fullscreen = enabled;
}

void Ui::SetToolbarState(const ToolbarState& state) {
  model_->toolbar_state = state;
}

void Ui::SetIncognito(bool enabled) {
  model_->incognito = enabled;
}

void Ui::SetLoading(bool loading) {
  model_->loading = loading;
}

void Ui::SetLoadProgress(float progress) {
  model_->load_progress = progress;
}

void Ui::SetIsExiting() {
  model_->exiting_vr = true;
}

void Ui::SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward) {
  // We don't yet support forward navigation so we ignore this parameter.
  model_->can_navigate_back = can_go_back;
}

void Ui::SetVideoCaptureEnabled(bool enabled) {
  model_->permissions.video_capture_enabled = enabled;
}

void Ui::SetScreenCaptureEnabled(bool enabled) {
  model_->permissions.screen_capture_enabled = enabled;
}

void Ui::SetAudioCaptureEnabled(bool enabled) {
  model_->permissions.audio_capture_enabled = enabled;
}

void Ui::SetBluetoothConnected(bool enabled) {
  model_->permissions.bluetooth_connected = enabled;
}

void Ui::SetLocationAccess(bool enabled) {
  model_->permissions.location_access = enabled;
}

void Ui::SetExitVrPromptEnabled(bool enabled, UiUnsupportedMode reason) {
  if (!enabled) {
    model_->active_modal_prompt_type = kModalPromptTypeNone;
    return;
  }

  if (model_->active_modal_prompt_type != kModalPromptTypeNone) {
    browser_->OnExitVrPromptResult(
        ExitVrPromptChoice::CHOICE_NONE,
        GetReasonForPrompt(model_->active_modal_prompt_type));
  }

  switch (reason) {
    case UiUnsupportedMode::kUnhandledPageInfo:
      model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
      break;
    case UiUnsupportedMode::kAndroidPermissionNeeded:
      model_->active_modal_prompt_type =
          kModalPromptTypeExitVRForAudioPermission;
      break;
    default:
      NOTREACHED();
  }
}

void Ui::SetSpeechRecognitionEnabled(bool enabled) {
  model_->speech.recognizing_speech = enabled;
  if (enabled)
    model_->speech.recognition_result.clear();
}

void Ui::SetRecognitionResult(const base::string16& result) {
  model_->speech.recognition_result = result;
}

void Ui::OnSpeechRecognitionStateChanged(int new_state) {
  model_->speech.speech_recognition_state = new_state;
}

void Ui::SetOmniboxSuggestions(
    std::unique_ptr<OmniboxSuggestions> suggestions) {
  model_->omnibox_suggestions = suggestions->suggestions;
}

bool Ui::ShouldRenderWebVr() {
  return model_->should_render_web_vr();
}

void Ui::OnGlInitialized(unsigned int content_texture_id,
                         UiElementRenderer::TextureLocation content_location,
                         bool use_ganesh) {
  ui_element_renderer_ = base::MakeUnique<UiElementRenderer>();
  ui_renderer_ =
      base::MakeUnique<UiRenderer>(scene_.get(), ui_element_renderer_.get());
  if (use_ganesh) {
    provider_ = base::MakeUnique<GaneshSurfaceProvider>();
  } else {
    provider_ = base::MakeUnique<CpuSurfaceProvider>();
  }
  scene_->OnGlInitialized(provider_.get());
  model_->content_texture_id = content_texture_id;
  model_->content_location = content_location;
}

void Ui::OnAppButtonClicked() {
  // App button clicks should be a no-op when auto-presenting WebVR.
  if (model_->web_vr_started_for_autopresentation) {
    return;
  }

  // If browsing mode is disabled, the app button should no-op.
  if (model_->browsing_disabled) {
    return;
  }

  // App button click exits the WebVR presentation and fullscreen.
  browser_->ExitPresent();
  browser_->ExitFullscreen();
}

void Ui::OnAppButtonGesturePerformed(
    PlatformController::SwipeDirection direction) {
}

void Ui::OnControllerUpdated(const ControllerModel& controller_model,
                             const ReticleModel& reticle_model) {
  model_->controller = controller_model;
  model_->reticle = reticle_model;
  model_->controller.quiescent = input_manager_->controller_quiescent();
}

void Ui::OnProjMatrixChanged(const gfx::Transform& proj_matrix) {
  model_->projection_matrix = proj_matrix;
}

void Ui::OnWebVrFrameAvailable() {
  model_->web_vr_timeout_state = kWebVrNoTimeoutPending;
}

void Ui::OnWebVrTimeoutImminent() {
  model_->web_vr_timeout_state = kWebVrTimeoutImminent;
}

void Ui::OnWebVrTimedOut() {
  model_->web_vr_timeout_state = kWebVrTimedOut;
}

void Ui::OnSwapContents(int new_content_id) {
  content_input_delegate_->OnSwapContents(new_content_id);
}

void Ui::OnContentBoundsChanged(int width, int height) {
  content_input_delegate_->OnContentBoundsChanged(width, height);
}

void Ui::OnPlatformControllerInitialized(PlatformController* controller) {
  content_input_delegate_->OnPlatformControllerInitialized(controller);
}

bool Ui::IsControllerVisible() const {
  UiElement* controller_group = scene_->GetUiElementByName(kControllerGroup);
  return controller_group && controller_group->GetTargetOpacity() > 0.0f;
}

void Ui::Dump() {
  std::ostringstream os;
  os << std::setprecision(3);
  os << std::endl;
  scene_->root_element().DumpHierarchy(std::vector<size_t>(), &os);
  LOG(ERROR) << os.str();
}

}  // namespace vr
