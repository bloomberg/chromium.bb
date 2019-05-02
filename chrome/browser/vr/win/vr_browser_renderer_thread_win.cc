// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"

#include <vector>

#include "base/bind.h"
#include "chrome/browser/vr/audio_delegate.h"
#include "chrome/browser/vr/browser_renderer.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/location_bar_state.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_initial_state.h"
#include "chrome/browser/vr/win/graphics_delegate_win.h"
#include "chrome/browser/vr/win/input_delegate_win.h"
#include "chrome/browser/vr/win/scheduler_delegate_win.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "ui/gfx/geometry/quaternion.h"

namespace {
constexpr base::TimeDelta kWebVrInitialFrameTimeout =
    base::TimeDelta::FromSeconds(5);
constexpr base::TimeDelta kWebVrSpinnerTimeout =
    base::TimeDelta::FromSeconds(2);

bool g_frame_timeout_ui_disabled_for_testing_ = false;
}  // namespace

namespace vr {

VRBrowserRendererThreadWin* VRBrowserRendererThreadWin::instance_for_testing_ =
    nullptr;

VRBrowserRendererThreadWin::VRBrowserRendererThreadWin(
    device::mojom::XRCompositorHost* compositor)
    : compositor_(compositor),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DCHECK(instance_for_testing_ == nullptr);
  instance_for_testing_ = this;
}

VRBrowserRendererThreadWin::~VRBrowserRendererThreadWin() {
  // Call Cleanup to ensure correct destruction order of VR-UI classes.
  StopOverlay();
  instance_for_testing_ = nullptr;
}

void VRBrowserRendererThreadWin::StopOverlay() {
  browser_renderer_ = nullptr;
  initializing_graphics_ = nullptr;
  started_ = false;
  graphics_ = nullptr;
  scheduler_ = nullptr;
  ui_ = nullptr;
  scheduler_ui_ = nullptr;
}

void VRBrowserRendererThreadWin::SetVRDisplayInfo(
    device::mojom::VRDisplayInfoPtr display_info) {
  display_info_ = std::move(display_info);
  if (graphics_)
    graphics_->SetVRDisplayInfo(display_info_.Clone());
}

void VRBrowserRendererThreadWin::SetLocationInfo(GURL gurl) {
  gurl_ = gurl;
}

void VRBrowserRendererThreadWin::SetWebXrPresenting(bool presenting) {
  webxr_presenting_ = presenting;

  if (g_frame_timeout_ui_disabled_for_testing_)
    return;

  if (presenting) {
    compositor_->CreateImmersiveOverlay(mojo::MakeRequest(&overlay_));
    StartWebXrTimeout();
  } else {
    StopWebXrTimeout();
  }
}

void VRBrowserRendererThreadWin::StartWebXrTimeout() {
  waiting_for_first_frame_ = true;
  overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                         draw_state_.ShouldDrawWebXR());

  overlay_->RequestNotificationOnWebXrSubmitted(base::BindOnce(
      &VRBrowserRendererThreadWin::OnWebXRSubmitted, base::Unretained(this)));

  webxr_spinner_timeout_closure_.Reset(base::BindOnce(
      &VRBrowserRendererThreadWin::OnWebXrTimeoutImminent,
      base::Unretained(
          this)));  // Unretained safe because we explicitly cancel.
  task_runner_->PostDelayedTask(FROM_HERE,
                                webxr_spinner_timeout_closure_.callback(),
                                kWebVrSpinnerTimeout);
  webxr_frame_timeout_closure_.Reset(base::BindOnce(
      &VRBrowserRendererThreadWin::OnWebXrTimedOut,
      base::Unretained(
          this)));  // Unretained safe because we explicitly cancel.
  task_runner_->PostDelayedTask(FROM_HERE,
                                webxr_frame_timeout_closure_.callback(),
                                kWebVrInitialFrameTimeout);
}

void VRBrowserRendererThreadWin::StopWebXrTimeout() {
  if (!webxr_spinner_timeout_closure_.IsCancelled())
    webxr_spinner_timeout_closure_.Cancel();
  if (!webxr_frame_timeout_closure_.IsCancelled())
    webxr_frame_timeout_closure_.Cancel();
  OnSpinnerVisibilityChanged(false);
  waiting_for_first_frame_ = false;
}

int VRBrowserRendererThreadWin::GetNextRequestId() {
  current_request_id_++;
  if (current_request_id_ >= 0x10000)
    current_request_id_ = 0;
  return current_request_id_;
}

void VRBrowserRendererThreadWin::OnWebXrTimeoutImminent() {
  OnSpinnerVisibilityChanged(true);
  scheduler_ui_->OnWebXrTimeoutImminent();
}

