// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/databinding/vector_binding.h"
#include "chrome/browser/vr/elements/audio_permission_prompt.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/controller.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/full_screen_rect.h"
#include "chrome/browser/vr/elements/grid.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/laser.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/elements/simple_textured_element.h"
#include "chrome/browser/vr/elements/spinner.h"
#include "chrome/browser/vr/elements/suggestion.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/throbber.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/elements/url_bar.h"
#include "chrome/browser/vr/elements/vector_icon.h"
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
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/transform_util.h"

namespace vr {

namespace {

template <typename V, typename C, typename S>
void BindColor(Model* model, V* view, C color, S setter) {
  view->AddBinding(base::MakeUnique<Binding<SkColor>>(
      base::Bind([](Model* m, C c) { return (m->color_scheme()).*c; },
                 base::Unretained(model), color),
      base::Bind([](V* v, S s, const SkColor& value) { (v->*s)(value); },
                 base::Unretained(view), setter)));
}

template <typename V, typename C, typename S>
void BindButtonColors(Model* model, V* view, C colors, S setter) {
  view->AddBinding(base::MakeUnique<Binding<ButtonColors>>(
      base::Bind([](Model* m, C c) { return (m->color_scheme()).*c; },
                 base::Unretained(model), colors),
      base::Bind([](V* v, S s, const ButtonColors& value) { (v->*s)(value); },
                 base::Unretained(view), setter)));
}

typedef VectorBinding<OmniboxSuggestion, Suggestion> SuggestionSetBinding;
typedef typename SuggestionSetBinding::ElementBinding SuggestionBinding;

void OnSuggestionModelAdded(UiScene* scene,
                            UiBrowserInterface* browser,
                            Model* model,
                            SuggestionBinding* element_binding) {
  auto icon = base::MakeUnique<VectorIcon>(100);
  icon->SetVisible(true);
  icon->SetSize(kSuggestionIconSize, kSuggestionIconSize);
  icon->set_draw_phase(kPhaseForeground);
  VectorIcon* p_icon = icon.get();

  auto content_text = base::MakeUnique<Text>(512, kSuggestionContentTextHeight,
                                             kSuggestionTextFieldWidth);
  content_text->set_draw_phase(kPhaseForeground);
  content_text->SetVisible(true);
  content_text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  Text* p_content_text = content_text.get();

  auto description_text = base::MakeUnique<Text>(
      512, kSuggestionDescriptionTextHeight, kSuggestionTextFieldWidth);
  description_text->set_draw_phase(kPhaseForeground);
  description_text->SetVisible(true);
  description_text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  Text* p_description_text = description_text.get();

  auto text_layout = base::MakeUnique<LinearLayout>(LinearLayout::kDown);
  text_layout->set_margin(kSuggestionLineGap);
  text_layout->AddChild(std::move(content_text));
  text_layout->AddChild(std::move(description_text));
  text_layout->SetVisible(true);

  auto suggestion = base::MakeUnique<Suggestion>(base::Bind(
      [](UiBrowserInterface* browser, Model* m, GURL gurl) {
        browser->Navigate(gurl);
        m->omnibox_input_active = false;
      },
      base::Unretained(browser), base::Unretained(model)));

  suggestion->set_margin(kSuggestionIconGap);
  suggestion->SetVisible(true);
  suggestion->AddChild(std::move(icon));
  suggestion->AddChild(std::move(text_layout));
  Suggestion* p_suggestion = suggestion.get();

  element_binding->set_view(suggestion.get());

  element_binding->bindings().push_back(
      VR_BIND_FUNC(base::string16, SuggestionBinding, element_binding,
                   model()->content, Text, p_content_text, SetText));
  element_binding->bindings().push_back(
      VR_BIND_FUNC(base::string16, SuggestionBinding, element_binding,
                   model()->description, Text, p_description_text, SetText));
  element_binding->bindings().push_back(
      VR_BIND(AutocompleteMatch::Type, SuggestionBinding, element_binding,
              model()->type, VectorIcon, p_icon,
              SetIcon(AutocompleteMatch::TypeToVectorIcon(value))));
  element_binding->bindings().push_back(VR_BIND_FUNC(
      GURL, SuggestionBinding, element_binding, model()->destination,
      Suggestion, p_suggestion, set_destination));

  scene->AddUiElement(kSuggestionLayout, std::move(suggestion));
}

void OnSuggestionModelRemoved(UiScene* scene, SuggestionBinding* binding) {
  scene->RemoveUiElement(binding->view()->id());
}

TransientElement* AddTransientParent(UiElementName name,
                                     UiElementName parent_name,
                                     int timeout_seconds,
                                     bool animate_opacity,
                                     UiScene* scene) {
  auto element = base::MakeUnique<SimpleTransientElement>(
      base::TimeDelta::FromSeconds(timeout_seconds));
  TransientElement* to_return = element.get();
  element->set_name(name);
  element->SetVisible(false);
  element->set_hit_testable(false);
  if (animate_opacity)
    element->SetTransitionedProperties({OPACITY});
  scene->AddUiElement(parent_name, std::move(element));
  return to_return;
}

}  // namespace

UiSceneManager::UiSceneManager(UiBrowserInterface* browser,
                               UiScene* scene,
                               ContentInputDelegate* content_input_delegate,
                               Model* model)
    : browser_(browser),
      scene_(scene),
      content_input_delegate_(content_input_delegate),
      model_(model) {}

UiSceneManager::~UiSceneManager() {}

void UiSceneManager::CreateScene() {
  Create2dBrowsingSubtreeRoots();
  CreateWebVrRoot();
  CreateBackground();
  CreateViewportAwareRoot();
  CreateContentQuad();
  CreateExitPrompt();
  CreateAudioPermissionPrompt();
  CreateWebVRExitWarning();
  CreateSystemIndicators();
  CreateUrlBar();
  CreateSuggestionList();
  CreateWebVrUrlToast();
  CreateCloseButton();
  CreateToasts();
  CreateSplashScreen();
  CreateUnderDevelopmentNotice();
  CreateVoiceSearchUiGroup();
  CreateController();
}

void UiSceneManager::Create2dBrowsingSubtreeRoots() {
  auto element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  element->AddBinding(VR_BIND_FUNC(bool, Model, model_, browsing_mode(),
                                   UiElement, element.get(), SetVisible));
  scene_->AddUiElement(kRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingBackground);
  element->SetVisible(true);
  element->set_hit_testable(false);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingForeground);
  element->set_hit_testable(false);
  element->SetTransitionedProperties({OPACITY});
  element->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  element->AddBinding(base::MakeUnique<Binding<ModalPromptType>>(
      base::Bind([](Model* m) { return m->active_modal_prompt_type; },
                 base::Unretained(model_)),
      base::Bind(
          [](UiElement* e, const ModalPromptType& t) {
            if (t == kModalPromptTypeExitVRForSiteInfo) {
              e->SetVisibleImmediately(false);
            } else if (t == kModalPromptTypeExitVRForAudioPermission) {
              e->SetOpacity(kModalPromptFadeOpacity);
            } else {
              e->SetVisible(true);
            }
          },
          base::Unretained(element.get()))));

  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->set_name(k2dBrowsingContentGroup);
  element->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
  element->SetSize(kContentWidth, kContentHeight);
  element->set_hit_testable(false);
  element->SetTransitionedProperties({TRANSFORM});
  element->AddBinding(VR_BIND(
      bool, Model, model_, fullscreen, UiElement, element.get(),
      SetTranslate(0,
                   value ? kFullscreenVerticalOffset : kContentVerticalOffset,
                   value ? -kFullscreenToastDistance : -kContentDistance)));
  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
}

