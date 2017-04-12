// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_gl_thread.h"

#include <utility>

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
  scene_manager_ = base::MakeUnique<UiSceneManager>(scene_.get());
  vr_shell_gl_ = base::MakeUnique<VrShellGl>(
      std::move(weak_vr_shell_), std::move(main_thread_task_runner_), gvr_api_,
      initially_web_vr_, reprojected_rendering_, scene_.get());
  weak_vr_shell_gl_ = vr_shell_gl_->GetWeakPtr();
  weak_scene_manager_ = scene_manager_->GetWeakPtr();
  vr_shell_gl_->Initialize();
}

void VrGLThread::CleanUp() {
  vr_shell_gl_.reset();
}

}  // namespace vr_shell
