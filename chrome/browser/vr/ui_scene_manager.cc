// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/close_button_texture.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/exclusive_screen_toast.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/exit_prompt_backplane.h"
#include "chrome/browser/vr/elements/full_screen_rect.h"
#include "chrome/browser/vr/elements/grid.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/loading_indicator.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/system_indicator.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_element_transform_operations.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/elements/url_bar.h"
#include "chrome/browser/vr/elements/viewport_aware_root.h"
#include "chrome/browser/vr/elements/webvr_url_toast.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/vr_gl_util.h"
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
// Make sure that the aspect ratio for fullscreen is 16:9. Otherwise, we may
// experience visual artefacts for fullscreened videos.
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

static constexpr float kWebVrUrlToastWidthDMM = 0.472;
static constexpr float kWebVrUrlToastHeightDMM = 0.064;
static constexpr float kWebVrUrlToastDistance = 1.0;
static constexpr float kWebVrUrlToastWidth =
    kWebVrUrlToastWidthDMM * kWebVrUrlToastDistance;
static constexpr float kWebVrUrlToastHeight =
    kWebVrUrlToastHeightDMM * kWebVrUrlToastDistance;
static constexpr int kWebVrUrlToastTimeoutSeconds = 6;
static constexpr float kWebVrUrlToastRotationRad = 14 * M_PI / 180.0;

static constexpr float kWebVrToastDistance = 1.0;
static constexpr float kFullscreenToastDistance = kFullscreenDistance;
static constexpr float kToastWidthDMM = 0.512;
static constexpr float kToastHeightDMM = 0.064;
static constexpr float kToastOffsetDMM = 0.004;
// When changing the value here, make sure it doesn't collide with
// kWarningAngleRadians.
static constexpr float kWebVrAngleRadians = 9.88 * M_PI / 180.0;
static constexpr int kToastTimeoutSeconds = kWebVrUrlToastTimeoutSeconds;

static constexpr float kSplashScreenTextDistance = 2.5;
static constexpr float kSplashScreenTextFontHeightM =
    0.05f * kSplashScreenTextDistance;
static constexpr float kSplashScreenTextWidthM =
    0.9f * kSplashScreenTextDistance;
static constexpr float kSplashScreenTextHeightM =
    0.08f * kSplashScreenTextDistance;
static constexpr float kSplashScreenTextVerticalOffset = -0.18;

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

static constexpr float kUnderDevelopmentNoticeFontHeightM =
    0.02f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeHeightM = 0.1f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeWidthM = 0.44f * kUrlBarDistance;
static constexpr float kUnderDevelopmentNoticeVerticalOffsetM =
    0.5f * kUnderDevelopmentNoticeHeightM + kUrlBarHeight;
static constexpr float kUnderDevelopmentNoticeRotationRad = -0.19;

// If the screen space bounds or the aspect ratio of the content quad change
// beyond these thresholds we propagate the new content bounds so that the
// content's resolution can be adjusted.
static constexpr float kContentBoundsPropagationThreshold = 0.2f;
// Changes of the aspect ratio lead to a
// distorted content much more quickly. Thus, have a smaller threshold here.
static constexpr float kContentAspectRatioPropagationThreshold = 0.01f;

static constexpr float kScreenDimmerOpacity = 0.9f;

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
  Create2dBrowsingSubtreeRoots();
  CreateWebVrRoot();
  CreateBackground();
  CreateViewportAwareRoot();
  CreateContentQuad(content_input_delegate);
  CreateSecurityWarnings();
  CreateSystemIndicators();
  CreateUrlBar();
  CreateWebVrUrlToast();
  CreateCloseButton();
  CreateScreenDimmer();
  CreateExitPrompt();
  CreateToasts();
  CreateSplashScreen();
  CreateUnderDevelopmentNotice();

  ConfigureScene();
}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::Create2dBrowsingSubtreeRoots() {
  auto element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(kRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingBackground);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingForeground);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingContentGroup);
  element->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
  element->SetSize(kContentWidth, kContentHeight);
  element->SetVisible(true);
  element->set_hit_testable(false);
  element->SetTransitionedProperties({TRANSFORM});
  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
}

