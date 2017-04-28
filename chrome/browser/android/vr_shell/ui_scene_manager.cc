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
static constexpr float kPermanentWarningHeight = 0.070f;
static constexpr float kPermanentWarningWidth = 0.224f;
static constexpr float kTransientWarningHeight = 0.160;
static constexpr float kTransientWarningWidth = 0.512;

static constexpr float kContentWidth = 2.4;
static constexpr float kContentHeight = 1.6;
static constexpr float kContentDistance = 2.5;
static constexpr float kContentVerticalOffset = -0.26;
static constexpr float kBackplaneSize = 1000.0;
static constexpr float kBackgroundDistanceMultiplier = 1.414;

static constexpr float kSceneSize = 25.0;
static constexpr float kSceneHeight = 4.0;
static constexpr int kFloorGridlineCount = 40;
static constexpr vr::Colorf kBackgroundHorizonColor = {0.57, 0.57, 0.57, 1.0};
static constexpr vr::Colorf kBackgroundCenterColor = {0.48, 0.48, 0.48, 1.0};

// Tiny distance to offset textures that should appear in the same plane.
static constexpr float kTextureOffset = 0.01;

}  // namespace

UiSceneManager::UiSceneManager(UiScene* scene)
    : scene_(scene), weak_ptr_factory_(this) {
  std::unique_ptr<UiElement> element;

  CreateBackground();
  CreateContentQuad();
  CreateSecurityWarnings();
}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::CreateSecurityWarnings() {
  std::unique_ptr<UiElement> element;

  // TODO(mthiesse): Programatically compute the proper texture size for these
  // textured UI elements.
  element = base::MakeUnique<PermanentSecurityWarning>(512);
  element->id = AllocateId();
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

  element = base::MakeUnique<TransientSecurityWarning>(1024);
  element->id = AllocateId();
  element->fill = vr_shell::Fill::NONE;
  element->size = {kTransientWarningWidth, kTransientWarningHeight, 1};
  element->scale = {kWarningDistance, kWarningDistance, 1};
  element->translation = {0, 0, -kWarningDistance};
  element->visible = false;
  element->hit_testable = false;
  element->lock_to_fov = true;
  transient_security_warning_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateContentQuad() {
  std::unique_ptr<UiElement> element;

  element = base::MakeUnique<UiElement>();
  element->id = AllocateId();
  element->fill = vr_shell::Fill::CONTENT;
  element->size = {kContentWidth, kContentHeight, 1};
  element->translation = {0, kContentVerticalOffset, -kContentDistance};
  element->visible = false;
  main_content_ = element.get();
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  element = base::MakeUnique<UiElement>();
  element->id = AllocateId();
  element->fill = vr_shell::Fill::NONE;
  element->size = {kBackplaneSize, kBackplaneSize, 1.0};
  element->translation = {0.0, 0.0, -kTextureOffset};
  element->parent_id = main_content_->id;
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Limit reticle distance to a sphere based on content distance.
  scene_->SetBackgroundDistance(main_content_->translation.z() *
                                -kBackgroundDistanceMultiplier);
}

void UiSceneManager::CreateBackground() {
  std::unique_ptr<UiElement> element;

  // Floor.
  element = base::MakeUnique<UiElement>();
  element->id = AllocateId();
  element->size = {kSceneSize, kSceneSize, 1.0};
  element->translation = {0.0, -kSceneHeight / 2, 0.0};
  element->rotation = {1.0, 0.0, 0.0, -M_PI / 2.0};
  element->fill = vr_shell::Fill::OPAQUE_GRADIENT;
  element->edge_color = kBackgroundHorizonColor;
  element->center_color = kBackgroundCenterColor;
  element->draw_phase = 0;
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Ceiling.
  element = base::MakeUnique<UiElement>();
  element->id = AllocateId();
  element->fill = vr_shell::Fill::OPAQUE_GRADIENT;
  element->size = {kSceneSize, kSceneSize, 1.0};
  element->translation = {0.0, kSceneHeight / 2, 0.0};
  element->rotation = {1.0, 0.0, 0.0, M_PI / 2};
  element->fill = vr_shell::Fill::OPAQUE_GRADIENT;
  element->edge_color = kBackgroundHorizonColor;
  element->center_color = kBackgroundCenterColor;
  element->draw_phase = 0;
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Floor grid.
  element = base::MakeUnique<UiElement>();
  element->id = AllocateId();
  element->fill = vr_shell::Fill::GRID_GRADIENT;
  element->size = {kSceneSize, kSceneSize, 1.0};
  element->translation = {0.0, -kSceneHeight / 2 + kTextureOffset, 0.0};
  element->rotation = {1.0, 0.0, 0.0, -M_PI / 2};
  element->fill = vr_shell::Fill::GRID_GRADIENT;
  element->center_color = kBackgroundHorizonColor;
  vr::Colorf edge_color = kBackgroundHorizonColor;
  edge_color.a = 0.0;
  element->edge_color = edge_color;
  element->gridline_count = kFloorGridlineCount;
  element->draw_phase = 0;
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  scene_->SetBackgroundColor(kBackgroundHorizonColor);
}

base::WeakPtr<UiSceneManager> UiSceneManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UiSceneManager::SetWebVRMode(bool web_vr) {
  web_vr_mode_ = web_vr;

  // Make all VR scene UI elements visible if not in WebVR.
  for (UiElement* element : browser_ui_elements_) {
    element->visible = !web_vr_mode_;
  }

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

int UiSceneManager::AllocateId() {
  return next_available_id_++;
}

}  // namespace vr_shell
