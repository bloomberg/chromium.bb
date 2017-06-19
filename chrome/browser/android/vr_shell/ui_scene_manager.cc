// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_scene_manager.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/close_button_texture.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "chrome/browser/android/vr_shell/ui_browser_interface.h"
#include "chrome/browser/android/vr_shell/ui_elements/audio_capture_indicator.h"
#include "chrome/browser/android/vr_shell/ui_elements/button.h"
#include "chrome/browser/android/vr_shell/ui_elements/exit_prompt.h"
#include "chrome/browser/android/vr_shell/ui_elements/exit_prompt_backplane.h"
#include "chrome/browser/android/vr_shell/ui_elements/exit_warning.h"
#include "chrome/browser/android/vr_shell/ui_elements/loading_indicator.h"
#include "chrome/browser/android/vr_shell/ui_elements/location_access_indicator.h"
#include "chrome/browser/android/vr_shell/ui_elements/permanent_security_warning.h"
#include "chrome/browser/android/vr_shell/ui_elements/screen_capture_indicator.h"
#include "chrome/browser/android/vr_shell/ui_elements/screen_dimmer.h"
#include "chrome/browser/android/vr_shell/ui_elements/transient_security_warning.h"
#include "chrome/browser/android/vr_shell/ui_elements/transient_url_bar.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element_debug_id.h"
#include "chrome/browser/android/vr_shell/ui_elements/url_bar.h"
#include "chrome/browser/android/vr_shell/ui_elements/video_capture_indicator.h"
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

static constexpr float kExitWarningDistance = 0.6;
static constexpr float kExitWarningHeight = 0.160;
static constexpr float kExitWarningWidth = 0.512;

static constexpr float kContentDistance = 2.5;
static constexpr float kContentWidth = 0.96 * kContentDistance;
static constexpr float kContentHeight = 0.64 * kContentDistance;
static constexpr float kContentVerticalOffset = -0.1 * kContentDistance;
static constexpr float kBackplaneSize = 1000.0;
static constexpr float kBackgroundDistanceMultiplier = 1.414;

static constexpr float kFullscreenDistance = 3;
static constexpr float kFullscreenHeight = 0.64 * kFullscreenDistance;
static constexpr float kFullscreenWidth = 1.138 * kFullscreenDistance;
static constexpr float kFullscreenVerticalOffset = -0.1 * kFullscreenDistance;

static constexpr float kExitPromptWidth = 0.672 * kContentDistance;
static constexpr float kExitPromptHeight = 0.2 * kContentDistance;
static constexpr float kExitPromptVerticalOffset = -0.09 * kContentDistance;
static constexpr float kExitPromptBackplaneSize = 1000.0;

// Distance-independent milimeter size of the URL bar.
static constexpr float kUrlBarWidthDMM = 0.672;
static constexpr float kUrlBarHeightDMM = 0.088;
static constexpr float kUrlBarDistance = 2.4;
static constexpr float kUrlBarWidth = kUrlBarWidthDMM * kUrlBarDistance;
static constexpr float kUrlBarHeight = kUrlBarHeightDMM * kUrlBarDistance;
static constexpr float kUrlBarVerticalOffset = -0.516 * kUrlBarDistance;
static constexpr float kUrlBarRotationRad = -0.175;

static constexpr float kIndicatorContentDistance = 0.1;
static constexpr float kAudioCaptureIndicatorWidth = 0.5;
static constexpr float kVideoCaptureIndicatorWidth = 0.5;
static constexpr float kScreenCaptureIndicatorWidth = 0.4;
static constexpr float kLocationIndicatorWidth = 0.088;

static constexpr float kCaptureIndicatorsVerticalOffset = 0.1;
static constexpr float kAudioCaptureHorizontalOffset = -0.6;
static constexpr float kVideoCaptureHorizontalOffset = 0;
static constexpr float kScreenCaptureHorizontalOffset = 0.6;
static constexpr float kLocationAccessHorizontalOffset = 1.0;

static constexpr float kTransientUrlBarDistance = 1.4;
static constexpr float kTransientUrlBarWidth =
    kUrlBarWidthDMM * kTransientUrlBarDistance;
static constexpr float kTransientUrlBarHeight =
    kUrlBarHeightDMM * kTransientUrlBarDistance;