void UiSceneManager::CreateWebVrRoot() {
  auto element = base::MakeUnique<UiElement>();
  element->set_name(kWebVrRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(kRoot, std::move(element));
}

void UiSceneManager::CreateScreenDimmer() {
  auto element = base::MakeUnique<FullScreenRect>();
  element->set_name(kScreenDimmer);
  element->set_draw_phase(kPhaseOverlayBackground);
  element->SetVisible(false);
  element->set_hit_testable(false);
  element->SetOpacity(kScreenDimmerOpacity);
  element->SetCenterColor(color_scheme().dimmer_inner);
  element->SetEdgeColor(color_scheme().dimmer_outer);
  screen_dimmer_ = element.get();
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));
}

void UiSceneManager::CreateSecurityWarnings() {
  std::unique_ptr<UiElement> element;

  // TODO(mthiesse): Programatically compute the proper texture size for these
  // textured UI elements.
  element = base::MakeUnique<PermanentSecurityWarning>(512);
  element->set_name(kWebVrPermanentHttpSecurityWarning);
  element->set_draw_phase(kPhaseOverlayForeground);
  element->SetSize(kPermanentWarningWidthDMM, kPermanentWarningHeightDMM);
  element->SetTranslate(0, kWarningDistance * sin(kWarningAngleRadians),
                        -kWarningDistance * cos(kWarningAngleRadians));
  element->SetRotate(1, 0, 0, kWarningAngleRadians);
  element->SetScale(kWarningDistance, kWarningDistance, 1);
  element->SetVisible(false);
  element->set_hit_testable(false);
  permanent_security_warning_ = element.get();
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(element));

  auto transient_warning = base::MakeUnique<TransientSecurityWarning>(
      1024, base::TimeDelta::FromSeconds(kWarningTimeoutSeconds));
  transient_security_warning_ = transient_warning.get();
  element = std::move(transient_warning);
  element->set_name(kWebVrTransientHttpSecurityWarning);
  element->set_draw_phase(kPhaseOverlayForeground);
  element->SetSize(kTransientWarningWidthDMM, kTransientWarningHeightDMM);
  element->SetTranslate(0, 0, -kWarningDistance);
  element->SetScale(kWarningDistance, kWarningDistance, 1);
  element->SetVisible(false);
  element->set_hit_testable(false);
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(element));

  element = base::MakeUnique<ExitWarning>(1024);
  element->set_name(kExitWarning);
  element->SetSize(kExitWarningWidth, kExitWarningHeight);
  element->SetTranslate(0, 0, -kExitWarningDistance);
  element->SetScale(kExitWarningDistance, kExitWarningDistance, 1);
  element->SetVisible(false);
  element->set_hit_testable(false);
  exit_warning_ = element.get();
  scene_->AddUiElement(k2dBrowsingViewportAwareRoot, std::move(element));
}

void UiSceneManager::CreateSystemIndicators() {
  std::unique_ptr<UiElement> element;

  struct Indicator {
    UiElement** element;
    UiElementName name;
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
  indicator_layout->set_name(kIndicatorLayout);
  indicator_layout->set_hit_testable(false);
  indicator_layout->set_y_anchoring(YAnchoring::YTOP);
  indicator_layout->SetTranslate(0, kIndicatorVerticalOffset,
                                 kIndicatorDistanceOffset);
  indicator_layout->set_margin(kIndicatorGap);
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(indicator_layout));

  for (const auto& indicator : indicators) {
    element = base::MakeUnique<SystemIndicator>(
        512, kIndicatorHeight, indicator.icon, indicator.resource_string);
    element->set_name(indicator.name);
    element->set_draw_phase(kPhaseForeground);
    element->SetVisible(false);
    *(indicator.element) = element.get();
    system_indicators_.push_back(element.get());
    scene_->AddUiElement(kIndicatorLayout, std::move(element));
  }

  ConfigureIndicators();
}