void VRBrowserRendererThreadWin::OnWebXrTimedOut() {
  OnSpinnerVisibilityChanged(true);
  scheduler_ui_->OnWebXrTimedOut();
}

void VRBrowserRendererThreadWin::SetVisibleExternalPromptNotification(
    ExternalPromptNotificationType prompt) {
  if (!draw_state_.SetPrompt(prompt))
    return;

  if (draw_state_.ShouldDrawUI())
    StartOverlay();

  ui_->SetVisibleExternalPromptNotification(prompt);

  if (overlay_)
    overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                           draw_state_.ShouldDrawWebXR());
  if (draw_state_.ShouldDrawUI()) {
    if (overlay_)  // False only while testing
      overlay_->RequestNextOverlayPose(
          base::BindOnce(&VRBrowserRendererThreadWin::OnPose,
                         base::Unretained(this), GetNextRequestId()));
  } else {
    StopOverlay();
  }
}

void VRBrowserRendererThreadWin::SetIndicatorsVisible(bool visible) {
  if (!draw_state_.SetIndicatorsVisible(visible))
    return;

  if (draw_state_.ShouldDrawUI())
    StartOverlay();

  if (overlay_)
    overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                           draw_state_.ShouldDrawWebXR());
  if (draw_state_.ShouldDrawUI()) {
    if (overlay_)  // False only while testing
      overlay_->RequestNextOverlayPose(
          base::BindOnce(&VRBrowserRendererThreadWin::OnPose,
                         base::Unretained(this), GetNextRequestId()));
  } else {
    StopOverlay();
  }
}

void VRBrowserRendererThreadWin::SetCapturingState(
    const CapturingStateModel& active_capturing,
    const CapturingStateModel& background_capturing,
    const CapturingStateModel& potential_capturing) {
  if (ui_)
    ui_->SetCapturingState(active_capturing, background_capturing,
                           potential_capturing);
}

VRBrowserRendererThreadWin*
VRBrowserRendererThreadWin::GetInstanceForTesting() {
  return instance_for_testing_;
}

BrowserRenderer* VRBrowserRendererThreadWin::GetBrowserRendererForTesting() {
  return browser_renderer_.get();
}

namespace {
// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kSlidingAverageSize = 5;
}  // namespace

void VRBrowserRendererThreadWin::DisableFrameTimeoutForTesting() {
  g_frame_timeout_ui_disabled_for_testing_ = true;
}

class VRUiBrowserInterface : public UiBrowserInterface {
 public:
  ~VRUiBrowserInterface() override = default;

  void ExitPresent() override {}
  void ExitFullscreen() override {}
  void Navigate(GURL gurl, NavigationMethod method) override {}
  void NavigateBack() override {}
  void NavigateForward() override {}
  void ReloadTab() override {}
  void OpenNewTab(bool incognito) override {}
  void OpenBookmarks() override {}
  void OpenRecentTabs() override {}
  void OpenHistory() override {}
  void OpenDownloads() override {}
  void OpenShare() override {}
  void OpenSettings() override {}
  void CloseAllIncognitoTabs() override {}
  void OpenFeedback() override {}
  void CloseHostedDialog() override {}
  void OnUnsupportedMode(UiUnsupportedMode mode) override {}
  void OnExitVrPromptResult(ExitVrPromptChoice choice,
                            UiUnsupportedMode reason) override {}
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds) override {}
  void SetVoiceSearchActive(bool active) override {}
  void StartAutocomplete(const AutocompleteRequest& request) override {}
  void StopAutocomplete() override {}
  void ShowPageInfo() override {}
};