void UiSceneManager::CreateWebVrRoot() {
  auto element = base::MakeUnique<UiElement>();
  element->set_name(kWebVrRoot);
  element->SetVisible(true);
  element->set_hit_testable(false);
  element->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                   browsing_mode() == false, UiElement,
                                   element.get(), SetVisible));
  scene_->AddUiElement(kRoot, std::move(element));
}

void UiSceneManager::CreateWebVRExitWarning() {
  auto scrim = base::MakeUnique<FullScreenRect>();
  scrim->set_name(kScreenDimmer);
  scrim->set_draw_phase(kPhaseOverlayBackground);
  scrim->SetVisible(false);
  scrim->set_hit_testable(false);
  scrim->SetOpacity(kScreenDimmerOpacity);
  scrim->SetCenterColor(model_->color_scheme().dimmer_inner);
  scrim->SetEdgeColor(model_->color_scheme().dimmer_outer);
  scrim->AddBinding(VR_BIND_FUNC(bool, Model, model_, exiting_vr, UiElement,
                                 scrim.get(), SetVisible));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(scrim));

  // TODO(mthiesse): Programatically compute the proper texture size for these
  // textured UI elements.
  // Create transient exit warning.
  auto exit_warning = base::MakeUnique<ExitWarning>(1024);
  exit_warning->set_name(kExitWarning);
  exit_warning->set_draw_phase(kPhaseOverlayForeground);
  exit_warning->SetSize(kExitWarningWidth, kExitWarningHeight);
  exit_warning->SetTranslate(0, 0, -kExitWarningDistance);
  exit_warning->SetScale(kExitWarningDistance, kExitWarningDistance, 1);
  exit_warning->SetVisible(false);
  exit_warning->set_hit_testable(false);
  exit_warning->AddBinding(VR_BIND_FUNC(bool, Model, model_, exiting_vr,
                                        UiElement, exit_warning.get(),
                                        SetVisible));
  scene_->AddUiElement(k2dBrowsingViewportAwareRoot, std::move(exit_warning));
}

void UiSceneManager::CreateSystemIndicators() {
  struct Indicator {
    UiElementName name;
    const gfx::VectorIcon& icon;
    int resource_string;
    bool PermissionsModel::*signal;
  };
  const std::vector<Indicator> indicators = {
      {kAudioCaptureIndicator, vector_icons::kMicrophoneIcon,
       IDS_AUDIO_CALL_NOTIFICATION_TEXT_2,
       &PermissionsModel::audio_capture_enabled},
      {kVideoCaptureIndicator, vector_icons::kVideocamIcon,
       IDS_VIDEO_CALL_NOTIFICATION_TEXT_2,
       &PermissionsModel::video_capture_enabled},
      {kScreenCaptureIndicator, vector_icons::kScreenShareIcon,
       IDS_SCREEN_CAPTURE_NOTIFICATION_TEXT_2,
       &PermissionsModel::screen_capture_enabled},
      {kBluetoothConnectedIndicator, vector_icons::kBluetoothConnectedIcon, 0,
       &PermissionsModel::bluetooth_connected},
      {kLocationAccessIndicator, vector_icons::kLocationOnIcon, 0,
       &PermissionsModel::location_access},
  };

  std::unique_ptr<LinearLayout> indicator_layout =
      base::MakeUnique<LinearLayout>(LinearLayout::kRight);
  indicator_layout->set_name(kIndicatorLayout);
  indicator_layout->set_hit_testable(false);
  indicator_layout->set_y_anchoring(TOP);
  indicator_layout->SetTranslate(0, kIndicatorVerticalOffset,
                                 kIndicatorDistanceOffset);
  indicator_layout->set_margin(kIndicatorGap);
  indicator_layout->AddBinding(
      VR_BIND_FUNC(bool, Model, model_, fullscreen == false, UiElement,
                   indicator_layout.get(), SetVisible));
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(indicator_layout));

  for (const auto& indicator : indicators) {
    auto element = base::MakeUnique<SystemIndicator>(512);
    element->GetDerivedTexture()->SetIcon(indicator.icon);
    element->GetDerivedTexture()->SetMessageId(indicator.resource_string);
    element->set_name(indicator.name);
    element->set_draw_phase(kPhaseForeground);
    element->set_requires_layout(false);
    element->SetSize(0, kIndicatorHeight);
    element->SetVisible(false);
    BindColor(model_, element.get(), &ColorScheme::system_indicator_background,
              &TexturedElement::SetBackgroundColor);
    BindColor(model_, element.get(), &ColorScheme::system_indicator_foreground,
              &TexturedElement::SetForegroundColor);
    element->AddBinding(base::MakeUnique<Binding<bool>>(
        base::Bind(
            [](Model* m, bool PermissionsModel::*permission) {
              return m->permissions.*permission;
            },
            base::Unretained(model_), indicator.signal),
        base::Bind(
            [](UiElement* e, const bool& v) {
              e->SetVisible(v);
              e->set_requires_layout(v);
            },
            base::Unretained(element.get()))));
    scene_->AddUiElement(kIndicatorLayout, std::move(element));
  }
}

