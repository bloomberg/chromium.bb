// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/exclusive_screen_toast.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/exit_prompt_backplane.h"
#include "chrome/browser/vr/elements/full_screen_rect.h"
#include "chrome/browser/vr/elements/grid.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/spinner.h"
#include "chrome/browser/vr/elements/system_indicator.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/throbber.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_element_transform_operations.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/elements/url_bar.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/elements/vector_icon_button_texture.h"
#include "chrome/browser/vr/elements/viewport_aware_root.h"
#include "chrome/browser/vr/elements/webvr_url_toast.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/transform_util.h"

namespace vr {

namespace {

template <typename P>
void BindColor(UiSceneManager* model, Rect* rect, P p) {
  rect->AddBinding(base::MakeUnique<Binding<SkColor>>(
      base::Bind([](UiSceneManager* m, P p) { return (m->color_scheme()).*p; },
                 base::Unretained(model), p),
      base::Bind([](Rect* r, const SkColor& c) { r->SetColor(c); },
                 base::Unretained(rect))));
}

template <typename P>
void BindColor(UiSceneManager* model, Text* text, P p) {
  text->AddBinding(base::MakeUnique<Binding<SkColor>>(
      base::Bind([](UiSceneManager* m, P p) { return (m->color_scheme()).*p; },
                 base::Unretained(model), p),
      base::Bind([](Text* t, const SkColor& c) { t->SetColor(c); },
                 base::Unretained(text))));
}

}  // namespace

UiSceneManager::UiSceneManager(UiBrowserInterface* browser,
                               UiScene* scene,
                               ContentInputDelegate* content_input_delegate,
                               Model* model,
                               const UiInitialState& ui_initial_state)
    : browser_(browser),
      scene_(scene),
      in_cct_(ui_initial_state.in_cct),
      web_vr_mode_(ui_initial_state.in_web_vr),
      started_for_autopresentation_(
          ui_initial_state.web_vr_autopresentation_expected),
      showing_web_vr_splash_screen_(
          ui_initial_state.web_vr_autopresentation_expected),
      browsing_disabled_(ui_initial_state.browsing_disabled) {
  Create2dBrowsingSubtreeRoots(model);
  CreateWebVrRoot();
  CreateBackground();
  CreateViewportAwareRoot();
  CreateContentQuad(content_input_delegate);
  CreateWebVRExitWarning();
  CreateSystemIndicators();
  CreateUrlBar(model);
  CreateWebVrUrlToast();
  CreateCloseButton();
  CreateScreenDimmer();
  CreateExitPrompt();
  CreateToasts(model);
  CreateSplashScreen(model);
  CreateUnderDevelopmentNotice();
  CreateVoiceSearchUiGroup(model);

  ConfigureScene();
}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::Create2dBrowsingSubtreeRoots(Model* model) {
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
  element->set_hit_testable(false);
  element->AddBinding(VR_BIND(bool, Model, model, recognizing_speech, UiElement,
                              element.get(), SetVisible(!value)));
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

void UiSceneManager::CreateWebVRExitWarning() {
  std::unique_ptr<UiElement> element;

  // TODO(mthiesse): Programatically compute the proper texture size for these
  // textured UI elements.
  // Create transient exit warning.
  element = base::MakeUnique<ExitWarning>(1024);
  element->set_name(kExitWarning);
  element->set_draw_phase(kPhaseOverlayForeground);
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
}

void UiSceneManager::CreateContentQuad(ContentInputDelegate* delegate) {
  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  auto hit_plane = base::MakeUnique<InvisibleHitTarget>();
  hit_plane->set_name(kBackplane);
  hit_plane->set_draw_phase(kPhaseForeground);
  hit_plane->SetSize(kBackplaneSize, kBackplaneSize);
  hit_plane->SetTranslate(0, 0, -kTextureOffset);
  content_elements_.push_back(hit_plane.get());
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(hit_plane));

  auto main_content = base::MakeUnique<ContentElement>(delegate);
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

void UiSceneManager::CreateSplashScreen(Model* model) {
  auto element = base::MakeUnique<UiElement>();
  element->set_name(kSplashScreenRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(kRoot, std::move(element));

  // Create viewport aware root.
  element = base::MakeUnique<ViewportAwareRoot>();
  element->set_name(kSplashScreenViewportAwareRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(kSplashScreenRoot, std::move(element));

  // Create transient parent.
  // TODO(crbug.com/762074): We should timeout after some time and show an
  // error if the user is stuck on the splash screen.
  auto transient_parent = base::MakeUnique<ShowUntilSignalTransientElement>(
      base::TimeDelta::FromSeconds(kSplashScreenMinDurationSeconds),
      base::TimeDelta::Max(),
      base::Bind(&UiSceneManager::OnSplashScreenHidden,
                 base::Unretained(this)));
  transient_parent->set_name(kSplashScreenTransientParent);
  transient_parent->SetVisible(started_for_autopresentation_);
  transient_parent->set_hit_testable(false);
  transient_parent->SetTransitionedProperties({OPACITY});
  splash_screen_transient_parent_ = transient_parent.get();
  scene_->AddUiElement(kSplashScreenViewportAwareRoot,
                       std::move(transient_parent));

  // Add "Powered by Chrome" text.
  auto text = base::MakeUnique<Text>(512, kSplashScreenTextFontHeightM,
                                     kSplashScreenTextWidthM);
  BindColor(this, text.get(), &ColorScheme::splash_screen_text_color);
  text->SetText(l10n_util::GetStringUTF16(IDS_VR_POWERED_BY_CHROME_MESSAGE));
  text->set_name(kSplashScreenText);
  text->SetVisible(true);
  text->set_draw_phase(kPhaseOverlayForeground);
  text->set_hit_testable(false);
  text->SetSize(kSplashScreenTextWidthM, kSplashScreenTextHeightM);
  text->SetTranslate(0, kSplashScreenTextVerticalOffset,
                     -kSplashScreenTextDistance);
  scene_->AddUiElement(kSplashScreenTransientParent, std::move(text));

  // Add splash screen background.
  auto bg = base::MakeUnique<FullScreenRect>();
  bg->set_name(kSplashScreenBackground);
  bg->set_draw_phase(kPhaseOverlayBackground);
  bg->SetVisible(true);
  bg->set_hit_testable(false);
  bg->SetColor(color_scheme().splash_screen_background);
  scene_->AddUiElement(kSplashScreenText, std::move(bg));

  auto spinner = base::MakeUnique<Spinner>(512);
  spinner->set_name(kWebVrTimeoutSpinner);
  spinner->set_draw_phase(kPhaseOverlayForeground);
  spinner->SetSize(kSpinnerWidth, kSpinnerHeight);
  spinner->SetTranslate(0, kSpinnerVerticalOffset, -kSpinnerDistance);
  spinner->SetColor(color_scheme().spinner_color);
  spinner->AddBinding(VR_BIND_FUNC(
      bool, Model, model, web_vr_timeout_state == kWebVrTimeoutImminent,
      Spinner, spinner.get(), SetVisible));
  spinner->SetTransitionedProperties({OPACITY});
  scene_->AddUiElement(kSplashScreenViewportAwareRoot, std::move(spinner));

  // Note, this cannot be a descendant of the viewport aware root, otherwise it
  // will fade out when the viewport aware elements reposition.
  auto spinner_bg = base::MakeUnique<FullScreenRect>();
  spinner_bg->set_name(kWebVrTimeoutSpinnerBackground);
  spinner_bg->set_draw_phase(kPhaseOverlayBackground);
  spinner_bg->set_hit_testable(false);
  spinner_bg->SetColor(color_scheme().spinner_background);
  spinner_bg->SetTransitionedProperties({OPACITY});
  spinner_bg->SetTransitionDuration(base::TimeDelta::FromMilliseconds(200));
  spinner_bg->AddBinding(VR_BIND_FUNC(
      bool, Model, model, web_vr_timeout_state != kWebVrNoTimeoutPending,
      FullScreenRect, spinner_bg.get(), SetVisible));
  scene_->AddUiElement(kSplashScreenRoot, std::move(spinner_bg));

  auto timeout_message = base::MakeUnique<Rect>();
  timeout_message->set_name(kWebVrTimeoutMessage);
  timeout_message->set_draw_phase(kPhaseOverlayForeground);
  timeout_message->SetSize(kTimeoutMessageBackgroundWidthM,
                           kTimeoutMessageBackgroundHeightM);
  timeout_message->SetTranslate(0, kSpinnerVerticalOffset, -kSpinnerDistance);
  timeout_message->set_corner_radius(kTimeoutMessageCornerRadius);
  timeout_message->SetTransitionedProperties({OPACITY});
  timeout_message->AddBinding(
      VR_BIND_FUNC(bool, Model, model, web_vr_timeout_state == kWebVrTimedOut,
                   Rect, timeout_message.get(), SetVisible));
  timeout_message->SetColor(color_scheme().timeout_message_background);
  scene_->AddUiElement(kSplashScreenViewportAwareRoot,
                       std::move(timeout_message));

  auto timeout_layout =
      base::MakeUnique<LinearLayout>(LinearLayout::kHorizontal);
  timeout_layout->set_name(kWebVrTimeoutMessageLayout);
  timeout_layout->set_hit_testable(false);
  timeout_layout->SetVisible(true);
  timeout_layout->set_margin(kTimeoutMessageLayoutGap);
  scene_->AddUiElement(kWebVrTimeoutMessage, std::move(timeout_layout));

  auto timeout_icon = base::MakeUnique<VectorIcon>(512);
  timeout_icon->SetIcon(kSadTabIcon);
  timeout_icon->set_name(kWebVrTimeoutMessageIcon);
  timeout_icon->set_draw_phase(kPhaseOverlayForeground);
  timeout_icon->SetVisible(true);
  timeout_icon->SetSize(kTimeoutMessageIconWidth, kTimeoutMessageIconHeight);
  scene_->AddUiElement(kWebVrTimeoutMessageLayout, std::move(timeout_icon));

  auto timeout_text = base::MakeUnique<Text>(
      512, kTimeoutMessageTextFontHeightM, kTimeoutMessageTextWidthM);
  timeout_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_TIMEOUT_MESSAGE));
  timeout_text->SetColor(color_scheme().timeout_message_foreground);
  timeout_text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  timeout_text->set_name(kWebVrTimeoutMessageText);
  timeout_text->set_draw_phase(kPhaseOverlayForeground);
  timeout_text->SetVisible(true);
  timeout_text->SetSize(kTimeoutMessageTextWidthM, kTimeoutMessageTextHeightM);
  scene_->AddUiElement(kWebVrTimeoutMessageLayout, std::move(timeout_text));

  auto button = base::MakeUnique<Button>(
      base::Bind(&UiSceneManager::OnWebVrTimedOut, base::Unretained(this)),
      base::MakeUnique<VectorIconButtonTexture>(vector_icons::kClose16Icon));
  button->set_name(kWebVrTimeoutMessageButton);
  button->set_draw_phase(kPhaseOverlayForeground);
  button->SetTranslate(0, kTimeoutButtonVerticalOffset,
                       -kTimeoutButtonDistance);
  button->SetRotate(1, 0, 0, kTimeoutButtonRotationRad);
  button->SetSize(kTimeoutButtonWidth, kTimeoutButtonHeight);
  button->SetTransitionedProperties({OPACITY});
  button->AddBinding(VR_BIND_FUNC(bool, Model, model,
                                  web_vr_timeout_state == kWebVrTimedOut,
                                  Button, button.get(), SetVisible));
  scene_->AddUiElement(kSplashScreenViewportAwareRoot, std::move(button));

  timeout_text = base::MakeUnique<Text>(512, kTimeoutMessageTextFontHeightM,
                                        kTimeoutButtonTextWidth);
  timeout_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_EXIT_BUTTON_LABEL));
  timeout_text->set_name(kWebVrTimeoutMessageButtonText);
  timeout_text->SetColor(color_scheme().spinner_color);
  timeout_text->set_draw_phase(kPhaseOverlayForeground);
  timeout_text->SetVisible(true);
  timeout_text->SetSize(kTimeoutButtonTextWidth, kTimeoutButtonTextHeight);
  timeout_text->set_y_anchoring(YBOTTOM);
  timeout_text->SetTranslate(0, -kTimeoutButtonTextVerticalOffset, 0);
  scene_->AddUiElement(kWebVrTimeoutMessageButton, std::move(timeout_text));
}

