// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/close_button_texture.h"
#include "chrome/browser/vr/elements/exclusive_screen_toast.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/exit_prompt_backplane.h"
#include "chrome/browser/vr/elements/loading_indicator.h"
#include "chrome/browser/vr/elements/screen_dimmer.h"
#include "chrome/browser/vr/elements/splash_screen_icon.h"
#include "chrome/browser/vr/elements/system_indicator.h"
#include "chrome/browser/vr/elements/transient_url_bar.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_debug_id.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/elements/url_bar.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/transform_util.h"

using cc::TargetProperty::BOUNDS;
using cc::TargetProperty::TRANSFORM;

namespace vr {

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
static constexpr float kContentCornerRadius = 0.005 * kContentWidth;
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

static constexpr float kIndicatorHeight = 0.08;
static constexpr float kIndicatorGap = 0.05;
static constexpr float kIndicatorVerticalOffset = 0.1;
static constexpr float kIndicatorDistanceOffset = 0.1;

static constexpr float kTransientUrlBarDistance = 1.4;
static constexpr float kTransientUrlBarWidth =
    kUrlBarWidthDMM * kTransientUrlBarDistance;
static constexpr float kTransientUrlBarHeight =
    kUrlBarHeightDMM * kTransientUrlBarDistance;
static constexpr float kTransientUrlBarVerticalOffset =
    -0.2 * kTransientUrlBarDistance;
static constexpr int kTransientUrlBarTimeoutSeconds = 6;

static constexpr float kWebVrToastDistance = 1.0;
static constexpr float kFullscreenToastDistance = kFullscreenDistance;
static constexpr float kToastWidthDMM = 0.512;
static constexpr float kToastHeightDMM = 0.064;
static constexpr float kToastOffsetDMM = 0.004;
// When changing the value here, make sure it doesn't collide with
// kWarningAngleRadians.
static constexpr float kWebVrAngleRadians = 9.88 * M_PI / 180.0;
static constexpr int kToastTimeoutSeconds = kTransientUrlBarTimeoutSeconds;

static constexpr float kSplashScreenDistance = 1;
static constexpr float kSplashScreenIconDMM = 0.12;
static constexpr float kSplashScreenIconHeight =
    kSplashScreenIconDMM * kSplashScreenDistance;
static constexpr float kSplashScreenIconWidth =
    kSplashScreenIconDMM * kSplashScreenDistance;
static constexpr float kSplashScreenIconVerticalOffset =
    -0.1 * kSplashScreenDistance;

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
                               bool web_vr_autopresentation_expected)
    : browser_(browser),
      scene_(scene),
      in_cct_(in_cct),
      web_vr_mode_(in_web_vr),
      started_for_autopresentation_(web_vr_autopresentation_expected),
      waiting_for_first_web_vr_frame_(web_vr_autopresentation_expected),
      weak_ptr_factory_(this) {
  CreateSplashScreen();
  CreateBackground();
  CreateContentQuad();
  CreateSecurityWarnings();
  CreateSystemIndicators();
  CreateUrlBar();
  CreateTransientUrlBar();
  CreateCloseButton();
  CreateScreenDimmer();
  CreateExitPrompt();
  CreateToasts();

  ConfigureScene();
}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::CreateScreenDimmer() {
  std::unique_ptr<UiElement> element;
  element = base::MakeUnique<ScreenDimmer>();
  element->set_debug_id(kScreenDimmer);
  element->set_id(AllocateId());
  element->set_fill(vr::Fill::NONE);
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
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kPermanentWarningWidth, kPermanentWarningHeight);
  element->SetTranslate(0, kWarningDistance * sin(kWarningAngleRadians),
                        -kWarningDistance * cos(kWarningAngleRadians));
  element->SetRotate(1, 0, 0, kWarningAngleRadians);
  element->SetScale(kWarningDistance, kWarningDistance, 1);
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  permanent_security_warning_ = element.get();
  scene_->AddUiElement(std::move(element));

  auto transient_warning = base::MakeUnique<TransientSecurityWarning>(
      1024, base::TimeDelta::FromSeconds(kWarningTimeoutSeconds));
  transient_security_warning_ = transient_warning.get();
  element = std::move(transient_warning);
  element->set_debug_id(kWebVrTransientHttpSecurityWarning);
  element->set_id(AllocateId());
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kTransientWarningWidth, kTransientWarningHeight);
  element->SetTranslate(0, 0, -kWarningDistance);
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  scene_->AddUiElement(std::move(element));

  element = base::MakeUnique<ExitWarning>(1024);
  element->set_debug_id(kExitWarning);
  element->set_id(AllocateId());
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kExitWarningWidth, kExitWarningHeight);
  element->SetTranslate(0, 0, -kExitWarningDistance);
  element->SetScale(kExitWarningDistance, kExitWarningDistance, 1);
  element->set_visible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  exit_warning_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateSystemIndicators() {
  std::unique_ptr<UiElement> element;

  struct Indicator {
    UiElement** element;
    UiElementDebugId debug_id;
    const gfx::VectorIcon& icon;
    int resource_string;
  };
  const std::vector<Indicator> indicators = {
      {&audio_capture_indicator_, kAudioCaptureIndicator,
       vector_icons::kMicrophoneIcon, IDS_AUDIO_CALL_NOTIFICATION_TEXT_2},
      {&video_capture_indicator_, kVideoCaptureIndicator,
       vector_icons::kVideocamIcon, IDS_VIDEO_CALL_NOTIFICATION_TEXT_2},
      {&screen_capture_indicator_, kScreenCaptureIndicator,
       vector_icons::kScreenShareIcon, IDS_SCREEN_CAPTURE_NOTIFICATION_TEXT_2},
      {&bluetooth_connected_indicator_, kBluetoothConnectedIndicator,
       vector_icons::kBluetoothConnectedIcon, 0},
      {&location_access_indicator_, kLocationAccessIndicator,
       vector_icons::kLocationOnIcon, 0},
  };

  for (const auto& indicator : indicators) {
    element = base::MakeUnique<SystemIndicator>(
        512, kIndicatorHeight, indicator.icon, indicator.resource_string);
    element->set_debug_id(indicator.debug_id);
    element->set_id(AllocateId());
    element->set_parent_id(main_content_->id());
    element->set_y_anchoring(YAnchoring::YTOP);
    element->set_visible(false);
    *(indicator.element) = element.get();
    system_indicators_.push_back(element.get());
    scene_->AddUiElement(std::move(element));
  }

  ConfigureIndicators();
}

