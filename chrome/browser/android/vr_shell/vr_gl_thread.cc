// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_gl_thread.h"

#include <utility>

#include "chrome/browser/android/vr_shell/vr_input_manager.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_gl.h"
#include "chrome/browser/vr/toolbar_state.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr_shell {

VrGLThread::VrGLThread(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    bool initially_web_vr,
    bool web_vr_autopresentation_expected,
    bool in_cct,
    bool reprojected_rendering,
    bool daydream_support)
    : base::Thread("VrShellGL"),
      weak_vr_shell_(weak_vr_shell),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      gvr_api_(gvr_api),
      initially_web_vr_(initially_web_vr),
      web_vr_autopresentation_expected_(web_vr_autopresentation_expected),
      in_cct_(in_cct),
      reprojected_rendering_(reprojected_rendering),
      daydream_support_(daydream_support) {}

VrGLThread::~VrGLThread() {
  Stop();
}

void VrGLThread::Init() {
  scene_ = base::MakeUnique<vr::UiScene>();
  vr_shell_gl_ = base::MakeUnique<VrShellGl>(this, gvr_api_, initially_web_vr_,
                                             reprojected_rendering_,
                                             daydream_support_, scene_.get());
  scene_manager_ = base::MakeUnique<vr::UiSceneManager>(
      this, scene_.get(), vr_shell_gl_.get(), in_cct_, initially_web_vr_,
      web_vr_autopresentation_expected_);

  weak_vr_shell_gl_ = vr_shell_gl_->GetWeakPtr();
  weak_scene_manager_ = scene_manager_->GetWeakPtr();
  vr_shell_gl_->Initialize();
}

void VrGLThread::ContentSurfaceChanged(jobject surface) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::ContentSurfaceChanged, weak_vr_shell_, surface));
}

void VrGLThread::GvrDelegateReady(gvr::ViewerType viewer_type) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::GvrDelegateReady, weak_vr_shell_, viewer_type));
}

void VrGLThread::UpdateGamepadData(device::GvrGamepadData pad) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::UpdateGamepadData, weak_vr_shell_, pad));
}

void VrGLThread::AppButtonClicked() {
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::UiSceneManager::OnAppButtonClicked, weak_scene_manager_));
}

void VrGLThread::AppButtonGesturePerformed(
    vr::UiInterface::Direction direction) {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::OnAppButtonGesturePerformed,
                            weak_scene_manager_, direction));
}

void VrGLThread::ProcessContentGesture(
    std::unique_ptr<blink::WebInputEvent> event) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ProcessContentGesture, weak_vr_shell_,
                            base::Passed(std::move(event))));
}

void VrGLThread::ForceExitVr() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ForceExitVr, weak_vr_shell_));
}

void VrGLThread::ExitPresent() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ExitPresent, weak_vr_shell_));
}

void VrGLThread::ExitFullscreen() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ExitFullscreen, weak_vr_shell_));
}

void VrGLThread::RunVRDisplayInfoCallback(
    const base::Callback<void(device::mojom::VRDisplayInfoPtr)>& callback,
    device::mojom::VRDisplayInfoPtr* info) {
  main_thread_task_runner_->PostTask(FROM_HERE,
                                     base::Bind(callback, base::Passed(info)));
}

void VrGLThread::OnContentPaused(bool enabled) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::OnContentPaused, weak_vr_shell_, enabled));
}

void VrGLThread::NavigateBack() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::NavigateBack, weak_vr_shell_));
}

void VrGLThread::ExitCct() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::ExitCct, weak_vr_shell_));
}

void VrGLThread::ToggleCardboardGamepad(bool enabled) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::ToggleCardboardGamepad, weak_vr_shell_, enabled));
}

void VrGLThread::OnGlInitialized(unsigned int content_texture_id) {
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::OnGlInitialized,
                                     weak_scene_manager_, content_texture_id));
}

void VrGLThread::OnUnsupportedMode(vr::UiUnsupportedMode mode) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnUnsupportedMode, weak_vr_shell_, mode));
}

void VrGLThread::OnExitVrPromptResult(vr::UiUnsupportedMode reason,
                                      vr::ExitVrPromptChoice choice) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::OnExitVrPromptResult, weak_vr_shell_,
                            reason, choice));
}

void VrGLThread::SetFullscreen(bool enabled) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::SetFullscreen,
                                     weak_scene_manager_, enabled));
}

void VrGLThread::SetIncognito(bool incognito) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::SetIncognito,
                                     weak_scene_manager_, incognito));
}

void VrGLThread::SetHistoryButtonsEnabled(bool can_go_back,
                                          bool can_go_forward) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::SetHistoryButtonsEnabled,
                            weak_scene_manager_, can_go_back, can_go_forward));
}

void VrGLThread::SetLoadProgress(float progress) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::SetLoadProgress,
                                     weak_scene_manager_, progress));
}

void VrGLThread::SetLoading(bool loading) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE, base::Bind(&vr::UiSceneManager::SetLoading,
                                                weak_scene_manager_, loading));
}

void VrGLThread::SetToolbarState(const vr::ToolbarState& state) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::SetToolbarState,
                                     weak_scene_manager_, state));
}

void VrGLThread::SetWebVrMode(bool enabled, bool show_toast) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::SetWebVrMode,
                                     weak_scene_manager_, enabled, show_toast));
}

void VrGLThread::SetWebVrSecureOrigin(bool secure) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::SetWebVrSecureOrigin,
                                     weak_scene_manager_, secure));
}

void VrGLThread::SetAudioCapturingIndicator(bool enabled) {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::SetAudioCapturingIndicator,
                            weak_scene_manager_, enabled));
}

void VrGLThread::SetLocationAccessIndicator(bool enabled) {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::SetLocationAccessIndicator,
                            weak_scene_manager_, enabled));
}

void VrGLThread::SetVideoCapturingIndicator(bool enabled) {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::SetVideoCapturingIndicator,
                            weak_scene_manager_, enabled));
}

void VrGLThread::SetScreenCapturingIndicator(bool enabled) {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::SetScreenCapturingIndicator,
                            weak_scene_manager_, enabled));
}

void VrGLThread::SetBluetoothConnectedIndicator(bool enabled) {
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::SetBluetoothConnectedIndicator,
                            weak_scene_manager_, enabled));
}

void VrGLThread::SetIsExiting() {
  WaitUntilThreadStarted();
  task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&vr::UiSceneManager::SetIsExiting, weak_scene_manager_));
}

void VrGLThread::SetSplashScreenIcon(const SkBitmap& bitmap) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(FROM_HERE,
                          base::Bind(&vr::UiSceneManager::SetSplashScreenIcon,
                                     weak_scene_manager_, bitmap));
}

void VrGLThread::OnWebVrFrameAvailable() {
  DCHECK(task_runner()->BelongsToCurrentThread());
  scene_manager_->OnWebVrFrameAvailable();
}

void VrGLThread::SetExitVrPromptEnabled(bool enabled,
                                        vr::UiUnsupportedMode reason) {
  WaitUntilThreadStarted();
  task_runner()->PostTask(
      FROM_HERE, base::Bind(&vr::UiSceneManager::SetExitVrPromptEnabled,
                            weak_scene_manager_, enabled, reason));
}

void VrGLThread::CleanUp() {
  scene_manager_.reset();
  vr_shell_gl_.reset();
  scene_.reset();
}

}  // namespace vr_shell