void UiSceneManager::CreateUnderDevelopmentNotice() {
  auto text = base::MakeUnique<Text>(512, kUnderDevelopmentNoticeFontHeightM,
                                     kUnderDevelopmentNoticeWidthM);
  BindColor(this, text.get(), &ColorScheme::world_background_text);
  text->SetText(l10n_util::GetStringUTF16(IDS_VR_UNDER_DEVELOPMENT_NOTICE));
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
                             base::kPiFloat / 2 * panel.angle);
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
  floor->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  floor->set_gridline_count(kFloorGridlineCount);
  floor_ = floor.get();
  scene_->AddUiElement(k2dBrowsingBackground, std::move(floor));

  // Ceiling.
  auto ceiling = base::MakeUnique<Rect>();
  ceiling->set_name(kCeiling);
  ceiling->set_draw_phase(kPhaseFloorCeiling);
  ceiling->SetSize(kSceneSize, kSceneSize);
  ceiling->SetTranslate(0.0, kSceneHeight / 2, 0.0);
  ceiling->SetRotate(1, 0, 0, base::kPiFloat / 2);
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

void UiSceneManager::CreateVoiceSearchUiGroup(Model* model) {
  std::unique_ptr<UiElement> element;

  auto voice_search_button = base::MakeUnique<Button>(
      base::Bind(&UiSceneManager::OnVoiceSearchButtonClicked,
                 base::Unretained(this)),
      base::MakeUnique<VectorIconButtonTexture>(vector_icons::kMicrophoneIcon));
  voice_search_button_ = voice_search_button.get();
  element = std::move(voice_search_button);
  element->set_name(kVoiceSearchButton);
  element->set_draw_phase(kPhaseForeground);
  element->SetTranslate(kVoiceSearchButtonXOffset, 0.f, 0.f);
  element->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  element->set_x_anchoring(XAnchoring::XRIGHT);
  control_elements_.push_back(element.get());
  scene_->AddUiElement(kUrlBar, std::move(element));

  auto speech_recognition_prompt = base::MakeUnique<UiElement>();
  element = std::move(speech_recognition_prompt);
  element->set_name(kSpeechRecognitionPrompt);
  element->SetTranslate(0.f, 0.f, -kContentDistance);
  element->set_hit_testable(false);
  element->AddBinding(VR_BIND_FUNC(bool, Model, model, recognizing_speech,
                                   UiElement, element.get(), SetVisible));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  auto growing_circle = base::MakeUnique<Throbber>();
  growing_circle->set_name(kSpeechRecognitionPromptGrowingCircle);
  growing_circle->set_draw_phase(kPhaseForeground);
  growing_circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  growing_circle->set_corner_radius(kCloseButtonWidth);
  growing_circle->SetTranslate(0.0, 0.0, -kTextureOffset * 2);
  growing_circle->set_hit_testable(false);
  BindColor(this, growing_circle.get(),
            &ColorScheme::speech_recognition_circle_background);
  growing_circle->AddBinding(VR_BIND(
      int, Model, model, speech_recognition_state, Throbber,
      growing_circle.get(),
      SetCircleGrowAnimationEnabled(value == SPEECH_RECOGNITION_IN_SPEECH ||
                                    value == SPEECH_RECOGNITION_RECOGNIZING ||
                                    value == SPEECH_RECOGNITION_READY)));
  scene_->AddUiElement(kSpeechRecognitionPrompt, std::move(growing_circle));

  auto inner_circle = base::MakeUnique<Rect>();
  inner_circle->set_name(kSpeechRecognitionPromptInnerCircle);
  inner_circle->set_draw_phase(kPhaseForeground);
  inner_circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  inner_circle->set_corner_radius(kCloseButtonWidth);
  inner_circle->SetTranslate(0.0, 0.0, -kTextureOffset);
  inner_circle->set_hit_testable(false);
  BindColor(this, inner_circle.get(),
            &ColorScheme::speech_recognition_circle_background);
  scene_->AddUiElement(kSpeechRecognitionPrompt, std::move(inner_circle));

  auto microphone_icon = base::MakeUnique<VectorIcon>(512);
  microphone_icon->SetIcon(vector_icons::kMicrophoneIcon);
  microphone_icon->set_name(kSpeechRecognitionPromptMicrophoneIcon);
  microphone_icon->set_draw_phase(kPhaseForeground);
  microphone_icon->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  scene_->AddUiElement(kSpeechRecognitionPrompt, std::move(microphone_icon));

  auto backplane = base::MakeUnique<ExitPromptBackplane>(base::Bind(
      &UiSceneManager::OnExitRecognizingSpeechClicked, base::Unretained(this)));
  speech_recognition_prompt_backplane_ = backplane.get();
  element = std::move(backplane);
  element->set_name(kSpeechRecognitionPromptBackplane);
  element->set_draw_phase(kPhaseForeground);
  element->SetSize(kExitPromptBackplaneSize, kExitPromptBackplaneSize);
  element->SetTranslate(0.0, 0.0, -kTextureOffset);
  scene_->AddUiElement(kSpeechRecognitionPrompt, std::move(element));
}

