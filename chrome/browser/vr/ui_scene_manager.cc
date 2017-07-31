// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/close_button_texture.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/exclusive_screen_toast.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/exit_prompt_backplane.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/loading_indicator.h"
#include "chrome/browser/vr/elements/screen_dimmer.h"
#include "chrome/browser/vr/elements/splash_screen_icon.h"
#include "chrome/browser/vr/elements/system_indicator.h"
#include "chrome/browser/vr/elements/transient_url_bar.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_debug_id.h"
#include "chrome/browser/vr/elements/ui_element_transform_operations.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/elements/url_bar.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/transform_util.h"

namespace vr {

using TargetProperty::BOUNDS;
using TargetProperty::TRANSFORM;

namespace {

static constexpr int kWarningTimeoutSeconds = 30;
static constexpr float kWarningDistance = 1.0;
static constexpr float kWarningAngleRadians = 16.3 * M_PI / 180.0;
static constexpr float kPermanentWarningHeightDMM = 0.049f;
static constexpr float kPermanentWarningWidthDMM = 0.1568f;
static constexpr float kTransientWarningHeightDMM = 0.160;
static constexpr float kTransientWarningWidthDMM = 0.512;

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

static constexpr float kTransientUrlBarDistance = 1.0;
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

static constexpr float kSplashScreenDistance = 2.5;
static constexpr float kSplashScreenIconSize = 0.405;
static constexpr float kSplashScreenIconVerticalOffset = -0.18;

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

enum DrawPhase : int {
  kPhaseBackground = 0,
  kPhaseFloorCeiling,
  kPhaseForeground,
};

}  // namespace

UiSceneManager::UiSceneManager(UiBrowserInterface* browser,
                               UiScene* scene,
                               ContentInputDelegate* content_input_delegate,
                               bool in_cct,
                               bool in_web_vr,
                               bool web_vr_autopresentation_expected)
    : browser_(browser),
      scene_(scene),
      in_cct_(in_cct),
      web_vr_mode_(in_web_vr),
      started_for_autopresentation_(web_vr_autopresentation_expected),
      showing_web_vr_splash_screen_(web_vr_autopresentation_expected),
      weak_ptr_factory_(this) {
  CreateBackground();
  CreateContentQuad(content_input_delegate);
  CreateSecurityWarnings();
  CreateSystemIndicators();
  CreateUrlBar();
  CreateTransientUrlBar();
  CreateCloseButton();
  CreateScreenDimmer();
  CreateExitPrompt();
  CreateToasts();
  CreateSplashScreen();

  ConfigureScene();
}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::CreateScreenDimmer() {
  std::unique_ptr<UiElement> element;
  element = base::MakeUnique<ScreenDimmer>();
  element->set_debug_id(kScreenDimmer);
  element->set_id(AllocateId());
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetVisible(false);
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
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kPermanentWarningWidthDMM, kPermanentWarningHeightDMM);
  element->SetTranslate(0, kWarningDistance * sin(kWarningAngleRadians),
                        -kWarningDistance * cos(kWarningAngleRadians));
  element->SetRotate(1, 0, 0, kWarningAngleRadians);
  element->SetScale(kWarningDistance, kWarningDistance, 1);
  element->SetVisible(false);
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
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kTransientWarningWidthDMM, kTransientWarningHeightDMM);
  element->SetTranslate(0, 0, -kWarningDistance);
  element->SetScale(kWarningDistance, kWarningDistance, 1);
  element->SetVisible(false);
  element->set_hit_testable(false);
  element->set_lock_to_fov(true);
  scene_->AddUiElement(std::move(element));

  element = base::MakeUnique<ExitWarning>(1024);
  element->set_debug_id(kExitWarning);
  element->set_id(AllocateId());
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kExitWarningWidth, kExitWarningHeight);
  element->SetTranslate(0, 0, -kExitWarningDistance);
  element->SetScale(kExitWarningDistance, kExitWarningDistance, 1);
  element->SetVisible(false);
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

  std::unique_ptr<LinearLayout> indicator_layout =
      base::MakeUnique<LinearLayout>(LinearLayout::kHorizontal);
  indicator_layout->set_id(AllocateId());
  indicator_layout->set_y_anchoring(YAnchoring::YTOP);
  indicator_layout->SetTranslate(0, kIndicatorVerticalOffset,
                                 kIndicatorDistanceOffset);
  indicator_layout->set_margin(kIndicatorGap);
  main_content_->AddChild(indicator_layout.get());

  for (const auto& indicator : indicators) {
    element = base::MakeUnique<SystemIndicator>(
        512, kIndicatorHeight, indicator.icon, indicator.resource_string);
    element->set_debug_id(indicator.debug_id);
    element->set_id(AllocateId());
    element->set_draw_phase(kPhaseForeground);
    element->SetVisible(false);
    indicator_layout->AddChild(element.get());
    *(indicator.element) = element.get();
    system_indicators_.push_back(element.get());
    scene_->AddUiElement(std::move(element));
  }

  scene_->AddUiElement(std::move(indicator_layout));

  ConfigureIndicators();
}

