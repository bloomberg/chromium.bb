// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

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
#include "chrome/browser/vr/elements/disc_button.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/exit_prompt.h"
#include "chrome/browser/vr/elements/full_screen_rect.h"
#include "chrome/browser/vr/elements/grid.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/laser.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/elements/scaled_depth_adjuster.h"
#include "chrome/browser/vr/elements/simple_textured_element.h"
#include "chrome/browser/vr/elements/spinner.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/elements/throbber.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/elements/url_bar.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/elements/viewport_aware_root.h"
#include "chrome/browser/vr/elements/webvr_url_toast.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "chrome/common/chrome_features.h"
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

template <typename T, typename... Args>
std::unique_ptr<T> Create(UiElementName name, DrawPhase phase, Args&&... args) {
  auto element = base::MakeUnique<T>(std::forward<Args>(args)...);
  element->SetName(name);
  element->SetDrawPhase(phase);
  return element;
}

typedef VectorBinding<OmniboxSuggestion, Button> SuggestionSetBinding;
typedef typename SuggestionSetBinding::ElementBinding SuggestionBinding;

void OnSuggestionModelAdded(UiScene* scene,
                            UiBrowserInterface* browser,
                            Model* model,
                            SuggestionBinding* element_binding) {
  auto icon = base::MakeUnique<VectorIcon>(100);
  icon->SetDrawPhase(kPhaseForeground);
  icon->SetType(kTypeOmniboxSuggestionIcon);
  icon->set_hit_testable(false);
  icon->SetSize(kSuggestionIconSizeDMM, kSuggestionIconSizeDMM);
  BindColor(model, icon.get(), &ColorScheme::omnibox_icon,
            &VectorIcon::SetColor);
  VectorIcon* p_icon = icon.get();

  auto icon_box = base::MakeUnique<UiElement>();
  icon_box->SetDrawPhase(kPhaseNone);
  icon_box->SetType(kTypeOmniboxSuggestionIconField);
  icon_box->SetSize(kSuggestionIconFieldWidthDMM, kSuggestionHeightDMM);
  icon_box->AddChild(std::move(icon));

  auto content_text = base::MakeUnique<Text>(kSuggestionContentTextHeightDMM);
  content_text->SetDrawPhase(kPhaseForeground);
  content_text->SetType(kTypeOmniboxSuggestionContentText);
  content_text->set_hit_testable(false);
  content_text->SetTextLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  content_text->SetSize(kSuggestionTextFieldWidthDMM, 0);
  content_text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  BindColor(model, content_text.get(), &ColorScheme::omnibox_suggestion_content,
            &Text::SetColor);
  Text* p_content_text = content_text.get();

  auto description_text =
      base::MakeUnique<Text>(kSuggestionDescriptionTextHeightDMM);
  description_text->SetDrawPhase(kPhaseForeground);
  description_text->SetType(kTypeOmniboxSuggestionDescriptionText);
  description_text->set_hit_testable(false);
  content_text->SetTextLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  description_text->SetSize(kSuggestionTextFieldWidthDMM, 0);
  description_text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  BindColor(model, description_text.get(),
            &ColorScheme::omnibox_suggestion_description, &Text::SetColor);
  Text* p_description_text = description_text.get();

  auto text_layout = base::MakeUnique<LinearLayout>(LinearLayout::kDown);
  text_layout->SetType(kTypeOmniboxSuggestionTextLayout);
  text_layout->set_hit_testable(false);
  text_layout->set_margin(kSuggestionLineGapDMM);
  text_layout->AddChild(std::move(content_text));
  text_layout->AddChild(std::move(description_text));

  auto right_margin = base::MakeUnique<UiElement>();
  right_margin->SetDrawPhase(kPhaseNone);
  right_margin->SetSize(kSuggestionRightMarginDMM, kSuggestionHeightDMM);

  auto suggestion_layout = base::MakeUnique<LinearLayout>(LinearLayout::kRight);
  suggestion_layout->SetType(kTypeOmniboxSuggestionLayout);
  suggestion_layout->set_hit_testable(false);
  suggestion_layout->AddChild(std::move(icon_box));
  suggestion_layout->AddChild(std::move(text_layout));
  suggestion_layout->AddChild(std::move(right_margin));

  auto background = Create<Button>(
      kNone, kPhaseForeground,
      base::BindRepeating(
          [](UiBrowserInterface* b, Model* m, SuggestionBinding* e) {
            b->Navigate(e->model()->destination);
            // TODO(vollick): set this as a side effect of navigating.
            m->omnibox_input_active = false;
          },
          base::Unretained(browser), base::Unretained(model),
          base::Unretained(element_binding)));

  background->SetType(kTypeOmniboxSuggestionBackground);
  background->set_hit_testable(true);
  background->set_bubble_events(true);
  background->set_bounds_contain_children(true);
  background->set_hover_offset(0.0);
  BindButtonColors(model, background.get(),
                   &ColorScheme::suggestion_button_colors,
                   &Button::SetButtonColors);
  background->AddChild(std::move(suggestion_layout));

  element_binding->bindings().push_back(
      VR_BIND_FUNC(base::string16, SuggestionBinding, element_binding,
                   model()->content, Text, p_content_text, SetText));
  element_binding->bindings().push_back(
      base::MakeUnique<Binding<base::string16>>(
          base::BindRepeating(
              [](SuggestionBinding* m) { return m->model()->description; },
              base::Unretained(element_binding)),
          base::BindRepeating(
              [](Text* v, const base::string16& text) {
                v->SetVisibleImmediately(!text.empty());
                v->set_requires_layout(!text.empty());
                if (!text.empty()) {
                  v->SetText(text);
                }
              },
              base::Unretained(p_description_text))));
  element_binding->bindings().push_back(
      VR_BIND(AutocompleteMatch::Type, SuggestionBinding, element_binding,
              model()->type, VectorIcon, p_icon,
              SetIcon(AutocompleteMatch::TypeToVectorIcon(value))));
  element_binding->set_view(background.get());
  scene->AddUiElement(kOmniboxSuggestions, std::move(background));
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
  element->SetName(name);
  element->SetVisible(false);
  element->set_hit_testable(false);
  if (animate_opacity)
    element->SetTransitionedProperties({OPACITY});
  scene->AddUiElement(parent_name, std::move(element));
  return to_return;
}

