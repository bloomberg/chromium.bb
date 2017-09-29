// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_manager.h"
#include "chrome/browser/vr/vr_shell_renderer.h"

namespace vr {

Ui::Ui(UiBrowserInterface* browser,
       ContentInputDelegate* content_input_delegate,
       const vr::UiInitialState& ui_initial_state)
    : scene_(base::MakeUnique<vr::UiScene>()),
      model_(base::MakeUnique<vr::Model>()),
      scene_manager_(
          base::MakeUnique<vr::UiSceneManager>(browser,
                                               scene_.get(),
                                               content_input_delegate,
                                               model_.get(),
                                               ui_initial_state)),
      input_manager_(base::MakeUnique<vr::UiInputManager>(scene_.get())),
      weak_ptr_factory_(this) {}

Ui::~Ui() = default;

base::WeakPtr<vr::BrowserUiInterface> Ui::GetBrowserUiWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void Ui::SetWebVrMode(bool enabled, bool show_toast) {
  scene_manager_->SetWebVrMode(enabled, show_toast);
}

void Ui::SetFullscreen(bool enabled) {
  scene_manager_->SetFullscreen(enabled);
}

void Ui::SetToolbarState(const ToolbarState& state) {
  scene_manager_->SetToolbarState(state);
}

void Ui::SetIncognito(bool enabled) {
  scene_manager_->SetIncognito(enabled);
}

void Ui::SetWebVrSecureOrigin(bool secure) {
  scene_manager_->SetWebVrSecureOrigin(secure);
}

void Ui::SetLoading(bool loading) {
  model_->loading = loading;
}

void Ui::SetLoadProgress(float progress) {
  model_->load_progress = progress;
}

void Ui::SetIsExiting() {
  scene_manager_->SetIsExiting();
}

void Ui::SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward) {
  scene_manager_->SetHistoryButtonsEnabled(can_go_back, can_go_forward);
}

void Ui::SetVideoCapturingIndicator(bool enabled) {
  scene_manager_->SetVideoCapturingIndicator(enabled);
}

void Ui::SetScreenCapturingIndicator(bool enabled) {
  scene_manager_->SetScreenCapturingIndicator(enabled);
}

void Ui::SetAudioCapturingIndicator(bool enabled) {
  scene_manager_->SetAudioCapturingIndicator(enabled);
}

void Ui::SetBluetoothConnectedIndicator(bool enabled) {
  scene_manager_->SetBluetoothConnectedIndicator(enabled);
}

void Ui::SetLocationAccessIndicator(bool enabled) {
  scene_manager_->SetLocationAccessIndicator(enabled);
}

void Ui::SetExitVrPromptEnabled(bool enabled, UiUnsupportedMode reason) {
  scene_manager_->SetExitVrPromptEnabled(enabled, reason);
}

bool Ui::ShouldRenderWebVr() {
  return scene_manager_->ShouldRenderWebVr();
}

void Ui::OnGlInitialized(unsigned int content_texture_id) {
  vr_shell_renderer_ = base::MakeUnique<vr::VrShellRenderer>();
  ui_renderer_ =
      base::MakeUnique<vr::UiRenderer>(scene_.get(), vr_shell_renderer_.get());
  scene_manager_->OnGlInitialized(content_texture_id);
}

void Ui::OnAppButtonClicked() {
  scene_manager_->OnAppButtonClicked();
}

void Ui::OnAppButtonGesturePerformed(UiInterface::Direction direction) {
  scene_manager_->OnAppButtonGesturePerformed(direction);
}

void Ui::OnProjMatrixChanged(const gfx::Transform& proj_matrix) {
  scene_manager_->OnProjMatrixChanged(proj_matrix);
}

void Ui::OnWebVrFrameAvailable() {
  scene_manager_->OnWebVrFrameAvailable();
}

void Ui::OnWebVrTimedOut() {
  scene_manager_->OnWebVrTimedOut();
}

}  // namespace vr