void VRBrowserRendererThreadWin::StartOverlay() {
  if (started_)
    return;

  initializing_graphics_ = std::make_unique<GraphicsDelegateWin>();
  if (!initializing_graphics_->InitializeOnMainThread()) {
    return;
  }

  initializing_graphics_->InitializeOnGLThread();
  initializing_graphics_->BindContext();

  // Create a vr::Ui
  BrowserRendererBrowserInterface* browser_renderer_interface = nullptr;
  ui_browser_interface_ = std::make_unique<VRUiBrowserInterface>();
  PlatformInputHandler* input = nullptr;
  std::unique_ptr<KeyboardDelegate> keyboard_delegate;
  std::unique_ptr<TextInputDelegate> text_input_delegate;
  std::unique_ptr<AudioDelegate> audio_delegate;
  UiInitialState ui_initial_state = {};
  ui_initial_state.in_web_vr = true;
  ui_initial_state.browsing_disabled = true;
  ui_initial_state.supports_selection = false;
  std::unique_ptr<Ui> ui = std::make_unique<Ui>(
      ui_browser_interface_.get(), input, std::move(keyboard_delegate),
      std::move(text_input_delegate), std::move(audio_delegate),
      ui_initial_state);
  static_cast<UiInterface*>(ui.get())->OnGlInitialized(
      kGlTextureLocationLocal,
      0 /* content_texture_id - we don't support content */,
      0 /* content_overlay_texture_id - we don't support content overlays */,
      0 /* platform_ui_texture_id - we don't support platform UI */);
  ui_ = static_cast<BrowserUiInterface*>(ui.get());
  ui_->SetWebVrMode(true);
  scheduler_ui_ = static_cast<UiInterface*>(ui.get())->GetSchedulerUiPtr();

  if (gurl_.is_valid()) {
    // TODO(https://crbug.com/905375): Set more of this state.  Only the GURL is
    // currently used, so its the only thing we are setting correctly. See
    // VRUiHostImpl::SetLocationInfoOnUi also.
    LocationBarState state(gurl_, security_state::SecurityLevel::SECURE,
                           nullptr /* vector icon */, true /* display url */,
                           false /* offline */);
    ui_->SetLocationBarState(state);
  }

  // Create the delegates, and keep raw pointers to them.  They are owned by
  // browser_renderer_.
  std::unique_ptr<SchedulerDelegateWin> scheduler_delegate =
      std::make_unique<SchedulerDelegateWin>();
  scheduler_ = scheduler_delegate.get();
  graphics_ = initializing_graphics_.get();
  graphics_->SetVRDisplayInfo(display_info_.Clone());
  std::unique_ptr<InputDelegateWin> input_delegate =
      std::make_unique<InputDelegateWin>();
  input_ = input_delegate.get();

  // Create the BrowserRenderer to drive UI rendering based on the delegates.
  browser_renderer_ = std::make_unique<BrowserRenderer>(
      std::move(ui), std::move(scheduler_delegate),
      std::move(initializing_graphics_), std::move(input_delegate),
      browser_renderer_interface, kSlidingAverageSize);

  graphics_->ClearContext();

  started_ = true;
}

void VRBrowserRendererThreadWin::OnSpinnerVisibilityChanged(bool visible) {
  if (!draw_state_.SetSpinnerVisible(visible))
    return;
  if (draw_state_.ShouldDrawUI()) {
    StartOverlay();
  }

  if (overlay_) {
    overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                           draw_state_.ShouldDrawWebXR());
  }

  if (draw_state_.ShouldDrawUI()) {
    if (overlay_)  // False only while testing.
      overlay_->RequestNextOverlayPose(
          base::BindOnce(&VRBrowserRendererThreadWin::OnPose,
                         base::Unretained(this), GetNextRequestId()));
  } else {
    StopOverlay();
  }
}

void VRBrowserRendererThreadWin::OnWebXRSubmitted() {
  if (scheduler_ui_)
    scheduler_ui_->OnWebXrFrameAvailable();
  StopWebXrTimeout();
}

device::mojom::XRFrameDataPtr ValidateFrameData(
    device::mojom::XRFrameDataPtr& data) {
  device::mojom::XRFrameDataPtr ret = device::mojom::XRFrameData::New();
  ret->pose = device::mojom::VRPose::New();

  if (data->pose) {
    if (data->pose->orientation && data->pose->orientation->size() == 4) {
      float length = 0;
      for (int i = 0; i < 4; ++i) {
        length += (*data->pose->orientation)[i] * (*data->pose->orientation)[i];
      }

      float kEpsilson = 0.1f;
      if (abs(length - 1) < kEpsilson) {
        ret->pose->orientation = std::vector<float>{0, 0, 0, 1};
        for (int i = 0; i < 4; ++i) {
          (*ret->pose->orientation)[i] = (*data->pose->orientation)[i] / length;
        }
      }
    }

    if (data->pose->position && data->pose->position->size() == 3) {
      ret->pose->position = data->pose->position;
      // We'll never give position values outside this range.
      float kMaxPosition = 1000000;
      float kMinPosition = -kMaxPosition;
      for (int i = 0; i < 3; ++i) {
        if (!((*ret->pose->position)[i] < kMaxPosition) ||
            !((*ret->pose->position)[i] > kMinPosition)) {
          ret->pose->position = base::nullopt;
          // If testing with unexpectedly high values, catch on debug builds
          // rather than silently change data.  On release builds its better to
          // be safe and validate.
          DCHECK(false);
          break;
        }
      }
    }
  }  // if (data->pose)

  if (!ret->pose->orientation) {
    ret->pose->orientation = std::vector<float>{0, 0, 0, 1};
  }

  if (!ret->pose->position) {
    ret->pose->position = std::vector<float>{0, 0, 0};
  }

  ret->frame_id = data->frame_id;

  // Frame data has several other fields that we are ignoring.  If they are
  // used, validate them before use.
  return ret;
}