void UiSceneManager::CreateUrlBar(Model* model) {
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

  auto indicator_bg = base::MakeUnique<Rect>();
  indicator_bg->set_name(kLoadingIndicator);
  indicator_bg->set_draw_phase(kPhaseForeground);
  indicator_bg->SetTranslate(0, kLoadingIndicatorVerticalOffset,
                             kLoadingIndicatorDepthOffset);
  indicator_bg->SetSize(kLoadingIndicatorWidth, kLoadingIndicatorHeight);
  indicator_bg->set_y_anchoring(YAnchoring::YTOP);
  indicator_bg->SetTransitionedProperties({OPACITY});
  indicator_bg->set_corner_radius(kLoadingIndicatorHeight * 0.5f);
  indicator_bg->AddBinding(VR_BIND_FUNC(bool, Model, model, loading, Rect,
                                        indicator_bg.get(), SetVisible));
  BindColor(this, indicator_bg.get(),
            &ColorScheme::loading_indicator_background);

  scene_->AddUiElement(kUrlBar, std::move(indicator_bg));

  auto indicator_fg = base::MakeUnique<Rect>();
  indicator_fg->set_draw_phase(kPhaseForeground);
  indicator_fg->set_name(kLoadingIndicatorForeground);
  indicator_fg->set_x_anchoring(XLEFT);
  indicator_fg->set_corner_radius(kLoadingIndicatorHeight * 0.5f);
  indicator_fg->set_hit_testable(false);
  BindColor(this, indicator_fg.get(),
            &ColorScheme::loading_indicator_foreground);
  indicator_fg->AddBinding(base::MakeUnique<Binding<float>>(
      base::Bind([](Model* m) { return m->load_progress; },
                 base::Unretained(model)),
      base::Bind(
          [](Rect* r, const float& value) {
            r->SetSize(kLoadingIndicatorWidth * value, kLoadingIndicatorHeight);
            r->SetTranslate(kLoadingIndicatorWidth * value * 0.5f, 0.0f,
                            0.001f);
          },
          base::Unretained(indicator_fg.get()))));
  scene_->AddUiElement(kLoadingIndicator, std::move(indicator_fg));
}