void UiSceneManager::CreateContentQuad() {
  std::unique_ptr<UiElement> element;

  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kContentQuad);
  element->set_id(AllocateId());
  element->set_fill(vr::Fill::CONTENT);
  element->SetSize(kContentWidth, kContentHeight);
  element->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
  element->set_visible(false);
  element->set_corner_radius(kContentCornerRadius);
  element->animation_player().SetTransitionedProperties({TRANSFORM, BOUNDS});
  main_content_ = element.get();
  content_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kBackplane);
  element->set_id(AllocateId());
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kBackplaneSize, kBackplaneSize);
  element->SetTranslate(0, 0, -kTextureOffset);
  element->set_parent_id(main_content_->id());
  content_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Limit reticle distance to a sphere based on content distance.
  scene_->SetBackgroundDistance(
      main_content_->transform_operations().Apply().matrix().get(2, 3) *
      -kBackgroundDistanceMultiplier);
}

void UiSceneManager::CreateSplashScreen() {
  // Chrome icon.
  std::unique_ptr<SplashScreenIcon> icon =
      base::MakeUnique<SplashScreenIcon>(256);
  icon->set_debug_id(kSplashScreenIcon);
  icon->set_id(AllocateId());
  icon->set_hit_testable(false);
  icon->SetSize(kSplashScreenIconWidth, kSplashScreenIconHeight);
  icon->SetTranslate(0, kSplashScreenIconVerticalOffset,
                     -kSplashScreenDistance);
  splash_screen_icon_ = icon.get();
  scene_->AddUiElement(std::move(icon));
}