std::unique_ptr<Rect> CreateOmniboxSpacer(Model* model) {
  auto spacer = Create<Rect>(kNone, kPhaseForeground);
  spacer->SetType(kTypeOmniboxSuggestionSpacer);
  spacer->SetSize(kOmniboxWidthDMM, kSuggestionVerticalPaddingDMM);
  spacer->AddBinding(base::MakeUnique<Binding<bool>>(
      base::BindRepeating(
          [](Model* m) { return !m->omnibox_suggestions.empty(); },
          base::Unretained(model)),
      base::BindRepeating(
          [](UiElement* e, const bool& v) {
            e->SetVisible(v);
            e->set_requires_layout(v);
          },
          base::Unretained(spacer.get()))));
  BindColor(model, spacer.get(), &ColorScheme::omnibox_background,
            &Rect::SetColor);
  return spacer;
}

}  // namespace

UiSceneCreator::UiSceneCreator(UiBrowserInterface* browser,
                               UiScene* scene,
                               ContentInputDelegate* content_input_delegate,
                               KeyboardDelegate* keyboard_delegate,
                               TextInputDelegate* text_input_delegate,
                               Model* model)
    : browser_(browser),
      scene_(scene),
      content_input_delegate_(content_input_delegate),
      keyboard_delegate_(keyboard_delegate),
      text_input_delegate_(text_input_delegate),
      model_(model) {}

UiSceneCreator::~UiSceneCreator() {}

void UiSceneCreator::CreateScene() {
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
  CreateOmnibox();
  CreateWebVrUrlToast();
  CreateCloseButton();
  CreateToasts();
  CreateSplashScreenForDirectWebVrLaunch();
  CreateWebVrTimeoutScreen();
  CreateUnderDevelopmentNotice();
  CreateVoiceSearchUiGroup();
  CreateController();
  CreateKeyboard();
}

void UiSceneCreator::Create2dBrowsingSubtreeRoots() {
  auto element = base::MakeUnique<UiElement>();
  element->SetName(k2dBrowsingRoot);
  element->set_hit_testable(false);
  element->AddBinding(VR_BIND_FUNC(bool, Model, model_, browsing_mode(),
                                   UiElement, element.get(), SetVisible));
  scene_->AddUiElement(kRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->SetName(k2dBrowsingBackground);
  element->set_hit_testable(false);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->SetName(k2dBrowsingVisibiltyControlForOmnibox);
  element->set_hit_testable(false);
  element->AddBinding(VR_BIND(bool, Model, model_, omnibox_input_active,
                              UiElement, element.get(), SetVisible(!value)));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  element = base::MakeUnique<UiElement>();
  element->SetName(k2dBrowsingForeground);
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
            } else if (
                t ==
                kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission) {
              e->SetOpacity(kModalPromptFadeOpacity);
            } else {
              e->SetVisible(true);
            }
          },
          base::Unretained(element.get()))));
  scene_->AddUiElement(k2dBrowsingVisibiltyControlForOmnibox,
                       std::move(element));

  element = base::MakeUnique<UiElement>();
  element->SetName(k2dBrowsingContentGroup);
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

void UiSceneCreator::CreateWebVrRoot() {
  auto element = base::MakeUnique<UiElement>();
  element->SetName(kWebVrRoot);
  element->set_hit_testable(false);
  element->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                   browsing_mode() == false, UiElement,
                                   element.get(), SetVisible));
  scene_->AddUiElement(kRoot, std::move(element));
}

void UiSceneCreator::CreateWebVRExitWarning() {
  auto scrim = base::MakeUnique<FullScreenRect>();
  scrim->SetName(kScreenDimmer);
  scrim->SetDrawPhase(kPhaseOverlayBackground);
  scrim->SetVisible(false);
  scrim->set_hit_testable(false);
  scrim->SetOpacity(kScreenDimmerOpacity);
  scrim->SetCenterColor(model_->color_scheme().dimmer_inner);
  scrim->SetEdgeColor(model_->color_scheme().dimmer_outer);
  scrim->AddBinding(VR_BIND_FUNC(bool, Model, model_, exiting_vr, UiElement,
                                 scrim.get(), SetVisible));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(scrim));

  // Create transient exit warning.
  auto scaler = base::MakeUnique<ScaledDepthAdjuster>(kExitWarningDistance);
  auto exit_warning_text = base::MakeUnique<Text>(kExitWarningFontHeightDMM);
  exit_warning_text->SetName(kExitWarningText);
  exit_warning_text->SetDrawPhase(kPhaseOverlayForeground);
  exit_warning_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_BROWSER_UNSUPPORTED_PAGE));
  exit_warning_text->SetSize(kExitWarningTextWidthDMM, 0);
  exit_warning_text->SetVisible(true);
  exit_warning_text->set_hit_testable(false);
  BindColor(model_, exit_warning_text.get(),
            &ColorScheme::exit_warning_foreground, &Text::SetColor);

  auto exit_warning_bg = base::MakeUnique<Rect>();
  exit_warning_bg->SetName(kExitWarningBackground);
  exit_warning_bg->SetDrawPhase(kPhaseOverlayForeground);
  exit_warning_bg->set_bounds_contain_children(true);
  exit_warning_bg->set_padding(kExitWarningXPaddingDMM,
                               kExitWarningYPaddingDMM);
  exit_warning_bg->set_corner_radius(kExitWarningCornerRadiusDMM);
  exit_warning_bg->set_hit_testable(false);
  exit_warning_bg->AddChild(std::move(exit_warning_text));
  exit_warning_bg->AddBinding(VR_BIND_FUNC(bool, Model, model_, exiting_vr,
                                           UiElement, exit_warning_bg.get(),
                                           SetVisible));
  BindColor(model_, exit_warning_bg.get(),
            &ColorScheme::exit_warning_background, &Rect::SetColor);
  scaler->AddChild(std::move(exit_warning_bg));
  scene_->AddUiElement(k2dBrowsingViewportAwareRoot, std::move(scaler));
}