static constexpr float kTransientUrlBarVerticalOffset =
    -0.2 * kTransientUrlBarDistance;
static constexpr int kTransientUrlBarTimeoutSeconds = 6;

static constexpr float kCloseButtonDistance = 2.4;
static constexpr float kCloseButtonHeight =
    kUrlBarHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonWidth =
    kUrlBarHeightDMM * kCloseButtonDistance;
static constexpr float kCloseButtonFullscreenDistance = 2.9;
static constexpr float kCloseButtonFullscreenHeight =
    kUrlBarHeightDMM * kCloseButtonFullscreenDistance;
static constexpr float kCloseButtonFullscreenWidth =
    kUrlBarHeightDMM * kCloseButtonFullscreenDistance;

static constexpr float kLoadingIndicatorWidth = 0.24 * kUrlBarDistance;
static constexpr float kLoadingIndicatorHeight = 0.008 * kUrlBarDistance;
static constexpr float kLoadingIndicatorVerticalOffset =
    (-kUrlBarVerticalOffset + kContentVerticalOffset - kContentHeight / 2 -
     kUrlBarHeight / 2) /
    2;
static constexpr float kLoadingIndicatorDepthOffset =
    (kUrlBarDistance - kContentDistance) / 2;

static constexpr float kSceneSize = 25.0;
static constexpr float kSceneHeight = 4.0;
static constexpr int kFloorGridlineCount = 40;

// Tiny distance to offset textures that should appear in the same plane.
static constexpr float kTextureOffset = 0.01;

}  // namespace

UiSceneManager::UiSceneManager(UiBrowserInterface* browser,
                               UiScene* scene,
                               bool in_cct,
                               bool in_web_vr,
                               bool web_vr_autopresented)
    : browser_(browser),
      scene_(scene),
      in_cct_(in_cct),
      web_vr_mode_(in_web_vr),
      web_vr_autopresented_(web_vr_autopresented),
      weak_ptr_factory_(this) {
  CreateBackground();
  CreateContentQuad();
  CreateSecurityWarnings();
  CreateSystemIndicators();
  CreateUrlBar();
  CreateTransientUrlBar();
  CreateCloseButton();
  CreateScreenDimmer();
  CreateExitPrompt();

  ConfigureScene();
  ConfigureSecurityWarnings();
  ConfigureTransientUrlBar();
}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::CreateScreenDimmer() {
  std::unique_ptr<UiElement> element;
  element = base::MakeUnique<ScreenDimmer>();
  element->set_debug_id(kScreenDimmer);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_is_overlay(true);
  screen_dimmer_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateSecurityWarnings() {
  std::unique_ptr<UiElement> element;

  // TODO(mthiesse): Programatically compute the proper texture size for these
  // textured UI elements.
  element = base::MakeUnique<PermanentSecurityWarning>(512);
  element->set_debug_id(kWebVrPermanentHttpSecurityWarning);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kPermanentWarningWidth, kPermanentWarningHeight, 1});
  element->set_scale({kWarningDistance, kWarningDistance, 1});
  element->set_translation(
      gfx::Vector3dF(0, kWarningDistance * sin(kWarningAngleRadians),
                     -kWarningDistance * cos(kWarningAngleRadians)));
  element->set_rotation(
      gfx::Quaternion(gfx::Vector3dF(1, 0, 0), kWarningAngleRadians));
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  permanent_security_warning_ = element.get();
  scene_->AddUiElement(std::move(element));

  element = base::MakeUnique<TransientSecurityWarning>(1024);
  element->set_debug_id(kWebVrTransientHttpSecurityWarning);
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

  element = base::MakeUnique<ExitWarning>(1024);
  element->set_debug_id(kExitWarning);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kExitWarningWidth, kExitWarningHeight, 1});
  element->set_scale({kExitWarningDistance, kExitWarningDistance, 1});
  element->set_translation({0, 0, -kExitWarningDistance});
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  exit_warning_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateSystemIndicators() {
  std::unique_ptr<UiElement> element;

  element = base::MakeUnique<AudioCaptureIndicator>(512);
  element->set_debug_id(kAudioCaptureIndicator);
  element->set_id(AllocateId());
  element->set_translation({kAudioCaptureHorizontalOffset,
                            kCaptureIndicatorsVerticalOffset,
                            kIndicatorContentDistance});
  element->set_parent_id(main_content_->id());
  element->set_y_anchoring(YAnchoring::YTOP);
  element->set_size({kAudioCaptureIndicatorWidth, 0, 1});
  element->set_visible(false);
  audio_capture_indicator_ = element.get();
  scene_->AddUiElement(std::move(element));

  element = base::MakeUnique<LocationAccessIndicator>(250);
  element->set_debug_id(kLocationAccessIndicator);
  element->set_id(AllocateId());
  element->set_translation({kLocationAccessHorizontalOffset,
                            kCaptureIndicatorsVerticalOffset,
                            kIndicatorContentDistance});
  element->set_size({kLocationIndicatorWidth, 0, 1});
  element->set_parent_id(main_content_->id());
  element->set_y_anchoring(YAnchoring::YTOP);
  element->set_visible(false);
  location_access_indicator_ = element.get();
  scene_->AddUiElement(std::move(element));

  element = base::MakeUnique<VideoCaptureIndicator>(512);
  element->set_debug_id(kVideoCaptureIndicator);
  element->set_id(AllocateId());
  element->set_translation({kVideoCaptureHorizontalOffset,
                            kCaptureIndicatorsVerticalOffset,
                            kIndicatorContentDistance});
  element->set_parent_id(main_content_->id());
  element->set_y_anchoring(YAnchoring::YTOP);
  element->set_size({kVideoCaptureIndicatorWidth, 0, 1});
  element->set_visible(false);
  video_capture_indicator_ = element.get();
  scene_->AddUiElement(std::move(element));

  element = base::MakeUnique<ScreenCaptureIndicator>(512);
  element->set_debug_id(kScreenCaptureIndicator);
  element->set_id(AllocateId());
  element->set_translation({kScreenCaptureHorizontalOffset,
                            kCaptureIndicatorsVerticalOffset,
                            kIndicatorContentDistance});
  element->set_parent_id(main_content_->id());
  element->set_y_anchoring(YAnchoring::YTOP);
  element->set_size({kScreenCaptureIndicatorWidth, 0, 1});
  element->set_visible(false);
  screen_capture_indicator_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateContentQuad() {
  std::unique_ptr<UiElement> element;

  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kContentQuad);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::CONTENT);
  element->set_size({kContentWidth, kContentHeight, 1});
  element->set_translation({0, kContentVerticalOffset, -kContentDistance});
  element->set_visible(false);
  main_content_ = element.get();
  content_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kBackplane);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kBackplaneSize, kBackplaneSize, 1.0});
  element->set_translation({0.0, 0.0, -kTextureOffset});
  element->set_parent_id(main_content_->id());
  content_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Limit reticle distance to a sphere based on content distance.
  scene_->SetBackgroundDistance(main_content_->translation().z() *
                                -kBackgroundDistanceMultiplier);
}