void UiSceneManager::CreateBackground() {
  std::unique_ptr<UiElement> element;

  // Floor.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kFloor);
  element->set_id(AllocateId());
  element->SetSize(kSceneSize, kSceneSize);
  element->SetTranslate(0.0, -kSceneHeight / 2, 0.0);
  element->SetRotate(1, 0, 0, -M_PI_2);
  element->set_fill(vr::Fill::GRID_GRADIENT);
  element->set_draw_phase(0);
  element->set_gridline_count(kFloorGridlineCount);
  floor_ = element.get();
  background_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));

  // Ceiling.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kCeiling);
  element->set_id(AllocateId());
  element->SetSize(kSceneSize, kSceneSize);
  element->SetTranslate(0.0, kSceneHeight / 2, 0.0);
  element->SetRotate(1, 0, 0, M_PI_2);
  element->set_fill(vr::Fill::OPAQUE_GRADIENT);
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
  url_bar->SetTranslate(0, kUrlBarVerticalOffset, -kUrlBarDistance);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->SetSize(kUrlBarWidth, kUrlBarHeight);
  url_bar_ = url_bar.get();
  control_elements_.push_back(url_bar.get());
  scene_->AddUiElement(std::move(url_bar));

  auto indicator = base::MakeUnique<LoadingIndicator>(256);
  indicator->set_debug_id(kLoadingIndicator);
  indicator->set_id(AllocateId());
  indicator->SetTranslate(0, kLoadingIndicatorVerticalOffset,
                          kLoadingIndicatorDepthOffset);
  indicator->SetSize(kLoadingIndicatorWidth, kLoadingIndicatorHeight);
  indicator->set_parent_id(url_bar_->id());
  indicator->set_y_anchoring(YAnchoring::YTOP);
  loading_indicator_ = indicator.get();
  control_elements_.push_back(indicator.get());
  scene_->AddUiElement(std::move(indicator));
}

void UiSceneManager::CreateTransientUrlBar() {
  auto url_bar = base::MakeUnique<TransientUrlBar>(
      512, base::TimeDelta::FromSeconds(kTransientUrlBarTimeoutSeconds),
      base::Bind(&UiSceneManager::OnUnsupportedMode, base::Unretained(this)));
  url_bar->set_debug_id(kTransientUrlBar);
  url_bar->set_id(AllocateId());
  url_bar->set_lock_to_fov(true);
  url_bar->set_visible(false);
  url_bar->set_hit_testable(false);
  url_bar->SetTranslate(0, kTransientUrlBarVerticalOffset,
                        -kTransientUrlBarDistance);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->SetSize(kTransientUrlBarWidth, kTransientUrlBarHeight);
  transient_url_bar_ = url_bar.get();
  scene_->AddUiElement(std::move(url_bar));
}

void UiSceneManager::CreateCloseButton() {
  std::unique_ptr<Button> element = base::MakeUnique<Button>(
      base::Bind(&UiSceneManager::OnCloseButtonClicked, base::Unretained(this)),
      base::MakeUnique<CloseButtonTexture>());
  element->set_debug_id(kCloseButton);
  element->set_id(AllocateId());
  element->set_fill(vr::Fill::NONE);
  element->SetTranslate(0, kContentVerticalOffset - (kContentHeight / 2) - 0.3,
                        -kCloseButtonDistance);
  element->SetSize(kCloseButtonWidth, kCloseButtonHeight);
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
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kExitPromptWidth, kExitPromptHeight);
  element->SetTranslate(0.0, kExitPromptVerticalOffset, kTextureOffset);
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
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kExitPromptBackplaneSize, kExitPromptBackplaneSize);
  element->SetTranslate(0.0, 0.0, -kTextureOffset);
  element->set_parent_id(exit_prompt_->id());
  exit_prompt_backplane_ = element.get();
  content_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateToasts() {
  auto element = base::MakeUnique<ExclusiveScreenToast>(
      512, base::TimeDelta::FromSeconds(kToastTimeoutSeconds));
  element->set_debug_id(kExclusiveScreenToast);
  element->set_id(AllocateId());
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kToastWidthDMM, kToastHeightDMM);
  element->set_visible(false);
  element->set_hit_testable(false);
  exclusive_screen_toast_ = element.get();
  scene_->AddUiElement(std::move(element));
}

