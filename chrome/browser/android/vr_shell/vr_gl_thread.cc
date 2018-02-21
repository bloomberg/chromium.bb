// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_gl_thread.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "chrome/browser/android/vr_shell/vr_input_connection.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_gl.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/common/chrome_features.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

VrGLThread::VrGLThread(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    const UiInitialState& ui_initial_state,
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

void VrGLThread::SetInputConnection(
    base::WeakPtr<VrInputConnection> weak_connection) {
  DCHECK(OnGlThread());
  weak_input_connection_ = weak_connection;
}

void VrGLThread::Init() {
  bool keyboard_enabled =
      base::FeatureList::IsEnabled(features::kVrBrowserKeyboard) &&
      !ui_initial_state_.web_vr_autopresentation_expected;
  if (keyboard_enabled) {
    keyboard_delegate_ = GvrKeyboardDelegate::Create();
    text_input_delegate_ = std::make_unique<TextInputDelegate>();
  }
  auto* keyboard_delegate =
      !keyboard_delegate_ ? nullptr : keyboard_delegate_.get();
  auto ui = std::make_unique<Ui>(this, this, keyboard_delegate,
                                 text_input_delegate_.get(), ui_initial_state_);
  if (keyboard_enabled) {
    text_input_delegate_->SetRequestFocusCallback(
        base::BindRepeating(&Ui::RequestFocus, base::Unretained(ui.get())));
    text_input_delegate_->SetRequestUnfocusCallback(
        base::BindRepeating(&Ui::RequestUnfocus, base::Unretained(ui.get())));
    if (keyboard_delegate) {
      keyboard_delegate_->SetUiInterface(ui.get());
      text_input_delegate_->SetUpdateInputCallback(
          base::BindRepeating(&GvrKeyboardDelegate::UpdateInput,
                              base::Unretained(keyboard_delegate_.get())));
    }
  }

  if (ui_initial_state_.assets_available) {
    LoadAssets();
  }

  vr_shell_gl_ = std::make_unique<VrShellGl>(
      this, std::move(ui), gvr_api_, reprojected_rendering_, daydream_support_,
      ui_initial_state_.in_web_vr);

  weak_browser_ui_ = vr_shell_gl_->GetBrowserUiWeakPtr();

  vr_shell_gl_->Initialize();
}

void VrGLThread::CleanUp() {
  vr_shell_gl_.reset();
}

void VrGLThread::ContentSurfaceCreated(jobject surface,
                                       gl::SurfaceTexture* texture) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::ContentSurfaceCreated, weak_vr_shell_,
                                surface, base::Unretained(texture)));
}

void VrGLThread::ContentOverlaySurfaceCreated(jobject surface,
                                              gl::SurfaceTexture* texture) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VrShell::ContentOverlaySurfaceCreated, weak_vr_shell_,
                     surface, base::Unretained(texture)));
}

void VrGLThread::DialogSurfaceCreated(jobject surface,
                                      gl::SurfaceTexture* texture) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::DialogSurfaceCreated, weak_vr_shell_,
                            surface, base::Unretained(texture)));
}

void VrGLThread::GvrDelegateReady(
    gvr::ViewerType viewer_type,
    device::mojom::VRDisplayFrameTransportOptionsPtr transport_options) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::GvrDelegateReady, weak_vr_shell_,
                                viewer_type, std::move(transport_options)));
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

void VrGLThread::OnWebInputEdited(const std::vector<vr::KeyboardEdit>& edits) {
  DCHECK(OnGlThread());
  DCHECK(weak_input_connection_);
  weak_input_connection_->OnKeyboardEdit(edits);
}

void VrGLThread::RequestWebInputText(vr::TextStateUpdateCallback callback) {
  DCHECK(OnGlThread());
  DCHECK(weak_input_connection_);
  weak_input_connection_->RequestTextState(std::move(callback));
}

void VrGLThread::ForwardDialogEvent(
    std::unique_ptr<blink::WebInputEvent> event) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ProcessDialogGesture, weak_vr_shell_,
                            base::Passed(std::move(event))));
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
  // TODO(vollick): Ui should hang onto the appropriate pointer rather than
  // bouncing through VrGLThread.
  vr_shell_gl_->OnExitPresent();
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

void VrGLThread::CloseHostedDialog() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindRepeating(&VrShell::CloseHostedDialog, weak_vr_shell_));
}

void VrGLThread::ToggleCardboardGamepad(bool enabled) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::ToggleCardboardGamepad, weak_vr_shell_, enabled));
}

void VrGLThread::OnUnsupportedMode(UiUnsupportedMode mode) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnUnsupportedMode, weak_vr_shell_, mode));
}