void UiSceneManager::CreateContentQuad(ContentInputDelegate* delegate) {
  std::unique_ptr<ContentElement> main_content =
      base::MakeUnique<ContentElement>(delegate);
  main_content->set_debug_id(kContentQuad);
  main_content->set_id(AllocateId());
  main_content->set_draw_phase(kPhaseForeground);
  main_content->SetSize(kContentWidth, kContentHeight);
  main_content->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
  main_content->SetVisible(false);
  main_content->set_corner_radius(kContentCornerRadius);
  main_content->animation_player().SetTransitionedProperties(
      {TRANSFORM, BOUNDS});
  main_content_ = main_content.get();
  content_elements_.push_back(main_content.get());
  scene_->AddUiElement(std::move(main_content));

  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  std::unique_ptr<UiElement> hit_plane = base::MakeUnique<UiElement>();
  hit_plane->set_debug_id(kBackplane);
  hit_plane->set_id(AllocateId());
  hit_plane->set_draw_phase(kPhaseForeground);
  hit_plane->set_fill(vr::Fill::NONE);
  hit_plane->SetSize(kBackplaneSize, kBackplaneSize);
  hit_plane->SetTranslate(0, 0, -kTextureOffset);
  main_content_->AddChild(hit_plane.get());
  content_elements_.push_back(hit_plane.get());
  scene_->AddUiElement(std::move(hit_plane));

  // Limit reticle distance to a sphere based on content distance.
  scene_->set_background_distance(
      main_content_->LocalTransform().matrix().get(2, 3) *
      -kBackgroundDistanceMultiplier);
}

void UiSceneManager::CreateSplashScreen() {
  // Chrome icon.
  std::unique_ptr<SplashScreenIcon> icon =
      base::MakeUnique<SplashScreenIcon>(256);
  icon->set_debug_id(kSplashScreenIcon);
  icon->set_id(AllocateId());
  icon->set_draw_phase(kPhaseForeground);
  icon->set_hit_testable(false);
  icon->SetSize(kSplashScreenIconSize, kSplashScreenIconSize);
  icon->SetTranslate(0, kSplashScreenIconVerticalOffset,
                     -kSplashScreenDistance);
  splash_screen_icon_ = icon.get();
  scene_->AddUiElement(std::move(icon));
}