base::WeakPtr<UiSceneManager> UiSceneManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void UiSceneManager::SetWebVrMode(bool web_vr, bool show_toast) {
  if (web_vr_mode_ == web_vr && web_vr_show_toast_ == show_toast) {
    return;
  }

  web_vr_mode_ = web_vr;
  web_vr_show_toast_ = show_toast;
  if (!web_vr_mode_) {
    waiting_for_first_web_vr_frame_ = false;
    started_for_autopresentation_ = false;
  }
  ConfigureScene();

  // Because we may be transitioning from and to fullscreen, where the toast is
  // also shown, explicitly kick or end visibility here.
  if (web_vr) {
    exclusive_screen_toast_->transience()->KickVisibilityIfEnabled();
  } else {
    exclusive_screen_toast_->transience()->EndVisibilityIfEnabled();
  }
}

void UiSceneManager::OnWebVrFrameAvailable() {
  if (!waiting_for_first_web_vr_frame_)
    return;
  waiting_for_first_web_vr_frame_ = false;
  ConfigureScene();
}

void UiSceneManager::ConfigureScene() {
  // We disable WebVR rendering if we're expecting to auto present so that we
  // can continue to show the 2D splash screen while the site submits the first
  // WebVR frame.
  scene_->SetWebVrRenderingEnabled(web_vr_mode_ &&
                                   !waiting_for_first_web_vr_frame_);
  // Splash screen.
  scene_->set_showing_splash_screen(waiting_for_first_web_vr_frame_);
  splash_screen_icon_->SetEnabled(waiting_for_first_web_vr_frame_);

  // Exit warning.
  exit_warning_->SetEnabled(scene_->is_exiting());
  screen_dimmer_->SetEnabled(scene_->is_exiting());

  bool browsing_mode = !web_vr_mode_ && !scene_->showing_splash_screen();

  // Controls (URL bar, loading progress, etc).
  bool controls_visible = browsing_mode && !fullscreen_;
  for (UiElement* element : control_elements_) {
    element->SetEnabled(controls_visible && !scene_->is_prompting_to_exit());
  }

  // Close button is a special control element that needs to be hidden when in
  // WebVR, but it needs to be visible when in cct or fullscreen.
  close_button_->SetEnabled(browsing_mode && (fullscreen_ || in_cct_));

  // Content elements.
  for (UiElement* element : content_elements_) {
    element->SetEnabled(browsing_mode && !scene_->is_prompting_to_exit());
  }

  // Background elements.
  for (UiElement* element : background_elements_) {
    element->SetEnabled(browsing_mode);
  }

  // Exit prompt.
  bool showExitPrompt = browsing_mode && scene_->is_prompting_to_exit();
  exit_prompt_->SetEnabled(showExitPrompt);
  exit_prompt_backplane_->SetEnabled(showExitPrompt);

  // Update content quad parameters depending on fullscreen.
  // TODO(http://crbug.com/642937): Animate fullscreen transitions.
  if (fullscreen_) {
    main_content_->SetTranslate(0, kFullscreenVerticalOffset,
                                -kFullscreenDistance);
    main_content_->SetSize(kFullscreenWidth, kFullscreenHeight);
    close_button_->SetTranslate(
        0, kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35,
        -kCloseButtonFullscreenDistance);
    close_button_->SetSize(kCloseButtonFullscreenWidth,
                           kCloseButtonFullscreenHeight);
  } else {
    // Note that main_content_ is already visible in this case.
    main_content_->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
    main_content_->SetSize(kContentWidth, kContentHeight);
    close_button_->SetTranslate(
        0, kContentVerticalOffset - (kContentHeight / 2) - 0.3,
        -kCloseButtonDistance);
    close_button_->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  }

  scene_->SetMode(mode());
  scene_->SetBackgroundDistance(
      main_content_->transform_operations().Apply().matrix().get(2, 3) *
      -kBackgroundDistanceMultiplier);
  UpdateBackgroundColor();

  transient_url_bar_->SetEnabled(started_for_autopresentation_ &&
                                 !scene_->showing_splash_screen());

  ConfigureExclusiveScreenToast();
  ConfigureSecurityWarnings();
  ConfigureIndicators();
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

void UiSceneManager::SetSplashScreenIcon(const SkBitmap& bitmap) {
  splash_screen_icon_->SetSplashScreenIconBitmap(bitmap);
  ConfigureScene();
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

void UiSceneManager::SetBluetoothConnectedIndicator(bool enabled) {
  bluetooth_connected_ = enabled;
  ConfigureIndicators();
}

void UiSceneManager::SetWebVrSecureOrigin(bool secure) {
  if (secure_origin_ == secure)
    return;
  secure_origin_ = secure;
  ConfigureSecurityWarnings();
}

void UiSceneManager::SetIncognito(bool incognito) {
  if (incognito == incognito_)
    return;
  incognito_ = incognito;
  ConfigureScene();
}

void UiSceneManager::OnGLInitialized() {
  scene_->OnGLInitialized();

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
  bool enabled =
      web_vr_mode_ && !secure_origin_ && !waiting_for_first_web_vr_frame_;
  permanent_security_warning_->SetEnabled(enabled);
  transient_security_warning_->SetEnabled(enabled);
}

void UiSceneManager::ConfigureIndicators() {
  bool allowed = !web_vr_mode_ && !fullscreen_;
  audio_capture_indicator_->set_visible(allowed && audio_capturing_);
  video_capture_indicator_->set_visible(allowed && video_capturing_);
  screen_capture_indicator_->set_visible(allowed && screen_capturing_);
  location_access_indicator_->set_visible(allowed && location_access_);
  bluetooth_connected_indicator_->set_visible(allowed && bluetooth_connected_);

  if (!allowed)
    return;

  // Position elements dynamically relative to each other, based on which
  // indicators are showing, and how big each one is.
  float total_width = 0;
  for (const UiElement* indicator : system_indicators_) {
    if (indicator->visible()) {
      if (total_width > 0)
        total_width += kIndicatorGap;
      total_width += indicator->size().width();
    }
  }
  float x_position = -total_width / 2;
  for (UiElement* indicator : system_indicators_) {
    if (!indicator->visible())
      continue;
    float width = indicator->size().width();
    indicator->SetTranslate(x_position + width / 2, kIndicatorVerticalOffset,
                            kIndicatorDistanceOffset);
    x_position += width + kIndicatorGap;
  }
}

void UiSceneManager::ConfigureExclusiveScreenToast() {
  exclusive_screen_toast_->SetEnabled((fullscreen_ && !web_vr_mode_) ||
                                      (web_vr_mode_ && web_vr_show_toast_));

  if (fullscreen_ && !web_vr_mode_) {
    // Do not set size again. The size might have been changed by the backing
    // texture size in UpdateElementSize.
    cc::TransformOperations operations;
    operations.AppendTranslate(
        0,
        kFullscreenVerticalOffset + kFullscreenHeight / 2 +
            (kToastOffsetDMM + kToastHeightDMM) * kFullscreenToastDistance,
        -kFullscreenToastDistance);
    operations.AppendRotate(1, 0, 0, 0);
    operations.AppendScale(kFullscreenToastDistance, kFullscreenToastDistance,
                           1);
    exclusive_screen_toast_->SetTransformOperations(operations);
    exclusive_screen_toast_->set_lock_to_fov(false);
  } else if (web_vr_mode_ && web_vr_show_toast_) {
    cc::TransformOperations operations;
    operations.AppendTranslate(0, kWebVrToastDistance * sin(kWebVrAngleRadians),
                               -kWebVrToastDistance * cos(kWebVrAngleRadians));
    operations.AppendRotate(1, 0, 0, cc::MathUtil::Rad2Deg(kWebVrAngleRadians));
    operations.AppendScale(kWebVrToastDistance, kWebVrToastDistance, 1);
    exclusive_screen_toast_->SetTransformOperations(operations);
    exclusive_screen_toast_->set_lock_to_fov(true);
  }
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

void UiSceneManager::SetToolbarState(const ToolbarState& state) {
  url_bar_->SetToolbarState(state);
  transient_url_bar_->SetToolbarState(state);
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

}  // namespace vr