void UiSceneManager::CreateContentQuad(ContentInputDelegate* delegate) {
  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  std::unique_ptr<UiElement> hit_plane = base::MakeUnique<UiElement>();
  hit_plane->set_name(kBackplane);
  hit_plane->SetSize(kBackplaneSize, kBackplaneSize);
  hit_plane->SetTranslate(0, 0, -kTextureOffset);
  content_elements_.push_back(hit_plane.get());
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(hit_plane));

  std::unique_ptr<ContentElement> main_content =
      base::MakeUnique<ContentElement>(delegate);
  main_content->set_name(kContentQuad);
  main_content->set_draw_phase(kPhaseForeground);
  main_content->SetSize(kContentWidth, kContentHeight);
  main_content->SetVisible(false);
  main_content->set_corner_radius(kContentCornerRadius);
  main_content->SetTransitionedProperties({BOUNDS});
  main_content_ = main_content.get();
  content_elements_.push_back(main_content.get());
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(main_content));

  // Limit reticle distance to a sphere based on content distance.
  scene_->set_background_distance(kContentDistance *
                                  kBackgroundDistanceMultiplier);
}

void UiSceneManager::CreateSplashScreen() {
  // Add "Powered by Chrome" text.
  auto text = base::MakeUnique<Text>(
      512, kSplashScreenTextFontHeightM, kSplashScreenTextWidthM,
      base::Bind([](ColorScheme color_scheme) {
        return color_scheme.splash_screen_text_color;
      }),
      IDS_VR_POWERED_BY_CHROME_MESSAGE);
  text->set_name(kSplashScreenText);
  text->set_draw_phase(kPhaseOverlayForeground);
  text->set_hit_testable(false);
  text->SetSize(kSplashScreenTextWidthM, kSplashScreenTextHeightM);
  text->SetTranslate(0, kSplashScreenTextVerticalOffset,
                     -kSplashScreenTextDistance);
  splash_screen_ = text.get();
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(text));

  auto bg = base::MakeUnique<FullScreenRect>();
  bg->set_name(kSplashScreenBackground);
  bg->set_draw_phase(kPhaseOverlayBackground);
  bg->SetVisible(true);
  bg->set_hit_testable(false);
  bg->SetCenterColor(color_scheme().splash_screen_background);
  bg->SetEdgeColor(color_scheme().splash_screen_background);
  scene_->AddUiElement(kSplashScreenText, std::move(bg));
}

void UiSceneManager::CreateUnderDevelopmentNotice() {
  auto text = base::MakeUnique<Text>(
      512, kUnderDevelopmentNoticeFontHeightM, kUnderDevelopmentNoticeWidthM,
      base::Bind([](ColorScheme color_scheme) {
        return color_scheme.world_background_text;
      }),
      IDS_VR_UNDER_DEVELOPMENT_NOTICE);
  text->set_name(kUnderDevelopmentNotice);
  text->set_draw_phase(kPhaseForeground);
  text->set_hit_testable(false);
  text->SetSize(kUnderDevelopmentNoticeWidthM, kUnderDevelopmentNoticeHeightM);
  text->SetTranslate(0, -kUnderDevelopmentNoticeVerticalOffsetM, 0);
  text->SetRotate(1, 0, 0, kUnderDevelopmentNoticeRotationRad);
  text->SetVisible(true);
  text->set_y_anchoring(YAnchoring::YBOTTOM);
  control_elements_.push_back(text.get());
  scene_->AddUiElement(kUrlBar, std::move(text));
}

