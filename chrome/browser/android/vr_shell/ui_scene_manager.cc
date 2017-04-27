// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "chrome/browser/android/vr_shell/ui_elements/permanent_security_warning.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"
#include "chrome/browser/android/vr_shell/ui_elements/transient_security_warning.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"

namespace vr_shell {

namespace {

static constexpr int kWarningTimeoutSeconds = 30;
static constexpr float kWarningDistance = 0.7;
static constexpr float kWarningAngleRadians = 16.3 * M_PI / 180.0;

static constexpr float kPermanentWarningHeight = 0.226;
static constexpr float kPermanentWarningWidth = 0.078;
static constexpr float kTransientWarningHeight = 0.512;
static constexpr float kTransientWarningWidth = 0.160;

static constexpr float kContentHeight = 2.4;
static constexpr float kContentWidth = 1.6;
static constexpr float kContentDistance = 2.5;
static constexpr float kContentVerticalOffset = -0.26;

}  // namespace

UiSceneManager::UiSceneManager(UiScene* scene)
    : scene_(scene), weak_ptr_factory_(this) {
  std::unique_ptr<UiElement> element;

  // For now, use an ID range that does not conflict with the HTML UI.
  int id = 1000;

  // Permanent WebVR security warning.
  // TODO(mthiesse): Programatically compute the proper texture size for these
  // textured UI elements.
  element = base::MakeUnique<PermanentSecurityWarning>(512);
  element->id = id++;
  element->name = "Permanent security warning";
  element->fill = vr_shell::Fill::NONE;
  element->size = {kPermanentWarningWidth, kPermanentWarningHeight, 1};
  element->scale = {kWarningDistance, kWarningDistance, 1};
  element->translation = {0, kWarningDistance * sin(kWarningAngleRadians),
                          -kWarningDistance * cos(kWarningAngleRadians)};
  element->rotation = {1.0f, 0, 0, kWarningAngleRadians};
  element->visible = false;
  element->hit_testable = false;
  element->lock_to_fov = true;
  permanent_security_warning_ = element.get();
  scene_->AddUiElement(std::move(element));

  // Transient WebVR security warning.
  element = base::MakeUnique<TransientSecurityWarning>(1024);
  element->id = id++;
  element->name = "Transient security warning";
  element->fill = vr_shell::Fill::NONE;
  element->size = {kTransientWarningWidth, kTransientWarningHeight, 1};
  element->scale = {kWarningDistance, kWarningDistance, 1};
  element->translation = {0, 0, -kWarningDistance};
  element->visible = false;
  element->hit_testable = false;
  element->lock_to_fov = true;
  transient_security_warning_ = element.get();
  scene_->AddUiElement(std::move(element));

  // Main web content quad.
  element = base::MakeUnique<UiElement>();
  element->id = id++;
  element->name = "Content";
  element->fill = vr_shell::Fill::CONTENT;
  element->size = {kContentWidth, kContentHeight, 1};
  element->translation = {0, kContentVerticalOffset, -kContentDistance};
  element->visible = false;
  main_content_ = element.get();
  scene_->AddUiElement(std::move(element));
}

UiSceneManager::~UiSceneManager() {}

base::WeakPtr<UiSceneManager> UiSceneManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UiSceneManager::SetWebVRMode(bool web_vr) {
  web_vr_mode_ = web_vr;
  main_content_->visible = !web_vr_mode_;
  ConfigureSecurityWarnings();
}

void UiSceneManager::SetWebVRSecureOrigin(bool secure) {
  secure_origin_ = secure;
  ConfigureSecurityWarnings();
}

void UiSceneManager::ConfigureSecurityWarnings() {
  bool enabled = web_vr_mode_ && !secure_origin_;
  permanent_security_warning_->visible = enabled;
  transient_security_warning_->visible = enabled;
  if (enabled) {
    security_warning_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kWarningTimeoutSeconds), this,
        &UiSceneManager::OnSecurityWarningTimer);
  } else {
    security_warning_timer_.Stop();
  }
}

void UiSceneManager::OnSecurityWarningTimer() {
  transient_security_warning_->visible = false;
}

}  // namespace vr_shell