void UiSceneManager::CreateContentQuad() {
  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  auto hit_plane = base::MakeUnique<InvisibleHitTarget>();
  hit_plane->set_name(kBackplane);
  hit_plane->set_draw_phase(kPhaseForeground);
  hit_plane->SetSize(kBackplaneSize, kSceneHeight);
  hit_plane->SetTranslate(0, 0, -kTextureOffset);
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(hit_plane));

  auto main_content = base::MakeUnique<ContentElement>(
      content_input_delegate_,
      base::Bind(&UiBrowserInterface::OnContentScreenBoundsChanged,
                 base::Unretained(browser_)));
  main_content->set_name(kContentQuad);
  main_content->set_draw_phase(kPhaseForeground);
  main_content->SetSize(kContentWidth, kContentHeight);
  main_content->set_corner_radius(kContentCornerRadius);
  main_content->SetTransitionedProperties({BOUNDS});
  main_content->AddBinding(
      VR_BIND(bool, Model, model_, fullscreen, UiElement, main_content.get(),
              SetSize(value ? kFullscreenWidth : kContentWidth,
                      value ? kFullscreenHeight : kContentHeight)));
  main_content->AddBinding(
      VR_BIND_FUNC(gfx::Transform, Model, model_, projection_matrix,
                   ContentElement, main_content.get(), SetProjectionMatrix));
  main_content->AddBinding(VR_BIND_FUNC(unsigned int, Model, model_,
                                        content_texture_id, ContentElement,
                                        main_content.get(), SetTextureId));
  main_content->AddBinding(VR_BIND_FUNC(
      UiElementRenderer::TextureLocation, Model, model_, content_location,
      ContentElement, main_content.get(), SetTextureLocation));
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(main_content));

  // Limit reticle distance to a sphere based on content distance.
  scene_->set_background_distance(kContentDistance *
                                  kBackgroundDistanceMultiplier);
}

void UiSceneManager::CreateSplashScreen() {
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
      base::Bind(
          [](Model* model, UiBrowserInterface* browser,
             TransientElementHideReason reason) {
            // NOTE: we are setting the model here. May want to post a task or
            // fire an event object instead of setting it here directly.
            model->web_vr_show_splash_screen = false;
            if (reason == TransientElementHideReason::kTimeout) {
              browser->ExitPresent();
            }
          },
          base::Unretained(model_), base::Unretained(browser_)));
  transient_parent->set_name(kSplashScreenTransientParent);
  transient_parent->AddBinding(
      VR_BIND_FUNC(bool, Model, model_, web_vr_show_splash_screen, UiElement,
                   transient_parent.get(), SetVisible));
  transient_parent->set_hit_testable(false);
  transient_parent->SetTransitionedProperties({OPACITY});
  transient_parent->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      web_vr_show_splash_screen &&
          model->web_vr_timeout_state == kWebVrNoTimeoutPending,
      ShowUntilSignalTransientElement, transient_parent.get(), Signal));
  scene_->AddUiElement(kSplashScreenViewportAwareRoot,
                       std::move(transient_parent));

  // Add "Powered by Chrome" text.
  auto text = base::MakeUnique<Text>(512, kSplashScreenTextFontHeightM,
                                     kSplashScreenTextWidthM);
  BindColor(model_, text.get(), &ColorScheme::splash_screen_text_color,
            &Text::SetColor);
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
  bg->SetColor(model_->color_scheme().splash_screen_background);
  scene_->AddUiElement(kSplashScreenText, std::move(bg));

  auto spinner = base::MakeUnique<Spinner>(512);
  spinner->set_name(kWebVrTimeoutSpinner);
  spinner->set_draw_phase(kPhaseOverlayForeground);
  spinner->SetVisible(false);
  spinner->SetSize(kSpinnerWidth, kSpinnerHeight);
  spinner->SetTranslate(0, kSpinnerVerticalOffset, -kSpinnerDistance);
  spinner->SetColor(model_->color_scheme().spinner_color);
  spinner->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, web_vr_timeout_state == kWebVrTimeoutImminent,
      Spinner, spinner.get(), SetVisible));
  spinner->SetTransitionedProperties({OPACITY});
  scene_->AddUiElement(kSplashScreenViewportAwareRoot, std::move(spinner));

  // Note, this cannot be a descendant of the viewport aware root, otherwise it
  // will fade out when the viewport aware elements reposition.
  auto spinner_bg = base::MakeUnique<FullScreenRect>();
  spinner_bg->set_name(kWebVrTimeoutSpinnerBackground);
  spinner_bg->set_draw_phase(kPhaseOverlayBackground);
  spinner_bg->SetVisible(false);
  spinner_bg->set_hit_testable(false);
  spinner_bg->SetColor(model_->color_scheme().spinner_background);
  spinner_bg->SetTransitionedProperties({OPACITY});
  spinner_bg->SetTransitionDuration(base::TimeDelta::FromMilliseconds(200));
  spinner_bg->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, web_vr_timeout_state != kWebVrNoTimeoutPending,
      FullScreenRect, spinner_bg.get(), SetVisible));
  scene_->AddUiElement(kSplashScreenRoot, std::move(spinner_bg));

  auto timeout_message = base::MakeUnique<Rect>();
  timeout_message->set_name(kWebVrTimeoutMessage);
  timeout_message->set_draw_phase(kPhaseOverlayForeground);
  timeout_message->SetVisible(false);
  timeout_message->SetSize(kTimeoutMessageBackgroundWidthM,
                           kTimeoutMessageBackgroundHeightM);
  timeout_message->SetTranslate(0, kSpinnerVerticalOffset, -kSpinnerDistance);
  timeout_message->set_corner_radius(kTimeoutMessageCornerRadius);
  timeout_message->SetTransitionedProperties({OPACITY});
  timeout_message->AddBinding(
      VR_BIND_FUNC(bool, Model, model_, web_vr_timeout_state == kWebVrTimedOut,
                   Rect, timeout_message.get(), SetVisible));
  timeout_message->SetColor(model_->color_scheme().timeout_message_background);
  scene_->AddUiElement(kSplashScreenViewportAwareRoot,
                       std::move(timeout_message));

  auto timeout_layout = base::MakeUnique<LinearLayout>(LinearLayout::kRight);
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
  timeout_text->SetColor(model_->color_scheme().timeout_message_foreground);
  timeout_text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  timeout_text->set_name(kWebVrTimeoutMessageText);
  timeout_text->set_draw_phase(kPhaseOverlayForeground);
  timeout_text->SetVisible(true);
  timeout_text->SetSize(kTimeoutMessageTextWidthM, kTimeoutMessageTextHeightM);
  scene_->AddUiElement(kWebVrTimeoutMessageLayout, std::move(timeout_text));

  auto button = base::MakeUnique<Button>(
      base::Bind(&UiBrowserInterface::ExitPresent, base::Unretained(browser_)),
      kPhaseOverlayForeground, kTimeoutButtonWidth, kTimeoutButtonHeight,
      kButtonZOffsetHoverDMM * kTimeoutButtonDistance,
      vector_icons::kClose16Icon);
  button->set_name(kWebVrTimeoutMessageButton);
  button->SetVisible(false);
  button->SetTranslate(0, kTimeoutButtonVerticalOffset,
                       -kTimeoutButtonDistance);
  button->SetRotate(1, 0, 0, kTimeoutButtonRotationRad);
  button->SetTransitionedProperties({OPACITY});
  button->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                  web_vr_timeout_state == kWebVrTimedOut,
                                  Button, button.get(), SetVisible));
  BindButtonColors(model_, button.get(), &ColorScheme::button_colors,
                   &Button::SetButtonColors);
  scene_->AddUiElement(kSplashScreenViewportAwareRoot, std::move(button));

  timeout_text = base::MakeUnique<Text>(512, kTimeoutMessageTextFontHeightM,
                                        kTimeoutButtonTextWidth);
  timeout_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_EXIT_BUTTON_LABEL));
  timeout_text->set_name(kWebVrTimeoutMessageButtonText);
  timeout_text->SetColor(model_->color_scheme().spinner_color);
  timeout_text->set_draw_phase(kPhaseOverlayForeground);
  timeout_text->SetVisible(true);
  timeout_text->SetSize(kTimeoutButtonTextWidth, kTimeoutButtonTextHeight);
  timeout_text->set_y_anchoring(BOTTOM);
  timeout_text->SetTranslate(0, -kTimeoutButtonTextVerticalOffset, 0);
  scene_->AddUiElement(kWebVrTimeoutMessageButton, std::move(timeout_text));
}