TransientElement* UiSceneManager::AddTransientParent(UiElementName name,
                                                     UiElementName parent_name,
                                                     int timeout_seconds,
                                                     bool animate_opacity) {
  auto element = base::MakeUnique<SimpleTransientElement>(
      base::TimeDelta::FromSeconds(timeout_seconds));
  TransientElement* to_return = element.get();
  element->set_name(name);
  element->SetVisible(false);
  element->set_hit_testable(false);
  if (animate_opacity)
    element->SetTransitionedProperties({OPACITY});
  scene_->AddUiElement(parent_name, std::move(element));
  return to_return;
}

void UiSceneManager::CreateWebVrUrlToast() {
  webvr_url_toast_transient_parent_ =
      AddTransientParent(kWebVrUrlToastTransientParent, kWebVrViewportAwareRoot,
                         kWebVrUrlToastTimeoutSeconds, true);
  auto url_bar = base::MakeUnique<WebVrUrlToast>(
      512,
      base::Bind(&UiSceneManager::OnUnsupportedMode, base::Unretained(this)));
  url_bar->set_name(kWebVrUrlToast);
  url_bar->set_opacity_when_visible(0.8f);
  url_bar->set_draw_phase(kPhaseOverlayForeground);
  url_bar->SetVisible(true);
  url_bar->set_hit_testable(false);
  url_bar->SetTranslate(0, kWebVrToastDistance * sin(kWebVrUrlToastRotationRad),
                        -kWebVrToastDistance * cos(kWebVrUrlToastRotationRad));
  url_bar->SetRotate(1, 0, 0, kWebVrUrlToastRotationRad);
  url_bar->SetSize(kWebVrUrlToastWidth, kWebVrUrlToastHeight);
  webvr_url_toast_ = url_bar.get();
  scene_->AddUiElement(kWebVrUrlToastTransientParent, std::move(url_bar));
}

