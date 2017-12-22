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
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/ganesh_surface_provider.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "chrome/common/chrome_features.h"

namespace vr {

Ui::Ui(UiBrowserInterface* browser,
       ContentInputForwarder* content_input_forwarder,
       vr::KeyboardDelegate* keyboard_delegate,
       vr::TextInputDelegate* text_input_delegate,
       const UiInitialState& ui_initial_state)
    : Ui(browser,
         base::MakeUnique<ContentInputDelegate>(content_input_forwarder),
         keyboard_delegate,
         text_input_delegate,
         ui_initial_state) {}

Ui::Ui(UiBrowserInterface* browser,
       std::unique_ptr<ContentInputDelegate> content_input_delegate,
       vr::KeyboardDelegate* keyboard_delegate,
       vr::TextInputDelegate* text_input_delegate,
       const UiInitialState& ui_initial_state)
    : browser_(browser),
      scene_(base::MakeUnique<UiScene>()),
      model_(base::MakeUnique<Model>()),
      content_input_delegate_(std::move(content_input_delegate)),
      input_manager_(base::MakeUnique<UiInputManager>(scene_.get())),
      weak_ptr_factory_(this) {
  InitializeModel(ui_initial_state);
  UiSceneCreator(browser, scene_.get(), this, content_input_delegate_.get(),
                 keyboard_delegate, text_input_delegate, model_.get())
      .CreateScene();
}

Ui::~Ui() = default;

base::WeakPtr<BrowserUiInterface> Ui::GetBrowserUiWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void Ui::SetWebVrMode(bool enabled, bool show_toast) {
  model_->web_vr.show_exit_toast = show_toast;
  if (enabled) {
    model_->web_vr.state = kWebVrAwaitingFirstFrame;
    // We have this check here so that we don't set the mode to kModeWebVr when
    // it should be kModeWebVrAutopresented. The latter is set when the UI is
    // initialized.
    if (!model_->web_vr_enabled())
      model_->push_mode(kModeWebVr);
  } else {
    model_->web_vr.state = kWebVrNoTimeoutPending;
    if (model_->web_vr_enabled())
      model_->pop_mode();
  }
}

void Ui::SetFullscreen(bool enabled) {
  if (enabled) {
    model_->push_mode(kModeFullscreen);
  } else {
    model_->pop_mode(kModeFullscreen);
  }
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
    DCHECK_EQ(reason, UiUnsupportedMode::kCount);
    model_->active_modal_prompt_type = kModalPromptTypeNone;
    return;
  }

  if (model_->active_modal_prompt_type != kModalPromptTypeNone) {
    browser_->OnExitVrPromptResult(
        ExitVrPromptChoice::CHOICE_NONE,
        GetReasonForPrompt(model_->active_modal_prompt_type));
  }

  switch (reason) {
    case UiUnsupportedMode::kUnhandledCodePoint:
      NOTREACHED();  // This mode does not prompt.
      return;
    case UiUnsupportedMode::kUnhandledPageInfo:
      model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
      return;
    case UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission:
      model_->active_modal_prompt_type =
          kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission;
      return;
    case UiUnsupportedMode::kCount:
      NOTREACHED();  // Should never be used as a mode (when |enabled| is true).
      return;
  }

  NOTREACHED();
}

void Ui::OnUiRequestedNavigation() {
  model_->pop_mode(kModeEditingOmnibox);
}

void Ui::SetOmniboxEditingEnabled(bool enabled) {
  if (enabled) {
    model_->push_mode(kModeEditingOmnibox);
  } else {
    model_->pop_mode(kModeEditingOmnibox);
  }
}

void Ui::SetSpeechRecognitionEnabled(bool enabled) {
  if (enabled) {
    model_->push_mode(kModeVoiceSearch);
    model_->speech.recognition_result.clear();
  } else {
    model_->pop_mode(kModeVoiceSearch);
  }
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
  return model_->web_vr.has_produced_frames();
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

void Ui::RequestFocus(int element_id) {
  input_manager_->RequestFocus(element_id);
}

void Ui::RequestUnfocus(int element_id) {
  input_manager_->RequestUnfocus(element_id);
}

void Ui::OnInputEdited(const TextInputInfo& info) {
  input_manager_->OnInputEdited(info);
}

void Ui::OnInputCommitted(const TextInputInfo& info) {
  input_manager_->OnInputCommitted(info);
}

void Ui::OnKeyboardHidden() {
  input_manager_->OnKeyboardHidden();
}

void Ui::OnAppButtonClicked() {
  // App button clicks should be a no-op when auto-presenting WebVR.
  if (model_->web_vr_autopresentation_enabled()) {
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
  if (model_->web_vr_enabled())
    model_->web_vr.state = kWebVrPresenting;
}

void Ui::OnWebVrTimeoutImminent() {
  if (model_->web_vr_enabled())
    model_->web_vr.state = kWebVrTimeoutImminent;
}

void Ui::OnWebVrTimedOut() {
  if (model_->web_vr_enabled())
    model_->web_vr.state = kWebVrTimedOut;
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

bool Ui::SkipsRedrawWhenNotDirty() const {
  return model_->skips_redraw_when_not_dirty;
}

void Ui::Dump() {
  std::ostringstream os;
  os << std::setprecision(3);
  os << std::endl;
  scene_->root_element().DumpHierarchy(std::vector<size_t>(), &os);
  LOG(ERROR) << os.str();
}

void Ui::SetBackgroundImage(std::unique_ptr<SkBitmap> bitmap) {
  Background* background = reinterpret_cast<Background*>(
      scene_->GetUiElementByName(k2dBrowsingTexturedBackground));
  DCHECK(background);
  background->SetImage(std::move(bitmap));
  model_->background_loaded = true;
}

void Ui::ReinitializeForTest(const UiInitialState& ui_initial_state) {
  InitializeModel(ui_initial_state);
}

void Ui::InitializeModel(const UiInitialState& ui_initial_state) {
  model_->experimental_features_enabled =
      base::FeatureList::IsEnabled(features::kVrBrowsingExperimentalFeatures);
  model_->speech.has_or_can_request_audio_permission =
      ui_initial_state.has_or_can_request_audio_permission;
  model_->ui_modes.clear();
  model_->push_mode(kModeBrowsing);
  if (ui_initial_state.in_web_vr) {
    model_->web_vr.state = kWebVrAwaitingFirstFrame;
    auto mode = ui_initial_state.web_vr_autopresentation_expected
                    ? kModeWebVrAutopresented
                    : kModeWebVr;
    model_->push_mode(mode);
  }

  model_->in_cct = ui_initial_state.in_cct;
  model_->browsing_disabled = ui_initial_state.browsing_disabled;
  model_->skips_redraw_when_not_dirty =
      ui_initial_state.skips_redraw_when_not_dirty;
  model_->background_available = ui_initial_state.assets_available;
}

}  // namespace vr