void UiSceneManager::CreateUnderDevelopmentNotice() {
  auto text = base::MakeUnique<Text>(512, kUnderDevelopmentNoticeFontHeightM,
                                     kUnderDevelopmentNoticeWidthM);
  BindColor(model_, text.get(), &ColorScheme::world_background_text,
            &Text::SetColor);
  text->SetText(l10n_util::GetStringUTF16(IDS_VR_UNDER_DEVELOPMENT_NOTICE));
  text->set_name(kUnderDevelopmentNotice);
  text->set_draw_phase(kPhaseForeground);
  text->set_hit_testable(false);
  text->SetSize(kUnderDevelopmentNoticeWidthM, kUnderDevelopmentNoticeHeightM);
  text->SetTranslate(0, -kUnderDevelopmentNoticeVerticalOffsetM, 0);
  text->SetRotate(1, 0, 0, kUnderDevelopmentNoticeRotationRad);
  text->SetVisible(true);
  text->set_y_anchoring(BOTTOM);
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
    BindColor(model_, panel_element.get(), &ColorScheme::world_background,
              &Rect::SetColor);
    panel_element->AddBinding(
        VR_BIND_FUNC(bool, Model, model_, should_render_web_vr() == false,
                     UiElement, panel_element.get(), SetVisible));
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
  BindColor(model_, floor.get(), &ColorScheme::floor, &Grid::SetCenterColor);
  BindColor(model_, floor.get(), &ColorScheme::world_background,
            &Grid::SetEdgeColor);
  BindColor(model_, floor.get(), &ColorScheme::floor_grid, &Grid::SetGridColor);
  scene_->AddUiElement(k2dBrowsingBackground, std::move(floor));

  // Ceiling.
  auto ceiling = base::MakeUnique<Rect>();
  ceiling->set_name(kCeiling);
  ceiling->set_draw_phase(kPhaseFloorCeiling);
  ceiling->SetSize(kSceneSize, kSceneSize);
  ceiling->SetTranslate(0.0, kSceneHeight / 2, 0.0);
  ceiling->SetRotate(1, 0, 0, base::kPiFloat / 2);
  BindColor(model_, ceiling.get(), &ColorScheme::ceiling,
            &Rect::SetCenterColor);
  BindColor(model_, ceiling.get(), &ColorScheme::world_background,
            &Rect::SetEdgeColor);
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

void UiSceneManager::CreateVoiceSearchUiGroup() {
  auto voice_search_button = base::MakeUnique<Button>(
      base::Bind(&UiBrowserInterface::SetVoiceSearchActive,
                 base::Unretained(browser_), true),
      kPhaseForeground, kVoiceSearchButtonWidth, kVoiceSearchButtonHeight,
      kButtonZOffsetHoverDMM * kUrlBarDistance, vector_icons::kMicrophoneIcon);
  voice_search_button->set_name(kVoiceSearchButton);
  voice_search_button->SetTranslate(0.f, -kVoiceSearchButtonYOffset, 0.f);
  voice_search_button->set_y_anchoring(BOTTOM);
  voice_search_button->AddBinding(base::MakeUnique<Binding<bool>>(
      base::Bind(
          [](Model* m) {
            return !m->incognito &&
                   m->speech.has_or_can_request_audio_permission;
          },
          base::Unretained(model_)),
      base::Bind([](UiElement* e, const bool& v) { e->SetVisible(v); },
                 voice_search_button.get())));
  BindButtonColors(model_, voice_search_button.get(),
                   &ColorScheme::button_colors, &Button::SetButtonColors);
  scene_->AddUiElement(kUrlBar, std::move(voice_search_button));

  auto speech_recognition_root = base::MakeUnique<UiElement>();
  speech_recognition_root->set_name(kSpeechRecognitionRoot);
  speech_recognition_root->SetTranslate(0.f, 0.f, -kContentDistance);
  speech_recognition_root->set_hit_testable(false);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(speech_recognition_root));

  TransientElement* speech_result_parent =
      AddTransientParent(kSpeechRecognitionResult, kSpeechRecognitionRoot,
                         kSpeechRecognitionResultTimeoutSeconds, false, scene_);
  // We need to explicitly set the initial visibility of
  // kSpeechRecognitionResult as k2dBrowsingForeground's visibility depends on
  // it in a binding. However, k2dBrowsingForeground's binding updated before
  // kSpeechRecognitionResult. So the initial value needs to be correctly set
  // instead of depend on binding to kick in.
  speech_result_parent->SetVisibleImmediately(false);
  speech_result_parent->SetTransitionedProperties({OPACITY});
  speech_result_parent->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  speech_result_parent->AddBinding(base::MakeUnique<Binding<bool>>(
      base::Bind([](Model* m) { return m->speech.recognition_result.empty(); },
                 base::Unretained(model_)),
      base::Bind(
          [](UiElement* e, const bool& v) {
            if (v) {
              e->SetVisible(false);
            } else {
              e->SetVisibleImmediately(true);
            }
          },
          speech_result_parent)));
  auto speech_result = base::MakeUnique<Text>(512, kSuggestionContentTextHeight,
                                              kSuggestionTextFieldWidth);
  speech_result->set_name(kSpeechRecognitionResultText);
  speech_result->set_draw_phase(kPhaseForeground);
  speech_result->SetTranslate(0.f, kSpeechRecognitionResultTextYOffset, 0.f);
  speech_result->set_hit_testable(false);
  speech_result->SetTextAlignment(UiTexture::kTextAlignmentCenter);
  BindColor(model_, speech_result.get(), &ColorScheme::prompt_foreground,
            &Text::SetColor);
  speech_result->AddBinding(VR_BIND_FUNC(base::string16, Model, model_,
                                         speech.recognition_result, Text,
                                         speech_result.get(), SetText));
  speech_result_parent->AddChild(std::move(speech_result));

  auto circle = base::MakeUnique<Rect>();
  circle->set_name(kSpeechRecognitionResultCircle);
  circle->set_draw_phase(kPhaseForeground);
  circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  circle->set_corner_radius(kCloseButtonWidth);
  circle->SetTranslate(0.0, 0.0, -kTextureOffset);
  circle->set_hit_testable(false);
  BindColor(model_, circle.get(),
            &ColorScheme::speech_recognition_circle_background,
            &Rect::SetColor);
  scene_->AddUiElement(kSpeechRecognitionResult, std::move(circle));

  auto microphone = base::MakeUnique<VectorIcon>(512);
  microphone->set_name(kSpeechRecognitionResultMicrophoneIcon);
  microphone->SetIcon(vector_icons::kMicrophoneIcon);
  microphone->set_draw_phase(kPhaseForeground);
  microphone->set_hit_testable(false);
  microphone->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  scene_->AddUiElement(kSpeechRecognitionResult, std::move(microphone));

  auto hit_target = base::MakeUnique<InvisibleHitTarget>();
  hit_target->set_name(kSpeechRecognitionResultBackplane);
  hit_target->set_draw_phase(kPhaseForeground);
  hit_target->SetSize(kPromptBackplaneSize, kPromptBackplaneSize);
  hit_target->SetTranslate(0.0, 0.0, kTextureOffset);
  scene_->AddUiElement(kSpeechRecognitionResult, std::move(hit_target));

  auto speech_recognition_listening = base::MakeUnique<UiElement>();
  UiElement* listening_ui_root = speech_recognition_listening.get();
  speech_recognition_listening->set_name(kSpeechRecognitionListening);
  speech_recognition_listening->set_hit_testable(false);
  // We need to explicitly set the initial visibility of this element for the
  // same reason as kSpeechRecognitionResult.
  speech_recognition_listening->SetVisibleImmediately(false);
  speech_recognition_listening->SetTransitionedProperties({OPACITY});
  speech_recognition_listening->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(
          kSpeechRecognitionOpacityAnimationDurationMs));
  speech_recognition_listening->AddBinding(base::MakeUnique<Binding<bool>>(
      base::Bind([](Model* m) { return m->speech.recognizing_speech; },
                 base::Unretained(model_)),
      base::Bind(
          [](UiElement* listening, UiElement* result, const bool& value) {
            if (result->GetTargetOpacity() != 0.f && !value) {
              listening->SetVisibleImmediately(false);
            } else {
              listening->SetVisible(value);
            }
          },
          base::Unretained(listening_ui_root),
          base::Unretained(speech_result_parent))));
  scene_->AddUiElement(kSpeechRecognitionRoot,
                       std::move(speech_recognition_listening));

  auto growing_circle = base::MakeUnique<Throbber>();
  growing_circle->set_name(kSpeechRecognitionListeningGrowingCircle);
  growing_circle->set_draw_phase(kPhaseForeground);
  growing_circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  growing_circle->set_corner_radius(kCloseButtonWidth);
  growing_circle->SetTranslate(0.0, 0.0, -kTextureOffset * 2);
  growing_circle->set_hit_testable(false);
  BindColor(model_, growing_circle.get(),
            &ColorScheme::speech_recognition_circle_background,
            &Rect::SetColor);
  growing_circle->AddBinding(VR_BIND(
      int, Model, model_, speech.speech_recognition_state, Throbber,
      growing_circle.get(),
      SetCircleGrowAnimationEnabled(value == SPEECH_RECOGNITION_IN_SPEECH ||
                                    value == SPEECH_RECOGNITION_RECOGNIZING ||
                                    value == SPEECH_RECOGNITION_READY)));
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(growing_circle));

  auto inner_circle = base::MakeUnique<Rect>();
  inner_circle->set_name(kSpeechRecognitionListeningInnerCircle);
  inner_circle->set_draw_phase(kPhaseForeground);
  inner_circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  inner_circle->set_corner_radius(kCloseButtonWidth);
  inner_circle->SetTranslate(0.0, 0.0, -kTextureOffset);
  inner_circle->set_hit_testable(false);
  BindColor(model_, inner_circle.get(),
            &ColorScheme::speech_recognition_circle_background,
            &Rect::SetColor);
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(inner_circle));

  auto microphone_icon = base::MakeUnique<VectorIcon>(512);
  microphone_icon->SetIcon(vector_icons::kMicrophoneIcon);
  microphone_icon->set_name(kSpeechRecognitionListeningMicrophoneIcon);
  microphone_icon->set_draw_phase(kPhaseForeground);
  microphone_icon->set_hit_testable(false);
  microphone_icon->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(microphone_icon));

  auto close_button = base::MakeUnique<Button>(
      base::Bind(&UiBrowserInterface::SetVoiceSearchActive,
                 base::Unretained(browser_), false),
      kPhaseForeground, kVoiceSearchCloseButtonWidth,
      kVoiceSearchCloseButtonHeight, kButtonZOffsetHoverDMM * kContentDistance,
      vector_icons::kClose16Icon);
  close_button->set_name(kSpeechRecognitionListeningCloseButton);
  close_button->SetTranslate(0.0, -kVoiceSearchCloseButtonYOffset, 0.f);
  BindButtonColors(model_, close_button.get(), &ColorScheme::button_colors,
                   &Button::SetButtonColors);
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(close_button));

  UiElement* browser_foregroud =
      scene_->GetUiElementByName(k2dBrowsingForeground);
  // k2dBrowsingForeground's visibility binding use the visibility of two
  // other elements which update their bindings after this one. So the
  // visibility of k2dBrowsingForeground will be one frame behind correct value.
  // This is not noticable in practice and simplify our logic a lot.
  browser_foregroud->AddBinding(base::MakeUnique<Binding<bool>>(
      base::Bind(
          [](UiElement* listening, UiElement* result) {
            return listening->GetTargetOpacity() == 0.f &&
                   result->GetTargetOpacity() == 0.f;
          },
          base::Unretained(listening_ui_root),
          base::Unretained(speech_result_parent)),
      base::Bind([](UiElement* e, const bool& value) { e->SetVisible(value); },
                 base::Unretained(browser_foregroud))));
}