void UiSceneManager::CreateBackground() {
  std::unique_ptr<UiElement> element;

  // Floor.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kFloor);
  element->set_id(AllocateId());
  element->set_size({kSceneSize, kSceneSize, 1.0});
  element->set_translation({0.0, -kSceneHeight / 2, 0.0});
  element->set_rotation(gfx::Quaternion(gfx::Vector3dF(1, 0, 0), -M_PI / 2));
  element->set_fill(vr_shell::Fill::GRID_GRADIENT);
  element->set_draw_phase(0);
  element->set_gridline_count(kFloorGridlineCount);
  floor_ = element.get();
  background_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Ceiling.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kCeiling);
  element->set_id(AllocateId());
  element->set_size({kSceneSize, kSceneSize, 1.0});
  element->set_translation({0.0, kSceneHeight / 2, 0.0});
  element->set_rotation(gfx::Quaternion(gfx::Vector3dF(1, 0, 0), M_PI / 2));
  element->set_fill(vr_shell::Fill::OPAQUE_GRADIENT);
  element->set_draw_phase(0);
  ceiling_ = element.get();
  background_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  UpdateBackgroundColor();
}

void UiSceneManager::CreateUrlBar() {
  // TODO(cjgrant): Incorporate final size and position.
  auto url_bar = base::MakeUnique<UrlBar>(
      512,
      base::Bind(&UiSceneManager::OnBackButtonClicked, base::Unretained(this)),
      base::Bind(&UiSceneManager::OnSecurityIconClicked,
                 base::Unretained(this)),
      base::Bind(&UiSceneManager::OnUnsupportedMode, base::Unretained(this)));
  url_bar->set_debug_id(kUrlBar);
  url_bar->set_id(AllocateId());
  url_bar->set_translation({0, kUrlBarVerticalOffset, -kUrlBarDistance});
  url_bar->set_rotation(
      gfx::Quaternion(gfx::Vector3dF(1, 0, 0), kUrlBarRotationRad));
  url_bar->set_size({kUrlBarWidth, kUrlBarHeight, 1});
  url_bar_ = url_bar.get();
  control_elements_.push_back(url_bar.get());
  scene_->AddUiElement(std::move(url_bar));

  auto indicator = base::MakeUnique<LoadingIndicator>(256);
  indicator->set_debug_id(kLoadingIndicator);
  indicator->set_id(AllocateId());
  indicator->set_translation(
      {0, kLoadingIndicatorVerticalOffset, kLoadingIndicatorDepthOffset});
  indicator->set_size({kLoadingIndicatorWidth, kLoadingIndicatorHeight, 1});
  indicator->set_parent_id(url_bar_->id());
  indicator->set_y_anchoring(YAnchoring::YTOP);
  loading_indicator_ = indicator.get();
  control_elements_.push_back(indicator.get());
  scene_->AddUiElement(std::move(indicator));
}

