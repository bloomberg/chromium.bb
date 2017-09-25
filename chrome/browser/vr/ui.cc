// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui.h"

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
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
      scene_manager_(
          base::MakeUnique<vr::UiSceneManager>(browser,
                                               scene_.get(),
                                               content_input_delegate,
                                               ui_initial_state)) {}

Ui::~Ui() = default;

UiInterface* Ui::ui() {
  return scene_manager_.get();
}

base::WeakPtr<vr::BrowserUiInterface> Ui::GetBrowserUiWeakPtr() {
  return scene_manager_->GetWeakPtr();
}

void Ui::OnGlInitialized(unsigned int content_texture_id) {
  ui()->OnGlInitialized(content_texture_id);
  input_manager_ = base::MakeUnique<vr::UiInputManager>(scene_.get());
  vr_shell_renderer_ = base::MakeUnique<vr::VrShellRenderer>();
  ui_renderer_ =
      base::MakeUnique<vr::UiRenderer>(scene_.get(), vr_shell_renderer_.get());
}

}  // namespace vr