void UiSceneManager::CreateController() {
  auto root = base::MakeUnique<UiElement>();
  root->set_name(kControllerRoot);
  root->SetVisible(true);
  root->set_hit_testable(false);
  root->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      browsing_mode() || model->web_vr_timeout_state == kWebVrTimedOut,
      UiElement, root.get(), SetVisible));
  scene_->AddUiElement(kRoot, std::move(root));

  auto group = base::MakeUnique<UiElement>();
  group->set_name(kControllerGroup);
  group->SetVisible(true);
  group->set_hit_testable(false);
  group->SetTransitionedProperties({OPACITY});
  group->AddBinding(base::MakeUnique<Binding<bool>>(
      base::Bind([](Model* m) { return !m->controller.quiescent; },
                 base::Unretained(model_)),
      base::Bind(
          [](UiElement* e, const bool& visible) {
            e->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
                visible ? kControllerFadeInMs : kControllerFadeOutMs));
            e->SetVisible(visible);
          },
          base::Unretained(group.get()))));
  scene_->AddUiElement(kControllerRoot, std::move(group));

  auto controller = base::MakeUnique<Controller>();
  controller->set_draw_phase(kPhaseForeground);
  controller->AddBinding(VR_BIND_FUNC(gfx::Transform, Model, model_,
                                      controller.transform, Controller,
                                      controller.get(), set_local_transform));
  controller->AddBinding(
      VR_BIND_FUNC(bool, Model, model_,
                   controller.touchpad_button_state == UiInputManager::DOWN,
                   Controller, controller.get(), set_touchpad_button_pressed));
  controller->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, controller.app_button_state == UiInputManager::DOWN,
      Controller, controller.get(), set_app_button_pressed));
  controller->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, controller.home_button_state == UiInputManager::DOWN,
      Controller, controller.get(), set_home_button_pressed));
  controller->AddBinding(VR_BIND_FUNC(float, Model, model_, controller.opacity,
                                      Controller, controller.get(),
                                      SetOpacity));
  scene_->AddUiElement(kControllerGroup, std::move(controller));

  auto laser = base::MakeUnique<Laser>(model_);
  laser->set_draw_phase(kPhaseForeground);
  laser->AddBinding(VR_BIND_FUNC(float, Model, model_, controller.opacity,
                                 Laser, laser.get(), SetOpacity));
  scene_->AddUiElement(kControllerGroup, std::move(laser));

  auto reticle = base::MakeUnique<Reticle>(scene_, model_);
  reticle->set_draw_phase(kPhaseForeground);
  scene_->AddUiElement(kControllerGroup, std::move(reticle));
}