void UiSceneCreator::CreateSystemIndicators() {
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
  indicator_layout->SetName(kIndicatorLayout);
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
    element->SetName(indicator.name);
    element->SetDrawPhase(kPhaseForeground);
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

void UiSceneCreator::CreateContentQuad() {
  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  auto hit_plane = base::MakeUnique<InvisibleHitTarget>();
  hit_plane->SetName(kBackplane);
  hit_plane->SetDrawPhase(kPhaseForeground);
  hit_plane->SetSize(kBackplaneSize, kSceneHeight);
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(hit_plane));

  auto main_content = base::MakeUnique<ContentElement>(
      content_input_delegate_,
      base::Bind(&UiBrowserInterface::OnContentScreenBoundsChanged,
                 base::Unretained(browser_)));
  main_content->SetName(kContentQuad);
  main_content->SetDrawPhase(kPhaseForeground);
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

void UiSceneCreator::CreateSplashScreenForDirectWebVrLaunch() {
  auto element = base::MakeUnique<UiElement>();
  element->SetName(kSplashScreenRoot);
  element->set_hit_testable(false);
  scene_->AddUiElement(kRoot, std::move(element));

  // Create viewport aware root.
  element = base::MakeUnique<ViewportAwareRoot>();
  element->SetName(kSplashScreenViewportAwareRoot);
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
  transient_parent->SetName(kSplashScreenTransientParent);
  transient_parent->AddBinding(
      VR_BIND_FUNC(bool, Model, model_, web_vr_show_splash_screen, UiElement,
                   transient_parent.get(), SetVisible));
  transient_parent->set_hit_testable(false);
  transient_parent->SetTransitionedProperties({OPACITY});
  transient_parent->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      web_vr_show_splash_screen && model->web_vr_has_produced_frames(),
      ShowUntilSignalTransientElement, transient_parent.get(), Signal));
  scene_->AddUiElement(kSplashScreenViewportAwareRoot,
                       std::move(transient_parent));

  // Add "Running in Chrome" text.
  auto text_scaler =
      base::MakeUnique<ScaledDepthAdjuster>(kSplashScreenTextDistance);
  auto text = base::MakeUnique<Text>(kSplashScreenTextFontHeightDMM);
  BindColor(model_, text.get(), &ColorScheme::splash_screen_text_color,
            &Text::SetColor);
  text->SetText(l10n_util::GetStringUTF16(IDS_VR_RUNNING_IN_CHROME_MESSAGE));
  text->SetName(kSplashScreenText);
  text->SetDrawPhase(kPhaseOverlayForeground);
  text->set_hit_testable(false);
  text->SetSize(kSplashScreenTextWidthDMM, 0);
  text->SetTranslate(0, kSplashScreenTextVerticalOffsetDMM, 0);
  text_scaler->AddChild(std::move(text));
  scene_->AddUiElement(kSplashScreenTransientParent, std::move(text_scaler));

  // Add splash screen background.
  auto bg = base::MakeUnique<FullScreenRect>();
  bg->SetName(kSplashScreenBackground);
  bg->SetDrawPhase(kPhaseOverlayBackground);
  bg->set_hit_testable(false);
  bg->SetColor(model_->color_scheme().splash_screen_background);
  scene_->AddUiElement(kSplashScreenText, std::move(bg));

  auto spinner = base::MakeUnique<Spinner>(512);
  spinner->SetName(kWebVrTimeoutSpinner);
  spinner->SetDrawPhase(kPhaseOverlayForeground);
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
  spinner_bg->SetName(kWebVrTimeoutSpinnerBackground);
  spinner_bg->SetDrawPhase(kPhaseOverlayBackground);
  spinner_bg->SetVisible(false);
  spinner_bg->set_hit_testable(false);
  spinner_bg->SetColor(model_->color_scheme().spinner_background);
  spinner_bg->SetTransitionedProperties({OPACITY});
  spinner_bg->SetTransitionDuration(base::TimeDelta::FromMilliseconds(200));
  spinner_bg->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, web_vr_timeout_state != kWebVrNoTimeoutPending,
      FullScreenRect, spinner_bg.get(), SetVisible));
  scene_->AddUiElement(kSplashScreenRoot, std::move(spinner_bg));
}