void VrGLThread::OnExitVrPromptResult(ExitVrPromptChoice choice,
                                      UiUnsupportedMode reason) {
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

void VrGLThread::StartAutocomplete(const AutocompleteRequest& request) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindRepeating(&VrShell::StartAutocomplete,
                                     weak_vr_shell_, request));
}

void VrGLThread::StopAutocomplete() {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::StopAutocomplete, weak_vr_shell_));
}

void VrGLThread::LoadAssets() {
  AssetsLoader::GetInstance()->Load(
      base::BindOnce(&VrGLThread::OnAssetsLoaded, base::Unretained(this)));
}

void VrGLThread::SetFullscreen(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&BrowserUiInterface::SetFullscreen,
                                     weak_browser_ui_, enabled));
}

void VrGLThread::SetIncognito(bool incognito) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetIncognito, weak_browser_ui_,
                            incognito));
}

void VrGLThread::SetHistoryButtonsEnabled(bool can_go_back,
                                          bool can_go_forward) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetHistoryButtonsEnabled,
                            weak_browser_ui_, can_go_back, can_go_forward));
}

void VrGLThread::SetLoadProgress(float progress) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&BrowserUiInterface::SetLoadProgress,
                                     weak_browser_ui_, progress));
}

void VrGLThread::SetLoading(bool loading) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE, base::Bind(&BrowserUiInterface::SetLoading,
                                                weak_browser_ui_, loading));
}

void VrGLThread::SetToolbarState(const ToolbarState& state) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&BrowserUiInterface::SetToolbarState,
                                     weak_browser_ui_, state));
}

void VrGLThread::SetWebVrMode(bool enabled, bool show_toast) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetWebVrMode, weak_browser_ui_,
                            enabled, show_toast));
}

void VrGLThread::SetAudioCaptureEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetAudioCaptureEnabled,
                            weak_browser_ui_, enabled));
}

void VrGLThread::SetLocationAccessEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetLocationAccessEnabled,
                            weak_browser_ui_, enabled));
}

void VrGLThread::SetVideoCaptureEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetVideoCaptureEnabled,
                            weak_browser_ui_, enabled));
}

void VrGLThread::SetScreenCaptureEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetScreenCaptureEnabled,
                            weak_browser_ui_, enabled));
}

void VrGLThread::SetBluetoothConnected(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&BrowserUiInterface::SetBluetoothConnected,
                                     weak_browser_ui_, enabled));
}

void VrGLThread::SetIsExiting() {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserUiInterface::SetIsExiting, weak_browser_ui_));
}

void VrGLThread::SetExitVrPromptEnabled(bool enabled,
                                        UiUnsupportedMode reason) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetExitVrPromptEnabled,
                            weak_browser_ui_, enabled, reason));
}

void VrGLThread::SetSpeechRecognitionEnabled(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&BrowserUiInterface::SetSpeechRecognitionEnabled,
                            weak_browser_ui_, enabled));
}

void VrGLThread::SetRecognitionResult(const base::string16& result) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&BrowserUiInterface::SetRecognitionResult,
                                     weak_browser_ui_, result));
}

void VrGLThread::OnSpeechRecognitionStateChanged(int new_state) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserUiInterface::OnSpeechRecognitionStateChanged,
                 weak_browser_ui_, new_state));
}

void VrGLThread::SetOmniboxSuggestions(
    std::unique_ptr<OmniboxSuggestions> suggestions) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserUiInterface::SetOmniboxSuggestions, weak_browser_ui_,
                 base::Passed(std::move(suggestions))));
}

void VrGLThread::OnAssetsComponentReady() {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::BindRepeating(&BrowserUiInterface::OnAssetsComponentReady,
                          weak_browser_ui_));
}

void VrGLThread::ShowSoftInput(bool show) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::BindRepeating(&BrowserUiInterface::ShowSoftInput,
                                     weak_browser_ui_, show));
}

void VrGLThread::UpdateWebInputIndices(int selection_start,
                                       int selection_end,
                                       int composition_start,
                                       int composition_end) {
  task_runner()->PostTask(
      FROM_HERE,
      base::BindRepeating(&BrowserUiInterface::UpdateWebInputIndices,
                          weak_browser_ui_, selection_start, selection_end,
                          composition_start, composition_end));
}

bool VrGLThread::OnMainThread() const {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

bool VrGLThread::OnGlThread() const {
  return task_runner()->BelongsToCurrentThread();
}

void VrGLThread::OnAssetsLoaded(AssetsLoadStatus status,
                                std::unique_ptr<Assets> assets,
                                const base::Version& component_version) {
  vr_shell_gl_->OnAssetsLoaded(status, std::move(assets), component_version);
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VrShell::OnAssetsLoaded, weak_vr_shell_,
                                status, component_version));
}

}  // namespace vr