void UiSceneManager::CreateTransientUrlBar() {
  auto url_bar = base::MakeUnique<TransientUrlBar>(
      512,
      base::Bind(&UiSceneManager::OnUnsupportedMode, base::Unretained(this)));
  url_bar->set_debug_id(kTransientUrlBar);
  url_bar->set_id(AllocateId());
  url_bar->set_lock_to_fov(true);
  url_bar->set_visible(false);
  url_bar->set_hit_testable(false);
  url_bar->set_translation(
      {0, kTransientUrlBarVerticalOffset, -kTransientUrlBarDistance});
  url_bar->set_rotation(
      gfx::Quaternion(gfx::Vector3dF(1, 0, 0), kUrlBarRotationRad));
  url_bar->set_size({kTransientUrlBarWidth, kTransientUrlBarHeight, 1});
  transient_url_bar_ = url_bar.get();
  scene_->AddUiElement(std::move(url_bar));
}

void UiSceneManager::CreateCloseButton() {
  std::unique_ptr<Button> element = base::MakeUnique<Button>(
      base::Bind(&UiSceneManager::OnCloseButtonClicked, base::Unretained(this)),
      base::MakeUnique<CloseButtonTexture>());
  element->set_debug_id(kCloseButton);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_translation(
      gfx::Vector3dF(0, kContentVerticalOffset - (kContentHeight / 2) - 0.3,
                     -kCloseButtonDistance));
  element->set_size(gfx::Vector3dF(kCloseButtonWidth, kCloseButtonHeight, 1));
  close_button_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateExitPrompt() {
  std::unique_ptr<UiElement> element = base::MakeUnique<ExitPrompt>(
      512,
      base::Bind(&UiSceneManager::OnExitPromptPrimaryButtonClicked,
                 base::Unretained(this)),
      base::Bind(&UiSceneManager::OnExitPromptSecondaryButtonClicked,
                 base::Unretained(this)));
  element->set_debug_id(kExitPrompt);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kExitPromptWidth, kExitPromptHeight, 1});
  element->set_translation({0.0, kExitPromptVerticalOffset, kTextureOffset});
  element->set_parent_id(main_content_->id());
  element->set_visible(false);
  exit_prompt_ = element.get();
  scene_->AddUiElement(std::move(element));

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  element = base::MakeUnique<ExitPromptBackplane>(base::Bind(
      &UiSceneManager::OnExitPromptBackplaneClicked, base::Unretained(this)));
  element->set_debug_id(kExitPromptBackplane);
  element->set_id(AllocateId());
  element->set_fill(vr_shell::Fill::NONE);
  element->set_size({kExitPromptBackplaneSize, kExitPromptBackplaneSize, 1.0});
  element->set_translation({0.0, 0.0, -kTextureOffset});
  element->set_parent_id(exit_prompt_->id());
  exit_prompt_backplane_ = element.get();
  content_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));
}