void UiSceneCreator::CreateWebVrTimeoutScreen() {
  auto scaler = base::MakeUnique<ScaledDepthAdjuster>(kSpinnerDistance);

  auto timeout_message =
      Create<Rect>(kWebVrTimeoutMessage, kPhaseOverlayForeground);
  timeout_message->SetVisible(false);
  timeout_message->set_bounds_contain_children(true);
  timeout_message->set_corner_radius(kTimeoutMessageCornerRadiusDMM);
  timeout_message->SetTransitionedProperties({OPACITY, TRANSFORM});
  timeout_message->set_padding(kTimeoutMessageHorizontalPaddingDMM,
                               kTimeoutMessageVerticalPaddingDMM);
  timeout_message->AddBinding(
      VR_BIND_FUNC(bool, Model, model_, web_vr_timeout_state == kWebVrTimedOut,
                   Rect, timeout_message.get(), SetVisible));
  timeout_message->SetColor(model_->color_scheme().timeout_message_background);

  auto timeout_layout = Create<LinearLayout>(kWebVrTimeoutMessageLayout,
                                             kPhaseNone, LinearLayout::kRight);
  timeout_layout->set_hit_testable(false);
  timeout_layout->set_margin(kTimeoutMessageLayoutGapDMM);

  auto timeout_icon = Create<VectorIcon>(kWebVrTimeoutMessageIcon,
                                         kPhaseOverlayForeground, 512);
  timeout_icon->SetIcon(kSadTabIcon);
  timeout_icon->SetSize(kTimeoutMessageIconWidthDMM,
                        kTimeoutMessageIconHeightDMM);

  auto timeout_text =
      Create<Text>(kWebVrTimeoutMessageText, kPhaseOverlayForeground,
                   kTimeoutMessageTextFontHeightDMM);
  timeout_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_TIMEOUT_MESSAGE));
  timeout_text->SetColor(model_->color_scheme().timeout_message_foreground);
  timeout_text->SetTextAlignment(UiTexture::kTextAlignmentLeft);
  timeout_text->SetSize(kTimeoutMessageTextWidthDMM,
                        kTimeoutMessageTextHeightDMM);

  auto button_scaler =
      base::MakeUnique<ScaledDepthAdjuster>(kTimeoutButtonDepthOffset);

  auto button = Create<DiscButton>(
      kWebVrTimeoutMessageButton, kPhaseOverlayForeground,
      base::Bind(&UiBrowserInterface::ExitPresent, base::Unretained(browser_)),
      vector_icons::kClose16Icon);
  button->SetVisible(false);
  button->SetTranslate(0, -kTimeoutMessageTextWidthDMM, 0);
  button->SetRotate(1, 0, 0, kTimeoutButtonRotationRad);
  button->SetTransitionedProperties({OPACITY});
  button->SetSize(kWebVrTimeoutMessageButtonDiameterDMM,
                  kWebVrTimeoutMessageButtonDiameterDMM);
  button->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                  web_vr_timeout_state == kWebVrTimedOut,
                                  DiscButton, button.get(), SetVisible));
  BindButtonColors(model_, button.get(), &ColorScheme::button_colors,
                   &DiscButton::SetButtonColors);

  auto timeout_button_text =
      Create<Text>(kWebVrTimeoutMessageButtonText, kPhaseOverlayForeground,
                   kTimeoutMessageTextFontHeightDMM);

  // Disk-style button text is not uppercase. See crbug.com/787654.
  timeout_button_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_EXIT_BUTTON_LABEL));
  timeout_button_text->SetColor(model_->color_scheme().spinner_color);
  timeout_button_text->SetSize(kTimeoutButtonTextWidthDMM,
                               kTimeoutButtonTextHeightDMM);
  timeout_button_text->set_y_anchoring(BOTTOM);
  timeout_button_text->SetTranslate(0, -kTimeoutButtonTextVerticalOffsetDMM, 0);

  button->AddChild(std::move(timeout_button_text));
  timeout_layout->AddChild(std::move(timeout_icon));
  timeout_layout->AddChild(std::move(timeout_text));
  timeout_message->AddChild(std::move(timeout_layout));
  button_scaler->AddChild(std::move(button));
  timeout_message->AddChild(std::move(button_scaler));
  scaler->AddChild(std::move(timeout_message));
  scene_->AddUiElement(kSplashScreenViewportAwareRoot, std::move(scaler));
}

void UiSceneCreator::CreateUnderDevelopmentNotice() {
  auto text = base::MakeUnique<Text>(kUnderDevelopmentNoticeFontHeightDMM);
  BindColor(model_, text.get(), &ColorScheme::world_background_text,
            &Text::SetColor);
  text->SetText(l10n_util::GetStringUTF16(IDS_VR_UNDER_DEVELOPMENT_NOTICE));
  text->SetName(kUnderDevelopmentNotice);
  text->SetDrawPhase(kPhaseForeground);
  text->set_hit_testable(false);
  text->SetSize(kUnderDevelopmentNoticeWidthDMM,
                kUnderDevelopmentNoticeHeightDMM);
  text->SetTranslate(0, -kUnderDevelopmentNoticeVerticalOffsetDMM, 0);
  text->SetRotate(1, 0, 0, kUnderDevelopmentNoticeRotationRad);
  text->set_y_anchoring(BOTTOM);
  scene_->AddUiElement(kUrlBar, std::move(text));
}

void UiSceneCreator::CreateBackground() {
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
    panel_element->SetName(panel.name);
    panel_element->SetDrawPhase(kPhaseBackground);
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
  floor->SetName(kFloor);
  floor->SetDrawPhase(kPhaseFloorCeiling);
  floor->SetSize(kSceneSize, kSceneSize);
  floor->SetTranslate(0.0, -kSceneHeight / 2, 0.0);
  floor->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  floor->set_gridline_count(kFloorGridlineCount);
  floor->set_focusable(false);
  BindColor(model_, floor.get(), &ColorScheme::floor, &Grid::SetCenterColor);
  BindColor(model_, floor.get(), &ColorScheme::world_background,
            &Grid::SetEdgeColor);
  BindColor(model_, floor.get(), &ColorScheme::floor_grid, &Grid::SetGridColor);
  scene_->AddUiElement(k2dBrowsingBackground, std::move(floor));

  // Ceiling.
  auto ceiling = base::MakeUnique<Rect>();
  ceiling->set_focusable(false);
  ceiling->SetName(kCeiling);
  ceiling->SetDrawPhase(kPhaseFloorCeiling);
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

void UiSceneCreator::CreateViewportAwareRoot() {
  auto element = base::MakeUnique<ViewportAwareRoot>();
  element->SetName(kWebVrViewportAwareRoot);
  element->set_hit_testable(false);
  scene_->AddUiElement(kWebVrRoot, std::move(element));

  element = base::MakeUnique<ViewportAwareRoot>();
  element->SetName(k2dBrowsingViewportAwareRoot);
  element->set_hit_testable(false);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));
}

