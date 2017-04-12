// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/ui_element.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"

namespace vr_shell {

namespace {

enum Elements {
  // For now, use fixed element IDs, and ensure they do not collide with HTML UI
  // dynamically-chosen ID numbers.
  SecureOriginWarning = 1000,
};

}  // namespace

UiSceneManager::UiSceneManager(UiScene* scene)
    : scene_(scene), weak_ptr_factory_(this) {
  // Create an invisible dummy warning quad. Actual security warnings will come
  // in a follow-on chance.
  auto warning = base::MakeUnique<UiElement>();
  warning->id = Elements::SecureOriginWarning;
  warning->name = "test quad";
  warning->translation = {0, 0, -1};
  warning->fill = vr_shell::Fill::NONE;
  scene_->AddUiElement(std::move(warning));
}

UiSceneManager::~UiSceneManager() {}

base::WeakPtr<UiSceneManager> UiSceneManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UiSceneManager::UpdateScene(std::unique_ptr<base::ListValue> commands) {
  scene_->HandleCommands(std::move(commands), base::TimeTicks::Now());
}

void UiSceneManager::SetWebVRMode(bool web_vr) {
  web_vr_mode_ = web_vr;
}

void UiSceneManager::SetWebVRSecureOrigin(bool secure) {
  secure_origin_ = secure;
}

}  // namespace vr_shell