void UiSceneManager::CreateUrlBar() {
  auto url_bar = base::MakeUnique<UrlBar>(
      512,
      base::Bind(&UiBrowserInterface::NavigateBack, base::Unretained(browser_)),
      base::Bind(&UiBrowserInterface::OnUnsupportedMode,
                 base::Unretained(browser_)));
  url_bar->set_name(kUrlBar);
  url_bar->set_draw_phase(kPhaseForeground);
  url_bar->SetTranslate(0, kUrlBarVerticalOffset, -kUrlBarDistance);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->SetSize(kUrlBarWidth, kUrlBarHeight);
  url_bar->AddBinding(VR_BIND_FUNC(bool, Model, model_, fullscreen == false,
                                   UiElement, url_bar.get(), SetVisible));
  url_bar->AddBinding(VR_BIND_FUNC(ToolbarState, Model, model_, toolbar_state,
                                   UrlBar, url_bar.get(), SetToolbarState));
  url_bar->AddBinding(VR_BIND_FUNC(UrlBarColors, Model, model_,
                                   color_scheme().url_bar, UrlBar,
                                   url_bar.get(), SetColors));
  url_bar->AddBinding(VR_BIND_FUNC(bool, Model, model_, can_navigate_back,
                                   UrlBar, url_bar.get(),
                                   SetHistoryButtonsEnabled));
  BindColor(model_, url_bar.get(), &ColorScheme::element_background,
            &TexturedElement::SetBackgroundColor);
  scene_->AddUiElement(k2dBrowsingForeground, std::move(url_bar));

  auto indicator_bg = base::MakeUnique<Rect>();
  indicator_bg->set_name(kLoadingIndicator);
  indicator_bg->set_draw_phase(kPhaseForeground);
  indicator_bg->SetTranslate(0, kLoadingIndicatorVerticalOffset,
                             kLoadingIndicatorDepthOffset);
  indicator_bg->SetSize(kLoadingIndicatorWidth, kLoadingIndicatorHeight);
  indicator_bg->set_y_anchoring(TOP);
  indicator_bg->SetTransitionedProperties({OPACITY});
  indicator_bg->set_corner_radius(kLoadingIndicatorHeight * 0.5f);
  indicator_bg->AddBinding(VR_BIND_FUNC(bool, Model, model_, loading, Rect,
                                        indicator_bg.get(), SetVisible));
  BindColor(model_, indicator_bg.get(),
            &ColorScheme::loading_indicator_background, &Rect::SetColor);

  scene_->AddUiElement(kUrlBar, std::move(indicator_bg));

  auto indicator_fg = base::MakeUnique<Rect>();
  indicator_fg->set_draw_phase(kPhaseForeground);
  indicator_fg->set_name(kLoadingIndicatorForeground);
  indicator_fg->set_x_anchoring(LEFT);
  indicator_fg->set_corner_radius(kLoadingIndicatorHeight * 0.5f);
  indicator_fg->set_hit_testable(false);
  BindColor(model_, indicator_fg.get(),
            &ColorScheme::loading_indicator_foreground, &Rect::SetColor);
  indicator_fg->AddBinding(base::MakeUnique<Binding<float>>(
      base::Bind([](Model* m) { return m->load_progress; },
                 base::Unretained(model_)),
      base::Bind(
          [](Rect* r, const float& value) {
            r->SetSize(kLoadingIndicatorWidth * value, kLoadingIndicatorHeight);
            r->SetTranslate(kLoadingIndicatorWidth * value * 0.5f, 0.0f,
                            0.001f);
          },
          base::Unretained(indicator_fg.get()))));
  scene_->AddUiElement(kLoadingIndicator, std::move(indicator_fg));
}