void UiSceneManager::CreateCloseButton() {
  std::unique_ptr<Button> element = base::MakeUnique<Button>(
      base::Bind(&UiSceneManager::OnCloseButtonClicked, base::Unretained(this)),
      base::MakeUnique<VectorIconButtonTexture>(vector_icons::kClose16Icon));
  element->set_name(kCloseButton);
  element->set_draw_phase(kPhaseForeground);
  element->SetTranslate(0, kContentVerticalOffset - (kContentHeight / 2) - 0.3f,
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
  element->set_draw_phase(kPhaseForeground);
  element->SetSize(kExitPromptBackplaneSize, kExitPromptBackplaneSize);
  element->SetTranslate(0.0, 0.0, -kTextureOffset);
  exit_prompt_backplane_ = element.get();
  content_elements_.push_back(element.get());
  scene_->AddUiElement(kExitPrompt, std::move(element));
}

void UiSceneManager::CreateToasts(Model* model) {
  // Create fullscreen toast.
  exclusive_screen_toast_transient_parent_ =
      AddTransientParent(kExclusiveScreenToastTransientParent,
                         k2dBrowsingForeground, kToastTimeoutSeconds, false);
  auto element = base::MakeUnique<ExclusiveScreenToast>(512);
  element->set_name(kExclusiveScreenToast);
  element->set_draw_phase(kPhaseForeground);
  element->SetSize(kToastWidthDMM, kToastHeightDMM);
  element->SetTranslate(
      0,
      kFullscreenVerticalOffset + kFullscreenHeight / 2 +
          (kToastOffsetDMM + kToastHeightDMM) * kFullscreenToastDistance,
      -kFullscreenToastDistance);
  element->SetScale(kFullscreenToastDistance, kFullscreenToastDistance, 1);
  element->set_hit_testable(false);
  scene_->AddUiElement(kExclusiveScreenToastTransientParent,
                       std::move(element));

  // Create WebVR toast.
  exclusive_screen_toast_viewport_aware_transient_parent_ =
      AddTransientParent(kExclusiveScreenToastViewportAwareTransientParent,
                         kWebVrViewportAwareRoot, kToastTimeoutSeconds, false);
  element = base::MakeUnique<ExclusiveScreenToast>(512);
  element->set_name(kExclusiveScreenToastViewportAware);
  element->set_draw_phase(kPhaseOverlayForeground);
  element->SetSize(kToastWidthDMM, kToastHeightDMM);
  element->SetTranslate(0, kWebVrToastDistance * sin(kWebVrAngleRadians),
                        -kWebVrToastDistance * cos(kWebVrAngleRadians));
  element->SetRotate(1, 0, 0, kWebVrAngleRadians);
  element->SetScale(kWebVrToastDistance, kWebVrToastDistance, 1);
  element->set_hit_testable(false);

  // When we first get a web vr frame, we switch states to
  // kWebVrNoTimeoutPending, when that happens, we want to SetVisible(true) to
  // kick the visibility of this element.
  element->AddBinding(base::MakeUnique<Binding<bool>>(
      base::Bind(
          [](Model* m) {
            return m->web_vr_timeout_state == kWebVrNoTimeoutPending;
          },
          base::Unretained(model)),
      base::Bind(
          [](UiElement* v, const bool& b) {
            if (b)
              v->SetVisible(b);
          },
          base::Unretained(
              exclusive_screen_toast_viewport_aware_transient_parent_))));

  scene_->AddUiElement(kExclusiveScreenToastViewportAwareTransientParent,
                       std::move(element));
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
    exclusive_screen_toast_viewport_aware_transient_parent_->RefreshVisible();
  } else {
    exclusive_screen_toast_transient_parent_->SetVisible(false);
  }
}