void UiSceneManager::CreateBackground() {
  std::unique_ptr<UiElement> element;

  // Background solid-color panels.
  struct Panel {
    UiElementDebugId debug_id;
    int x_offset;
    int y_offset;
    int z_offset;
    int x_rotation;
    int y_rotation;
    int angle;
  };
  const std::vector<Panel> panels = {
      {kBackgroundFront, 0, 0, -1, 0, 1, 0},
      {kBackgroundLeft, -1, 0, 0, 0, 1, 1},
      {kBackgroundBack, 0, 0, 1, 0, 1, 2},
      {kBackgroundRight, 1, 0, 0, 0, 1, 3},
      {kBackgroundTop, 0, 1, 0, 1, 0, 1},
      {kBackgroundBottom, 0, -1, 0, 1, 0, -1},
  };
  for (auto& panel : panels) {
    element = base::MakeUnique<UiElement>();
    element->set_debug_id(panel.debug_id);
    element->set_id(AllocateId());
    element->set_draw_phase(kPhaseBackground);
    element->SetSize(kSceneSize, kSceneSize);
    element->set_fill(vr::Fill::OPAQUE_GRADIENT);
    element->SetTranslate(panel.x_offset * kSceneSize / 2,
                          panel.y_offset * kSceneSize / 2,
                          panel.z_offset * kSceneSize / 2);
    element->SetRotate(panel.x_rotation, panel.y_rotation, 0,
                       M_PI_2 * panel.angle);
    element->set_fill(vr::Fill::OPAQUE_GRADIENT);
    element->set_hit_testable(false);
    background_panels_.push_back(element.get());
    scene_->AddUiElement(std::move(element));
  }

  // Floor.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kFloor);
  element->set_id(AllocateId());
  element->set_draw_phase(kPhaseFloorCeiling);
  element->SetSize(kSceneSize, kSceneSize);
  element->SetTranslate(0.0, -kSceneHeight / 2, 0.0);
  element->SetRotate(1, 0, 0, -M_PI_2);
  element->set_fill(vr::Fill::GRID_GRADIENT);
  element->set_gridline_count(kFloorGridlineCount);
  floor_ = element.get();
  scene_->AddUiElement(std::move(element));

  // Ceiling.
  element = base::MakeUnique<UiElement>();
  element->set_debug_id(kCeiling);
  element->set_id(AllocateId());
  element->set_draw_phase(kPhaseFloorCeiling);
  element->SetSize(kSceneSize, kSceneSize);
  element->SetTranslate(0.0, kSceneHeight / 2, 0.0);
  element->SetRotate(1, 0, 0, M_PI_2);
  element->set_fill(vr::Fill::OPAQUE_GRADIENT);
  ceiling_ = element.get();
  scene_->AddUiElement(std::move(element));

  scene_->set_first_foreground_draw_phase(kPhaseForeground);
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
  url_bar->set_draw_phase(kPhaseForeground);
  url_bar->SetTranslate(0, kUrlBarVerticalOffset, -kUrlBarDistance);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->SetSize(kUrlBarWidth, kUrlBarHeight);
  url_bar_ = url_bar.get();
  control_elements_.push_back(url_bar.get());
  scene_->AddUiElement(std::move(url_bar));

  auto indicator = base::MakeUnique<LoadingIndicator>(256);
  indicator->set_debug_id(kLoadingIndicator);
  indicator->set_id(AllocateId());
  indicator->set_draw_phase(kPhaseForeground);
  indicator->SetTranslate(0, kLoadingIndicatorVerticalOffset,
                          kLoadingIndicatorDepthOffset);
  indicator->SetSize(kLoadingIndicatorWidth, kLoadingIndicatorHeight);
  url_bar_->AddChild(indicator.get());
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
  url_bar->set_draw_phase(kPhaseForeground);
  url_bar->set_lock_to_fov(true);
  url_bar->SetVisible(false);
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
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetTranslate(0, kContentVerticalOffset - (kContentHeight / 2) - 0.3,
                        -kCloseButtonDistance);
  element->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  close_button_ = element.get();
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateExitPrompt() {
  std::unique_ptr<UiElement> element;

  std::unique_ptr<ExitPrompt> exit_prompt = base::MakeUnique<ExitPrompt>(
      512,
      base::Bind(&UiSceneManager::OnExitPromptChoice, base::Unretained(this),
                 false),
      base::Bind(&UiSceneManager::OnExitPromptChoice, base::Unretained(this),
                 true));
  exit_prompt_ = exit_prompt.get();
  element = std::move(exit_prompt);
  element->set_debug_id(kExitPrompt);
  element->set_id(AllocateId());
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kExitPromptWidth, kExitPromptHeight);
  element->SetTranslate(0.0, kExitPromptVerticalOffset, kTextureOffset);
  element->SetVisible(false);
  main_content_->AddChild(element.get());
  scene_->AddUiElement(std::move(element));

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = base::MakeUnique<ExitPromptBackplane>(base::Bind(
      &UiSceneManager::OnExitPromptBackplaneClicked, base::Unretained(this)));
  exit_prompt_backplane_ = backplane.get();
  element = std::move(backplane);
  element->set_debug_id(kExitPromptBackplane);
  element->set_id(AllocateId());
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kExitPromptBackplaneSize, kExitPromptBackplaneSize);
  element->SetTranslate(0.0, 0.0, -kTextureOffset);
  exit_prompt_->AddChild(element.get());
  exit_prompt_backplane_ = element.get();
  content_elements_.push_back(element.get());
  scene_->AddUiElement(std::move(element));
}