void UiSceneManager::CreateSuggestionList() {
  auto layout = base::MakeUnique<LinearLayout>(LinearLayout::kDown);

  layout->set_name(kSuggestionLayout);
  layout->set_hit_testable(false);
  layout->set_y_anchoring(BOTTOM);
  layout->set_y_centering(TOP);
  layout->SetTranslate(0, -0.05f, 0);
  layout->set_margin(kSuggestionGap);
  layout->SetVisible(true);

  SuggestionSetBinding::ModelAddedCallback added_callback =
      base::Bind(&OnSuggestionModelAdded, base::Unretained(scene_),
                 base::Unretained(browser_), base::Unretained(model_));
  SuggestionSetBinding::ModelRemovedCallback removed_callback =
      base::Bind(&OnSuggestionModelRemoved, base::Unretained(scene_));

  auto binding = base::MakeUnique<SuggestionSetBinding>(
      &model_->omnibox_suggestions, added_callback, removed_callback);
  layout->AddBinding(std::move(binding));
  scene_->AddUiElement(kUrlBar, std::move(layout));
}

void UiSceneManager::CreateWebVrUrlToast() {
  auto* parent =
      AddTransientParent(kWebVrUrlToastTransientParent, kWebVrViewportAwareRoot,
                         kWebVrUrlToastTimeoutSeconds, true, scene_);
  parent->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      web_vr_started_for_autopresentation && !model->web_vr_show_splash_screen,
      UiElement, parent, SetVisible));

  auto element = base::MakeUnique<WebVrUrlToast>(
      512, base::Bind(&UiBrowserInterface::OnUnsupportedMode,
                      base::Unretained(browser_)));
  element->set_name(kWebVrUrlToast);
  element->set_opacity_when_visible(0.8f);
  element->set_draw_phase(kPhaseOverlayForeground);
  element->SetVisible(true);
  element->set_hit_testable(false);
  element->SetTranslate(0, kWebVrToastDistance * sin(kWebVrUrlToastRotationRad),
                        -kWebVrToastDistance * cos(kWebVrUrlToastRotationRad));
  element->SetRotate(1, 0, 0, kWebVrUrlToastRotationRad);
  element->SetSize(kWebVrUrlToastWidth, kWebVrUrlToastHeight);
  BindColor(model_, element.get(),
            &ColorScheme::web_vr_transient_toast_background,
            &TexturedElement::SetBackgroundColor);
  BindColor(model_, element.get(),
            &ColorScheme::web_vr_transient_toast_foreground,
            &TexturedElement::SetForegroundColor);
  element->AddBinding(VR_BIND_FUNC(ToolbarState, Model, model_, toolbar_state,
                                   WebVrUrlToast, element.get(),
                                   SetToolbarState));
  scene_->AddUiElement(kWebVrUrlToastTransientParent, std::move(element));
}

void UiSceneManager::CreateCloseButton() {
  std::unique_ptr<Button> element = base::MakeUnique<Button>(
      base::Bind(
          [](Model* model, UiBrowserInterface* browser) {
            if (model->fullscreen) {
              browser->ExitFullscreen();
            }
            if (model->in_cct) {
              browser->ExitCct();
            }
          },
          base::Unretained(model_), base::Unretained(browser_)),
      kPhaseForeground, kCloseButtonWidth, kCloseButtonHeight,
      kButtonZOffsetHoverDMM * kCloseButtonDistance,
      vector_icons::kClose16Icon);
  element->set_name(kCloseButton);
  element->SetTranslate(0, kCloseButtonVerticalOffset, -kCloseButtonDistance);
  BindButtonColors(model_, element.get(), &ColorScheme::button_colors,
                   &Button::SetButtonColors);

  // Close button is a special control element that needs to be hidden when in
  // WebVR, but it needs to be visible when in cct or fullscreen.
  element->AddBinding(
      VR_BIND_FUNC(bool, Model, model_,
                   browsing_mode() && (model->fullscreen || model->in_cct),
                   UiElement, element.get(), SetVisible));
  element->AddBinding(
      VR_BIND(bool, Model, model_, fullscreen, UiElement, element.get(),
              SetTranslate(0,
                           value ? kCloseButtonFullscreenVerticalOffset
                                 : kCloseButtonVerticalOffset,
                           value ? -kCloseButtonFullscreenDistance
                                 : -kCloseButtonDistance)));
  element->AddBinding(VR_BIND(
      bool, Model, model_, fullscreen, UiElement, element.get(),
      SetSize(value ? kCloseButtonFullscreenWidth : kCloseButtonWidth,
              value ? kCloseButtonFullscreenHeight : kCloseButtonHeight)));

  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
}

void UiSceneManager::CreateExitPrompt() {
  std::unique_ptr<UiElement> element;

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = base::MakeUnique<InvisibleHitTarget>();
  backplane->set_name(kExitPromptBackplane);
  backplane->set_draw_phase(kPhaseForeground);
  backplane->SetSize(kPromptBackplaneSize, kPromptBackplaneSize);
  backplane->SetTranslate(0.0,
                          kContentVerticalOffset + kExitPromptVerticalOffset,
                          kTextureOffset - kContentDistance);
  EventHandlers event_handlers;
  event_handlers.button_up = base::Bind(
      [](UiBrowserInterface* browser, Model* m) {
        browser->OnExitVrPromptResult(
            ExitVrPromptChoice::CHOICE_NONE,
            GetReasonForPrompt(m->active_modal_prompt_type));
      },
      base::Unretained(browser_), base::Unretained(model_));
  backplane->set_event_handlers(event_handlers);
  backplane->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      active_modal_prompt_type == kModalPromptTypeExitVRForSiteInfo, UiElement,
      backplane.get(), SetVisible));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(backplane));

  std::unique_ptr<ExitPrompt> exit_prompt = base::MakeUnique<ExitPrompt>(
      512,
      base::Bind(&UiBrowserInterface::OnExitVrPromptResult,
                 base::Unretained(browser_), ExitVrPromptChoice::CHOICE_STAY),
      base::Bind(&UiBrowserInterface::OnExitVrPromptResult,
                 base::Unretained(browser_), ExitVrPromptChoice::CHOICE_EXIT));
  exit_prompt->set_name(kExitPrompt);
  exit_prompt->set_draw_phase(kPhaseForeground);
  exit_prompt->SetVisible(true);
  exit_prompt->SetSize(kExitPromptWidth, kExitPromptHeight);
  exit_prompt->SetTranslate(0.0, 0.0, kTextureOffset);
  BindColor(model_, exit_prompt.get(), &ColorScheme::prompt_foreground,
            &TexturedElement::SetForegroundColor);
  BindButtonColors(model_, exit_prompt.get(),
                   &ColorScheme::prompt_primary_button_colors,
                   &ExitPrompt::SetPrimaryButtonColors);
  BindButtonColors(model_, exit_prompt.get(),
                   &ColorScheme::prompt_secondary_button_colors,
                   &ExitPrompt::SetSecondaryButtonColors);
  exit_prompt->AddBinding(base::MakeUnique<Binding<ModalPromptType>>(
      base::Bind([](Model* m) { return m->active_modal_prompt_type; },
                 base::Unretained(model_)),
      base::Bind(
          [](ExitPrompt* e, const ModalPromptType& p) {
            e->set_reason(GetReasonForPrompt(p));
            switch (p) {
              case kModalPromptTypeExitVRForSiteInfo:
                e->SetContentMessageId(
                    IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION_SITE_INFO);
                break;
              default:
                e->SetContentMessageId(IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION);
                break;
            }
          },
          base::Unretained(exit_prompt.get()))));
  scene_->AddUiElement(kExitPromptBackplane, std::move(exit_prompt));
}

