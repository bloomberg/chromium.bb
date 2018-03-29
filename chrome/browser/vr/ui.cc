// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iomanip>
#include <sstream>

#include "chrome/browser/vr/ui.h"

#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/cpu_surface_provider.h"
#include "chrome/browser/vr/elements/prompt.h"
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/ganesh_surface_provider.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/platform_toast.h"
#include "chrome/browser/vr/model/sound_id.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "chrome/common/chrome_features.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

Ui::Ui(UiBrowserInterface* browser,
       ContentInputForwarder* content_input_forwarder,
       KeyboardDelegate* keyboard_delegate,
       TextInputDelegate* text_input_delegate,
       AudioDelegate* audio_delegate,
       const UiInitialState& ui_initial_state)
    : Ui(browser,
         std::make_unique<ContentInputDelegate>(content_input_forwarder),
         keyboard_delegate,
         text_input_delegate,
         audio_delegate,
         ui_initial_state) {}

Ui::Ui(UiBrowserInterface* browser,
       std::unique_ptr<ContentInputDelegate> content_input_delegate,
       KeyboardDelegate* keyboard_delegate,
       TextInputDelegate* text_input_delegate,
       AudioDelegate* audio_delegate,
       const UiInitialState& ui_initial_state)
    : browser_(browser),
      scene_(std::make_unique<UiScene>()),
      model_(std::make_unique<Model>()),
      content_input_delegate_(std::move(content_input_delegate)),
      input_manager_(std::make_unique<UiInputManager>(scene_.get())),
      audio_delegate_(audio_delegate),
      weak_ptr_factory_(this) {
  UiInitialState state = ui_initial_state;
  if (keyboard_delegate != nullptr)
    state.supports_selection = keyboard_delegate->SupportsSelection();
  InitializeModel(state);

  UiSceneCreator(browser, scene_.get(), this, content_input_delegate_.get(),
                 keyboard_delegate, text_input_delegate, audio_delegate,
                 model_.get())
      .CreateScene();
}

Ui::~Ui() = default;