void UiSceneManager::CreateBackground() {
  // Background solid-color panels.
  struct Panel {
    UiElementName name;
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
    auto panel_element = base::MakeUnique<Rect>();
    panel_element->set_name(panel.name);
    panel_element->set_draw_phase(kPhaseBackground);
    panel_element->SetSize(kSceneSize, kSceneSize);
    panel_element->SetTranslate(panel.x_offset * kSceneSize / 2,
                                panel.y_offset * kSceneSize / 2,
                                panel.z_offset * kSceneSize / 2);
    panel_element->SetRotate(panel.x_rotation, panel.y_rotation, 0,
                             M_PI_2 * panel.angle);
    panel_element->set_hit_testable(false);
    background_panels_.push_back(panel_element.get());
    scene_->AddUiElement(k2dBrowsingBackground, std::move(panel_element));
  }

  // Floor.
  auto floor = base::MakeUnique<Grid>();
  floor->set_name(kFloor);
  floor->set_draw_phase(kPhaseFloorCeiling);
  floor->SetSize(kSceneSize, kSceneSize);
  floor->SetTranslate(0.0, -kSceneHeight / 2, 0.0);
  floor->SetRotate(1, 0, 0, -M_PI_2);
  floor->set_gridline_count(kFloorGridlineCount);
  floor_ = floor.get();
  scene_->AddUiElement(k2dBrowsingBackground, std::move(floor));

  // Ceiling.
  auto ceiling = base::MakeUnique<Rect>();
  ceiling->set_name(kCeiling);
  ceiling->set_draw_phase(kPhaseFloorCeiling);
  ceiling->SetSize(kSceneSize, kSceneSize);
  ceiling->SetTranslate(0.0, kSceneHeight / 2, 0.0);
  ceiling->SetRotate(1, 0, 0, M_PI_2);
  ceiling_ = ceiling.get();
  scene_->AddUiElement(k2dBrowsingBackground, std::move(ceiling));

  scene_->set_first_foreground_draw_phase(kPhaseForeground);
}

void UiSceneManager::CreateViewportAwareRoot() {
  auto element = base::MakeUnique<ViewportAwareRoot>();
  element->set_name(kWebVrViewportAwareRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(kWebVrRoot, std::move(element));

  element = base::MakeUnique<ViewportAwareRoot>();
  element->set_name(k2dBrowsingViewportAwareRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));
}

void UiSceneManager::CreateUrlBar() {
  // TODO(cjgrant): Incorporate final size and position.
  auto url_bar = base::MakeUnique<UrlBar>(
      512,
      base::Bind(&UiSceneManager::OnBackButtonClicked, base::Unretained(this)),
      base::Bind(&UiSceneManager::OnSecurityIconClicked,
                 base::Unretained(this)),
      base::Bind(&UiSceneManager::OnUnsupportedMode, base::Unretained(this)));
  url_bar->set_name(kUrlBar);
  url_bar->set_draw_phase(kPhaseForeground);
  url_bar->SetTranslate(0, kUrlBarVerticalOffset, -kUrlBarDistance);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->SetSize(kUrlBarWidth, kUrlBarHeight);
  url_bar_ = url_bar.get();
  control_elements_.push_back(url_bar.get());
  scene_->AddUiElement(k2dBrowsingForeground, std::move(url_bar));

  auto indicator = base::MakeUnique<LoadingIndicator>(256);
  indicator->set_name(kLoadingIndicator);
  indicator->set_draw_phase(kPhaseForeground);
  indicator->SetTranslate(0, kLoadingIndicatorVerticalOffset,
                          kLoadingIndicatorDepthOffset);
  indicator->SetSize(kLoadingIndicatorWidth, kLoadingIndicatorHeight);
  indicator->set_y_anchoring(YAnchoring::YTOP);
  loading_indicator_ = indicator.get();
  control_elements_.push_back(indicator.get());
  scene_->AddUiElement(kUrlBar, std::move(indicator));
}

void UiSceneManager::CreateWebVrUrlToast() {
  auto url_bar = base::MakeUnique<WebVrUrlToast>(
      512, base::TimeDelta::FromSeconds(kWebVrUrlToastTimeoutSeconds),
      base::Bind(&UiSceneManager::OnUnsupportedMode, base::Unretained(this)));
  url_bar->set_name(kWebVrUrlToast);
  url_bar->set_opacity_when_visible(0.8);
  url_bar->set_draw_phase(kPhaseOverlayForeground);
  url_bar->SetVisible(false);
  url_bar->set_hit_testable(false);
  url_bar->SetTranslate(0, kWebVrToastDistance * sin(kWebVrUrlToastRotationRad),
                        -kWebVrToastDistance * cos(kWebVrUrlToastRotationRad));
  url_bar->SetRotate(1, 0, 0, kWebVrUrlToastRotationRad);
  url_bar->SetSize(kWebVrUrlToastWidth, kWebVrUrlToastHeight);
  webvr_url_toast_ = url_bar.get();
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(url_bar));
}