void UiSceneManager::CreateAudioPermissionPrompt() {
  std::unique_ptr<UiElement> element;

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = base::MakeUnique<InvisibleHitTarget>();
  backplane->set_draw_phase(kPhaseForeground);
  backplane->set_name(kAudioPermissionPromptBackplane);
  backplane->SetSize(kPromptBackplaneSize, kPromptBackplaneSize);
  backplane->SetTranslate(0.0, kContentVerticalOffset, -kOverlayPlaneDistance);
  EventHandlers event_handlers;
  event_handlers.button_up = base::Bind(
      [](UiBrowserInterface* browser, Model* m) {
        browser->OnExitVrPromptResult(
            ExitVrPromptChoice::CHOICE_NONE,
            GetReasonForPrompt(m->active_modal_prompt_type));
      },
      base::Unretained(browser_), base::Unretained(model_));
  backplane->set_event_handlers(event_handlers);
  backplane->SetVisible(false);
  backplane->SetTransitionedProperties({OPACITY});
  backplane->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      active_modal_prompt_type == kModalPromptTypeExitVRForAudioPermission,
      UiElement, backplane.get(), SetVisible));

  std::unique_ptr<Shadow> shadow = base::MakeUnique<Shadow>();
  shadow->set_draw_phase(kPhaseForeground);
  shadow->set_name(kAudioPermissionPromptShadow);
  shadow->set_corner_radius(kContentCornerRadius);

  std::unique_ptr<AudioPermissionPrompt> prompt =
      base::MakeUnique<AudioPermissionPrompt>(
          1024,
          base::Bind(&UiBrowserInterface::OnExitVrPromptResult,
                     base::Unretained(browser_),
                     ExitVrPromptChoice::CHOICE_EXIT,
                     UiUnsupportedMode::kAndroidPermissionNeeded),
          base::Bind(&UiBrowserInterface::OnExitVrPromptResult,
                     base::Unretained(browser_),
                     ExitVrPromptChoice::CHOICE_STAY,
                     UiUnsupportedMode::kAndroidPermissionNeeded));
  prompt->set_name(kAudioPermissionPrompt);
  prompt->set_draw_phase(kPhaseForeground);
  prompt->SetSize(kAudioPermissionPromptWidth, kAudioPermissionPromptHeight);
  prompt->SetTranslate(0.0, 0.0f, kAudionPermisionPromptDepth);
  BindButtonColors(model_, prompt.get(),
                   &ColorScheme::audio_permission_prompt_primary_button_colors,
                   &AudioPermissionPrompt::SetPrimaryButtonColors);
  BindButtonColors(
      model_, prompt.get(),
      &ColorScheme::audio_permission_prompt_secondary_button_colors,
      &AudioPermissionPrompt::SetSecondaryButtonColors);
  BindColor(model_, prompt.get(),
            &ColorScheme::audio_permission_prompt_icon_foreground,
            &AudioPermissionPrompt::SetIconColor);
  BindColor(model_, prompt.get(),
            &ColorScheme::audio_permission_prompt_background,
            &TexturedElement::SetBackgroundColor);
  BindColor(model_, prompt.get(), &ColorScheme::element_foreground,
            &TexturedElement::SetForegroundColor);
  shadow->AddChild(std::move(prompt));
  backplane->AddChild(std::move(shadow));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(backplane));
}

void UiSceneManager::CreateToasts() {
  // Create fullscreen toast.
  auto* parent = AddTransientParent(kExclusiveScreenToastTransientParent,
                                    k2dBrowsingForeground, kToastTimeoutSeconds,
                                    false, scene_);
  // This binding toggles fullscreen toast's visibility if fullscreen state
  // changed and makes sure fullscreen toast becomes invisible if entering
  // webvr mode.
  parent->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                  fullscreen && !model->web_vr_mode, UiElement,
                                  parent, SetVisible));

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
  BindColor(model_, element.get(),
            &ColorScheme::exclusive_screen_toast_background,
            &TexturedElement::SetBackgroundColor);
  BindColor(model_, element.get(),
            &ColorScheme::exclusive_screen_toast_foreground,
            &TexturedElement::SetForegroundColor);
  scene_->AddUiElement(kExclusiveScreenToastTransientParent,
                       std::move(element));

  // Create WebVR toast.
  parent = AddTransientParent(kExclusiveScreenToastViewportAwareTransientParent,
                              kWebVrViewportAwareRoot, kToastTimeoutSeconds,
                              false, scene_);
  // When we first get a web vr frame, we switch states to
  // kWebVrNoTimeoutPending, when that happens, we want to SetVisible(true) to
  // kick the visibility of this element.
  parent->AddBinding(
      VR_BIND_FUNC(bool, Model, model_,
                   web_vr_timeout_state == kWebVrNoTimeoutPending &&
                       model->web_vr_mode && model->web_vr_show_toast,
                   UiElement, parent, SetVisible));

  element = base::MakeUnique<ExclusiveScreenToast>(512);
  element->set_name(kExclusiveScreenToastViewportAware);
  element->set_draw_phase(kPhaseOverlayForeground);
  element->SetSize(kToastWidthDMM, kToastHeightDMM);
  element->SetTranslate(0, kWebVrToastDistance * sin(kWebVrAngleRadians),
                        -kWebVrToastDistance * cos(kWebVrAngleRadians));
  element->SetRotate(1, 0, 0, kWebVrAngleRadians);
  element->SetScale(kWebVrToastDistance, kWebVrToastDistance, 1);
  element->set_hit_testable(false);
  BindColor(model_, element.get(),
            &ColorScheme::exclusive_screen_toast_background,
            &TexturedElement::SetBackgroundColor);
  BindColor(model_, element.get(),
            &ColorScheme::exclusive_screen_toast_foreground,
            &TexturedElement::SetForegroundColor);
  scene_->AddUiElement(kExclusiveScreenToastViewportAwareTransientParent,
                       std::move(element));
}

}  // namespace vr
