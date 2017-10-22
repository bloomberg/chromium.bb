// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_gl_thread.h"

#include <utility>

#include "base/message_loop/message_loop.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_gl.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/toolbar_state.h"
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
  vr_shell_gl_ =
      base::MakeUnique<VrShellGl>(this, this, ui_initial_state_, gvr_api_,
                                  reprojected_rendering_, daydream_support_);

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

void VrGLThread::ProcessContentGesture(
    std::unique_ptr<blink::WebInputEvent> event,
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

void VrGLThread::OnUnsupportedMode(vr::UiUnsupportedMode mode) {
  DCHECK(OnGlThread());
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnUnsupportedMode, weak_vr_shell_, mode));
}

void VrGLThread::OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                                      vr::ExitVrPromptChoice choice) {
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

void VrGLThread::SetAudioCapturingIndicator(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetAudioCapturingIndicator,
                            browser_ui_, enabled));
}

void VrGLThread::SetLocationAccessIndicator(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetLocationAccessIndicator,
                            browser_ui_, enabled));
}

void VrGLThread::SetVideoCapturingIndicator(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::BrowserUiInterface::SetVideoCapturingIndicator,
                            browser_ui_, enabled));
}

void VrGLThread::SetScreenCapturingIndicator(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::SetScreenCapturingIndicator,
                 browser_ui_, enabled));
}

void VrGLThread::SetBluetoothConnectedIndicator(bool enabled) {
  DCHECK(OnMainThread());
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::BrowserUiInterface::SetBluetoothConnectedIndicator,
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

bool VrGLThread::OnMainThread() const {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

bool VrGLThread::OnGlThread() const {
  return task_runner()->BelongsToCurrentThread();
}

}  // namespace vr_shell