void UiSceneManager::CreateCloseButton() {
  std::unique_ptr<Button> element = base::MakeUnique<Button>(
      base::Bind(&UiSceneManager::OnCloseButtonClicked, base::Unretained(this)),
      base::MakeUnique<CloseButtonTexture>());
  element->set_name(kCloseButton);
  element->set_draw_phase(kPhaseForeground);
  element->SetTranslate(0, kContentVerticalOffset - (kContentHeight / 2) - 0.3,
                        -kCloseButtonDistance);
  element->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  close_button_ = element.get();
  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
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
  element->set_name(kExitPrompt);
  element->set_draw_phase(kPhaseForeground);
  element->SetSize(kExitPromptWidth, kExitPromptHeight);
  element->SetTranslate(0.0, kExitPromptVerticalOffset, kTextureOffset);
  element->SetVisible(false);
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(element));

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = base::MakeUnique<ExitPromptBackplane>(base::Bind(
      &UiSceneManager::OnExitPromptBackplaneClicked, base::Unretained(this)));
  exit_prompt_backplane_ = backplane.get();
  element = std::move(backplane);
  element->set_name(kExitPromptBackplane);
  element->SetSize(kExitPromptBackplaneSize, kExitPromptBackplaneSize);
  element->SetTranslate(0.0, 0.0, -kTextureOffset);
  exit_prompt_backplane_ = element.get();
  content_elements_.push_back(element.get());
  scene_->AddUiElement(kExitPrompt, std::move(element));
}

void UiSceneManager::CreateToasts() {
  auto element = base::MakeUnique<ExclusiveScreenToast>(
      512, base::TimeDelta::FromSeconds(kToastTimeoutSeconds));
  element->set_name(kExclusiveScreenToast);
  element->set_draw_phase(kPhaseForeground);
  element->SetSize(kToastWidthDMM, kToastHeightDMM);
  element->SetTranslate(
      0,
      kFullscreenVerticalOffset + kFullscreenHeight / 2 +
          (kToastOffsetDMM + kToastHeightDMM) * kFullscreenToastDistance,
      -kFullscreenToastDistance);
  element->SetScale(kFullscreenToastDistance, kFullscreenToastDistance, 1);
  element->SetVisible(false);
  element->set_hit_testable(false);
  exclusive_screen_toast_ = element.get();
  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));

  element = base::MakeUnique<ExclusiveScreenToast>(
      512, base::TimeDelta::FromSeconds(kToastTimeoutSeconds));
  element->set_name(kExclusiveScreenToastViewportAware);
  element->set_draw_phase(kPhaseOverlayForeground);
  element->SetSize(kToastWidthDMM, kToastHeightDMM);
  element->SetTranslate(0, kWebVrToastDistance * sin(kWebVrAngleRadians),
                        -kWebVrToastDistance * cos(kWebVrAngleRadians));
  element->SetRotate(1, 0, 0, kWebVrAngleRadians);
  element->SetScale(kWebVrToastDistance, kWebVrToastDistance, 1);
  element->SetVisible(false);
  element->set_hit_testable(false);
  exclusive_screen_toast_viewport_aware_ = element.get();
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(element));
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
    exclusive_screen_toast_viewport_aware_->transience()->KickVisibility();
  } else {
    exclusive_screen_toast_->transience()->EndVisibility();
  }
}

void UiSceneManager::OnWebVrFrameAvailable() {
  if (!showing_web_vr_splash_screen_)
    return;
  showing_web_vr_splash_screen_ = false;
  ConfigureScene();
}

void UiSceneManager::OnWebVrTimedOut() {
  browser_->ExitPresent();
}

