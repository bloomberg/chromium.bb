// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_gl_thread.h"

#include <utility>

#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/ui_scene_manager.h"
#include "chrome/browser/android/vr_shell/vr_input_manager.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"
#include "chrome/browser/android/vr_shell/vr_shell_gl.h"

namespace vr_shell {

VrGLThread::VrGLThread(
    const base::WeakPtr<VrShell>& weak_vr_shell,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
    gvr_context* gvr_api,
    bool initially_web_vr,
    bool reprojected_rendering)
    : base::Thread("VrShellGL"),
      weak_vr_shell_(weak_vr_shell),
      main_thread_task_runner_(std::move(main_thread_task_runner)),
      gvr_api_(gvr_api),
      initially_web_vr_(initially_web_vr),
      reprojected_rendering_(reprojected_rendering) {}

VrGLThread::~VrGLThread() {
  Stop();
}

void VrGLThread::Init() {
  scene_ = base::MakeUnique<UiScene>();
  vr_shell_gl_ = base::MakeUnique<VrShellGl>(
      this, gvr_api_, initially_web_vr_, reprojected_rendering_, scene_.get());
  scene_manager_ = base::MakeUnique<UiSceneManager>(this, scene_.get());

  weak_vr_shell_gl_ = vr_shell_gl_->GetWeakPtr();
  weak_scene_manager_ = scene_manager_->GetWeakPtr();
  vr_shell_gl_->Initialize();
}

void VrGLThread::ContentSurfaceChanged(jobject surface) {
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VrShell::ContentSurfaceChanged, weak_vr_shell_, surface));
}

void VrGLThread::GvrDelegateReady() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::GvrDelegateReady, weak_vr_shell_));
}

void VrGLThread::UpdateGamepadData(device::GvrGamepadData pad) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::UpdateGamepadData, weak_vr_shell_, pad));
}

void VrGLThread::AppButtonGesturePerformed(UiInterface::Direction direction) {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&VrShell::AppButtonGesturePerformed, weak_vr_shell_,
                            direction));
}

void VrGLThread::OnAppButtonClicked() {
  weak_vr_shell_gl_->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&UiSceneManager::OnAppButtonClicked, weak_scene_manager_));
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

void VrGLThread::CleanUp() {
  scene_manager_.reset();
  vr_shell_gl_.reset();
  scene_.reset();
}

}  // namespace vr_shell