void UiSceneManager::OnWebVrFrameAvailable() {
  if (!showing_web_vr_splash_screen_)
    return;

  splash_screen_transient_parent_->Signal();
}

void UiSceneManager::OnWebVrTimedOut() {
  browser_->ExitPresent();
}

void UiSceneManager::OnSplashScreenHidden(TransientElementHideReason reason) {
  showing_web_vr_splash_screen_ = false;
  if (reason == TransientElementHideReason::kTimeout) {
    OnWebVrTimedOut();
    return;
  }
  ConfigureScene();
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

  gfx::SizeF main_content_size = main_content_->GetTargetSize();
  // We take the target transform in case the content quad's parent's translate
  // is animated. This approach only works with the current scene hierarchy and
  // set of animated properties.
  // TODO(crbug.com/766318): Find a way to get the target value of the
  // inheritable transfrom that works with any scene hierarchy and set of
  // animated properties.
  gfx::Transform main_content_transform =
      scene_->GetUiElementByName(k2dBrowsingContentGroup)
          ->GetTargetTransform()
          .Apply();
  gfx::SizeF screen_size = CalculateScreenSize(
      proj_matrix, main_content_transform, main_content_size);

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
  // Everything we do to configure scenes here should eventually be moved to
  // bindings.
  base::AutoReset<bool> configuring(&configuring_scene_, true);

  // We disable WebVR rendering if we're expecting to auto present so that we
  // can continue to show the 2D splash screen while the site submits the first
  // WebVR frame.
  bool showing_web_vr_content = web_vr_mode_ && !showing_web_vr_splash_screen_;
  scene_->set_web_vr_rendering_enabled(showing_web_vr_content);

  // Exit warning.
  exit_warning_->SetVisible(exiting_);
  screen_dimmer_->SetVisible(exiting_);

  bool browsing_mode = !web_vr_mode_ && !showing_web_vr_splash_screen_;
  scene_->GetUiElementByName(k2dBrowsingRoot)->SetVisible(browsing_mode);
  scene_->GetUiElementByName(kWebVrRoot)->SetVisible(!browsing_mode);

  // Controls (URL bar, loading progress, voice search button etc).
  bool controls_visible = browsing_mode && !fullscreen_ && !prompting_to_exit_;
  for (UiElement* element : control_elements_) {
    element->SetVisible(controls_visible);
  }

  if (!base::FeatureList::IsEnabled(features::kExperimentalVRFeatures)) {
    voice_search_button_->SetVisible(false);
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
        0, kFullscreenVerticalOffset - (kFullscreenHeight / 2) - 0.35f,
        -kCloseButtonFullscreenDistance);
    close_button_->SetSize(kCloseButtonFullscreenWidth,
                           kCloseButtonFullscreenHeight);
  } else {
    // Note that main_content_ is already visible in this case.
    scene_->GetUiElementByName(k2dBrowsingContentGroup)
        ->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
    main_content_->SetSize(kContentWidth, kContentHeight);
    close_button_->SetTranslate(
        0, kContentVerticalOffset - (kContentHeight / 2) - 0.3f,
        -kCloseButtonDistance);
    close_button_->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  }
  scene_->set_background_distance(kContentDistance *
                                  kBackgroundDistanceMultiplier);

  scene_->root_element().SetMode(mode());

  webvr_url_toast_transient_parent_->SetVisible(started_for_autopresentation_ &&
                                                !showing_web_vr_splash_screen_);

  scene_->set_reticle_rendering_enabled(
      !(web_vr_mode_ || exiting_ || showing_web_vr_splash_screen_));

  ConfigureExclusiveScreenToast();
  ConfigureIndicators();
  ConfigureBackgroundColor();
  scene_->set_dirty();
}