void UiSceneManager::OnProjMatrixChanged(const gfx::Transform& proj_matrix) {
  // Determine if the projected size of the content quad changed more than a
  // given threshold. If so, propagate this info so that the content's
  // resolution and size can be adjusted. For the calculation, we cannot take
  // the content quad's actual size (main_content_->size()) if this property
  // is animated. If we took the actual size during an animation we would
  // surpass the threshold with differing projected sizes and aspect ratios
  // (depending on the animation's timing). The differing values may cause
  // visual artefacts if, for instance, the fullscreen aspect ratio is not 16:9.
  // As a workaround, take the final size of the content quad after the
  // animation as the basis for the calculation.
  DCHECK(main_content_);

  gfx::SizeF main_content_size;
  if (main_content_->animation_player().IsAnimatingProperty(
          TargetProperty::BOUNDS)) {
    main_content_size = main_content_->animation_player().GetTargetSizeValue(
        TargetProperty::BOUNDS);
  } else {
    main_content_size = main_content_->size();
  }

  // We take the target transform in case the content quad's parent's translate
  // is animated. This approach only works with the current scene hierarchy and
  // set of animated properties.
  // TODO(tiborg): Find a way to get the target value of the inheritable
  // transfrom that works with any scene hierarchy and set of animated
  // properties.
  gfx::Transform main_content_inheritable_transform;
  if (scene_->GetUiElementByName(k2dBrowsingContentGroup)
          ->animation_player()
          .IsAnimatingProperty(TargetProperty::TRANSFORM)) {
    main_content_inheritable_transform =
        scene_->GetUiElementByName(k2dBrowsingContentGroup)
            ->animation_player()
            .GetTargetTransformOperationsValue(TargetProperty::TRANSFORM)
            .Apply();
  } else {
    main_content_inheritable_transform = main_content_->inheritable_transform();
  }

  gfx::SizeF screen_size = CalculateScreenSize(
      proj_matrix, main_content_inheritable_transform, main_content_size);

  float aspect_ratio = main_content_size.width() / main_content_size.height();
  gfx::SizeF screen_bounds;
  if (screen_size.width() < screen_size.height() * aspect_ratio) {
    screen_bounds.set_width(screen_size.height() * aspect_ratio);
    screen_bounds.set_height(screen_size.height());
  } else {
    screen_bounds.set_width(screen_size.width());
    screen_bounds.set_height(screen_size.width() / aspect_ratio);
  }

  if (std::abs(screen_bounds.width() - last_content_screen_bounds_.width()) >
          kContentBoundsPropagationThreshold ||
      std::abs(screen_bounds.height() - last_content_screen_bounds_.height()) >
          kContentBoundsPropagationThreshold ||
      std::abs(aspect_ratio - last_content_aspect_ratio_) >
          kContentAspectRatioPropagationThreshold) {
    browser_->OnContentScreenBoundsChanged(screen_bounds);

    last_content_screen_bounds_.set_width(screen_bounds.width());
    last_content_screen_bounds_.set_height(screen_bounds.height());
    last_content_aspect_ratio_ = aspect_ratio;
  }
}