void UiSceneManager::CreateToasts() {
  auto element = base::MakeUnique<ExclusiveScreenToast>(
      512, base::TimeDelta::FromSeconds(kToastTimeoutSeconds));
  element->set_debug_id(kExclusiveScreenToast);
  element->set_id(AllocateId());
  element->set_draw_phase(kPhaseForeground);
  element->set_fill(vr::Fill::NONE);
  element->SetSize(kToastWidthDMM, kToastHeightDMM);
  element->SetVisible(false);
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
    showing_web_vr_splash_screen_ = false;
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
  if (!showing_web_vr_splash_screen_)
    return;
  showing_web_vr_splash_screen_ = false;
  ConfigureScene();
}

void UiSceneManager::ConfigureScene() {
  // We disable WebVR rendering if we're expecting to auto present so that we
  // can continue to show the 2D splash screen while the site submits the first
  // WebVR frame.
  bool showing_web_vr_content = web_vr_mode_ && !showing_web_vr_splash_screen_;
  scene_->set_web_vr_rendering_enabled(showing_web_vr_content);
  // Splash screen.
  splash_screen_icon_->SetEnabled(showing_web_vr_splash_screen_);

  // Exit warning.
  exit_warning_->SetEnabled(exiting_);
  screen_dimmer_->SetEnabled(exiting_);

  bool browsing_mode = !web_vr_mode_ && !showing_web_vr_splash_screen_;

  // Controls (URL bar, loading progress, etc).
  bool controls_visible = browsing_mode && !fullscreen_ && !prompting_to_exit_;
  for (UiElement* element : control_elements_) {
    element->SetEnabled(controls_visible);
  }

  // Close button is a special control element that needs to be hidden when in
  // WebVR, but it needs to be visible when in cct or fullscreen.
  close_button_->SetEnabled(browsing_mode && (fullscreen_ || in_cct_));

  // Content elements.
  for (UiElement* element : content_elements_) {
    element->SetEnabled(browsing_mode && !prompting_to_exit_);
  }

  // Background elements.
  for (UiElement* element : background_panels_) {
    element->SetEnabled(!showing_web_vr_content);
  }
  floor_->SetEnabled(browsing_mode);
  ceiling_->SetEnabled(browsing_mode);

  // Exit prompt.
  bool showExitPrompt = browsing_mode && prompting_to_exit_;
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
  scene_->set_background_distance(
      main_content_->LocalTransform().matrix().get(2, 3) *
      -kBackgroundDistanceMultiplier);

  for (auto& element : scene_->GetUiElements())
    element->SetMode(mode());

  transient_url_bar_->SetEnabled(started_for_autopresentation_ &&
                                 !showing_web_vr_splash_screen_);

  scene_->set_reticle_rendering_enabled(
      !(web_vr_mode_ || exiting_ || showing_web_vr_splash_screen_));

  ConfigureExclusiveScreenToast();
  ConfigureSecurityWarnings();
  ConfigureIndicators();
  ConfigureBackgroundColor();
}