void UiSceneManager::ConfigureExclusiveScreenToast() {
  DCHECK(configuring_scene_);
  exclusive_screen_toast_transient_parent_->SetVisible(fullscreen_ &&
                                                       !web_vr_mode_);
  exclusive_screen_toast_viewport_aware_transient_parent_->SetVisible(
      web_vr_mode_ && web_vr_show_toast_);
}

void UiSceneManager::ConfigureIndicators() {
  DCHECK(configuring_scene_);
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

void UiSceneManager::ConfigureBackgroundColor() {
  DCHECK(configuring_scene_);
  for (Rect* panel : background_panels_) {
    panel->SetColor(color_scheme().world_background);
  }
  ceiling_->SetCenterColor(color_scheme().ceiling);
  ceiling_->SetEdgeColor(color_scheme().world_background);
  floor_->SetCenterColor(color_scheme().floor);
  floor_->SetEdgeColor(color_scheme().world_background);
  floor_->SetGridColor(color_scheme().floor_grid);
}

void UiSceneManager::SetAudioCapturingIndicator(bool enabled) {
  audio_capturing_ = enabled;
  ConfigureScene();
}

void UiSceneManager::SetVideoCapturingIndicator(bool enabled) {
  video_capturing_ = enabled;
  ConfigureScene();
}

void UiSceneManager::SetScreenCapturingIndicator(bool enabled) {
  screen_capturing_ = enabled;
  ConfigureScene();
}

void UiSceneManager::SetLocationAccessIndicator(bool enabled) {
  location_access_ = enabled;
  ConfigureScene();
}

void UiSceneManager::SetBluetoothConnectedIndicator(bool enabled) {
  bluetooth_connected_ = enabled;
  ConfigureScene();
}

void UiSceneManager::SetIncognito(bool incognito) {
  if (incognito == incognito_)
    return;
  incognito_ = incognito;
  ConfigureScene();
}

bool UiSceneManager::ShouldRenderWebVr() {
  return scene_->web_vr_rendering_enabled();
}

void UiSceneManager::OnGlInitialized(
    unsigned int content_texture_id,
    UiElementRenderer::TextureLocation content_location) {
  main_content_->SetTexture(content_texture_id, content_location);
  scene_->OnGlInitialized();

  ConfigureScene();
}

void UiSceneManager::OnAppButtonClicked() {
  // App button clicks should be a no-op when auto-presenting WebVR.
  if (started_for_autopresentation_)
    return;

  // If browsing mode is disabled, the app button should no-op.
  if (browsing_disabled_)
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

void UiSceneManager::OnExitRecognizingSpeechClicked() {
  browser_->SetVoiceSearchActive(false);
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

void UiSceneManager::OnVoiceSearchButtonClicked() {
  browser_->SetVoiceSearchActive(true);
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