void UiSceneManager::ConfigureScene() {
  // We disable WebVR rendering if we're expecting to auto present so that we
  // can continue to show the 2D splash screen while the site submits the first
  // WebVR frame.
  bool showing_web_vr_content = web_vr_mode_ && !showing_web_vr_splash_screen_;
  scene_->set_web_vr_rendering_enabled(showing_web_vr_content);
  // Splash screen.
  splash_screen_->SetVisible(showing_web_vr_splash_screen_);

  // Exit warning.
  exit_warning_->SetVisible(exiting_);
  screen_dimmer_->SetVisible(exiting_);

  bool browsing_mode = !web_vr_mode_ && !showing_web_vr_splash_screen_;
  scene_->GetUiElementByName(k2dBrowsingRoot)->SetVisible(browsing_mode);
  scene_->GetUiElementByName(kWebVrRoot)->SetVisible(!browsing_mode);

  // Controls (URL bar, loading progress, etc).
  bool controls_visible = browsing_mode && !fullscreen_ && !prompting_to_exit_;
  for (UiElement* element : control_elements_) {
    element->SetVisible(controls_visible);
  }

  // Close button is a special control element that needs to be hidden when in
  // WebVR, but it needs to be visible when in cct or fullscreen.
  close_button_->SetVisible(browsing_mode && (fullscreen_ || in_cct_));

  // Content elements.
  for (UiElement* element : content_elements_) {
    element->SetVisible(browsing_mode && !prompting_to_exit_);
  }

  // Background elements.
  for (UiElement* element : background_panels_) {
    element->SetVisible(!showing_web_vr_content);
  }
  floor_->SetVisible(browsing_mode);
  ceiling_->SetVisible(browsing_mode);

  // Exit prompt.
  bool showExitPrompt = browsing_mode && prompting_to_exit_;
  exit_prompt_->SetVisible(showExitPrompt);
  exit_prompt_backplane_->SetVisible(showExitPrompt);

  // Update content quad parameters depending on fullscreen.
  // TODO(http://crbug.com/642937): Animate fullscreen transitions.
  if (fullscreen_) {
    scene_->GetUiElementByName(k2dBrowsingContentGroup)
        ->SetTranslate(0, kFullscreenVerticalOffset, -kFullscreenDistance);
    main_content_->SetSize(kFullscreenWidth, kFullscreenHeight);
    close_button_->SetTranslate(
        0, kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35,
        -kCloseButtonFullscreenDistance);
    close_button_->SetSize(kCloseButtonFullscreenWidth,
                           kCloseButtonFullscreenHeight);
  } else {
    // Note that main_content_ is already visible in this case.
    scene_->GetUiElementByName(k2dBrowsingContentGroup)
        ->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
    main_content_->SetSize(kContentWidth, kContentHeight);
    close_button_->SetTranslate(
        0, kContentVerticalOffset - (kContentHeight / 2) - 0.3,
        -kCloseButtonDistance);
    close_button_->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  }
  scene_->set_background_distance(kContentDistance *
                                  kBackgroundDistanceMultiplier);

  scene_->root_element().SetMode(mode());

  webvr_url_toast_->SetVisible(started_for_autopresentation_ &&
                               !showing_web_vr_splash_screen_);

  scene_->set_reticle_rendering_enabled(
      !(web_vr_mode_ || exiting_ || showing_web_vr_splash_screen_));

  ConfigureExclusiveScreenToast();
  ConfigureSecurityWarnings();
  ConfigureIndicators();
  ConfigureBackgroundColor();
}

void UiSceneManager::ConfigureBackgroundColor() {
  for (Rect* panel : background_panels_) {
    panel->SetCenterColor(color_scheme().world_background);
    panel->SetEdgeColor(color_scheme().world_background);
  }
  ceiling_->SetCenterColor(color_scheme().ceiling);
  ceiling_->SetEdgeColor(color_scheme().world_background);
  floor_->SetCenterColor(color_scheme().floor);
  floor_->SetEdgeColor(color_scheme().world_background);
  floor_->SetGridColor(color_scheme().floor_grid);
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
  // App button clicks should be a no-op when auto-presenting WebVR.
  if (started_for_autopresentation_)
    return;

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
  permanent_security_warning_->SetVisible(enabled);
  transient_security_warning_->SetVisible(enabled);
}

void UiSceneManager::ConfigureIndicators() {
  bool allowed = !web_vr_mode_ && !fullscreen_;
  audio_capture_indicator_->SetVisible(allowed && audio_capturing_);
  video_capture_indicator_->SetVisible(allowed && video_capturing_);
  screen_capture_indicator_->SetVisible(allowed && screen_capturing_);
  location_access_indicator_->SetVisible(allowed && location_access_);
  bluetooth_connected_indicator_->SetVisible(allowed && bluetooth_connected_);

  audio_capture_indicator_->set_requires_layout(allowed && audio_capturing_);
  video_capture_indicator_->set_requires_layout(allowed && video_capturing_);
  screen_capture_indicator_->set_requires_layout(allowed && screen_capturing_);
  location_access_indicator_->set_requires_layout(allowed && location_access_);
  bluetooth_connected_indicator_->set_requires_layout(allowed &&
                                                      bluetooth_connected_);
}

void UiSceneManager::ConfigureExclusiveScreenToast() {
  exclusive_screen_toast_->SetVisible(fullscreen_ && !web_vr_mode_);
  exclusive_screen_toast_viewport_aware_->SetVisible(web_vr_mode_ &&
                                                     web_vr_show_toast_);
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
  webvr_url_toast_->SetToolbarState(state);
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