base::WeakPtr<UiSceneManager> UiSceneManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UiSceneManager::SetWebVrMode(bool web_vr, bool auto_presented) {
  if (web_vr_mode_ == web_vr)
    return;
  web_vr_mode_ = web_vr;
  web_vr_autopresented_ = auto_presented;
  ConfigureScene();
  ConfigureSecurityWarnings();
  ConfigureTransientUrlBar();
  ConfigureIndicators();
}

void UiSceneManager::ConfigureScene() {
  exit_warning_->SetEnabled(scene_->is_exiting());
  screen_dimmer_->SetEnabled(scene_->is_exiting());

  // Controls (URL bar, loading progress, etc).
  bool controls_visible = !web_vr_mode_ && !fullscreen_;
  for (UiElement* element : control_elements_) {
    element->SetEnabled(controls_visible && !scene_->is_prompting_to_exit());
  }

  // Close button is a special control element that needs to be hidden when in
  // WebVR, but it needs to be visible when in cct or fullscreen.
  close_button_->SetEnabled(!web_vr_mode_ && (fullscreen_ || in_cct_));

  // Content elements.
  for (UiElement* element : content_elements_) {
    element->SetEnabled(!web_vr_mode_ && !scene_->is_prompting_to_exit());
  }

  // Background elements.
  for (UiElement* element : background_elements_) {
    element->SetEnabled(!web_vr_mode_);
  }

  // Exit prompt.
  bool showExitPrompt = !web_vr_mode_ && scene_->is_prompting_to_exit();
  exit_prompt_->SetEnabled(showExitPrompt);
  exit_prompt_backplane_->SetEnabled(showExitPrompt);

  // Update content quad parameters depending on fullscreen.
  // TODO(http://crbug.com/642937): Animate fullscreen transitions.
  if (fullscreen_) {
    main_content_->set_translation(
        {0, kFullscreenVerticalOffset, -kFullscreenDistance});
    main_content_->set_size({kFullscreenWidth, kFullscreenHeight, 1});

    close_button_->set_translation(gfx::Vector3dF(
        0, kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35,
        -kCloseButtonFullscreenDistance));
    close_button_->set_size(gfx::Vector3dF(kCloseButtonFullscreenWidth,
                                           kCloseButtonFullscreenHeight, 1));
  } else {
    // Note that main_content_ is already visible in this case.
    main_content_->set_translation(
        {0, kContentVerticalOffset, -kContentDistance});
    main_content_->set_size({kContentWidth, kContentHeight, 1});

    close_button_->set_translation(
        gfx::Vector3dF(0, kContentVerticalOffset - (kContentHeight / 2) - 0.3,
                       -kCloseButtonDistance));
    close_button_->set_size(
        gfx::Vector3dF(kCloseButtonWidth, kCloseButtonHeight, 1));
  }

  scene_->SetMode(mode());
  scene_->SetBackgroundDistance(main_content_->translation().z() *
                                -kBackgroundDistanceMultiplier);
  UpdateBackgroundColor();
}

void UiSceneManager::UpdateBackgroundColor() {
  // TODO(vollick): it would be nice if ceiling, floor and the grid were
  // UiElement subclasses and could respond to the OnSetMode signal.
  ceiling_->set_center_color(color_scheme().ceiling);
  ceiling_->set_edge_color(color_scheme().world_background);
  floor_->set_center_color(color_scheme().floor);
  floor_->set_edge_color(color_scheme().world_background);
  floor_->set_grid_color(color_scheme().floor_grid);
}

void UiSceneManager::SetAudioCapturingIndicator(bool enabled) {
  audio_capturing_ = enabled;
  ConfigureIndicators();
}

void UiSceneManager::SetVideoCapturingIndicator(bool enabled) {
  video_capturing_ = enabled;
  ConfigureIndicators();
}

void UiSceneManager::SetScreenCapturingIndicator(bool enabled) {
  screen_capturing_ = enabled;
  ConfigureIndicators();
}

void UiSceneManager::SetLocationAccessIndicator(bool enabled) {
  location_access_ = enabled;
  ConfigureIndicators();
}

void UiSceneManager::SetWebVrSecureOrigin(bool secure) {
  secure_origin_ = secure;
  ConfigureSecurityWarnings();
}

void UiSceneManager::SetIncognito(bool incognito) {
  if (incognito == incognito_)
    return;
  incognito_ = incognito;
  ConfigureScene();
}

