// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "chrome/browser/android/vr_shell/ui_elements/permanent_security_warning.h"
#include "chrome/browser/android/vr_shell/ui_elements/transient_security_warning.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "chrome/browser/android/vr_shell/ui_elements/url_bar.h"
#include "chrome/browser/android/vr_shell/ui_scene.h"
#include "chrome/browser/android/vr_shell/vr_browser_interface.h"
#include "chrome/browser/android/vr_shell/vr_shell.h"

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

// Placeholders to demonstrate UI changes when in CCT.
static constexpr vr::Colorf kCctBackgroundHorizonColor = {0.2, 0.6, 0.2, 1.0};
static constexpr vr::Colorf kCctBackgroundCenterColor = {0.13, 0.52, 0.13, 1.0};

// Tiny distance to offset textures that should appear in the same plane.
static constexpr float kTextureOffset = 0.01;

}  // namespace

UiSceneManager::UiSceneManager(VrBrowserInterface* browser,
                               UiScene* scene,
                               bool in_cct)
    : browser_(browser),
      scene_(scene),
      in_cct_(in_cct),
      weak_ptr_factory_(this) {
  CreateBackground();
  CreateContentQuad();
  CreateSecurityWarnings();
  CreateUrlBar();
}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::CreateSecurityWarnings() {
  std::unique_ptr<UiElement> element;

  // TODO(mthiesse): Programatically compute the proper texture size for these
  // textured UI elements.
  element = base::MakeUnique<PermanentSecurityWarning>(512);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kPermanentWarningWidth, kPermanentWarningHeight, 1});
  element->set_scale({kWarningDistance, kWarningDistance, 1});
  element->set_translation(
      gfx::Vector3dF(0, kWarningDistance * sin(kWarningAngleRadians),
                     -kWarningDistance * cos(kWarningAngleRadians)));
  element->set_rotation({1.0f, 0, 0, kWarningAngleRadians});
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  permanent_security_warning_ = element.get();
  scene_->AddUiElement(std::move(element));

  element = base::MakeUnique<TransientSecurityWarning>(1024);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kTransientWarningWidth, kTransientWarningHeight, 1});
  element->set_scale({kWarningDistance, kWarningDistance, 1});
  element->set_translation({0, 0, -kWarningDistance});
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  transient_security_warning_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateContentQuad() {
  std::unique_ptr<UiElement> element;

  element = base::MakeUnique<UiElement>();
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::CONTENT);
  element->set_size({kContentWidth, kContentHeight, 1});
  element->set_translation({0, kContentVerticalOffset, -kContentDistance});
  element->set_visible(false);
  main_content_ = element.get();
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  element = base::MakeUnique<UiElement>();
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kBackplaneSize, kBackplaneSize, 1.0});
  element->set_translation({0.0, 0.0, -kTextureOffset});
  element->set_parent_id(main_content_->id());
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Limit reticle distance to a sphere based on content distance.
  scene_->SetBackgroundDistance(main_content_->translation().z() *
                                -kBackgroundDistanceMultiplier);
}

void UiSceneManager::CreateBackground() {
  std::unique_ptr<UiElement> element;
  vr::Colorf horizon =
      in_cct_ ? kCctBackgroundHorizonColor : kBackgroundHorizonColor;
  vr::Colorf center =
      in_cct_ ? kCctBackgroundCenterColor : kBackgroundCenterColor;

  // Floor.
  element = base::MakeUnique<UiElement>();
  element->set_id(AllocateId());
  element->set_size({kSceneSize, kSceneSize, 1.0});
  element->set_translation({0.0, -kSceneHeight / 2, 0.0});
  element->set_rotation({1.0, 0.0, 0.0, -M_PI / 2.0});
  element->set_fill(vr_shell::Fill::OPAQUE_GRADIENT);
  element->set_edge_color(horizon);
  element->set_center_color(center);
  element->set_draw_phase(0);
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Ceiling.
  element = base::MakeUnique<UiElement>();
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::OPAQUE_GRADIENT);
  element->set_size({kSceneSize, kSceneSize, 1.0});
  element->set_translation({0.0, kSceneHeight / 2, 0.0});
  element->set_rotation({1.0, 0.0, 0.0, M_PI / 2});
  element->set_fill(vr_shell::Fill::OPAQUE_GRADIENT);
  element->set_edge_color(horizon);
  element->set_center_color(center);
  element->set_draw_phase(0);
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Floor grid.
  element = base::MakeUnique<UiElement>();
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::GRID_GRADIENT);
  element->set_size({kSceneSize, kSceneSize, 1.0});
  element->set_translation({0.0, -kSceneHeight / 2 + kTextureOffset, 0.0});
  element->set_rotation({1.0, 0.0, 0.0, -M_PI / 2});
  element->set_fill(vr_shell::Fill::GRID_GRADIENT);
  element->set_center_color(horizon);
  vr::Colorf edge_color = horizon;
  edge_color.a = 0.0;
  element->set_edge_color(edge_color);
  element->set_gridline_count(kFloorGridlineCount);
  element->set_draw_phase(0);
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  scene_->SetBackgroundColor(horizon);
}

void UiSceneManager::CreateUrlBar() {
  // TODO(cjgrant): Incorporate final size and position.
  // TODO(cjgrant): Add the loading progress indicator element.
  auto element = base::MakeUnique<UrlBar>(512);
  element->set_id(AllocateId());
  element->set_translation({0, -0.9, -1.8});
  element->set_size({0.9, 0, 1});
  url_bar_ = element.get();
  browser_ui_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));
}

base::WeakPtr<UiSceneManager> UiSceneManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UiSceneManager::SetWebVRMode(bool web_vr) {
  web_vr_mode_ = web_vr;

  // Make all VR scene UI elements visible if not in WebVR.
  for (UiElement* element : browser_ui_elements_) {
    element->set_visible(!web_vr_mode_);
  }

  ConfigureSecurityWarnings();
}

void UiSceneManager::SetWebVRSecureOrigin(bool secure) {
  secure_origin_ = secure;
  ConfigureSecurityWarnings();
}

void UiSceneManager::OnAppButtonClicked() {
  // Pressing the app button currenly pauses content rendering. Note: its still
  // unclear what we want to do here and this will most likely change.
  content_rendering_enabled_ = !content_rendering_enabled_;
  scene_->SetWebVrRenderingEnabled(content_rendering_enabled_);
  browser_->OnContentPaused(!content_rendering_enabled_);
}

void UiSceneManager::ConfigureSecurityWarnings() {
  bool enabled = web_vr_mode_ && !secure_origin_;
  permanent_security_warning_->set_visible(enabled);
  transient_security_warning_->set_visible(enabled);
  if (enabled) {
    security_warning_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kWarningTimeoutSeconds), this,
        &UiSceneManager::OnSecurityWarningTimer);
  } else {
    security_warning_timer_.Stop();
  }
}

void UiSceneManager::OnSecurityWarningTimer() {
  transient_security_warning_->set_visible(false);
}

void UiSceneManager::OnUrlChange(const GURL& gurl) {
  url_bar_->SetURL(gurl);
}

int UiSceneManager::AllocateId() {
  return next_available_id_++;
}

}  // namespace vr_shell