base::WeakPtr<BrowserUiInterface> Ui::GetBrowserUiWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void Ui::SetWebVrMode(bool enabled) {
  if (enabled) {
    model_->web_vr.has_received_permissions = false;
    if (!model_->web_vr_autopresentation_enabled()) {
      // When auto-presenting, we transition into this state when the minimum
      // splash-screen duration has passed.
      model_->web_vr.state = kWebVrAwaitingFirstFrame;
    }
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
  model_->can_navigate_back = can_go_back;
  model_->can_navigate_forward = can_go_forward;
}

void Ui::SetCapturingState(const CapturingStateModel& state) {
  model_->capturing_state = state;
  model_->web_vr.has_received_permissions = true;
}

void Ui::ShowExitVrPrompt(UiUnsupportedMode reason) {
  // Shouldn't request to exit VR when we're already prompting to exit VR.
  CHECK(model_->active_modal_prompt_type == kModalPromptTypeNone);

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
    case UiUnsupportedMode::kGenericUnsupportedFeature:
      model_->active_modal_prompt_type =
          kModalPromptTypeGenericUnsupportedFeature;
      return;
    case UiUnsupportedMode::kNeedsKeyboardUpdate:
      model_->active_modal_prompt_type = kModalPromptTypeUpdateKeyboard;
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

void Ui::SetSpeechRecognitionEnabled(bool enabled) {
  if (enabled) {
    model_->push_mode(kModeVoiceSearch);
    model_->speech.recognition_result.clear();
  } else {
    model_->pop_mode(kModeVoiceSearch);
    if (model_->omnibox_editing_enabled() &&
        !model_->speech.recognition_result.empty()) {
      model_->pop_mode(kModeEditingOmnibox);
    }
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

bool Ui::CanSendWebVrVSync() {
  return model_->web_vr_enabled() &&
         !model_->web_vr.awaiting_min_splash_screen_duration() &&
         !model_->web_vr.showing_hosted_ui;
}

void Ui::ShowSoftInput(bool show) {
  if (model_->needs_keyboard_update) {
    browser_->OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate);
    return;
  }
  model_->editing_web_input = show;
}

void Ui::UpdateWebInputIndices(int selection_start,
                               int selection_end,
                               int composition_start,
                               int composition_end) {
  content_input_delegate_->OnWebInputIndicesChanged(
      selection_start, selection_end, composition_start, composition_end,
      base::BindOnce(
          [](TextInputInfo* model, const TextInputInfo& new_state) {
            *model = new_state;
          },
          base::Unretained(&model_->web_input_text_field_info.current)));
}

void Ui::SetAlertDialogEnabled(bool enabled,
                               ContentInputDelegate* delegate,
                               float width,
                               float height) {
  model_->web_vr.showing_hosted_ui = enabled;
  model_->hosted_platform_ui.hosted_ui_enabled = enabled;
  model_->hosted_platform_ui.rect.set_height(height);
  model_->hosted_platform_ui.rect.set_width(width);
  model_->hosted_platform_ui.delegate = delegate;
}

void Ui::SetAlertDialogSize(float width, float height) {
  model_->hosted_platform_ui.rect.set_height(height);
  model_->hosted_platform_ui.rect.set_width(width);
}

void Ui::SetDialogLocation(float x, float y) {
  model_->hosted_platform_ui.rect.set_y(y);
  model_->hosted_platform_ui.rect.set_x(x);
}

void Ui::SetDialogFloating() {
  model_->hosted_platform_ui.floating = true;
}

void Ui::ShowPlatformToast(const base::string16& text) {
  model_->platform_toast = std::make_unique<PlatformToast>(text);
}

void Ui::CancelPlatformToast() {
  model_->platform_toast.reset();
}

bool Ui::ShouldRenderWebVr() {
  return model_->web_vr.presenting_web_vr();
}

void Ui::OnGlInitialized(
    unsigned int content_texture_id,
    UiElementRenderer::TextureLocation content_location,
    unsigned int content_overlay_texture_id,
    UiElementRenderer::TextureLocation content_overlay_location,
    unsigned int ui_texture_id,
    bool use_ganesh) {
  ui_element_renderer_ = std::make_unique<UiElementRenderer>();
  ui_renderer_ =
      std::make_unique<UiRenderer>(scene_.get(), ui_element_renderer_.get());
  if (use_ganesh) {
    provider_ = std::make_unique<GaneshSurfaceProvider>();
  } else {
    provider_ = std::make_unique<CpuSurfaceProvider>();
  }
  scene_->OnGlInitialized(provider_.get());
  model_->content_texture_id = content_texture_id;
  model_->content_overlay_texture_id = content_overlay_texture_id;
  model_->content_location = content_location;
  model_->content_overlay_location = content_overlay_location;
  model_->hosted_platform_ui.texture_id = ui_texture_id;
}

void Ui::RequestFocus(int element_id) {
  input_manager_->RequestFocus(element_id);
}

void Ui::RequestUnfocus(int element_id) {
  input_manager_->RequestUnfocus(element_id);
}

void Ui::OnInputEdited(const EditedText& info) {
  input_manager_->OnInputEdited(info);
}

void Ui::OnInputCommitted(const EditedText& info) {
  input_manager_->OnInputCommitted(info);
}

void Ui::OnKeyboardHidden() {
  input_manager_->OnKeyboardHidden();
}

void Ui::OnAppButtonClicked() {
  // App button clicks should be a no-op when auto-presenting WebVR or if
  // browsing mode is disabled.
  if (model_->web_vr_autopresentation_enabled() || model_->browsing_disabled)
    return;

  if (model_->reposition_window_enabled()) {
    model_->pop_mode(kModeRepositionWindow);
    return;
  }

  if (model_->editing_web_input) {
    ShowSoftInput(false);
    return;
  }

  if (model_->hosted_platform_ui.hosted_ui_enabled) {
    browser_->CloseHostedDialog();
    return;
  }

  // App button click exits the WebVR presentation and fullscreen.
  browser_->ExitPresent();
  browser_->ExitFullscreen();

  switch (model_->get_last_opaque_mode()) {
    case kModeVoiceSearch:
      browser_->SetVoiceSearchActive(false);
      break;
    case kModeEditingOmnibox:
      model_->pop_mode(kModeEditingOmnibox);
      break;
    default:
      break;
  }
}

void Ui::OnAppButtonSwipePerformed(
    PlatformController::SwipeDirection direction) {}

void Ui::OnControllerUpdated(const ControllerModel& controller_model,
                             const ReticleModel& reticle_model) {
  model_->controller = controller_model;
  model_->reticle = reticle_model;
  model_->controller.quiescent = input_manager_->controller_quiescent();
  model_->controller.resting_in_viewport =
      input_manager_->controller_resting_in_viewport();
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

bool Ui::IsAppButtonLongPressed() const {
  return model_->controller.app_button_long_pressed;
}

bool Ui::SkipsRedrawWhenNotDirty() const {
  return model_->skips_redraw_when_not_dirty;
}

void Ui::Dump(bool include_bindings) {
#ifndef NDEBUG
  std::ostringstream os;
  os << std::setprecision(3);
  os << std::endl;
  scene_->root_element().DumpHierarchy(std::vector<size_t>(), &os,
                                       include_bindings);

  std::stringstream ss(os.str());
  std::string line;
  while (std::getline(ss, line, '\n')) {
    LOG(ERROR) << line;
  }
#endif
}

void Ui::OnAssetsLoaded(AssetsLoadStatus status,
                        std::unique_ptr<Assets> assets,
                        const base::Version& component_version) {
  model_->waiting_for_background = false;

  if (status != AssetsLoadStatus::kSuccess) {
    return;
  }

  Background* background = static_cast<Background*>(
      scene_->GetUiElementByName(k2dBrowsingTexturedBackground));
  DCHECK(background);
  background->SetBackgroundImage(std::move(assets->background));
  background->SetGradientImages(std::move(assets->normal_gradient),
                                std::move(assets->incognito_gradient),
                                std::move(assets->fullscreen_gradient));

  ColorScheme::UpdateForComponent(component_version);
  model_->background_loaded = true;

  if (audio_delegate_) {
    std::vector<std::pair<SoundId, std::unique_ptr<std::string>&>> sounds = {
        {kSoundButtonHover, assets->button_hover_sound},
        {kSoundButtonClick, assets->button_click_sound},
        {kSoundBackButtonClick, assets->back_button_click_sound},
        {kSoundInactiveButtonClick, assets->inactive_button_click_sound},
    };
    audio_delegate_->ResetSounds();
    for (auto& sound : sounds) {
      if (sound.second)
        audio_delegate_->RegisterSound(sound.first, std::move(sound.second));
    }
  }
}

void Ui::OnAssetsUnavailable() {
  model_->waiting_for_background = false;
}

void Ui::SetIncognitoTabsOpen(bool open) {
  model_->incognito_tabs_open = open;
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
    auto mode = kModeWebVr;
    model_->web_vr.has_received_permissions = false;
    if (ui_initial_state.web_vr_autopresentation_expected) {
      mode = kModeWebVrAutopresented;
      model_->web_vr.state = kWebVrAwaitingMinSplashScreenDuration;
    } else {
      model_->web_vr.state = kWebVrAwaitingFirstFrame;
    }
    model_->push_mode(mode);
  }

  model_->in_cct = ui_initial_state.in_cct;
  model_->browsing_disabled = ui_initial_state.browsing_disabled;
  model_->skips_redraw_when_not_dirty =
      ui_initial_state.skips_redraw_when_not_dirty;
  model_->waiting_for_background = ui_initial_state.assets_supported;
  model_->supports_selection = ui_initial_state.supports_selection;
  model_->needs_keyboard_update = ui_initial_state.needs_keyboard_update;
}

void Ui::AcceptDoffPromptForTesting() {
  DCHECK(model_->active_modal_prompt_type != kModalPromptTypeNone);
  if (model_->active_modal_prompt_type ==
      kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission) {
    static_cast<Prompt*>(scene_->GetUiElementByName(kAudioPermissionPrompt))
        ->ClickPrimaryButtonForTesting();
  } else {
    static_cast<Prompt*>(scene_->GetUiElementByName(kExitPrompt))
        ->ClickSecondaryButtonForTesting();
  }
}

}  // namespace vr