void UiSceneManager::ConfigureBackgroundColor() {
  // TODO(vollick): it would be nice if ceiling, floor and the grid were
  // UiElement subclasses and could respond to the OnSetMode signal.
  for (UiElement* panel : background_panels_) {
    panel->set_center_color(color_scheme().world_background);
    panel->set_edge_color(color_scheme().world_background);
  }
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

void UiSceneManager::OnGlInitialized(unsigned int content_texture_id) {
  main_content_->set_texture_id(content_texture_id);
  scene_->OnGlInitialized();

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

void UiSceneManager::SetExitVrPromptEnabled(bool enabled,
                                            UiUnsupportedMode reason) {
  DCHECK(enabled || reason == UiUnsupportedMode::kCount);
  if (prompting_to_exit_ && enabled) {
    browser_->OnExitVrPromptResult(exit_vr_prompt_reason_,
                                   ExitVrPromptChoice::CHOICE_NONE);
  }
  exit_prompt_->SetContentMessageId(
      (reason == UiUnsupportedMode::kUnhandledPageInfo)
          ? IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION_SITE_INFO
          : IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION);
  exit_vr_prompt_reason_ = reason;
  prompting_to_exit_ = enabled;
  ConfigureScene();
}

void UiSceneManager::ConfigureSecurityWarnings() {
  bool enabled =
      web_vr_mode_ && !secure_origin_ && !showing_web_vr_splash_screen_;
  permanent_security_warning_->SetEnabled(enabled);
  transient_security_warning_->SetEnabled(enabled);
}

void UiSceneManager::ConfigureIndicators() {
  bool allowed = !web_vr_mode_ && !fullscreen_;
  audio_capture_indicator_->SetVisible(allowed && audio_capturing_);
  video_capture_indicator_->SetVisible(allowed && video_capturing_);
  screen_capture_indicator_->SetVisible(allowed && screen_capturing_);
  location_access_indicator_->SetVisible(allowed && location_access_);
  bluetooth_connected_indicator_->SetVisible(allowed && bluetooth_connected_);
}

void UiSceneManager::ConfigureExclusiveScreenToast() {
  exclusive_screen_toast_->SetEnabled((fullscreen_ && !web_vr_mode_) ||
                                      (web_vr_mode_ && web_vr_show_toast_));

  if (fullscreen_ && !web_vr_mode_) {
    // Do not set size again. The size might have been changed by the backing
    // texture size in UpdateElementSize.
    UiElementTransformOperations operations;
    operations.SetTranslate(
        0,
        kFullscreenVerticalOffset + kFullscreenHeight / 2 +
            (kToastOffsetDMM + kToastHeightDMM) * kFullscreenToastDistance,
        -kFullscreenToastDistance);
    operations.SetRotate(1, 0, 0, 0);
    operations.SetScale(kFullscreenToastDistance, kFullscreenToastDistance, 1);
    exclusive_screen_toast_->SetTransformOperations(operations);
    exclusive_screen_toast_->set_lock_to_fov(false);
  } else if (web_vr_mode_ && web_vr_show_toast_) {
    UiElementTransformOperations operations;
    operations.SetTranslate(0, kWebVrToastDistance * sin(kWebVrAngleRadians),
                            -kWebVrToastDistance * cos(kWebVrAngleRadians));
    operations.SetRotate(1, 0, 0, cc::MathUtil::Rad2Deg(kWebVrAngleRadians));
    operations.SetScale(kWebVrToastDistance, kWebVrToastDistance, 1);
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

void UiSceneManager::OnExitPromptChoiceForTesting(bool chose_exit) {
  OnExitPromptChoice(chose_exit);
}

void UiSceneManager::OnSecurityIconClicked() {
  browser_->OnUnsupportedMode(UiUnsupportedMode::kUnhandledPageInfo);
}

void UiSceneManager::OnExitPromptBackplaneClicked() {
  browser_->OnExitVrPromptResult(exit_vr_prompt_reason_,
                                 ExitVrPromptChoice::CHOICE_NONE);
}

void UiSceneManager::OnExitPromptChoice(bool chose_exit) {
  browser_->OnExitVrPromptResult(exit_vr_prompt_reason_,
                                 chose_exit ? ExitVrPromptChoice::CHOICE_EXIT
                                            : ExitVrPromptChoice::CHOICE_STAY);
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
  if (exiting_)
    return;
  exiting_ = true;
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