void UiSceneCreator::CreateVoiceSearchUiGroup() {
  auto voice_search_button = Create<DiscButton>(
      kVoiceSearchButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::SetVoiceSearchActive,
                          base::Unretained(browser_), true),
      vector_icons::kMicrophoneIcon);
  voice_search_button->SetSize(kVoiceSearchButtonDiameterDMM,
                               kVoiceSearchButtonDiameterDMM);
  voice_search_button->set_hover_offset(kButtonZOffsetHoverDMM);
  voice_search_button->SetTranslate(0.f, -kVoiceSearchButtonYOffsetDMM, 0.f);
  voice_search_button->set_y_anchoring(BOTTOM);
  voice_search_button->set_y_centering(TOP);
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
                   &ColorScheme::button_colors, &DiscButton::SetButtonColors);
  scene_->AddUiElement(kUrlBar, std::move(voice_search_button));

  auto speech_recognition_root = base::MakeUnique<UiElement>();
  speech_recognition_root->SetName(kSpeechRecognitionRoot);
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
  auto speech_result =
      base::MakeUnique<Text>(kVoiceSearchRecognitionResultTextHeight);
  speech_result->SetName(kSpeechRecognitionResultText);
  speech_result->SetDrawPhase(kPhaseForeground);
  speech_result->SetTranslate(0.f, kSpeechRecognitionResultTextYOffset, 0.f);
  speech_result->set_hit_testable(false);
  speech_result->SetSize(kVoiceSearchRecognitionResultTextWidth, 0);
  speech_result->SetTextAlignment(UiTexture::kTextAlignmentCenter);
  BindColor(model_, speech_result.get(), &ColorScheme::prompt_foreground,
            &Text::SetColor);
  speech_result->AddBinding(VR_BIND_FUNC(base::string16, Model, model_,
                                         speech.recognition_result, Text,
                                         speech_result.get(), SetText));
  speech_result_parent->AddChild(std::move(speech_result));

  auto circle = base::MakeUnique<Rect>();
  circle->SetName(kSpeechRecognitionResultCircle);
  circle->SetDrawPhase(kPhaseForeground);
  circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  circle->set_corner_radius(kCloseButtonWidth);
  circle->set_hit_testable(false);
  BindColor(model_, circle.get(),
            &ColorScheme::speech_recognition_circle_background,
            &Rect::SetColor);
  scene_->AddUiElement(kSpeechRecognitionResult, std::move(circle));

  auto microphone = base::MakeUnique<VectorIcon>(512);
  microphone->SetName(kSpeechRecognitionResultMicrophoneIcon);
  microphone->SetIcon(vector_icons::kMicrophoneIcon);
  microphone->SetDrawPhase(kPhaseForeground);
  microphone->set_hit_testable(false);
  microphone->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  scene_->AddUiElement(kSpeechRecognitionResult, std::move(microphone));

  auto hit_target = base::MakeUnique<InvisibleHitTarget>();
  hit_target->SetName(kSpeechRecognitionResultBackplane);
  hit_target->SetDrawPhase(kPhaseForeground);
  hit_target->SetSize(kPromptBackplaneSize, kPromptBackplaneSize);
  scene_->AddUiElement(kSpeechRecognitionResult, std::move(hit_target));

  auto speech_recognition_listening = base::MakeUnique<UiElement>();
  UiElement* listening_ui_root = speech_recognition_listening.get();
  speech_recognition_listening->SetName(kSpeechRecognitionListening);
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
  growing_circle->SetName(kSpeechRecognitionListeningGrowingCircle);
  growing_circle->SetDrawPhase(kPhaseForeground);
  growing_circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  growing_circle->set_corner_radius(kCloseButtonWidth);
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
  inner_circle->SetName(kSpeechRecognitionListeningInnerCircle);
  inner_circle->SetDrawPhase(kPhaseForeground);
  inner_circle->SetSize(kCloseButtonWidth * 2, kCloseButtonHeight * 2);
  inner_circle->set_corner_radius(kCloseButtonWidth);
  inner_circle->set_hit_testable(false);
  BindColor(model_, inner_circle.get(),
            &ColorScheme::speech_recognition_circle_background,
            &Rect::SetColor);
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(inner_circle));

  auto microphone_icon = base::MakeUnique<VectorIcon>(512);
  microphone_icon->SetIcon(vector_icons::kMicrophoneIcon);
  microphone_icon->SetName(kSpeechRecognitionListeningMicrophoneIcon);
  microphone_icon->SetDrawPhase(kPhaseForeground);
  microphone_icon->set_hit_testable(false);
  microphone_icon->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(microphone_icon));

  auto close_button = Create<DiscButton>(
      kSpeechRecognitionListeningCloseButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::SetVoiceSearchActive,
                          base::Unretained(browser_), false),
      vector_icons::kClose16Icon);
  close_button->SetSize(kVoiceSearchCloseButtonWidth,
                        kVoiceSearchCloseButtonHeight);
  close_button->set_hover_offset(kButtonZOffsetHoverDMM * kContentDistance);
  close_button->SetTranslate(0.0, -kVoiceSearchCloseButtonYOffset, 0.f);
  BindButtonColors(model_, close_button.get(), &ColorScheme::button_colors,
                   &DiscButton::SetButtonColors);
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