void VRBrowserRendererThreadWin::OnPose(int request_id,
                                        device::mojom::XRFrameDataPtr data) {
  if (request_id != current_request_id_) {
    // Old request. Do nothing.
    return;
  }

  if (!draw_state_.ShouldDrawUI()) {
    // We shouldn't be showing UI.
    overlay_->SetOverlayAndWebXRVisibility(draw_state_.ShouldDrawUI(),
                                           draw_state_.ShouldDrawWebXR());
    if (graphics_)
      graphics_->ResetMemoryBuffer();
    return;
  }

  data = ValidateFrameData(data);

  // Deliver pose to input and scheduler.
  DCHECK(data);
  DCHECK(data->pose);
  DCHECK(data->pose->orientation);
  DCHECK(data->pose->position);
  const std::vector<float>& quat = *data->pose->orientation;
  const std::vector<float>& pos = *data->pose->position;

  // The incoming pose represents where the headset is in "world space".  So
  // we'll need to invert to get the view transform.

  // Negating the w component will invert the rotation.
  gfx::Transform head_from_unoriented_head(
      gfx::Quaternion(quat[0], quat[1], quat[2], -quat[3]));

  // Negating all components will invert the translation.
  gfx::Transform unoriented_head_from_world;
  unoriented_head_from_world.Translate3d(-pos[0], -pos[1], -pos[2]);

  // Compose these to get the base "view" matrix (before accounting for per-eye
  // transforms).
  gfx::Transform head_from_world =
      head_from_unoriented_head * unoriented_head_from_world;

  input_->OnPose(head_from_world);
  graphics_->PreRender();

  // base::Unretained is safe because scheduler_ will be destroyed without
  // calling the callback if we are destroyed.
  scheduler_->OnPose(base::BindOnce(&VRBrowserRendererThreadWin::SubmitFrame,
                                    base::Unretained(this), std::move(data)),
                     head_from_world, draw_state_.ShouldDrawWebXR(),
                     draw_state_.ShouldDrawUI());
}

void VRBrowserRendererThreadWin::SubmitFrame(
    device::mojom::XRFrameDataPtr data) {
  graphics_->PostRender();

  overlay_->SubmitOverlayTexture(
      data->frame_id, graphics_->GetTexture(), graphics_->GetLeft(),
      graphics_->GetRight(),
      base::BindOnce(&VRBrowserRendererThreadWin::SubmitResult,
                     base::Unretained(this)));
}

void VRBrowserRendererThreadWin::SubmitResult(bool success) {
  if (!success && graphics_) {
    graphics_->ResetMemoryBuffer();
  }
  if (scheduler_ui_ && success && !waiting_for_first_frame_)
    scheduler_ui_->OnWebXrFrameAvailable();
  if (draw_state_.ShouldDrawUI() && started_) {
    overlay_->RequestNextOverlayPose(
        base::BindOnce(&VRBrowserRendererThreadWin::OnPose,
                       base::Unretained(this), GetNextRequestId()));
  }
}

// VRBrowserRendererThreadWin::DrawContentType functions.
bool VRBrowserRendererThreadWin::DrawState::ShouldDrawUI() {
  return prompt_ != ExternalPromptNotificationType::kPromptNone ||
         spinner_visible_ || indicators_visible_;
}

bool VRBrowserRendererThreadWin::DrawState::ShouldDrawWebXR() {
  return ((prompt_ == ExternalPromptNotificationType::kPromptNone ||
           indicators_visible_) &&
          !spinner_visible_);
}

bool VRBrowserRendererThreadWin::DrawState::SetPrompt(
    ExternalPromptNotificationType prompt) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  prompt_ = prompt;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

bool VRBrowserRendererThreadWin::DrawState::SetSpinnerVisible(bool visible) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  spinner_visible_ = visible;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

bool VRBrowserRendererThreadWin::DrawState::SetIndicatorsVisible(bool visible) {
  bool old_ui = ShouldDrawUI();
  bool old_webxr = ShouldDrawWebXR();
  indicators_visible_ = visible;
  return old_ui != ShouldDrawUI() || old_webxr != ShouldDrawWebXR();
}

}  // namespace vr
