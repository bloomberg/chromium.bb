// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_gl_thread.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/version.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_gl.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/common/chrome_features.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr_shell {

VrGLThread::VrGLThread(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    const vr::UiInitialState& ui_initial_state,
    bool reprojected_rendering,
    bool daydream_support)
    : base::android::JavaHandlerThread("VrShellGL"),
      weak_vr_shell_(weak_vr_shell),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      gvr_api_(gvr_api),
      ui_initial_state_(ui_initial_state),
      reprojected_rendering_(reprojected_rendering),
      daydream_support_(daydream_support) {}

VrGLThread::~VrGLThread() {
  Stop();
}

base::WeakPtr<VrShellGl> VrGLThread::GetVrShellGl() {
  return vr_shell_gl_->GetWeakPtr();
}

void VrGLThread::Init() {
  bool keyboard_enabled =
      base::FeatureList::IsEnabled(features::kVrBrowserKeyboard);
  if (keyboard_enabled) {
    keyboard_delegate_ = GvrKeyboardDelegate::Create();
    text_input_delegate_ = base::MakeUnique<vr::TextInputDelegate>();
  }
  auto* keyboard_delegate =
      !keyboard_delegate_ ? nullptr : keyboard_delegate_.get();
  auto ui =
      base::MakeUnique<vr::Ui>(this, this, keyboard_delegate,
                               text_input_delegate_.get(), ui_initial_state_);
  if (keyboard_enabled) {
    text_input_delegate_->SetRequestFocusCallback(
        base::BindRepeating(&vr::Ui::RequestFocus, base::Unretained(ui.get())));
    if (keyboard_delegate) {
      keyboard_delegate_->SetUiInterface(ui.get());
      text_input_delegate_->SetUpdateInputCallback(
          base::BindRepeating(&GvrKeyboardDelegate::UpdateInput,
                              base::Unretained(keyboard_delegate_.get())));
    }
  }

  vr_shell_gl_ = base::MakeUnique<VrShellGl>(
      this, std::move(ui), gvr_api_, reprojected_rendering_, daydream_support_,
      ui_initial_state_.in_web_vr, keyboard_delegate_.get());

  browser_ui_ = vr_shell_gl_->GetBrowserUiWeakPtr();

  vr_shell_gl_->Initialize();
}

void VrGLThread::CleanUp() {
  vr_shell_gl_.reset();
}

void VrGLThread::ContentSurfaceChanged(jobject surface) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::ContentSurfaceChanged, weak_vr_shell_, surface));
}

void VrGLThread::GvrDelegateReady(gvr::ViewerType viewer_type) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::GvrDelegateReady, weak_vr_shell_, viewer_type));
}

void VrGLThread::UpdateGamepadData(device::GvrGamepadData pad) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::UpdateGamepadData, weak_vr_shell_, pad));
}

void VrGLThread::ForwardEvent(std::unique_ptr<blink::WebInputEvent> event,
                              int content_id) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ProcessContentGesture, weak_vr_shell_,
                            base::Passed(std::move(event)), content_id));
}

void VrGLThread::ForceExitVr() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ForceExitVr, weak_vr_shell_));
}

void VrGLThread::ExitPresent() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ExitPresent, weak_vr_shell_));
}

void VrGLThread::ExitFullscreen() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ExitFullscreen, weak_vr_shell_));
}

void VrGLThread::OnContentPaused(bool enabled) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::OnContentPaused, weak_vr_shell_, enabled));
}

void VrGLThread::Navigate(GURL gurl) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::Navigate, weak_vr_shell_, gurl));
}

void VrGLThread::NavigateBack() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::NavigateBack, weak_vr_shell_));
}

void VrGLThread::ExitCct() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ExitCct, weak_vr_shell_));
}

void VrGLThread::ToggleCardboardGamepad(bool enabled) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::ToggleCardboardGamepad, weak_vr_shell_, enabled));
}

void VrGLThread::OnAssetsLoaded(vr::AssetsLoadStatus status,
                                const base::Version& component_version) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnAssetsLoaded, weak_vr_shell_, status,
                            component_version));
}

void VrGLThread::OnUnsupportedMode(vr::UiUnsupportedMode mode) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnUnsupportedMode, weak_vr_shell_, mode));
}

void VrGLThread::OnExitVrPromptResult(vr::ExitVrPromptChoice choice,
                                      vr::UiUnsupportedMode reason) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnExitVrPromptResult, weak_vr_shell_,
                            reason, choice));
}

void VrGLThread::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnContentScreenBoundsChanged,
                            weak_vr_shell_, bounds));
}

void VrGLThread::SetVoiceSearchActive(bool active) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::SetVoiceSearchActive, weak_vr_shell_, active));
}

void VrGLThread::StartAutocomplete(const base::string16& string) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::StartAutocomplete, weak_vr_shell_, string));
}

void VrGLThread::StopAutocomplete() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::StopAutocomplete, weak_vr_shell_));
}

void VrGLThread::SetFullscreen(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::SetFullscreen, browser_ui_, enabled));
}

void VrGLThread::SetIncognito(bool incognito) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetIncognito, browser_ui_,
                            incognito));
}

void VrGLThread::SetHistoryButtonsEnabled(bool can_go_back,
                                          bool can_go_forward) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetHistoryButtonsEnabled,
                            browser_ui_, can_go_back, can_go_forward));
}

void VrGLThread::SetLoadProgress(float progress) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::BrowserUiInterface::SetLoadProgress,
                                     browser_ui_, progress));
}

void VrGLThread::SetLoading(bool loading) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::SetLoading, browser_ui_, loading));
}

void VrGLThread::SetToolbarState(const vr::ToolbarState& state) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::SetToolbarState, browser_ui_, state));
}

void VrGLThread::SetWebVrMode(bool enabled, bool show_toast) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetWebVrMode, browser_ui_,
                            enabled, show_toast));
}

void VrGLThread::SetAudioCaptureEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetAudioCaptureEnabled,
                            browser_ui_, enabled));
}

void VrGLThread::SetLocationAccess(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::BrowserUiInterface::SetLocationAccess,
                                     browser_ui_, enabled));
}

void VrGLThread::SetVideoCaptureEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetVideoCaptureEnabled,
                            browser_ui_, enabled));
}

void VrGLThread::SetScreenCaptureEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetScreenCaptureEnabled,
                            browser_ui_, enabled));
}

void VrGLThread::SetBluetoothConnected(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetBluetoothConnected,
                            browser_ui_, enabled));
}

void VrGLThread::SetIsExiting() {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::SetIsExiting, browser_ui_));
}

void VrGLThread::SetExitVrPromptEnabled(bool enabled,
                                        vr::UiUnsupportedMode reason) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetExitVrPromptEnabled,
                            browser_ui_, enabled, reason));
}

void VrGLThread::SetSpeechRecognitionEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::SetSpeechRecognitionEnabled,
                 browser_ui_, enabled));
}

void VrGLThread::SetRecognitionResult(const base::string16& result) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetRecognitionResult,
                            browser_ui_, result));
}

void VrGLThread::OnSpeechRecognitionStateChanged(int new_state) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::OnSpeechRecognitionStateChanged,
                 browser_ui_, new_state));
}

void VrGLThread::SetOmniboxSuggestions(
    std::unique_ptr<vr::OmniboxSuggestions> suggestions) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetOmniboxSuggestions,
                            browser_ui_, base::Passed(std::move(suggestions))));
}

bool VrGLThread::OnMainThread() const {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

bool VrGLThread::OnGlThread() const {
  return task_runner()->BelongsToCurrentThread();
}

}  // namespace vr_shell