void UiSceneCreator::CreateController() {
  auto root = base::MakeUnique<UiElement>();
  root->SetName(kControllerRoot);
  root->set_hit_testable(false);
  root->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      browsing_mode() || model->web_vr_timeout_state == kWebVrTimedOut,
      UiElement, root.get(), SetVisible));
  scene_->AddUiElement(kRoot, std::move(root));

  auto group = base::MakeUnique<UiElement>();
  group->SetName(kControllerGroup);
  group->set_hit_testable(false);
  group->SetTransitionedProperties({OPACITY});
  group->AddBinding(base::MakeUnique<Binding<bool>>(
      base::Bind(
          [](Model* m) {
            return !m->controller.quiescent || !m->skips_redraw_when_not_dirty;
          },
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
  controller->SetDrawPhase(kPhaseForeground);
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
  laser->SetDrawPhase(kPhaseForeground);
  laser->AddBinding(VR_BIND_FUNC(float, Model, model_, controller.opacity,
                                 Laser, laser.get(), SetOpacity));
  scene_->AddUiElement(kControllerGroup, std::move(laser));

  auto reticle = base::MakeUnique<Reticle>(scene_, model_);
  reticle->SetDrawPhase(kPhaseForeground);
  scene_->AddUiElement(kControllerGroup, std::move(reticle));
}

std::unique_ptr<TextInput> UiSceneCreator::CreateTextInput(
    float font_height_meters,
    Model* model,
    TextInputInfo* text_input_model,
    TextInputDelegate* text_input_delegate) {
  auto text_input = base::MakeUnique<TextInput>(
      font_height_meters,
      base::BindRepeating(
          [](Model* model, bool focused) { model->editing_input = focused; },
          base::Unretained(model)),
      base::BindRepeating(
          [](TextInputInfo* model, const TextInputInfo& text_input_info) {
            *model = text_input_info;
          },
          base::Unretained(text_input_model)));
  text_input->SetDrawPhase(kPhaseNone);
  text_input->set_hit_testable(false);
  text_input->SetTextInputDelegate(text_input_delegate);
  text_input->AddBinding(base::MakeUnique<Binding<TextInputInfo>>(
      base::BindRepeating([](TextInputInfo* info) { return *info; },
                          base::Unretained(text_input_model)),
      base::BindRepeating(
          [](TextInput* e, const TextInputInfo& value) {
            e->UpdateInput(value);
          },
          base::Unretained(text_input.get()))));
  return text_input;
}

void UiSceneCreator::CreateKeyboard() {
  auto scaler = base::MakeUnique<ScaledDepthAdjuster>(kKeyboardDistance);
  scaler->SetName(kKeyboardDmmRoot);
  scene_->AddUiElement(kRoot, std::move(scaler));

  auto keyboard = base::MakeUnique<Keyboard>();
  keyboard->SetKeyboardDelegate(keyboard_delegate_);
  keyboard->SetDrawPhase(kPhaseForeground);
  keyboard->SetTranslate(0.0, kKeyboardVerticalOffsetDMM, 0.0);
  keyboard->AddBinding(VR_BIND_FUNC(bool, Model, model_, editing_input,
                                    UiElement, keyboard.get(), SetVisible));
  scene_->AddUiElement(kKeyboardDmmRoot, std::move(keyboard));
}

void UiSceneCreator::CreateUrlBar() {
  auto scaler = base::MakeUnique<ScaledDepthAdjuster>(kUrlBarDistance);
  scaler->SetName(kUrlBarDmmRoot);
  scene_->AddUiElement(k2dBrowsingForeground, std::move(scaler));

  base::RepeatingCallback<void()> url_click_callback;
  if (base::FeatureList::IsEnabled(features::kVrBrowserKeyboard)) {
    url_click_callback = base::BindRepeating(
        [](Model* m, UiBrowserInterface* browser) {
          m->omnibox_input_active = true;
        },
        base::Unretained(model_), base::Unretained(browser_));
  } else {
    url_click_callback = base::BindRepeating([] {});
  }

  auto url_bar = base::MakeUnique<UrlBar>(
      512,
      base::BindRepeating(&UiBrowserInterface::NavigateBack,
                          base::Unretained(browser_)),
      url_click_callback,
      base::BindRepeating(&UiBrowserInterface::OnUnsupportedMode,
                          base::Unretained(browser_)));
  url_bar->SetName(kUrlBar);
  url_bar->SetDrawPhase(kPhaseForeground);
  url_bar->SetTranslate(0, kUrlBarVerticalOffsetDMM, 0);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->SetSize(kUrlBarWidthDMM, kUrlBarHeightDMM);
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
  scene_->AddUiElement(kUrlBarDmmRoot, std::move(url_bar));

  auto indicator_bg = base::MakeUnique<Rect>();
  indicator_bg->SetName(kLoadingIndicator);
  indicator_bg->SetDrawPhase(kPhaseForeground);
  indicator_bg->SetTranslate(0, kLoadingIndicatorVerticalOffsetDMM, 0);
  indicator_bg->SetSize(kLoadingIndicatorWidthDMM, kLoadingIndicatorHeightDMM);
  indicator_bg->set_y_anchoring(TOP);
  indicator_bg->SetTransitionedProperties({OPACITY});
  indicator_bg->set_corner_radius(kLoadingIndicatorHeightDMM * 0.5f);
  indicator_bg->AddBinding(VR_BIND_FUNC(bool, Model, model_, loading, Rect,
                                        indicator_bg.get(), SetVisible));
  BindColor(model_, indicator_bg.get(),
            &ColorScheme::loading_indicator_background, &Rect::SetColor);

  scene_->AddUiElement(kUrlBar, std::move(indicator_bg));

  auto indicator_fg = base::MakeUnique<Rect>();
  indicator_fg->SetDrawPhase(kPhaseForeground);
  indicator_fg->SetName(kLoadingIndicatorForeground);
  indicator_fg->set_x_anchoring(LEFT);
  indicator_fg->set_corner_radius(kLoadingIndicatorHeightDMM * 0.5f);
  indicator_fg->set_hit_testable(false);
  BindColor(model_, indicator_fg.get(),
            &ColorScheme::loading_indicator_foreground, &Rect::SetColor);
  indicator_fg->AddBinding(base::MakeUnique<Binding<float>>(
      base::Bind([](Model* m) { return m->load_progress; },
                 base::Unretained(model_)),
      base::Bind(
          [](Rect* r, const float& value) {
            r->SetSize(kLoadingIndicatorWidthDMM * value,
                       kLoadingIndicatorHeightDMM);
            r->SetTranslate(kLoadingIndicatorWidthDMM * value * 0.5f, 0.0f,
                            0.001f);
          },
          base::Unretained(indicator_fg.get()))));
  scene_->AddUiElement(kLoadingIndicator, std::move(indicator_fg));
}

void UiSceneCreator::CreateOmnibox() {
  auto scaler = base::MakeUnique<ScaledDepthAdjuster>(kUrlBarDistance);
  scaler->SetName(kOmniboxDmmRoot);

  auto omnibox_root = base::MakeUnique<UiElement>();
  omnibox_root->SetName(kOmniboxRoot);
  omnibox_root->SetDrawPhase(kPhaseNone);
  omnibox_root->SetVisible(false);
  omnibox_root->set_hit_testable(false);
  omnibox_root->SetTransitionedProperties({OPACITY});
  omnibox_root->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                        omnibox_input_active, UiElement,
                                        omnibox_root.get(), SetVisible));

  auto shadow = base::MakeUnique<Shadow>();
  shadow->SetName(kOmniboxShadow);
  shadow->SetDrawPhase(kPhaseForeground);
  shadow->set_intensity(kOmniboxShadowIntensity);
  shadow->set_y_anchoring(TOP);
  shadow->set_y_centering(BOTTOM);
  shadow->set_corner_radius(kOmniboxCornerRadiusDMM);

  auto omnibox_outer_layout = base::MakeUnique<LinearLayout>(LinearLayout::kUp);
  omnibox_outer_layout->set_hit_testable(false);
  omnibox_outer_layout->SetName(kOmniboxOuterLayout);
  omnibox_outer_layout->set_margin(kSuggestionGapDMM);
  omnibox_outer_layout->SetTranslate(
      0, kUrlBarVerticalOffsetDMM - 0.5 * kOmniboxHeightDMM,
      kOmniboxShadowOffset);
  omnibox_outer_layout->AddBinding(base::MakeUnique<Binding<bool>>(
      base::BindRepeating([](Model* m) { return m->omnibox_input_active; },
                          base::Unretained(model_)),
      base::BindRepeating(
          [](UiElement* e, const bool& v) {
            float y_offset =
                v ? kOmniboxVerticalOffsetDMM : kUrlBarVerticalOffsetDMM;
            y_offset -= 0.5 * kOmniboxHeightDMM;
            e->SetTranslate(0, y_offset, kOmniboxShadowOffset);
          },
          omnibox_outer_layout.get())));

  auto omnibox_container = base::MakeUnique<Rect>();
  omnibox_container->SetName(kOmniboxContainer);
  omnibox_container->SetDrawPhase(kPhaseForeground);
  omnibox_container->SetSize(kOmniboxWidthDMM, kOmniboxHeightDMM);
  omnibox_container->SetTransitionedProperties({TRANSFORM, OPACITY});
  omnibox_container->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kOmniboxTransitionMs));
  omnibox_container->set_focusable(false);
  omnibox_container->AddBinding(base::MakeUnique<Binding<bool>>(
      base::BindRepeating(
          [](Model* m) { return m->omnibox_suggestions.empty(); },
          base::Unretained(model_)),
      base::BindRepeating(
          [](Rect* r, const bool& v) {
            if (v) {
              r->set_corner_radii(
                  {kOmniboxCornerRadiusDMM, kOmniboxCornerRadiusDMM,
                   kOmniboxCornerRadiusDMM, kOmniboxCornerRadiusDMM});
            } else {
              r->set_corner_radii(
                  {0, 0, kOmniboxCornerRadiusDMM, kOmniboxCornerRadiusDMM});
            }
          },
          omnibox_container.get())));
  BindColor(model_, omnibox_container.get(), &ColorScheme::omnibox_background,
            &Rect::SetColor);

  float width = kOmniboxWidthDMM - 2 * kOmniboxTextMarginDMM;
  auto omnibox_text_field =
      CreateTextInput(kOmniboxTextHeightDMM, model_,
                      &model_->omnibox_text_field_info, text_input_delegate_);
  omnibox_text_field->set_input_committed_callback(base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser, const TextInputInfo& text) {
        if (!model->omnibox_suggestions.empty()) {
          browser->Navigate(model->omnibox_suggestions.front().destination);
          model->omnibox_input_active = false;
        }
      },
      base::Unretained(model_), base::Unretained(browser_)));
  omnibox_text_field->AddBinding(
      VR_BIND(TextInputInfo, Model, model_, omnibox_text_field_info,
              UiBrowserInterface, browser_, StartAutocomplete(value.text)));
  omnibox_text_field->SetSize(width, 0);
  omnibox_text_field->SetHintText(
      l10n_util::GetStringUTF16(IDS_SEARCH_OR_TYPE_URL));
  omnibox_text_field->SetName(kOmniboxTextField);
  omnibox_text_field->set_x_anchoring(LEFT);
  omnibox_text_field->set_x_centering(LEFT);
  omnibox_text_field->SetTranslate(kOmniboxTextMarginDMM, 0, 0);
  omnibox_text_field->AddBinding(base::MakeUnique<Binding<bool>>(
      base::BindRepeating([](Model* m) { return m->omnibox_input_active; },
                          base::Unretained(model_)),
      base::BindRepeating(
          [](TextInput* e, Model* m, const bool& v) {
            m->omnibox_text_field_info = TextInputInfo();
            if (v) {
              e->RequestFocus();
            } else {
              e->RequestUnfocus();
            }
          },
          base::Unretained(omnibox_text_field.get()),
          base::Unretained(model_))));
  omnibox_text_field->AddBinding(base::MakeUnique<Binding<AutocompleteStatus>>(
      base::BindRepeating(
          [](Model* m) {
            AutocompleteStatus state;
            state.active = m->omnibox_input_active;
            state.input = m->omnibox_text_field_info.text;
            return state;
          },
          base::Unretained(model_)),
      base::BindRepeating(
          [](UiBrowserInterface* browser, const AutocompleteStatus& r) {
            if (r.active) {
              browser->StartAutocomplete(r.input);
            } else {
              browser->StopAutocomplete();
            }
          },
          base::Unretained(browser_))));
  BindColor(model_, omnibox_text_field.get(), &ColorScheme::omnibox_text,
            &TextInput::SetTextColor);
  BindColor(model_, omnibox_text_field.get(), &ColorScheme::cursor,
            &TextInput::SetCursorColor);
  BindColor(model_, omnibox_text_field.get(), &ColorScheme::omnibox_hint,
            &TextInput::SetHintColor);

  // Set up the vector binding to manage suggestions dynamically.
  SuggestionSetBinding::ModelAddedCallback added_callback =
      base::Bind(&OnSuggestionModelAdded, base::Unretained(scene_),
                 base::Unretained(browser_), base::Unretained(model_));
  SuggestionSetBinding::ModelRemovedCallback removed_callback =
      base::Bind(&OnSuggestionModelRemoved, base::Unretained(scene_));

  auto suggestions_outer_layout =
      base::MakeUnique<LinearLayout>(LinearLayout::kDown);
  suggestions_outer_layout->SetName(kOmniboxSuggestionsOuterLayout);
  suggestions_outer_layout->set_hit_testable(false);

  auto suggestions_layout = base::MakeUnique<LinearLayout>(LinearLayout::kUp);
  suggestions_layout->SetName(kOmniboxSuggestions);
  suggestions_layout->SetDrawPhase(kPhaseNone);
  suggestions_layout->set_hit_testable(false);
  suggestions_layout->AddBinding(base::MakeUnique<SuggestionSetBinding>(
      &model_->omnibox_suggestions, added_callback, removed_callback));

  auto button_scaler =
      base::MakeUnique<ScaledDepthAdjuster>(kOmniboxCloseButtonDepthOffset);

  auto close_button = Create<DiscButton>(
      kOmniboxCloseButton, kPhaseForeground,
      base::BindRepeating([](Model* m) { m->omnibox_input_active = false; },
                          base::Unretained(model_)),
      vector_icons::kBackArrowIcon);
  close_button->SetSize(kOmniboxCloseButtonDiameterDMM,
                        kOmniboxCloseButtonDiameterDMM);
  close_button->SetTranslate(0, kOmniboxCloseButtonVerticalOffsetDMM, 0);
  close_button->SetRotate(1, 0, 0, atan(kOmniboxCloseButtonVerticalOffsetDMM));
  close_button->set_hover_offset(kButtonZOffsetHoverDMM);
  BindButtonColors(model_, close_button.get(), &ColorScheme::button_colors,
                   &DiscButton::SetButtonColors);

  auto spacer = CreateOmniboxSpacer(model_);
  spacer->set_corner_radii(
      {kOmniboxCornerRadiusDMM, kOmniboxCornerRadiusDMM, 0, 0});
  suggestions_outer_layout->AddChild(std::move(spacer));
  suggestions_outer_layout->AddChild(std::move(suggestions_layout));
  suggestions_outer_layout->AddChild(CreateOmniboxSpacer(model_));

  omnibox_container->AddChild(std::move(omnibox_text_field));

  omnibox_outer_layout->AddChild(std::move(omnibox_container));
  omnibox_outer_layout->AddChild(std::move(suggestions_outer_layout));

  shadow->AddChild(std::move(omnibox_outer_layout));

  button_scaler->AddChild(std::move(close_button));

  omnibox_root->AddChild(std::move(shadow));
  omnibox_root->AddChild(std::move(button_scaler));

  scaler->AddChild(std::move(omnibox_root));

  scene_->AddUiElement(k2dBrowsingRoot, std::move(scaler));
}