void UiSceneManager::OnAppButtonClicked() {
  // App button click exits the WebVR presentation and fullscreen.
  browser_->ExitPresent();
  browser_->ExitFullscreen();
}

void UiSceneManager::OnAppButtonGesturePerformed(
    UiInterface::Direction direction) {}

void UiSceneManager::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return;
  fullscreen_ = fullscreen;
  ConfigureScene();
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

void UiSceneManager::ConfigureIndicators() {
  audio_capture_indicator_->set_visible(!web_vr_mode_ && audio_capturing_);
  video_capture_indicator_->set_visible(!web_vr_mode_ && video_capturing_);
  screen_capture_indicator_->set_visible(!web_vr_mode_ && screen_capturing_);
  location_access_indicator_->set_visible(!web_vr_mode_ && location_access_);
}

void UiSceneManager::OnSecurityWarningTimer() {
  transient_security_warning_->set_visible(false);
}

void UiSceneManager::ConfigureTransientUrlBar() {
  bool enabled = web_vr_mode_ && web_vr_autopresented_;
  transient_url_bar_->set_visible(enabled);
  if (enabled) {
    transient_url_bar_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kTransientUrlBarTimeoutSeconds),
        this, &UiSceneManager::OnTransientUrlBarTimer);
  } else {
    transient_url_bar_timer_.Stop();
  }
}

void UiSceneManager::OnTransientUrlBarTimer() {
  transient_url_bar_->set_visible(false);
}

void UiSceneManager::OnBackButtonClicked() {
  browser_->NavigateBack();
}

void UiSceneManager::OnSecurityIconClickedForTesting() {
  OnSecurityIconClicked();
}

void UiSceneManager::OnExitPromptPrimaryButtonClickedForTesting() {
  OnExitPromptPrimaryButtonClicked();
}

void UiSceneManager::OnSecurityIconClicked() {
  if (scene_->is_prompting_to_exit())
    return;
  scene_->set_is_prompting_to_exit(true);
  ConfigureScene();
}

void UiSceneManager::OnExitPromptBackplaneClicked() {
  CloseExitPrompt();
}

void UiSceneManager::CloseExitPrompt() {
  if (!scene_->is_prompting_to_exit())
    return;
  scene_->set_is_prompting_to_exit(false);
  ConfigureScene();
}

void UiSceneManager::OnExitPromptPrimaryButtonClicked() {
  CloseExitPrompt();
}

void UiSceneManager::OnExitPromptSecondaryButtonClicked() {
  OnUnsupportedMode(UiUnsupportedMode::kUnhandledPageInfo);
}

void UiSceneManager::SetURL(const GURL& gurl) {
  url_bar_->SetURL(gurl);
  transient_url_bar_->SetURL(gurl);
}

void UiSceneManager::SetSecurityLevel(security_state::SecurityLevel level) {
  url_bar_->SetSecurityLevel(level);
  transient_url_bar_->SetSecurityLevel(level);
}

void UiSceneManager::SetLoading(bool loading) {
  loading_indicator_->SetLoading(loading);
}

void UiSceneManager::SetLoadProgress(float progress) {
  loading_indicator_->SetLoadProgress(progress);
}

void UiSceneManager::SetIsExiting() {
  if (scene_->is_exiting())
    return;
  scene_->set_is_exiting();
  ConfigureScene();
}

void UiSceneManager::SetHistoryButtonsEnabled(bool can_go_back,
                                              bool can_go_forward) {
  url_bar_->SetHistoryButtonsEnabled(can_go_back);
}

void UiSceneManager::OnCloseButtonClicked() {
  if (fullscreen_) {
    browser_->ExitFullscreen();
  }
  if (in_cct_) {
    browser_->ExitCct();
  }
}

void UiSceneManager::OnUnsupportedMode(UiUnsupportedMode mode) {
  browser_->OnUnsupportedMode(mode);
}

int UiSceneManager::AllocateId() {
  return next_available_id_++;
}

ColorScheme::Mode UiSceneManager::mode() const {
  if (incognito_)
    return ColorScheme::kModeIncognito;
  if (fullscreen_)
    return ColorScheme::kModeFullscreen;
  return ColorScheme::kModeNormal;
}

const ColorScheme& UiSceneManager::color_scheme() const {
  return ColorScheme::GetColorScheme(mode());
}

}  // namespace vr_shell