void UiSceneCreator::CreateWebVrUrlToast() {
  auto* parent =
      AddTransientParent(kWebVrUrlToastTransientParent, kWebVrViewportAwareRoot,
                         kWebVrUrlToastTimeoutSeconds, true, scene_);
  parent->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                  web_vr_started_for_autopresentation &&
                                      !model->web_vr_show_splash_screen &&
                                      model->web_vr_has_produced_frames(),
                                  UiElement, parent, SetVisible));

  auto element = base::MakeUnique<WebVrUrlToast>(
      512, base::Bind(&UiBrowserInterface::OnUnsupportedMode,
                      base::Unretained(browser_)));
  element->SetName(kWebVrUrlToast);
  element->set_opacity_when_visible(0.8f);
  element->SetDrawPhase(kPhaseOverlayForeground);
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

void UiSceneCreator::CreateCloseButton() {
  base::Callback<void()> click_handler = base::Bind(
      [](Model* model, UiBrowserInterface* browser) {
        if (model->fullscreen) {
          browser->ExitFullscreen();
        }
        if (model->in_cct) {
          browser->ExitCct();
        }
      },
      base::Unretained(model_), base::Unretained(browser_));
  std::unique_ptr<DiscButton> element =
      Create<DiscButton>(kCloseButton, kPhaseForeground, click_handler,
                         vector_icons::kClose16Icon);
  element->SetSize(kCloseButtonWidth, kCloseButtonHeight);
  element->set_hover_offset(kButtonZOffsetHoverDMM * kCloseButtonDistance);
  element->SetTranslate(0, kCloseButtonVerticalOffset, -kCloseButtonDistance);
  BindButtonColors(model_, element.get(), &ColorScheme::button_colors,
                   &DiscButton::SetButtonColors);

  // Close button is a special control element that needs to be hidden when
  // in WebVR, but it needs to be visible when in cct or fullscreen.
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

void UiSceneCreator::CreateExitPrompt() {
  std::unique_ptr<UiElement> element;

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = base::MakeUnique<InvisibleHitTarget>();
  backplane->SetName(kExitPromptBackplane);
  backplane->SetDrawPhase(kPhaseForeground);
  backplane->SetSize(kPromptBackplaneSize, kPromptBackplaneSize);
  backplane->SetTranslate(0.0,
                          kContentVerticalOffset + kExitPromptVerticalOffset,
                          -kContentDistance);
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
  exit_prompt->SetName(kExitPrompt);
  exit_prompt->SetDrawPhase(kPhaseForeground);
  exit_prompt->SetSize(kExitPromptWidth, kExitPromptHeight);
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

void UiSceneCreator::CreateAudioPermissionPrompt() {
  std::unique_ptr<UiElement> element;

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = base::MakeUnique<InvisibleHitTarget>();
  backplane->SetDrawPhase(kPhaseForeground);
  backplane->SetName(kAudioPermissionPromptBackplane);
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
      active_modal_prompt_type ==
          kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission,
      UiElement, backplane.get(), SetVisible));

  std::unique_ptr<Shadow> shadow = base::MakeUnique<Shadow>();
  shadow->SetName(kAudioPermissionPromptShadow);
  shadow->SetDrawPhase(kPhaseForeground);

  std::unique_ptr<AudioPermissionPrompt> prompt =
      base::MakeUnique<AudioPermissionPrompt>(
          1024,
          base::Bind(
              &UiBrowserInterface::OnExitVrPromptResult,
              base::Unretained(browser_), ExitVrPromptChoice::CHOICE_EXIT,
              UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission),
          base::Bind(
              &UiBrowserInterface::OnExitVrPromptResult,
              base::Unretained(browser_), ExitVrPromptChoice::CHOICE_STAY,
              UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission));
  prompt->SetName(kAudioPermissionPrompt);
  prompt->SetDrawPhase(kPhaseForeground);
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

void UiSceneCreator::CreateToasts() {
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
  element->SetName(kExclusiveScreenToast);
  element->SetDrawPhase(kPhaseForeground);
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
                   web_vr_has_produced_frames() && model->web_vr_show_toast,
                   UiElement, parent, SetVisible));

  element = base::MakeUnique<ExclusiveScreenToast>(512);
  element->SetName(kExclusiveScreenToastViewportAware);
  element->SetDrawPhase(kPhaseOverlayForeground);
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
