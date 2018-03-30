// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/numerics/math_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_target.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/databinding/vector_binding.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/controller.h"
#include "chrome/browser/vr/elements/disc_button.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/environment/background.h"
#include "chrome/browser/vr/elements/environment/grid.h"
#include "chrome/browser/vr/elements/environment/stars.h"
#include "chrome/browser/vr/elements/full_screen_rect.h"
#include "chrome/browser/vr/elements/indicator_spec.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/laser.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/omnibox_formatting.h"
#include "chrome/browser/vr/elements/omnibox_text_field.h"
#include "chrome/browser/vr/elements/oval.h"
#include "chrome/browser/vr/elements/prompt.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/repositioner.h"
#include "chrome/browser/vr/elements/resizer.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/elements/scaled_depth_adjuster.h"
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
#include "chrome/browser/vr/model/platform_toast.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/transform_util.h"

namespace vr {

namespace {

template <typename V, typename C, typename S>
void BindColor(Model* model,
               V* view,
               C color,
               const std::string& color_string,
               S setter,
               const std::string& setter_string) {
  view->AddBinding(std::make_unique<Binding<SkColor>>(
      base::BindRepeating([](Model* m, C c) { return (m->color_scheme()).*c; },
                          base::Unretained(model), color),
      color_string,
      base::BindRepeating(
          [](V* v, S s, const SkColor& value) { (v->*s)(value); },
          base::Unretained(view), setter),
      setter_string));
}

#ifndef NDEBUG
#define VR_BIND_COLOR(m, v, c, s) BindColor(m, v, c, #c, s, #s)
#else
#define VR_BIND_COLOR(m, v, c, s) BindColor(m, v, c, "", s, "")
#endif

template <typename V, typename C, typename S>
void BindButtonColors(Model* model,
                      V* view,
                      C colors,
                      const std::string& colors_string,
                      S setter,
                      const std::string& setter_string) {
  view->AddBinding(std::make_unique<Binding<ButtonColors>>(
      base::BindRepeating([](Model* m, C c) { return (m->color_scheme()).*c; },
                          base::Unretained(model), colors),
      colors_string,
      base::BindRepeating(
          [](V* v, S s, const ButtonColors& value) { (v->*s)(value); },
          base::Unretained(view), setter),
      setter_string));
}

#ifndef NDEBUG
#define VR_BIND_BUTTON_COLORS(m, v, c, s) BindButtonColors(m, v, c, #c, s, #s)
#else
#define VR_BIND_BUTTON_COLORS(m, v, c, s) BindButtonColors(m, v, c, "", s, "")
#endif

#define VR_BIND_VISIBILITY(v, c) \
  v->AddBinding(                 \
      VR_BIND_FUNC(bool, Model, model_, c, UiElement, v.get(), SetVisible));

template <typename T, typename... Args>
std::unique_ptr<T> Create(UiElementName name, DrawPhase phase, Args&&... args) {
  auto element = std::make_unique<T>(std::forward<Args>(args)...);
  element->SetName(name);
  element->SetDrawPhase(phase);
  return element;
}

typedef VectorBinding<OmniboxSuggestion, Button> SuggestionSetBinding;
typedef typename SuggestionSetBinding::ElementBinding SuggestionBinding;

void OnSuggestionModelAdded(UiScene* scene,
                            UiBrowserInterface* browser,
                            Ui* ui,
                            Model* model,
                            AudioDelegate* audio_delegate,
                            SuggestionBinding* element_binding) {
  auto icon = std::make_unique<VectorIcon>(100);
  icon->SetDrawPhase(kPhaseForeground);
  icon->SetType(kTypeOmniboxSuggestionIcon);
  icon->SetSize(kSuggestionIconSizeDMM, kSuggestionIconSizeDMM);
  icon->AddBinding(VR_BIND_FUNC(SkColor, Model, model,
                                model->color_scheme().url_bar_button.foreground,
                                VectorIcon, icon.get(), SetColor));
  VectorIcon* p_icon = icon.get();

  auto icon_box = std::make_unique<UiElement>();
  icon_box->SetDrawPhase(kPhaseNone);
  icon_box->SetType(kTypeOmniboxSuggestionIconField);
  icon_box->set_hit_testable(true);
  icon_box->SetSize(kSuggestionIconFieldWidthDMM, kSuggestionHeightDMM);
  icon_box->AddChild(std::move(icon));

  auto content_text = std::make_unique<Text>(kSuggestionContentTextHeightDMM);
  content_text->SetDrawPhase(kPhaseForeground);
  content_text->SetType(kTypeOmniboxSuggestionContentText);
  content_text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  content_text->SetSize(kSuggestionTextFieldWidthDMM, 0);
  content_text->SetAlignment(UiTexture::kTextAlignmentLeft);
  Text* p_content_text = content_text.get();

  auto description_text =
      std::make_unique<Text>(kSuggestionDescriptionTextHeightDMM);
  description_text->SetDrawPhase(kPhaseForeground);
  description_text->SetType(kTypeOmniboxSuggestionDescriptionText);
  description_text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  description_text->SetSize(kSuggestionTextFieldWidthDMM, 0);
  description_text->SetAlignment(UiTexture::kTextAlignmentLeft);
  Text* p_description_text = description_text.get();

  auto text_layout = std::make_unique<LinearLayout>(LinearLayout::kDown);
  text_layout->SetType(kTypeOmniboxSuggestionTextLayout);
  text_layout->set_margin(kSuggestionLineGapDMM);
  text_layout->AddChild(std::move(content_text));
  text_layout->AddChild(std::move(description_text));

  auto right_margin = std::make_unique<UiElement>();
  right_margin->SetDrawPhase(kPhaseNone);
  right_margin->set_hit_testable(true);
  right_margin->SetSize(kSuggestionRightMarginDMM, kSuggestionHeightDMM);

  auto suggestion_layout = std::make_unique<LinearLayout>(LinearLayout::kRight);
  suggestion_layout->SetType(kTypeOmniboxSuggestionLayout);
  suggestion_layout->AddChild(std::move(icon_box));
  suggestion_layout->AddChild(std::move(text_layout));
  suggestion_layout->AddChild(std::move(right_margin));

  auto background = Create<Button>(
      kNone, kPhaseForeground,
      base::BindRepeating(
          [](UiBrowserInterface* b, Ui* ui, Model* m, SuggestionBinding* e) {
            b->Navigate(e->model()->destination,
                        NavigationMethod::kOmniboxSuggestionSelected);
            ui->OnUiRequestedNavigation();
          },
          base::Unretained(browser), base::Unretained(ui),
          base::Unretained(model), base::Unretained(element_binding)),
      audio_delegate);

  background->SetType(kTypeOmniboxSuggestionBackground);
  background->set_hit_testable(true);
  background->set_bubble_events(true);
  background->set_bounds_contain_children(true);
  background->set_hover_offset(0.0);
  VR_BIND_BUTTON_COLORS(model, background.get(), &ColorScheme::url_bar_button,
                        &Button::SetButtonColors);
  background->AddChild(std::move(suggestion_layout));

  element_binding->bindings().push_back(
      VR_BIND_FUNC(base::string16, SuggestionBinding, element_binding,
                   model->model()->contents, Text, p_content_text, SetText));
  element_binding->bindings().push_back(
      std::make_unique<Binding<TextFormatting>>(
          VR_BIND_LAMBDA(
              [](SuggestionBinding* suggestion, Model* model) {
                return ConvertClassification(
                    suggestion->model()->contents_classifications,
                    suggestion->model()->contents.size(),
                    model->color_scheme());
              },
              base::Unretained(element_binding), base::Unretained(model)),
          VR_BIND_LAMBDA(
              [](Text* v, const TextFormatting& formatting) {
                v->SetFormatting(formatting);
              },
              base::Unretained(p_content_text))));
  element_binding->bindings().push_back(
      std::make_unique<Binding<base::string16>>(
          VR_BIND_LAMBDA(
              [](SuggestionBinding* m) { return m->model()->description; },
              base::Unretained(element_binding)),
          VR_BIND_LAMBDA(
              [](Text* v, const base::string16& text) {
                v->SetVisibleImmediately(!text.empty());
                v->set_requires_layout(!text.empty());
                if (!text.empty()) {
                  v->SetText(text);
                }
              },
              base::Unretained(p_description_text))));
  element_binding->bindings().push_back(
      std::make_unique<Binding<TextFormatting>>(
          VR_BIND_LAMBDA(
              [](SuggestionBinding* suggestion, Model* model) {
                return ConvertClassification(
                    suggestion->model()->description_classifications,
                    suggestion->model()->description.size(),
                    model->color_scheme());
              },
              base::Unretained(element_binding), base::Unretained(model)),
          VR_BIND_LAMBDA(
              [](Text* v, const TextFormatting& formatting) {
                v->SetFormatting(formatting);
              },
              base::Unretained(p_description_text))));
  element_binding->bindings().push_back(
      VR_BIND(AutocompleteMatch::Type, SuggestionBinding, element_binding,
              model->model()->type, VectorIcon, p_icon,
              view->SetIcon(AutocompleteMatch::TypeToVectorIcon(
                  value, /*is_bookmark=*/false))));
  element_binding->set_view(background.get());
  scene->AddUiElement(kOmniboxSuggestions, std::move(background));
}

void OnSuggestionModelRemoved(UiScene* scene, SuggestionBinding* binding) {
  scene->RemoveUiElement(binding->view()->id());
}

std::unique_ptr<TransientElement> CreateTransientParent(UiElementName name,
                                                        int timeout_seconds,
                                                        bool animate_opacity) {
  auto element = std::make_unique<SimpleTransientElement>(
      base::TimeDelta::FromSeconds(timeout_seconds));
  element->SetName(name);
  element->SetVisible(false);
  if (animate_opacity)
    element->SetTransitionedProperties({OPACITY});
  return element;
}

// Util to bind the visibility of the given control element to the given
// property in the model and the visibility of the voice search UI root.
#define BIND_VISIBILITY_CONTROL_FOR_VOICE(control_element, model, property) \
  control_element->AddBinding(std::make_unique<Binding<bool>>(              \
      VR_BIND_LAMBDA(                                                       \
          [](Model* model, UiElement* voice_search_root) {                  \
            return model->property &&                                       \
                   voice_search_root->GetTargetOpacity() == 0.f;            \
          },                                                                \
          base::Unretained(model),                                          \
          base::Unretained(                                                 \
              scene_->GetUiElementByName(kSpeechRecognitionRoot))),         \
      VR_BIND_LAMBDA([](UiElement* control,                                 \
                        const bool& value) { control->SetVisible(value); }, \
                     base::Unretained(control_element))))

std::unique_ptr<UiElement> CreateSpacer(float width, float height) {
  auto spacer = Create<UiElement>(kNone, kPhaseNone);
  spacer->SetType(kTypeSpacer);
  spacer->SetSize(width, height);
  return spacer;
}

std::unique_ptr<UiElement> CreateSnackbar(
    UiElementName name,
    Model* model,
    const gfx::VectorIcon& vector_icon,
    const base::string16& label,
    const base::string16& button_label,
    base::RepeatingCallback<void()> callback,
    AudioDelegate* audio_delegate) {
  auto scaler = std::make_unique<ScaledDepthAdjuster>(kSnackbarDistance);

  auto snackbar_layout =
      Create<LinearLayout>(name, kPhaseNone, LinearLayout::kRight);
  snackbar_layout->SetTranslate(0, kSnackbarDistance * sin(kSnackbarAngle), 0);
  snackbar_layout->SetRotate(1, 0, 0, kSnackbarAngle);

  float radius = 0.5f * kSnackbarHeightDMM;
  auto snackbar_oval_left = Create<Rect>(kNone, kPhaseForeground);
  snackbar_oval_left->SetType(kTypeSnackbarDescription);
  snackbar_oval_left->SetSize(0, kSnackbarHeightDMM);
  snackbar_oval_left->SetCornerRadii({radius, 0, radius, 0});
  snackbar_oval_left->set_owner_name_for_test(name);
  VR_BIND_COLOR(model, snackbar_oval_left.get(),
                &ColorScheme::snackbar_background, &Rect::SetColor);

  auto snackbar_inner_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kRight);
  snackbar_inner_layout->set_margin(kSnackbarPaddingDMM * 0.5f);
  snackbar_oval_left->AddBinding(VR_BIND(
      float, UiElement, snackbar_inner_layout.get(),
      model->stale_size().width() + kSnackbarPaddingDMM, UiElement,
      snackbar_oval_left.get(), view->SetSize(value, kSnackbarHeightDMM)));

  auto icon = Create<VectorIcon>(kNone, kPhaseForeground, 256);
  icon->SetSize(kSnackbarIconWidthDMM, kSnackbarIconWidthDMM);
  icon->SetBackgroundColor(SK_ColorTRANSPARENT);
  icon->SetIcon(vector_icon);
  VR_BIND_COLOR(model, icon.get(), &ColorScheme::snackbar_foreground,
                &VectorIcon::SetColor);

  auto text = Create<Text>(kNone, kPhaseForeground, kSnackbarFontHeightDMM);
  text->SetText(label);
  text->SetLayoutMode(TextLayoutMode::kSingleLine);
  VR_BIND_COLOR(model, text.get(), &ColorScheme::snackbar_foreground,
                &Text::SetColor);

  auto button =
      Create<Button>(kNone, kPhaseForeground, callback, audio_delegate);
  button->SetType(kTypeSnackbarButton);
  VR_BIND_BUTTON_COLORS(model, button.get(),
                        &ColorScheme::snackbar_button_colors,
                        &Button::SetButtonColors);
  button->set_hover_offset(0.0f);
  button->SetCornerRadii({0, radius, 0, radius});
  button->SetSize(0, kSnackbarHeightDMM);
  button->set_owner_name_for_test(name);

  auto button_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kRight);

  auto button_text =
      Create<Text>(kNone, kPhaseForeground, kSnackbarFontHeightDMM);
  button_text->SetText(button_label);
  button_text->SetLayoutMode(TextLayoutMode::kSingleLine);
  button_text->AddBinding(
      VR_BIND_FUNC(SkColor, Model, model,
                   model->color_scheme().snackbar_button_colors.foreground,
                   Text, button_text.get(), SetColor));

  button->AddBinding(VR_BIND(float, UiElement, button_layout.get(),
                             model->stale_size().width() + kSnackbarPaddingDMM,
                             UiElement, button.get(),
                             view->SetSize(value, kSnackbarHeightDMM)));

  button_layout->AddChild(std::move(button_text));
  button_layout->AddChild(CreateSpacer(0.5f * kSnackbarPaddingDMM, 0.0f));

  button->AddChild(std::move(button_layout));

  snackbar_inner_layout->AddChild(CreateSpacer(0.0f, 0.0f));
  snackbar_inner_layout->AddChild(std::move(icon));
  snackbar_inner_layout->AddChild(std::move(text));
  snackbar_oval_left->AddChild(std::move(snackbar_inner_layout));

  snackbar_layout->AddChild(std::move(snackbar_oval_left));
  snackbar_layout->AddChild(std::move(button));

  scaler->AddChild(std::move(snackbar_layout));
  auto snackbar_root = Create<UiElement>(kNone, kPhaseNone);
  snackbar_root->AddChild(std::move(scaler));
  return snackbar_root;
}

std::pair<std::unique_ptr<UiElement>, Prompt*> CreatePrompt(
    UiElementName name,
    UiElementName backplane_name,
    Model* model,
    int content_message_id,
    const gfx::VectorIcon& icon,
    int primary_button_message_id,
    int secondary_button_message_id,
    const Prompt::PromptCallback& result_callback) {
  auto prompt = Create<Prompt>(name, kPhaseForeground, 1024, content_message_id,
                               icon, primary_button_message_id,
                               secondary_button_message_id, result_callback);
  auto* prompt_ptr = prompt.get();
  prompt->SetDrawPhase(kPhaseForeground);
  prompt->SetTranslate(0, 0, kPromptShadowOffsetDMM);
  prompt->SetSize(kPromptWidthDMM, kPromptHeightDMM);
  prompt->set_hit_testable(true);
  VR_BIND_BUTTON_COLORS(model, prompt.get(),
                        &ColorScheme::modal_prompt_primary_button_colors,
                        &Prompt::SetPrimaryButtonColors);
  VR_BIND_BUTTON_COLORS(model, prompt.get(),
                        &ColorScheme::modal_prompt_secondary_button_colors,
                        &Prompt::SetSecondaryButtonColors);
  VR_BIND_COLOR(model, prompt.get(), &ColorScheme::modal_prompt_icon_foreground,
                &Prompt::SetIconColor);
  VR_BIND_COLOR(model, prompt.get(), &ColorScheme::modal_prompt_background,
                &TexturedElement::SetBackgroundColor);
  VR_BIND_COLOR(model, prompt.get(), &ColorScheme::modal_prompt_foreground,
                &TexturedElement::SetForegroundColor);

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = Create<InvisibleHitTarget>(backplane_name, kPhaseForeground);
  backplane->SetType(kTypePromptBackplane);
  backplane->SetSize(kBackplaneSize, kBackplaneSize);
  backplane->SetTranslate(0, kPromptVerticalOffsetDMM, 0);
  backplane->SetTransitionedProperties({OPACITY});
  EventHandlers event_handlers;
  event_handlers.button_up = base::BindRepeating(
      [](Prompt* prompt) { prompt->Cancel(); }, base::Unretained(prompt.get()));
  backplane->set_event_handlers(event_handlers);

  auto shadow = Create<Shadow>(kNone, kPhaseForeground);
  shadow->SetType(kTypePromptShadow);
  shadow->AddChild(std::move(prompt));
  backplane->AddChild(std::move(shadow));

  auto scaler = Create<ScaledDepthAdjuster>(kNone, kPhaseNone, kPromptDistance);
  scaler->SetType(kTypeScaledDepthAdjuster);
  scaler->AddChild(std::move(backplane));
  scaler->set_contributes_to_parent_bounds(false);
  return {std::move(scaler), prompt_ptr};
}

std::unique_ptr<UiElement> CreateControllerLabel(UiElementName name,
                                                 float z_offset,
                                                 const base::string16& text,
                                                 Model* model) {
  auto layout = Create<LinearLayout>(name, kPhaseNone, LinearLayout::kLeft);
  layout->set_margin(kControllerLabelLayoutMargin);
  layout->SetTranslate(0, 0, z_offset);
  layout->AddBinding(VR_BIND_FUNC(
      LayoutAlignment, Model, model,
      model->controller.handedness == PlatformController::kRightHanded ? LEFT
                                                                       : RIGHT,
      LinearLayout, layout.get(), set_x_centering));
  layout->AddBinding(VR_BIND_FUNC(
      LinearLayout::Direction, Model, model,
      model->controller.handedness == PlatformController::kRightHanded
          ? LinearLayout::kRight
          : LinearLayout::kLeft,
      LinearLayout, layout.get(), set_direction));

  auto spacer = std::make_unique<UiElement>();
  spacer->SetVisible(true);
  spacer->set_requires_layout(true);
  spacer->SetSize(kControllerLabelSpacerSize, kControllerLabelSpacerSize);

  auto callout = Create<Rect>(kNone, kPhaseForeground);
  callout->SetVisible(true);
  callout->SetColor(SK_ColorWHITE);
  callout->SetSize(kControllerLabelCalloutWidth, kControllerLabelCalloutHeight);
  callout->SetRotate(1, 0, 0, -base::kPiFloat / 2);

  auto label =
      Create<Text>(kNone, kPhaseForeground, kControllerLabelFontHeight);
  label->SetText(text);
  label->SetColor(model->color_scheme().controller_label_callout);
  label->SetVisible(true);
  label->SetAlignment(UiTexture::kTextAlignmentRight);
  label->SetLayoutMode(kSingleLine);
  label->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  label->SetShadowsEnabled(true);
  label->SetScale(kControllerLabelScale, kControllerLabelScale,
                  kControllerLabelScale);

  layout->AddChild(std::move(spacer));
  layout->AddChild(std::move(callout));
  layout->AddChild(std::move(label));

  return layout;
}

std::unique_ptr<UiElement> CreateControllerElement(Model* model) {
  auto controller = Create<Controller>(kController, kPhaseForeground);
  controller->AddBinding(VR_BIND_FUNC(gfx::Transform, Model, model,
                                      model->controller.transform, Controller,
                                      controller.get(), set_local_transform));
  controller->AddBinding(VR_BIND_FUNC(float, Model, model,
                                      model->controller.opacity, Controller,
                                      controller.get(), SetOpacity));

  auto touchpad_button =
      Create<Rect>(kControllerTouchpadButton, kPhaseForeground);
  touchpad_button->SetColor(model->color_scheme().controller_button);
  touchpad_button->SetSize(kControllerWidth, kControllerWidth);
  touchpad_button->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  touchpad_button->SetTranslate(0.0f, 0.0f,
                                -(kControllerLength - kControllerWidth) / 2);
  touchpad_button->SetCornerRadii({kControllerWidth / 2, kControllerWidth / 2,
                                   kControllerWidth / 2, kControllerWidth / 2});
  touchpad_button->AddBinding(VR_BIND_FUNC(
      SkColor, Model, model,
      model->controller.touchpad_button_state == UiInputManager::DOWN
          ? model->color_scheme().controller_button_down
          : model->color_scheme().controller_button,
      Rect, touchpad_button.get(), SetColor));
  controller->AddChild(std::move(touchpad_button));

  auto app_button =
      Create<VectorIcon>(kControllerAppButton, kPhaseForeground, 100);
  app_button->SetIcon(kDaydreamControllerAppButtonIcon);
  app_button->SetColor(model->color_scheme().controller_button);
  app_button->SetSize(kControllerSmallButtonSize, kControllerSmallButtonSize);
  app_button->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  app_button->SetTranslate(0.0f, 0.0f, kControllerAppButtonZ);
  app_button->AddBinding(
      VR_BIND_FUNC(SkColor, Model, model,
                   model->controller.app_button_state == UiInputManager::DOWN
                       ? model->color_scheme().controller_button_down
                       : model->color_scheme().controller_button,
                   VectorIcon, app_button.get(), SetColor));
  controller->AddChild(std::move(app_button));

  auto home_button =
      Create<VectorIcon>(kControllerHomeButton, kPhaseForeground, 100);
  home_button->SetIcon(kDaydreamControllerHomeButtonIcon);
  home_button->SetColor(model->color_scheme().controller_button);
  home_button->SetSize(kControllerSmallButtonSize, kControllerSmallButtonSize);
  home_button->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  home_button->SetTranslate(0.0f, 0.0f, kControllerHomeButtonZ);
  home_button->AddBinding(
      VR_BIND_FUNC(SkColor, Model, model,
                   model->controller.home_button_state == UiInputManager::DOWN
                       ? model->color_scheme().controller_button_down
                       : model->color_scheme().controller_button,
                   VectorIcon, home_button.get(), SetColor));
  controller->AddChild(std::move(home_button));

  return controller;
}

EventHandlers CreateRepositioningHandlers(Model* model, UiScene* scene) {
  EventHandlers handlers;
  handlers.button_down = base::BindRepeating(
      [](Model* model) { model->push_mode(kModeRepositionWindow); },
      base::Unretained(model));
  handlers.button_up = base::BindRepeating(
      [](Model* model, Repositioner* repositioner) {
        if (repositioner->HasMovedBeyondThreshold())
          model->pop_mode(kModeRepositionWindow);
      },
      base::Unretained(model),
      base::Unretained(static_cast<Repositioner*>(
          scene->GetUiElementByName(k2dBrowsingRepositioner))));
  return handlers;
}

void BindIndicatorText(Model* model, Text* text, const IndicatorSpec& spec) {
  text->AddBinding(std::make_unique<Binding<std::pair<bool, bool>>>(
      VR_BIND_LAMBDA(
          [](Model* model, bool CapturingStateModel::*signal,
             bool CapturingStateModel::*background_signal) {
            return std::make_pair(model->capturing_state.*signal,
                                  model->capturing_state.*background_signal);
          },
          base::Unretained(model), spec.signal, spec.background_signal),
      VR_BIND_LAMBDA(
          [](Text* view, int resource, int background_resource,
             int potential_resource, const std::pair<bool, bool>& value) {
            if (value.first)
              view->SetText(l10n_util::GetStringUTF16(resource));
            else if (value.second)
              view->SetText(l10n_util::GetStringUTF16(background_resource));
            else
              view->SetText(l10n_util::GetStringUTF16(potential_resource));
          },
          base::Unretained(text), spec.resource_string,
          spec.background_resource_string, spec.potential_resource_string)));
}

std::unique_ptr<UiElement> CreateWebVrIndicator(Model* model,
                                                UiBrowserInterface* browser,
                                                IndicatorSpec spec) {
  auto container = Create<Rect>(spec.webvr_name, kPhaseOverlayForeground);
  VR_BIND_COLOR(model, container.get(),
                &ColorScheme::webvr_permission_background, &Rect::SetColor);
  container->set_corner_radius(kWebVrPermissionCornerRadius);
  container->set_bounds_contain_children(true);
  container->SetVisible(false);
  container->set_padding(
      kWebVrPermissionLeftPadding, kWebVrPermissionTopPadding,
      kWebVrPermissionRightPadding, kWebVrPermissionBottomPadding);

  auto layout = Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kRight);
  layout->set_margin(kWebVrPermissionMargin);

  auto icon_element = Create<VectorIcon>(kNone, kPhaseOverlayForeground, 128);
  VR_BIND_COLOR(model, icon_element.get(),
                &ColorScheme::webvr_permission_foreground,
                &VectorIcon::SetColor);
  icon_element->set_y_anchoring(TOP);
  icon_element->SetSize(kWebVrPermissionIconSize, kWebVrPermissionIconSize);
  if (spec.is_url) {
    icon_element->AddBinding(VR_BIND_FUNC(
        const gfx::VectorIcon*, Model, model, model->toolbar_state.vector_icon,
        VectorIcon, icon_element.get(), SetIcon));
  } else {
    icon_element->SetIcon(spec.icon);
  }

  std::unique_ptr<UiElement> description_element;
  if (spec.is_url) {
    auto url_text = Create<UrlBar>(
        kNone, kPhaseOverlayForeground, 512,
        base::BindRepeating(&UiBrowserInterface::OnUnsupportedMode,
                            base::Unretained(browser)));
    url_text->SetSize(kWebVrPermissionTextWidth, 0);
    url_text->AddBinding(VR_BIND_FUNC(ToolbarState, Model, model,
                                      model->toolbar_state, UrlBar,
                                      url_text.get(), SetToolbarState));
    VR_BIND_COLOR(model, url_text.get(),
                  &ColorScheme::webvr_permission_foreground,
                  &UrlBar::SetSingleColor);
    description_element = std::move(url_text);

  } else {
    auto text_element = Create<Text>(kNone, kPhaseOverlayForeground,
                                     kWebVrPermissionFontHeight);
    text_element->SetLayoutMode(kMultiLineFixedWidth);
    text_element->SetAlignment(UiTexture::kTextAlignmentLeft);
    text_element->SetColor(SK_ColorWHITE);
    text_element->SetSize(kWebVrPermissionTextWidth, 0.0f);
    if (spec.signal)
      BindIndicatorText(model, text_element.get(), spec);
    else
      text_element->SetText(l10n_util::GetStringUTF16(spec.resource_string));
    VR_BIND_COLOR(model, text_element.get(),
                  &ColorScheme::webvr_permission_foreground, &Text::SetColor);
    description_element = std::move(text_element);
  }

  layout->AddChild(std::move(icon_element));
  layout->AddChild(std::move(description_element));
  container->AddChild(std::move(layout));

  return container;
}

std::unique_ptr<UiElement> CreateHostedUi(
    Model* model,
    UiBrowserInterface* browser,
    ContentInputDelegate* content_input_delegate,
    UiElementName name,
    float distance) {
  auto hosted_ui = Create<ContentElement>(
      kNone, kPhaseForeground, content_input_delegate, base::DoNothing());
  hosted_ui->SetSize(kContentWidth * kHostedUiWidthRatio,
                     kContentHeight * kHostedUiHeightRatio);
  hosted_ui->SetVisible(false);
  hosted_ui->set_opacity_when_visible(1.0);
  hosted_ui->set_requires_layout(false);
  hosted_ui->set_corner_radius(kContentCornerRadius);
  hosted_ui->SetTransitionedProperties({OPACITY});
  hosted_ui->SetTranslate(0, 0, kHostedUiDepthOffset);
  hosted_ui->AddBinding(VR_BIND_FUNC(
      ContentInputDelegatePtr, Model, model, model->hosted_platform_ui.delegate,
      ContentElement, hosted_ui.get(), SetDelegate));
  hosted_ui->AddBinding(VR_BIND_FUNC(
      unsigned int, Model, model, model->hosted_platform_ui.texture_id,
      ContentElement, hosted_ui.get(), SetTextureId));
  hosted_ui->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return m->hosted_platform_ui.hosted_ui_enabled; },
          base::Unretained(model)),
      VR_BIND_LAMBDA(
          [](ContentElement* dialog, const bool& enabled) {
            dialog->SetVisible(enabled);
            dialog->set_requires_layout(enabled);
            dialog->set_hit_testable(enabled);
          },
          base::Unretained(hosted_ui.get()))));
  hosted_ui->AddBinding(std::make_unique<Binding<std::pair<bool, gfx::PointF>>>(
      base::BindRepeating(
          [](Model* m) {
            return std::pair<bool, gfx::PointF>(
                m->hosted_platform_ui.floating,
                gfx::PointF(m->hosted_platform_ui.rect.x(),
                            m->hosted_platform_ui.rect.y()));
          },
          base::Unretained(model)),
      base::BindRepeating(
          [](ContentElement* dialog,
             const std::pair<bool, gfx::PointF>& value) {
            if (value.first) {
              dialog->set_x_centering(LEFT);
              dialog->set_y_centering(TOP);
              dialog->SetTranslate((value.second.x() - 0.5) * kContentWidth,
                                   (0.5 - value.second.y()) * kContentHeight,
                                   kFloatingHostedUiDistance);
            } else {
              dialog->set_x_centering(NONE);
              dialog->set_y_centering(NONE);
              dialog->SetTranslate(0, 0, kHostedUiDepthOffset);
            }
          },
          base::Unretained(hosted_ui.get()))));

  hosted_ui->AddBinding(std::make_unique<Binding<std::pair<bool, gfx::SizeF>>>(
      base::BindRepeating(
          [](Model* m) {
            return std::pair<bool, gfx::SizeF>(
                m->hosted_platform_ui.floating,
                gfx::SizeF(m->hosted_platform_ui.rect.width(),
                           m->hosted_platform_ui.rect.height()));
          },
          base::Unretained(model)),
      base::BindRepeating(
          [](ContentElement* dialog, const std::pair<bool, gfx::SizeF>& value) {
            if (!value.first && value.second.width() > 0) {
              float ratio = static_cast<float>(value.second.height()) /
                            value.second.width();
              dialog->SetSize(kContentWidth * kHostedUiWidthRatio,
                              kContentWidth * kHostedUiWidthRatio * ratio);
            } else if (value.first) {
              dialog->SetSize(kContentWidth * value.second.width(),
                              kContentWidth * value.second.height());
            }
          },
          base::Unretained(hosted_ui.get()))));

  auto backplane = Create<InvisibleHitTarget>(name, kPhaseForeground);
  backplane->SetType(kTypeHostedUiBackplane);
  backplane->SetSize(kSceneSize, kSceneSize);
  backplane->SetTranslate(0.0, kContentVerticalOffset, -kContentDistance);
  backplane->set_contributes_to_parent_bounds(false);
  EventHandlers event_handlers;
  event_handlers.button_up = base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser) {
        if (model->hosted_platform_ui.hosted_ui_enabled) {
          browser->CloseHostedDialog();
        }
      },
      base::Unretained(model), base::Unretained(browser));
  backplane->set_event_handlers(event_handlers);
  backplane->AddChild(std::move(hosted_ui));
  backplane->AddBinding(VR_BIND_FUNC(
      bool, Model, model, model->hosted_platform_ui.hosted_ui_enabled,
      InvisibleHitTarget, backplane.get(), SetVisible));

  return backplane;
}

std::unique_ptr<Grid> CreateGrid(Model* model, UiElementName name) {
  auto grid = Create<Grid>(name, kPhaseBackground);
  grid->SetSize(kSceneSize, kSceneSize);
  grid->SetTranslate(0.0, -kSceneHeight / 2, 0.0);
  grid->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  grid->set_gridline_count(kFloorGridlineCount);
  grid->SetEdgeColor(SK_ColorTRANSPARENT);
  grid->SetCenterColor(SK_ColorTRANSPARENT);
  grid->SetGridColor(model->color_scheme().floor_grid);
  grid->set_focusable(false);
  return grid;
}

void SetVisibleInLayout(UiElement* e, bool v) {
  e->SetVisible(v);
  e->set_requires_layout(v);
}

std::unique_ptr<TransientElement> CreateTextToast(
    UiElementName transient_parent_name,
    UiElementName toast_name,
    Model* model,
    const base::string16& text) {
  auto parent =
      CreateTransientParent(transient_parent_name, kToastTimeoutSeconds, false);
  parent->set_bounds_contain_children(true);
  parent->SetScale(kContentDistance, kContentDistance, 1.0f);

  auto background_element = Create<Rect>(toast_name, kPhaseForeground);
  VR_BIND_COLOR(model, background_element.get(), &ColorScheme::toast_background,
                &Rect::SetColor);

  background_element->set_bounds_contain_children(true);
  background_element->set_padding(kToastXPaddingDMM, kToastYPaddingDMM,
                                  kToastXPaddingDMM, kToastYPaddingDMM);
  background_element->SetTransitionedProperties({OPACITY});
  background_element->SetType(kTypeToastBackground);
  background_element->set_corner_radius(kToastCornerRadiusDMM);

  auto text_element =
      Create<Text>(kNone, kPhaseForeground, kToastTextFontHeightDMM);
  text_element->SetLayoutMode(kSingleLine);
  text_element->SetColor(SK_ColorWHITE);
  text_element->set_owner_name_for_test(toast_name);
  text_element->SetSize(0.0f, kToastTextFontHeightDMM);
  text_element->SetType(kTypeToastText);
  text_element->SetText(text);

  VR_BIND_COLOR(model, text_element.get(), &ColorScheme::toast_foreground,
                &Text::SetColor);

  background_element->AddChild(std::move(text_element));
  parent->AddChild(std::move(background_element));
  return parent;
}

}  // namespace

UiSceneCreator::UiSceneCreator(UiBrowserInterface* browser,
                               UiScene* scene,
                               Ui* ui,
                               ContentInputDelegate* content_input_delegate,
                               KeyboardDelegate* keyboard_delegate,
                               TextInputDelegate* text_input_delegate,
                               AudioDelegate* audio_delegate,
                               Model* model)
    : browser_(browser),
      scene_(scene),
      ui_(ui),
      content_input_delegate_(content_input_delegate),
      keyboard_delegate_(keyboard_delegate),
      text_input_delegate_(text_input_delegate),
      audio_delegate_(audio_delegate),
      model_(model) {}

UiSceneCreator::~UiSceneCreator() {}

void UiSceneCreator::CreateScene() {
  Create2dBrowsingSubtreeRoots();
  CreateWebVrRoot();
  CreateBackground();
  CreateViewportAwareRoot();
  CreateContentQuad();
  Create2dBrowsingHostedUi();
  CreatePrompts();
  CreateSystemIndicators();
  CreateUrlBar();
  CreateOverflowMenu();
  if (model_->update_ready_snackbar_enabled) {
    CreateSnackbars();
  }
  CreateOmnibox();
  CreateCloseButton();
  CreateToasts();
  CreateVoiceSearchUiGroup();
  CreateContentRepositioningAffordance();
  CreateExitWarning();
  CreateWebVrSubtree();
  CreateKeyboard();
  CreateController();
}

void UiSceneCreator::Create2dBrowsingHostedUi() {
  auto hosted_ui_root =
      CreateHostedUi(model_, browser_, content_input_delegate_,
                     k2dBrowsingHostedUi, kContentDistance);
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(hosted_ui_root));
}

void UiSceneCreator::Create2dBrowsingSubtreeRoots() {
  auto element = Create<UiElement>(k2dBrowsingRoot, kPhaseNone);
  element->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return m->browsing_enabled() && !m->waiting_for_background;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA([](UiElement* e, const bool& v) { e->SetVisible(v); },
                     base::Unretained(element.get()))));

  scene_->AddUiElement(kRoot, std::move(element));

  element = Create<UiElement>(k2dBrowsingBackground, kPhaseNone);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  auto repositioner = Create<Repositioner>(k2dBrowsingRepositioner, kPhaseNone);
  repositioner->set_bounds_contain_children(true);
  repositioner->AddBinding(
      VR_BIND_FUNC(bool, Model, model_, model->reposition_window_enabled(),
                   Repositioner, repositioner.get(), SetEnabled));
  repositioner->AddBinding(VR_BIND_FUNC(
      gfx::Vector3dF, Model, model_, model->controller.laser_direction,
      Repositioner, repositioner.get(), set_laser_direction));
  repositioner->AddBinding(
      VR_BIND(bool, Model, model_, model->controller.recentered, Repositioner,
              repositioner.get(), if (value) { view->Reset(); }));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(repositioner));

  element = Create<UiElement>(k2dBrowsingVisibiltyControlForVoice, kPhaseNone);
  element->set_bounds_contain_children(true);
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(element));

  element =
      Create<UiElement>(k2dBrowsingVisibilityControlForPrompt, kPhaseNone);
  element->set_bounds_contain_children(true);
  VR_BIND_VISIBILITY(
      element,
      model->active_modal_prompt_type == kModalPromptTypeNone ||
          model->active_modal_prompt_type ==
              kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission ||
          model->active_modal_prompt_type == kModalPromptTypeUpdateKeyboard);
  scene_->AddUiElement(k2dBrowsingVisibiltyControlForVoice, std::move(element));

  element = Create<UiElement>(k2dBrowsingVisibiltyControlForSiteInfoPrompt,
                              kPhaseNone);
  element->set_bounds_contain_children(true);
  VR_BIND_VISIBILITY(element, model->active_modal_prompt_type !=
                                  kModalPromptTypeExitVRForSiteInfo);
  scene_->AddUiElement(k2dBrowsingVisibilityControlForPrompt,
                       std::move(element));

  element = Create<UiElement>(k2dBrowsingOpacityControlForAudioPermissionPrompt,
                              kPhaseNone);
  element->set_bounds_contain_children(true);
  element->AddBinding(
      VR_BIND(bool, Model, model_,
              model->active_modal_prompt_type !=
                  kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission,
              UiElement, element.get(),
              view->SetOpacity(value ? 1.0 : kModalPromptFadeOpacity)));
  scene_->AddUiElement(k2dBrowsingVisibiltyControlForSiteInfoPrompt,
                       std::move(element));

  element = Create<UiElement>(k2dBrowsingOpacityControlForUpdateKeyboardPrompt,
                              kPhaseNone);
  element->set_bounds_contain_children(true);
  element->SetTransitionedProperties({OPACITY});
  element->AddBinding(
      VR_BIND(bool, Model, model_,
              model->active_modal_prompt_type != kModalPromptTypeUpdateKeyboard,
              UiElement, element.get(),
              view->SetOpacity(value ? 1.0 : kModalPromptFadeOpacity)));
  scene_->AddUiElement(k2dBrowsingOpacityControlForAudioPermissionPrompt,
                       std::move(element));

  element = Create<UiElement>(k2dBrowsingOpacityControlForNativeDialogPrompt,
                              kPhaseNone);
  element->set_bounds_contain_children(true);
  element->SetTransitionedProperties({OPACITY});
  element->AddBinding(
      VR_BIND(bool, Model, model_, !model->hosted_platform_ui.hosted_ui_enabled,
              UiElement, element.get(),
              view->SetOpacity(value ? 1.0 : kModalPromptFadeOpacity)));
  scene_->AddUiElement(k2dBrowsingOpacityControlForUpdateKeyboardPrompt,
                       std::move(element));

  element = Create<UiElement>(k2dBrowsingForeground, kPhaseNone);
  element->set_bounds_contain_children(true);
  element->SetTransitionedProperties({OPACITY});
  element->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  VR_BIND_VISIBILITY(element, model->default_browsing_enabled() ||
                                  model->fullscreen_enabled());
  scene_->AddUiElement(k2dBrowsingOpacityControlForNativeDialogPrompt,
                       std::move(element));

  element = Create<UiElement>(k2dBrowsingContentGroup, kPhaseNone);
  element->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
  element->SetTransitionedProperties({TRANSFORM});
  element->set_bounds_contain_children(true);
  element->AddBinding(
      VR_BIND(bool, Model, model_, model->fullscreen_enabled(), UiElement,
              element.get(),
              view->SetTranslate(
                  0, value ? kFullscreenVerticalOffset : kContentVerticalOffset,
                  value ? -kFullscreenDistance : -kContentDistance)));
  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
}

void UiSceneCreator::CreateWebVrRoot() {
  auto element = std::make_unique<UiElement>();
  element->SetName(kWebVrRoot);
  VR_BIND_VISIBILITY(element, model->web_vr_enabled());
  scene_->AddUiElement(kRoot, std::move(element));
}

void UiSceneCreator::CreateExitWarning() {
  auto scrim = std::make_unique<FullScreenRect>();
  scrim->SetName(kScreenDimmer);
  scrim->SetDrawPhase(kPhaseForeground);
  scrim->SetVisible(false);
  scrim->SetOpacity(kScreenDimmerOpacity);
  scrim->SetCenterColor(model_->color_scheme().dimmer_inner);
  scrim->SetEdgeColor(model_->color_scheme().dimmer_outer);
  VR_BIND_VISIBILITY(scrim, model->exiting_vr);
  scene_->AddUiElement(k2dBrowsingViewportAwareRoot, std::move(scrim));

  // Create transient exit warning.
  auto scaler = std::make_unique<ScaledDepthAdjuster>(kExitWarningDistance);
  auto exit_warning_text = std::make_unique<Text>(kExitWarningFontHeightDMM);
  exit_warning_text->SetName(kExitWarningText);
  exit_warning_text->SetDrawPhase(kPhaseForeground);
  exit_warning_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_BROWSER_UNSUPPORTED_PAGE));
  exit_warning_text->SetSize(kExitWarningTextWidthDMM, 0);
  exit_warning_text->SetVisible(true);
  VR_BIND_COLOR(model_, exit_warning_text.get(),
                &ColorScheme::exit_warning_foreground, &Text::SetColor);

  auto exit_warning_bg = std::make_unique<Rect>();
  exit_warning_bg->SetName(kExitWarningBackground);
  exit_warning_bg->SetDrawPhase(kPhaseForeground);
  exit_warning_bg->set_bounds_contain_children(true);
  exit_warning_bg->set_padding(kExitWarningXPaddingDMM,
                               kExitWarningYPaddingDMM);
  exit_warning_bg->set_corner_radius(kExitWarningCornerRadiusDMM);
  exit_warning_bg->AddChild(std::move(exit_warning_text));
  VR_BIND_VISIBILITY(exit_warning_bg, model->exiting_vr);
  VR_BIND_COLOR(model_, exit_warning_bg.get(),
                &ColorScheme::exit_warning_background, &Rect::SetColor);
  scaler->AddChild(std::move(exit_warning_bg));
  scene_->AddUiElement(k2dBrowsingViewportAwareRoot, std::move(scaler));
}

void UiSceneCreator::CreateSystemIndicators() {
  auto backplane =
      Create<InvisibleHitTarget>(kIndicatorBackplane, kPhaseForeground);
  backplane->set_bounds_contain_children(true);
  backplane->set_contributes_to_parent_bounds(false);
  backplane->set_y_anchoring(TOP);
  backplane->set_corner_radius(kIndicatorCornerRadiusDMM);
  backplane->SetTranslate(0, kIndicatorVerticalOffset,
                          kIndicatorDistanceOffset);
  backplane->SetScale(kIndicatorDepth, kIndicatorDepth, 1.0f);
  VR_BIND_VISIBILITY(backplane, !model->fullscreen_enabled());

  auto indicator_layout =
      Create<LinearLayout>(kIndicatorLayout, kPhaseNone, LinearLayout::kRight);
  indicator_layout->set_margin(kIndicatorMarginDMM);

  auto specs = GetIndicatorSpecs();
  for (const auto& spec : specs) {
    auto element = std::make_unique<VectorIconButton>(
        base::RepeatingCallback<void()>(), spec.icon, audio_delegate_);
    element->SetName(spec.name);
    element->SetDrawPhase(kPhaseForeground);
    element->SetSize(kIndicatorHeightDMM, kIndicatorHeightDMM);
    element->SetIconScaleFactor(kIndicatorIconScaleFactor);
    element->set_hover_offset(0.0f);
    element->AddBinding(std::make_unique<Binding<bool>>(
        VR_BIND_LAMBDA(
            [](Model* model, bool CapturingStateModel::*signal,
               bool CapturingStateModel::*background_signal) {
              return model->capturing_state.*signal ||
                     model->capturing_state.*background_signal;
            },
            base::Unretained(model_), spec.signal, spec.background_signal),
        VR_BIND_LAMBDA(
            [](UiElement* view, const bool& value) {
              view->SetVisible(value);
              view->set_requires_layout(value);
            },
            base::Unretained(element.get()))));
    element->AddBinding(std::make_unique<Binding<std::pair<bool, bool>>>(
        VR_BIND_LAMBDA(
            [](UiElement* parent, UiElement* child) {
              return std::make_pair(parent->FirstLaidOutChild() == child,
                                    parent->LastLaidOutChild() == child);
            },
            base::Unretained(indicator_layout.get()),
            base::Unretained(element.get())),
        VR_BIND_LAMBDA(
            [](UiElement* view, const std::pair<bool, bool>& value) {
              CornerRadii radii;
              radii.upper_left = value.first ? kIndicatorCornerRadiusDMM : 0.0f;
              radii.lower_left = radii.upper_left;
              radii.upper_right =
                  value.second ? kIndicatorCornerRadiusDMM : 0.0f;
              radii.lower_right = radii.upper_right;
              view->SetCornerRadii(radii);
            },
            base::Unretained(element.get()))));
    VR_BIND_BUTTON_COLORS(model_, element.get(), &ColorScheme::indicator,
                          &Button::SetButtonColors);

    auto tooltip = Create<Oval>(kNone, kPhaseForeground);
    VR_BIND_COLOR(model_, tooltip.get(),
                  &ColorScheme::webvr_permission_background, &Rect::SetColor);
    tooltip->set_bounds_contain_children(true);
    tooltip->set_padding(kIndicatorXPaddingDMM, kIndicatorYPaddingDMM,
                         kIndicatorXPaddingDMM, kIndicatorYPaddingDMM);
    tooltip->set_y_anchoring(BOTTOM);
    tooltip->set_y_centering(TOP);
    tooltip->SetVisible(false);
    tooltip->SetTranslate(0, kIndicatorOffsetDMM, 0);
    tooltip->set_owner_name_for_test(element->name());
    tooltip->SetTransitionedProperties({OPACITY});
    tooltip->SetType(kTypeTooltip);
    tooltip->AddBinding(VR_BIND_FUNC(bool, Button, element.get(),
                                     model->hovered(), UiElement, tooltip.get(),
                                     SetVisible));

    auto text_element =
        Create<Text>(kNone, kPhaseForeground, kWebVrPermissionFontHeight);
    text_element->SetLayoutMode(kSingleLine);
    text_element->SetColor(SK_ColorWHITE);
    text_element->set_owner_name_for_test(element->name());
    text_element->SetSize(0.0f, kWebVrPermissionFontHeight);
    text_element->SetType(kTypeLabel);
    BindIndicatorText(model_, text_element.get(), spec);
    VR_BIND_COLOR(model_, text_element.get(),
                  &ColorScheme::webvr_permission_foreground, &Text::SetColor);

    tooltip->AddChild(std::move(text_element));
    element->AddChild(std::move(tooltip));
    indicator_layout->AddChild(std::move(element));
  }
  backplane->AddChild(std::move(indicator_layout));
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(backplane));
}

void UiSceneCreator::CreateContentQuad() {
  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  auto hit_plane = Create<InvisibleHitTarget>(kBackplane, kPhaseBackplanes);
  hit_plane->SetSize(kBackplaneSize, kSceneHeight);
  hit_plane->set_contributes_to_parent_bounds(false);

  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(hit_plane));

  auto resizer = Create<Resizer>(kContentResizer, kPhaseNone);
  resizer->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                   model->reposition_window_enabled(), Resizer,
                                   resizer.get(), SetEnabled));
  resizer->AddBinding(VR_BIND_FUNC(gfx::PointF, Model, model_,
                                   model->controller.touchpad_touch_position,
                                   Resizer, resizer.get(), set_touch_position));
  resizer->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                   model->controller.touching_touchpad, Resizer,
                                   resizer.get(), SetTouchingTouchpad));
  resizer->AddBinding(VR_BIND(bool, Model, model_, model->controller.recentered,
                              Resizer, resizer.get(),
                              if (value) { view->Reset(); }));

  auto shadow = Create<Shadow>(kContentQuadShadow, kPhaseForeground);
  shadow->set_intensity(kContentShadowIntesity);
  shadow->SetTranslate(0, 0, -kContentShadowOffset);
  shadow->set_corner_radius(kContentCornerRadius);

  auto main_content = std::make_unique<ContentElement>(
      content_input_delegate_,
      base::BindRepeating(&UiBrowserInterface::OnContentScreenBoundsChanged,
                          base::Unretained(browser_)));
  EventHandlers event_handlers;
  event_handlers.focus_change = base::BindRepeating(
      [](Model* model, ContentElement* e, bool focused) {
        if (focused) {
          e->UpdateInput(model->web_input_text_field_info);
        } else {
          e->UpdateInput(EditedText());
        }
      },
      model_, base::Unretained(main_content.get()));
  main_content->set_event_handlers(event_handlers);
  main_content->SetName(kContentQuad);
  main_content->set_hit_testable(true);
  main_content->SetDrawPhase(kPhaseForeground);
  main_content->SetSize(kContentWidth, kContentHeight);
  main_content->set_corner_radius(kContentCornerRadius);
  main_content->SetTranslate(0, 0, kContentShadowOffset);
  main_content->SetTransitionedProperties({BOUNDS});
  main_content->SetTextInputDelegate(text_input_delegate_);
  main_content->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA([](Model* m) { return m->editing_web_input; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](ContentElement* e, const bool& v) {
            if (v) {
              e->RequestFocus();
            } else {
              e->RequestUnfocus();
            }
          },
          base::Unretained(main_content.get()))));
  main_content->AddBinding(
      VR_BIND(bool, Model, model_, model->fullscreen_enabled(), UiElement,
              main_content.get(),
              view->SetSize(value ? kFullscreenWidth : kContentWidth,
                            value ? kFullscreenHeight : kContentHeight)));
  main_content->AddBinding(
      VR_BIND_FUNC(gfx::Transform, Model, model_, model->projection_matrix,
                   ContentElement, main_content.get(), SetProjectionMatrix));
  main_content->AddBinding(
      VR_BIND_FUNC(unsigned int, Model, model_, model->content_texture_id,
                   ContentElement, main_content.get(), SetTextureId));
  main_content->AddBinding(VR_BIND_FUNC(UiElementRenderer::TextureLocation,
                                        Model, model_, model->content_location,
                                        ContentElement, main_content.get(),
                                        SetTextureLocation));
  main_content->AddBinding(VR_BIND_FUNC(
      unsigned int, Model, model_, model->content_overlay_texture_id,
      ContentElement, main_content.get(), SetOverlayTextureId));
  main_content->AddBinding(
      VR_BIND_FUNC(UiElementRenderer::TextureLocation, Model, model_,
                   model->content_overlay_location, ContentElement,
                   main_content.get(), SetOverlayTextureLocation));
  main_content->AddBinding(std::make_unique<Binding<EditedText>>(
      VR_BIND_LAMBDA([](EditedText* info) { return *info; },
                     base::Unretained(&model_->web_input_text_field_info)),
      VR_BIND_LAMBDA([](ContentElement* e,
                        const EditedText& value) { e->UpdateInput(value); },
                     base::Unretained(main_content.get()))));

  auto frame = Create<Rect>(kContentFrame, kPhaseForeground);
  frame->set_hit_testable(true);
  frame->set_bounds_contain_children(true);
  frame->set_padding(kRepositionFrameEdgePadding, kRepositionFrameTopPadding,
                     kRepositionFrameEdgePadding, kRepositionFrameEdgePadding);
  frame->set_corner_radius(kContentCornerRadius);
  frame->set_bounds_contain_padding(false);
  frame->SetTransitionedProperties({LOCAL_OPACITY});
  frame->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kRepositionFrameTransitionDurationMs));
  VR_BIND_COLOR(model_, frame.get(), &ColorScheme::content_reposition_frame,
                &Rect::SetColor);
  frame->AddBinding(std::make_unique<Binding<gfx::PointF>>(
      VR_BIND_LAMBDA(
          [](Model* model, UiElement* e) {
            HitTestRequest request;
            request.ray_origin = model->controller.laser_origin;
            request.ray_target = model->reticle.target_point;
            HitTestResult result;
            e->HitTest(request, &result);
            return gfx::PointF(
                result.local_hit_point.x() * e->stale_size().width(),
                result.local_hit_point.y() * e->stale_size().height());
          },
          base::Unretained(model_), base::Unretained(frame.get())),
      VR_BIND_LAMBDA(
          [](Model* model, Rect* e, const gfx::PointF& value) {
            gfx::RectF inner(e->stale_size());
            inner.Inset(kRepositionFrameEdgePadding, kRepositionFrameTopPadding,
                        kRepositionFrameEdgePadding,
                        kRepositionFrameEdgePadding);
            gfx::RectF outer(e->stale_size());
            outer.Inset(
                kRepositionFrameEdgePadding,
                kRepositionFrameTopPadding - kRepositionFrameHitPlaneTopPadding,
                kRepositionFrameEdgePadding, kRepositionFrameEdgePadding);
            const bool is_on_frame = outer.Contains(value) &&
                                     !inner.Contains(value) &&
                                     model->reposition_window_permitted();
            e->SetLocalOpacity(is_on_frame ? 1.0f : 0.0f);
          },
          base::Unretained(model_), base::Unretained(frame.get()))));

  auto plane =
      Create<InvisibleHitTarget>(kContentFrameHitPlane, kPhaseForeground);
  plane->set_bounds_contain_children(true);
  plane->set_bounds_contain_padding(false);
  plane->set_corner_radius(kContentCornerRadius);
  plane->set_cursor_type(kCursorReposition);
  Sounds sounds;
  sounds.button_up = kSoundButtonClick;
  plane->SetSounds(sounds, audio_delegate_);
  plane->set_padding(0, kRepositionFrameHitPlaneTopPadding, 0, 0);
  plane->set_event_handlers(CreateRepositioningHandlers(model_, scene_));
  plane->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                 model->reposition_window_permitted(),
                                 UiElement, plane.get(), set_hit_testable));

  shadow->AddChild(std::move(main_content));
  resizer->AddChild(std::move(shadow));
  plane->AddChild(std::move(resizer));
  frame->AddChild(std::move(plane));
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(frame));

  // Limit reticle distance to a sphere based on maximum content distance.
  scene_->set_background_distance(kFullscreenDistance *
                                  kBackgroundDistanceMultiplier);
}

void UiSceneCreator::CreateWebVrSubtree() {
  CreateSplashScreenForDirectWebVrLaunch();
  CreateWebVrOverlayElements();
  CreateWebVrTimeoutScreen();

  // This is needed to for accepting permissions in WebVR mode.
  auto hosted_ui_root =
      CreateHostedUi(model_, browser_, content_input_delegate_, kWebVrHostedUi,
                     kTimeoutScreenDisatance);
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(hosted_ui_root));

  // Note, this cannot be a descendant of the viewport aware root, otherwise it
  // will fade out when the viewport aware elements reposition.
  auto bg = std::make_unique<FullScreenRect>();
  bg->SetName(kWebVrBackground);
  bg->SetDrawPhase(kPhaseBackground);
  bg->SetVisible(false);
  bg->SetColor(model_->color_scheme().web_vr_background);
  bg->SetTransitionedProperties({OPACITY});
  VR_BIND_VISIBILITY(
      bg, model->web_vr_enabled() && (!model->web_vr.presenting_web_vr() ||
                                      model->web_vr.showing_hosted_ui));
  auto grid = CreateGrid(model_, kWebVrFloor);
  VR_BIND_COLOR(model_, grid.get(), &ColorScheme::web_vr_floor_center,
                &Grid::SetCenterColor);
  VR_BIND_COLOR(model_, grid.get(), &ColorScheme::web_vr_floor_edge,
                &Grid::SetEdgeColor);
  VR_BIND_COLOR(model_, grid.get(), &ColorScheme::web_vr_floor_grid,
                &Grid::SetGridColor);
  grid->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* model, UiElement* timeout_screen) {
            return model->web_vr_enabled() &&
                   (model->web_vr.showing_hosted_ui ||
                    timeout_screen->GetTargetOpacity() != 0.f);
          },
          base::Unretained(model_),
          base::Unretained(scene_->GetUiElementByName(kWebVrTimeoutRoot))),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          base::Unretained(grid.get()))));
  bg->AddChild(std::move(grid));
  scene_->AddUiElement(kWebVrRoot, std::move(bg));
}

void UiSceneCreator::CreateSplashScreenForDirectWebVrLaunch() {
  // Create transient parent.
  // TODO(crbug.com/762074): We should timeout after some time and show an
  // error if the user is stuck on the splash screen.
  auto transient_parent = std::make_unique<ShowUntilSignalTransientElement>(
      base::TimeDelta::FromSeconds(kSplashScreenMinDurationSeconds),
      base::TimeDelta::Max(),
      base::BindRepeating(
          [](Model* model) {
            DCHECK(model->web_vr.awaiting_min_splash_screen_duration());
            // TODO(ymalik): The assumption here is that the WebVR VSync will be
            // paused until the min splash screen duration passes. This state
            // change should be driven by the scheduler in the future and the UI
            // should act on it.
            model->web_vr.state = kWebVrAwaitingFirstFrame;
          },
          base::Unretained(model_)),
      base::BindRepeating(
          [](Model* model, UiBrowserInterface* browser,
             TransientElementHideReason reason) {
            if (reason == TransientElementHideReason::kTimeout) {
              browser->ExitPresent();
            }
          },
          base::Unretained(model_), base::Unretained(browser_)));
  transient_parent->SetName(kSplashScreenTransientParent);
  transient_parent->SetTransitionedProperties({OPACITY});
  VR_BIND_VISIBILITY(transient_parent,
                     model->web_vr_autopresentation_enabled());
  transient_parent->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      model->web_vr_autopresentation_enabled() &&
          model->web_vr.state > kWebVrAwaitingFirstFrame,
      ShowUntilSignalTransientElement, transient_parent.get(), Signal));
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(transient_parent));

  // Add "Running in Chrome" text.
  auto text_scaler =
      std::make_unique<ScaledDepthAdjuster>(kSplashScreenTextDistance);
  auto text = std::make_unique<Text>(kSplashScreenTextFontHeightDMM);
  VR_BIND_COLOR(model_, text.get(), &ColorScheme::splash_screen_text_color,
                &Text::SetColor);
  text->SetText(l10n_util::GetStringUTF16(IDS_VR_RUNNING_IN_CHROME_MESSAGE));
  text->SetName(kSplashScreenText);
  text->SetDrawPhase(kPhaseForeground);
  text->SetSize(kSplashScreenTextWidthDMM, 0);
  text->SetTranslate(0, kSplashScreenTextVerticalOffsetDMM, 0);
  text_scaler->AddChild(std::move(text));
  scene_->AddUiElement(kSplashScreenTransientParent, std::move(text_scaler));
}

void UiSceneCreator::CreateWebVrTimeoutScreen() {
  auto scaler = std::make_unique<ScaledDepthAdjuster>(kTimeoutScreenDisatance);
  scaler->SetName(kWebVrTimeoutRoot);
  scaler->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* model, UiElement* splash_screen) {
            // The timeout UI should only be visible when the splash screen is
            // not visible.
            return (model->web_vr.state == kWebVrTimeoutImminent ||
                    model->web_vr.state == kWebVrTimedOut) &&
                   splash_screen->GetTargetOpacity() == 0.f;
          },
          base::Unretained(model_),
          base::Unretained(
              scene_->GetUiElementByName(kSplashScreenTransientParent))),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          base::Unretained(scaler.get()))));

  auto spinner = std::make_unique<Spinner>(512);
  spinner->SetName(kWebVrTimeoutSpinner);
  spinner->SetDrawPhase(kPhaseForeground);
  spinner->SetTransitionedProperties({OPACITY});
  spinner->SetVisible(false);
  spinner->SetSize(kTimeoutSpinnerSizeDMM, kTimeoutSpinnerSizeDMM);
  spinner->SetTranslate(0, kTimeoutSpinnerVerticalOffsetDMM, 0);
  spinner->SetColor(model_->color_scheme().web_vr_timeout_spinner);
  spinner->set_hit_testable(true);
  VR_BIND_VISIBILITY(spinner, model->web_vr.state == kWebVrTimeoutImminent);

  auto timeout_message = Create<Rect>(kWebVrTimeoutMessage, kPhaseForeground);
  timeout_message->SetVisible(false);
  timeout_message->set_hit_testable(true);
  timeout_message->set_bounds_contain_children(true);
  timeout_message->set_corner_radius(kTimeoutMessageCornerRadiusDMM);
  timeout_message->SetTransitionedProperties({OPACITY, TRANSFORM});
  timeout_message->set_padding(kTimeoutMessageHorizontalPaddingDMM,
                               kTimeoutMessageVerticalPaddingDMM);
  VR_BIND_VISIBILITY(timeout_message, model->web_vr.state == kWebVrTimedOut);
  timeout_message->SetColor(
      model_->color_scheme().web_vr_timeout_message_background);

  auto timeout_layout = Create<LinearLayout>(kWebVrTimeoutMessageLayout,
                                             kPhaseNone, LinearLayout::kRight);
  timeout_layout->set_margin(kTimeoutMessageLayoutGapDMM);

  auto timeout_icon =
      Create<VectorIcon>(kWebVrTimeoutMessageIcon, kPhaseForeground, 512);
  timeout_icon->SetIcon(kSadTabIcon);
  timeout_icon->set_hit_testable(true);
  timeout_icon->SetSize(kTimeoutMessageIconWidthDMM,
                        kTimeoutMessageIconHeightDMM);

  auto timeout_text = Create<Text>(kWebVrTimeoutMessageText, kPhaseForeground,
                                   kTimeoutMessageTextFontHeightDMM);
  timeout_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_TIMEOUT_MESSAGE));
  timeout_text->SetColor(
      model_->color_scheme().web_vr_timeout_message_foreground);
  timeout_text->SetAlignment(UiTexture::kTextAlignmentLeft);
  timeout_text->SetSize(kTimeoutMessageTextWidthDMM,
                        kTimeoutMessageTextHeightDMM);
  timeout_text->set_hit_testable(true);

  auto button_scaler =
      std::make_unique<ScaledDepthAdjuster>(kTimeoutButtonDepthOffset);

  auto button =
      Create<DiscButton>(kWebVrTimeoutMessageButton, kPhaseForeground,
                         base::BindRepeating(&UiBrowserInterface::ExitPresent,
                                             base::Unretained(browser_)),
                         vector_icons::kClose16Icon, audio_delegate_);
  button->SetVisible(false);
  button->SetTranslate(0, -kTimeoutMessageTextWidthDMM, 0);
  button->SetRotate(1, 0, 0, kTimeoutButtonRotationRad);
  button->SetTransitionedProperties({OPACITY});
  button->SetSize(kWebVrTimeoutMessageButtonDiameterDMM,
                  kWebVrTimeoutMessageButtonDiameterDMM);
  VR_BIND_VISIBILITY(button, model->web_vr.state == kWebVrTimedOut);
  VR_BIND_BUTTON_COLORS(model_, button.get(), &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);

  auto timeout_button_text =
      Create<Text>(kWebVrTimeoutMessageButtonText, kPhaseForeground,
                   kTimeoutMessageTextFontHeightDMM);

  // Disc-style button text is not uppercase. See https://crbug.com/787654.
  timeout_button_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_EXIT_BUTTON_LABEL));
  timeout_button_text->SetColor(model_->color_scheme().web_vr_timeout_spinner);
  timeout_button_text->SetSize(kTimeoutButtonTextWidthDMM,
                               kTimeoutButtonTextHeightDMM);
  timeout_button_text->set_y_anchoring(BOTTOM);
  timeout_button_text->SetTranslate(0, -kTimeoutButtonTextVerticalOffsetDMM, 0);
  timeout_button_text->set_hit_testable(true);

  button->AddChild(std::move(timeout_button_text));
  timeout_layout->AddChild(std::move(timeout_icon));
  timeout_layout->AddChild(std::move(timeout_text));
  timeout_message->AddChild(std::move(timeout_layout));
  button_scaler->AddChild(std::move(button));
  timeout_message->AddChild(std::move(button_scaler));
  scaler->AddChild(std::move(timeout_message));
  scaler->AddChild(std::move(spinner));
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(scaler));
}

void UiSceneCreator::CreateBackground() {
  // Textured background.
  auto background =
      Create<Background>(k2dBrowsingTexturedBackground, kPhaseBackground);
  background->SetVisible(true);
  VR_BIND_VISIBILITY(background, model->background_loaded);
  background->AddBinding(
      VR_BIND_FUNC(float, Model, model_, model->color_scheme().normal_factor,
                   Background, background.get(), SetNormalFactor));
  background->AddBinding(
      VR_BIND_FUNC(float, Model, model_, model->color_scheme().incognito_factor,
                   Background, background.get(), SetIncognitoFactor));
  background->AddBinding(VR_BIND_FUNC(
      float, Model, model_, model->color_scheme().fullscreen_factor, Background,
      background.get(), SetFullscreenFactor));
  scene_->AddUiElement(k2dBrowsingBackground, std::move(background));

  auto element = Create<UiElement>(k2dBrowsingDefaultBackground, kPhaseNone);
  VR_BIND_VISIBILITY(element, !model->background_loaded);
  scene_->AddUiElement(k2dBrowsingBackground, std::move(element));

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
    auto panel_element = Create<Rect>(panel.name, kPhaseBackground);
    panel_element->SetSize(kSceneSize, kSceneSize);
    panel_element->SetTranslate(panel.x_offset * kSceneSize / 2,
                                panel.y_offset * kSceneSize / 2,
                                panel.z_offset * kSceneSize / 2);
    panel_element->SetRotate(panel.x_rotation, panel.y_rotation, 0,
                             base::kPiFloat / 2 * panel.angle);
    VR_BIND_COLOR(model_, panel_element.get(), &ColorScheme::world_background,
                  &Rect::SetColor);
    VR_BIND_VISIBILITY(panel_element, model->browsing_enabled());
    scene_->AddUiElement(k2dBrowsingDefaultBackground,
                         std::move(panel_element));
  }

  auto stars = Create<Stars>(kStars, kPhaseBackground);
  stars->SetRotate(1, 0, 0, base::kPiFloat * 0.5);
  scene_->AddUiElement(k2dBrowsingTexturedBackground, std::move(stars));

  auto grid = CreateGrid(model_, kNone);
  grid->SetOpacity(kGridOpacity);
  scene_->AddUiElement(k2dBrowsingTexturedBackground, std::move(grid));

  auto floor = CreateGrid(model_, kFloor);
  VR_BIND_COLOR(model_, floor.get(), &ColorScheme::floor,
                &Grid::SetCenterColor);
  VR_BIND_COLOR(model_, floor.get(), &ColorScheme::world_background,
                &Grid::SetEdgeColor);
  VR_BIND_COLOR(model_, floor.get(), &ColorScheme::floor_grid,
                &Grid::SetGridColor);
  scene_->AddUiElement(k2dBrowsingDefaultBackground, std::move(floor));

  // Ceiling.
  auto ceiling = Create<Rect>(kCeiling, kPhaseBackground);
  ceiling->set_hit_testable(true);
  ceiling->set_focusable(false);
  ceiling->SetSize(kSceneSize, kSceneSize);
  ceiling->SetTranslate(0.0, kSceneHeight / 2, 0.0);
  ceiling->SetRotate(1, 0, 0, base::kPiFloat / 2);
  VR_BIND_COLOR(model_, ceiling.get(), &ColorScheme::ceiling,
                &Rect::SetCenterColor);
  VR_BIND_COLOR(model_, ceiling.get(), &ColorScheme::world_background,
                &Rect::SetEdgeColor);
  scene_->AddUiElement(k2dBrowsingDefaultBackground, std::move(ceiling));
}

void UiSceneCreator::CreateViewportAwareRoot() {
  auto element = std::make_unique<ViewportAwareRoot>();
  element->SetName(kWebVrViewportAwareRoot);
  scene_->AddUiElement(kWebVrRoot, std::move(element));

  element = std::make_unique<ViewportAwareRoot>();
  element->SetName(k2dBrowsingViewportAwareRoot);
  element->set_contributes_to_parent_bounds(false);
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(element));
}

void UiSceneCreator::CreateVoiceSearchUiGroup() {
  auto speech_recognition_root = std::make_unique<UiElement>();
  speech_recognition_root->SetName(kSpeechRecognitionRoot);
  speech_recognition_root->set_contributes_to_parent_bounds(false);
  speech_recognition_root->SetTranslate(0.f, 0.f, -kContentDistance);
  speech_recognition_root->SetTransitionedProperties({OPACITY});
  speech_recognition_root->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(
          kSpeechRecognitionOpacityAnimationDurationMs));
  // Set initial visibility so we don't see the voice search ui fade out.
  speech_recognition_root->SetVisibleImmediately(false);
  scene_->AddUiElement(k2dBrowsingRepositioner,
                       std::move(speech_recognition_root));

  auto inner_circle = std::make_unique<Rect>();
  inner_circle->SetName(kSpeechRecognitionCircle);
  inner_circle->SetDrawPhase(kPhaseForeground);
  inner_circle->SetSize(kCloseButtonDiameter * 2, kCloseButtonDiameter * 2);
  inner_circle->set_corner_radius(kCloseButtonDiameter);
  VR_BIND_COLOR(model_, inner_circle.get(),
                &ColorScheme::speech_recognition_circle_background,
                &Rect::SetColor);
  scene_->AddUiElement(kSpeechRecognitionRoot, std::move(inner_circle));

  auto microphone_icon = std::make_unique<VectorIcon>(512);
  microphone_icon->SetIcon(vector_icons::kMicIcon);
  microphone_icon->SetName(kSpeechRecognitionMicrophoneIcon);
  microphone_icon->SetDrawPhase(kPhaseForeground);
  microphone_icon->SetSize(kCloseButtonDiameter, kCloseButtonDiameter);
  scene_->AddUiElement(kSpeechRecognitionRoot, std::move(microphone_icon));

  auto speech_result_parent = CreateTransientParent(
      kSpeechRecognitionResult, kSpeechRecognitionResultTimeoutSeconds, false);

  // We need to explicitly set the initial visibility of
  // kSpeechRecognitionResult as k2dBrowsingForeground's visibility depends on
  // it in a binding. However, k2dBrowsingForeground's binding updated before
  // kSpeechRecognitionResult. So the initial value needs to be correctly set
  // instead of depend on binding to kick in.
  speech_result_parent->SetVisibleImmediately(false);
  speech_result_parent->SetTransitionedProperties({OPACITY});
  speech_result_parent->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  speech_result_parent->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return m->speech.recognition_result.empty(); },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& v) {
            if (v) {
              e->SetVisible(false);
            } else {
              e->SetVisibleImmediately(true);
            }
          },
          speech_result_parent.get())));
  auto speech_result =
      std::make_unique<Text>(kVoiceSearchRecognitionResultTextHeight);
  speech_result->SetName(kSpeechRecognitionResultText);
  speech_result->SetDrawPhase(kPhaseForeground);
  speech_result->SetTranslate(0.f, kSpeechRecognitionResultTextYOffset, 0.f);
  speech_result->SetSize(kVoiceSearchRecognitionResultTextWidth, 0);
  speech_result->SetAlignment(UiTexture::kTextAlignmentCenter);
  VR_BIND_COLOR(model_, speech_result.get(), &ColorScheme::prompt_foreground,
                &Text::SetColor);
  speech_result->AddBinding(VR_BIND_FUNC(base::string16, Model, model_,
                                         model->speech.recognition_result, Text,
                                         speech_result.get(), SetText));
  speech_result_parent->AddChild(std::move(speech_result));

  auto hit_target = std::make_unique<InvisibleHitTarget>();
  hit_target->SetName(kSpeechRecognitionResultBackplane);
  hit_target->SetDrawPhase(kPhaseForeground);
  hit_target->SetSize(kBackplaneSize, kBackplaneSize);
  speech_result_parent->AddChild(std::move(hit_target));

  auto speech_recognition_listening = std::make_unique<UiElement>();
  UiElement* listening_ui_root = speech_recognition_listening.get();
  speech_recognition_listening->SetName(kSpeechRecognitionListening);
  // We need to explicitly set the initial visibility of this element for the
  // same reason as kSpeechRecognitionResult.
  speech_recognition_listening->SetVisibleImmediately(false);
  speech_recognition_listening->SetTransitionedProperties({OPACITY});
  speech_recognition_listening->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(
          kSpeechRecognitionOpacityAnimationDurationMs));
  speech_recognition_listening->AddBinding(
      std::make_unique<Binding<std::pair<bool, float>>>(
          VR_BIND_LAMBDA(
              [](Model* m, UiElement* result_parent) {
                return std::pair<bool, float>(
                    m->voice_search_enabled(),
                    result_parent->GetTargetOpacity());
              },
              base::Unretained(model_),
              base::Unretained(speech_result_parent.get())),
          VR_BIND_LAMBDA(
              [](UiElement* listening, const std::pair<bool, float>& value) {
                if (!value.first && value.second != 0.f) {
                  listening->SetVisibleImmediately(false);
                } else {
                  listening->SetVisible(value.first);
                }
              },
              base::Unretained(listening_ui_root))));
  scene_->AddUiElement(kSpeechRecognitionRoot,
                       std::move(speech_recognition_listening));
  scene_->AddUiElement(kSpeechRecognitionRoot, std::move(speech_result_parent));

  auto growing_circle = std::make_unique<Throbber>();
  growing_circle->SetName(kSpeechRecognitionListeningGrowingCircle);
  growing_circle->SetDrawPhase(kPhaseForeground);
  growing_circle->SetSize(kCloseButtonDiameter * 2, kCloseButtonDiameter * 2);
  growing_circle->set_corner_radius(kCloseButtonDiameter);
  VR_BIND_COLOR(model_, growing_circle.get(),
                &ColorScheme::speech_recognition_circle_background,
                &Rect::SetColor);
  growing_circle->AddBinding(
      VR_BIND(int, Model, model_, model->speech.speech_recognition_state,
              Throbber, growing_circle.get(),
              view->SetCircleGrowAnimationEnabled(
                  value == SPEECH_RECOGNITION_IN_SPEECH ||
                  value == SPEECH_RECOGNITION_RECOGNIZING ||
                  value == SPEECH_RECOGNITION_READY)));
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(growing_circle));

  auto close_button = Create<DiscButton>(
      kSpeechRecognitionListeningCloseButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::SetVoiceSearchActive,
                          base::Unretained(browser_), false),
      vector_icons::kClose16Icon, audio_delegate_);
  close_button->SetSize(kVoiceSearchCloseButtonDiameter,
                        kVoiceSearchCloseButtonDiameter);
  close_button->set_hover_offset(kButtonZOffsetHoverDMM * kContentDistance);
  close_button->SetTranslate(0, -kVoiceSearchCloseButtonYOffset, 0);
  close_button->SetRotate(
      1, 0, 0, atan(-kVoiceSearchCloseButtonYOffset / kContentDistance));
  VR_BIND_BUTTON_COLORS(model_, close_button.get(),
                        &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);
  scene_->AddUiElement(kSpeechRecognitionListening, std::move(close_button));

  auto* root = scene_->GetUiElementByName(kSpeechRecognitionRoot);
  root->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* model, UiElement* speech_listening,
             UiElement* speech_result_parent) {
            // The speech recognition root should be visible ad long as the
            // speech listening or result subtree is visibe.
            return model->voice_search_enabled() ||
                   speech_listening->GetTargetOpacity() != 0.f ||
                   speech_result_parent->GetTargetOpacity() != 0.f;
          },
          base::Unretained(model_),
          base::Unretained(
              scene_->GetUiElementByName(kSpeechRecognitionListening)),
          base::Unretained(
              scene_->GetUiElementByName(kSpeechRecognitionResult))),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          base::Unretained(root))));

  BIND_VISIBILITY_CONTROL_FOR_VOICE(
      scene_->GetUiElementByName(k2dBrowsingVisibiltyControlForVoice), model_,
      browsing_enabled());
  BIND_VISIBILITY_CONTROL_FOR_VOICE(
      scene_->GetUiElementByName(kOmniboxVisibiltyControlForVoice), model_,
      omnibox_editing_enabled());
}

void UiSceneCreator::CreateContentRepositioningAffordance() {
  auto content_toggle =
      Create<UiElement>(kContentRepositionVisibilityToggle, kPhaseNone);
  content_toggle->SetTransitionedProperties({OPACITY});
  content_toggle->set_bounds_contain_children(true);
  content_toggle->AddBinding(VR_BIND_FUNC(
      float, Model, model_,
      model->reposition_window_enabled() ? kRepositionContentOpacity : 1.0f,
      UiElement, content_toggle.get(), SetOpacity));
  scene_->AddParentUiElement(k2dBrowsingForeground,
                             std::move(content_toggle));

  auto hit_plane =
      Create<InvisibleHitTarget>(kContentRepositionHitPlane, kPhaseForeground);
  hit_plane->set_contributes_to_parent_bounds(false);
  hit_plane->SetSize(kSceneSize, kSceneSize);
  hit_plane->SetTranslate(0.0f, 0.0f, -kContentDistance);
  hit_plane->set_cursor_type(kCursorReposition);
  Sounds sounds;
  sounds.button_up = kSoundButtonClick;
  hit_plane->SetSounds(sounds, audio_delegate_);
  EventHandlers event_handlers;
  event_handlers.button_up = base::BindRepeating(
      [](Model* m) {
        DCHECK(m->reposition_window_enabled());
        m->pop_mode(kModeRepositionWindow);
      },
      base::Unretained(model_));
  hit_plane->set_event_handlers(event_handlers);
  VR_BIND_VISIBILITY(hit_plane, model->reposition_window_enabled());
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(hit_plane));
}

void UiSceneCreator::CreateController() {
  auto root = std::make_unique<UiElement>();
  root->SetName(kControllerRoot);
  VR_BIND_VISIBILITY(root, model->browsing_enabled() ||
                               model->web_vr.state == kWebVrTimedOut ||
                               model->hosted_platform_ui.hosted_ui_enabled);
  scene_->AddUiElement(kRoot, std::move(root));

  auto group = std::make_unique<UiElement>();
  group->SetName(kControllerGroup);
  group->SetTransitionedProperties({OPACITY});
  group->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return !m->controller.quiescent || !m->skips_redraw_when_not_dirty;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& visible) {
            e->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
                visible ? kControllerFadeInMs : kControllerFadeOutMs));
            e->SetVisible(visible);
          },
          base::Unretained(group.get()))));
  scene_->AddUiElement(kControllerRoot, std::move(group));

  auto controller = CreateControllerElement(model_);

  auto callout_group = Create<UiElement>(kNone, kPhaseNone);
  callout_group->SetVisible(false);
  callout_group->SetTransitionedProperties({OPACITY});
  callout_group->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kControllerLabelTransitionDurationMs));
  VR_BIND_VISIBILITY(callout_group, model->controller.resting_in_viewport);

  auto trackpad_button = CreateControllerLabel(
      kControllerTrackpadLabel, kControllerTrackpadOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_TRACKPAD), model_);
  VR_BIND_VISIBILITY(trackpad_button, !model->reposition_window_enabled());
  callout_group->AddChild(std::move(trackpad_button));

  auto reposition_button = CreateControllerLabel(
      kControllerTrackpadRepositionLabel, kControllerTrackpadOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_TRACKPAD_REPOSITION), model_);
  VR_BIND_VISIBILITY(reposition_button, model->reposition_window_enabled());
  callout_group->AddChild(std::move(reposition_button));

  auto exit_button_label = CreateControllerLabel(
      kControllerExitButtonLabel, kControllerExitButtonOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_EXIT), model_);
  VR_BIND_VISIBILITY(exit_button_label, model->fullscreen_enabled());
  callout_group->AddChild(std::move(exit_button_label));

  auto back_button_label = CreateControllerLabel(
      kControllerBackButtonLabel, kControllerBackButtonOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_BACK), model_);
  VR_BIND_VISIBILITY(back_button_label, model->omnibox_editing_enabled() ||
                                            model->voice_search_enabled());
  callout_group->AddChild(std::move(back_button_label));

  auto reposition_finish_button = CreateControllerLabel(
      kControllerRepositionFinishLabel, kControllerBackButtonOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_APP_REPOSITION), model_);
  VR_BIND_VISIBILITY(reposition_finish_button,
                     model->reposition_window_enabled());
  callout_group->AddChild(std::move(reposition_finish_button));

  controller->AddChild(std::move(callout_group));

  scene_->AddUiElement(kControllerGroup, std::move(controller));

  auto reticle_laser_group = Create<UiElement>(kReticleLaserGroup, kPhaseNone);
  reticle_laser_group->SetTransitionedProperties({OPACITY});
  reticle_laser_group->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kControllerLabelTransitionDurationMs));
  VR_BIND_VISIBILITY(reticle_laser_group, !model->reposition_window_enabled());

  auto laser = std::make_unique<Laser>(model_);
  laser->SetDrawPhase(kPhaseForeground);
  laser->AddBinding(VR_BIND_FUNC(float, Model, model_,
                                 model->controller.opacity, Laser, laser.get(),
                                 SetOpacity));

  auto reticle = std::make_unique<Reticle>(scene_, model_);
  reticle->SetDrawPhase(kPhaseForeground);

  auto reposition_group = Create<UiElement>(kRepositionCursor, kPhaseNone);
  VR_BIND_VISIBILITY(reposition_group,
                     model->reticle.cursor_type == kCursorReposition);

  auto reposition_bg = Create<Rect>(kNone, kPhaseForeground);
  reposition_bg->set_owner_name_for_test(kRepositionCursor);
  reposition_bg->SetType(kTypeCursorBackground);
  reposition_bg->SetSize(kRepositionCursorBackgroundSize,
                         kRepositionCursorBackgroundSize);
  reposition_bg->SetDrawPhase(kPhaseForeground);
  VR_BIND_COLOR(model_, reposition_bg.get(),
                &ColorScheme::cursor_background_edge, &Rect::SetEdgeColor);
  VR_BIND_COLOR(model_, reposition_bg.get(),
                &ColorScheme::cursor_background_center, &Rect::SetCenterColor);

  auto reposition_icon = std::make_unique<VectorIcon>(128);
  reposition_icon->set_owner_name_for_test(kRepositionCursor);
  reposition_icon->SetType(kTypeCursorForeground);
  reposition_icon->SetIcon(kRepositionIcon);
  reposition_icon->SetDrawPhase(kPhaseForeground);
  reposition_icon->SetSize(kRepositionCursorSize, kRepositionCursorSize);
  VR_BIND_COLOR(model_, reposition_icon.get(), &ColorScheme::cursor_foreground,
                &VectorIcon::SetColor);

  reposition_group->AddChild(std::move(reposition_bg));
  reposition_group->AddChild(std::move(reposition_icon));

  reticle->AddChild(std::move(reposition_group));

  reticle_laser_group->AddChild(std::move(laser));
  reticle_laser_group->AddChild(std::move(reticle));

  scene_->AddUiElement(kControllerGroup, std::move(reticle_laser_group));
}

void UiSceneCreator::CreateKeyboard() {
  auto visibility_control_root =
      Create<UiElement>(kKeyboardVisibilityControlForVoice, kPhaseNone);
  visibility_control_root->set_contributes_to_parent_bounds(false);
  BIND_VISIBILITY_CONTROL_FOR_VOICE(visibility_control_root.get(), model_,
                                    editing_enabled());

  auto scaler = std::make_unique<ScaledDepthAdjuster>(kKeyboardDistance);
  scaler->SetName(kKeyboardDmmRoot);

  auto keyboard = std::make_unique<Keyboard>();
  keyboard->SetKeyboardDelegate(keyboard_delegate_);
  keyboard->SetDrawPhase(kPhaseForeground);
  keyboard->SetTranslate(0.0, kKeyboardVerticalOffsetDMM, 0.0);
  keyboard->AddBinding(std::make_unique<Binding<std::pair<bool, gfx::PointF>>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return std::pair<bool, gfx::PointF>(
                m->controller.touching_touchpad,
                m->controller.touchpad_touch_position);
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Keyboard* keyboard, const std::pair<bool, gfx::PointF>& value) {
            keyboard->OnTouchStateUpdated(value.first, value.second);
          },
          base::Unretained(keyboard.get()))));
  VR_BIND_VISIBILITY(keyboard,
                     model->editing_input || model->editing_web_input);
  scaler->AddChild(std::move(keyboard));
  visibility_control_root->AddChild(std::move(scaler));
  scene_->AddUiElement(k2dBrowsingRepositioner,
                       std::move(visibility_control_root));
}

void UiSceneCreator::CreateUrlBar() {
  auto positioner = Create<UiElement>(kUrlBarPositioner, kPhaseNone);
  positioner->set_y_anchoring(BOTTOM);
  positioner->SetTranslate(0, kUrlBarRelativeOffset, 0);
  positioner->set_contributes_to_parent_bounds(false);
  scene_->AddUiElement(k2dBrowsingForeground, std::move(positioner));

  auto scaler = std::make_unique<ScaledDepthAdjuster>(kUrlBarDistance);
  scaler->SetName(kUrlBarDmmRoot);
  scaler->set_contributes_to_parent_bounds(false);
  scene_->AddUiElement(kUrlBarPositioner, std::move(scaler));

  auto url_bar = Create<Rect>(kUrlBar, kPhaseForeground);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->set_bounds_contain_children(true);
  url_bar->set_corner_radius(kUrlBarHeightDMM / 2);
  VR_BIND_VISIBILITY(url_bar, !model->fullscreen_enabled());
  VR_BIND_COLOR(model_, url_bar.get(), &ColorScheme::url_bar_background,
                &Rect::SetColor);
  scene_->AddUiElement(kUrlBarDmmRoot, std::move(url_bar));

  auto indicator_bg = std::make_unique<Rect>();
  indicator_bg->SetName(kLoadingIndicator);
  indicator_bg->set_hit_testable(true);
  indicator_bg->SetDrawPhase(kPhaseForeground);
  indicator_bg->SetTranslate(0, kLoadingIndicatorVerticalOffsetDMM, 0);
  indicator_bg->SetSize(kLoadingIndicatorWidthDMM, kLoadingIndicatorHeightDMM);
  indicator_bg->set_y_anchoring(TOP);
  indicator_bg->SetTransitionedProperties({OPACITY});
  indicator_bg->set_corner_radius(kLoadingIndicatorHeightDMM * 0.5f);
  indicator_bg->set_contributes_to_parent_bounds(false);
  VR_BIND_VISIBILITY(indicator_bg, model->loading);
  VR_BIND_COLOR(model_, indicator_bg.get(),
                &ColorScheme::loading_indicator_background, &Rect::SetColor);

  scene_->AddUiElement(kUrlBar, std::move(indicator_bg));

  auto indicator_fg = std::make_unique<Rect>();
  indicator_fg->SetDrawPhase(kPhaseForeground);
  indicator_fg->SetName(kLoadingIndicatorForeground);
  indicator_fg->set_x_anchoring(LEFT);
  indicator_fg->set_corner_radius(kLoadingIndicatorHeightDMM * 0.5f);
  VR_BIND_COLOR(model_, indicator_fg.get(),
                &ColorScheme::loading_indicator_foreground, &Rect::SetColor);
  indicator_fg->AddBinding(std::make_unique<Binding<float>>(
      VR_BIND_LAMBDA([](Model* m) { return m->load_progress; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Rect* r, const float& value) {
            r->SetSize(kLoadingIndicatorWidthDMM * value,
                       kLoadingIndicatorHeightDMM);
            r->SetTranslate(kLoadingIndicatorWidthDMM * value * 0.5f, 0.0f,
                            0.001f);
          },
          base::Unretained(indicator_fg.get()))));

  scene_->AddUiElement(kLoadingIndicator, std::move(indicator_fg));

  auto layout =
      Create<LinearLayout>(kUrlBarLayout, kPhaseNone, LinearLayout::kRight);
  layout->set_bounds_contain_children(true);
  scene_->AddUiElement(kUrlBar, std::move(layout));

  auto back_button = Create<VectorIconButton>(
      kUrlBarBackButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::NavigateBack,
                          base::Unretained(browser_)),
      vector_icons::kBackArrowIcon, audio_delegate_);
  back_button->SetSize(kUrlBarEndButtonWidthDMM, kUrlBarHeightDMM);
  back_button->SetCornerRadii(
      {kUrlBarHeightDMM / 2, 0, kUrlBarHeightDMM / 2, 0});
  back_button->set_hover_offset(0);
  back_button->SetIconScaleFactor(kUrlBarButtonIconSizeDMM / kUrlBarHeightDMM);
  back_button->SetIconTranslation(kUrlBarEndButtonIconOffsetDMM, 0);
  back_button->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                       model->can_navigate_back, Button,
                                       back_button.get(), SetEnabled));
  VR_BIND_BUTTON_COLORS(model_, back_button.get(), &ColorScheme::url_bar_button,
                        &Button::SetButtonColors);
  scene_->AddUiElement(kUrlBarLayout, std::move(back_button));

  auto separator = Create<Rect>(kUrlBarSeparator, kPhaseForeground);
  separator->set_hit_testable(true);
  separator->SetSize(kUrlBarSeparatorWidthDMM, kUrlBarHeightDMM);
  VR_BIND_COLOR(model_, separator.get(), &ColorScheme::url_bar_separator,
                &Rect::SetColor);
  scene_->AddUiElement(kUrlBarLayout, std::move(separator));

  base::RepeatingCallback<void()> url_click_callback;
  if (base::FeatureList::IsEnabled(features::kVrBrowserKeyboard)) {
    url_click_callback = base::BindRepeating(
        [](Model* model, UiBrowserInterface* browser) {
          if (model->needs_keyboard_update) {
            browser->OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate);
          } else {
            model->push_mode(kModeEditingOmnibox);
          }
        },
        base::Unretained(model_), base::Unretained(browser_));
  } else {
    url_click_callback = base::BindRepeating([] {});
  }

  auto origin_region = Create<Button>(kUrlBarOriginRegion, kPhaseForeground,
                                      url_click_callback, audio_delegate_);
  origin_region->set_hit_testable(true);
  origin_region->set_bounds_contain_children(true);
  origin_region->set_hover_offset(0);
  VR_BIND_BUTTON_COLORS(model_, origin_region.get(),
                        &ColorScheme::url_bar_button, &Button::SetButtonColors);
  scene_->AddUiElement(kUrlBarLayout, std::move(origin_region));

  auto origin_layout = Create<LinearLayout>(kUrlBarOriginLayout, kPhaseNone,
                                            LinearLayout::kRight);
  scene_->AddUiElement(kUrlBarOriginRegion, std::move(origin_layout));

  auto security_button_region =
      Create<Rect>(kUrlBarSecurityButtonRegion, kPhaseNone);
  security_button_region->SetType(kTypeSpacer);
  security_button_region->SetSize(kUrlBarEndButtonWidthDMM, kUrlBarHeightDMM);
  scene_->AddUiElement(kUrlBarOriginLayout, std::move(security_button_region));

  auto security_button = Create<VectorIconButton>(
      kUrlBarSecurityButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::ShowPageInfo,
                          base::Unretained(browser_)),
      toolbar::kHttpsInvalidIcon, audio_delegate_);
  security_button->SetIconScaleFactor(kUrlBarButtonIconScaleFactor);
  security_button->SetSize(kUrlBarButtonSizeDMM, kUrlBarButtonSizeDMM);
  security_button->set_corner_radius(kUrlBarItemCornerRadiusDMM);
  security_button->set_hover_offset(kUrlBarButtonHoverOffsetDMM);
  VR_BIND_VISIBILITY(security_button, model->toolbar_state.should_display_url);
  VR_BIND_BUTTON_COLORS(model_, security_button.get(),
                        &ColorScheme::url_bar_button, &Button::SetButtonColors);
  security_button->AddBinding(std::make_unique<Binding<const gfx::VectorIcon*>>(
      VR_BIND_LAMBDA([](Model* m) { return m->toolbar_state.vector_icon; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](VectorIconButton* e, const gfx::VectorIcon* const& icon) {
            if (icon != nullptr) {
              e->SetIcon(*icon);
            }
          },
          security_button.get())));
  security_button->AddBinding(std::make_unique<Binding<ButtonColors>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            ButtonColors colors = m->color_scheme().url_bar_button;
            if (m->toolbar_state.security_level ==
                security_state::SecurityLevel::DANGEROUS) {
              colors.foreground = m->color_scheme().url_bar_dangerous_icon;
            }
            return colors;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](VectorIconButton* e, const ButtonColors& colors) {
            e->SetButtonColors(colors);
          },
          base::Unretained(security_button.get()))));
  scene_->AddUiElement(kUrlBarSecurityButtonRegion, std::move(security_button));

  auto url_text =
      Create<UrlBar>(kUrlBarUrlText, kPhaseForeground, 512,
                     base::BindRepeating(&UiBrowserInterface::OnUnsupportedMode,
                                         base::Unretained(browser_)));
  url_text->SetSize(kUrlBarUrlWidthDMM, kUrlBarHeightDMM);
  VR_BIND_VISIBILITY(url_text, model->toolbar_state.should_display_url);
  url_text->AddBinding(VR_BIND_FUNC(ToolbarState, Model, model_,
                                    model->toolbar_state, UrlBar,
                                    url_text.get(), SetToolbarState));
  url_text->AddBinding(VR_BIND_FUNC(UrlTextColors, Model, model_,
                                    model->color_scheme().url_text, UrlBar,
                                    url_text.get(), SetColors));
  scene_->AddUiElement(kUrlBarOriginLayout, std::move(url_text));

  auto right_margin = Create<Rect>(kNone, kPhaseNone);
  right_margin->SetType(kTypeSpacer);
  right_margin->SetSize(kUrlBarOriginRightMarginDMM, 0);
  scene_->AddUiElement(kUrlBarOriginLayout, std::move(right_margin));

  auto hint_text =
      Create<Text>(kUrlBarHintText, kPhaseForeground, kUrlBarFontHeightDMM);
  hint_text->set_contributes_to_parent_bounds(false);
  hint_text->set_x_anchoring(LEFT);
  hint_text->set_x_centering(LEFT);
  hint_text->SetSize(kUrlBarUrlWidthDMM, kUrlBarHeightDMM);
  hint_text->SetTranslate(kUrlBarOriginContentOffsetDMM, 0, 0);
  hint_text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  hint_text->SetAlignment(UiTexture::kTextAlignmentLeft);
  hint_text->SetText(l10n_util::GetStringUTF16(IDS_SEARCH_OR_TYPE_WEB_ADDRESS));
  VR_BIND_VISIBILITY(hint_text, !model->toolbar_state.should_display_url);
  VR_BIND_COLOR(model_, hint_text.get(), &ColorScheme::url_bar_hint_text,
                &Text::SetColor);
  scene_->AddUiElement(kUrlBarOriginRegion, std::move(hint_text));

  separator = Create<Rect>(kUrlBarSeparator, kPhaseForeground);
  separator->set_hit_testable(true);
  separator->SetSize(kUrlBarSeparatorWidthDMM, kUrlBarHeightDMM);
  VR_BIND_COLOR(model_, separator.get(), &ColorScheme::url_bar_separator,
                &Rect::SetColor);
  scene_->AddUiElement(kUrlBarLayout, std::move(separator));

  auto overflow_button = Create<VectorIconButton>(
      kUrlBarOverflowButton, kPhaseForeground,
      base::BindRepeating(
          [](Model* model) { model->overflow_menu_enabled = true; },
          base::Unretained(model_)),
      kMoreVertIcon, audio_delegate_);
  overflow_button->SetSize(kUrlBarEndButtonWidthDMM, kUrlBarHeightDMM);
  overflow_button->SetCornerRadii(
      {0, kUrlBarHeightDMM / 2, 0, kUrlBarHeightDMM / 2});
  overflow_button->set_hover_offset(0);
  overflow_button->SetIconScaleFactor(kUrlBarButtonIconSizeDMM /
                                      kUrlBarHeightDMM);
  overflow_button->SetIconTranslation(-kUrlBarEndButtonIconOffsetDMM, 0);
  VR_BIND_BUTTON_COLORS(model_, overflow_button.get(),
                        &ColorScheme::url_bar_button, &Button::SetButtonColors);
  scene_->AddUiElement(kUrlBarLayout, std::move(overflow_button));
}

void UiSceneCreator::CreateOverflowMenu() {
  auto overflow_backplane =
      Create<InvisibleHitTarget>(kOverflowMenuBackplane, kPhaseForeground);
  EventHandlers event_handlers;
  event_handlers.button_up = base::BindRepeating(
      [](Model* model) { model->overflow_menu_enabled = false; },
      base::Unretained(model_));
  overflow_backplane->set_event_handlers(event_handlers);
  overflow_backplane->SetSize(kBackplaneSize, kBackplaneSize);
  overflow_backplane->set_y_anchoring(TOP);
  overflow_backplane->SetRotate(1, 0, 0, -kUrlBarRotationRad);
  VR_BIND_VISIBILITY(overflow_backplane, model->overflow_menu_enabled);

  auto overflow_menu = Create<Rect>(kOverflowMenu, kPhaseForeground);
  overflow_menu->set_hit_testable(true);
  overflow_menu->set_y_centering(BOTTOM);
  overflow_menu->set_bounds_contain_children(true);
  overflow_menu->SetTranslate(0, kOverflowMenuOffset, 0);
  overflow_menu->set_corner_radius(kUrlBarItemCornerRadiusDMM);
  VR_BIND_COLOR(model_, overflow_menu.get(), &ColorScheme::omnibox_background,
                &Rect::SetColor);

  auto overflow_layout =
      Create<LinearLayout>(kOverflowMenuLayout, kPhaseNone, LinearLayout::kUp);

  // The forward and refresh buttons are not in the menu layout, but appear as
  // such. Instead, a placeholder element is inserted into the layout to make
  // space for them, and the buttons themselves are anchored to the bottom
  // corners of the overall layout. In the future, when we have more buttons,
  // they may instead be placed in a linear layout (locked to one side).
  std::vector<
      std::tuple<UiElementName, LayoutAlignment, const gfx::VectorIcon&>>
      menu_buttons = {
          {kOverflowMenuFordwardButton, LEFT, vector_icons::kForwardArrowIcon},
          {kOverflowMenuReloadButton, RIGHT, vector_icons::kReloadIcon},
      };
  for (auto& item : menu_buttons) {
    auto button = Create<VectorIconButton>(std::get<0>(item), kPhaseForeground,
                                           base::DoNothing(), std::get<2>(item),
                                           audio_delegate_);
    button->SetType(kTypeOverflowMenuButton);
    button->SetDrawPhase(kPhaseForeground);
    button->SetSize(kUrlBarButtonSizeDMM, kUrlBarButtonSizeDMM);
    button->SetIconScaleFactor(kUrlBarButtonIconScaleFactor);
    button->set_hover_offset(kUrlBarButtonHoverOffsetDMM);
    button->set_corner_radius(kUrlBarItemCornerRadiusDMM);
    button->set_requires_layout(false);
    button->set_contributes_to_parent_bounds(false);
    button->set_x_anchoring(std::get<1>(item));
    button->set_x_centering(std::get<1>(item));
    button->set_y_anchoring(BOTTOM);
    button->set_y_centering(BOTTOM);
    button->SetTranslate(
        kOverflowButtonXOffset * (std::get<1>(item) == RIGHT ? -1 : 1),
        kOverflowMenuYPadding, 0);
    VR_BIND_BUTTON_COLORS(model_, button.get(), &ColorScheme::url_bar_button,
                          &Button::SetButtonColors);

    switch (std::get<0>(item)) {
      case kOverflowMenuFordwardButton:
        button->set_click_handler(base::BindRepeating(
            [](Model* model, UiBrowserInterface* browser) {
              model->overflow_menu_enabled = false;
              browser->NavigateForward();
            },
            base::Unretained(model_), base::Unretained(browser_)));
        button->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                        model->can_navigate_forward, Button,
                                        button.get(), SetEnabled));
        break;
      case kOverflowMenuReloadButton:
        button->set_click_handler(base::BindRepeating(
            [](Model* model, UiBrowserInterface* browser) {
              model->overflow_menu_enabled = false;
              browser->ReloadTab();
            },
            base::Unretained(model_), base::Unretained(browser_)));
        break;
      default:
        break;
    }

    overflow_menu->AddChild(std::move(button));
  }

  // The item that reserves space in the menu layout for the buttons.
  auto button_spacer = Create<Rect>(kNone, kPhaseNone);
  button_spacer->SetType(kTypeSpacer);
  button_spacer->SetSize(kOverflowMenuMinimumWidth,
                         kOverflowButtonRegionHeight);
  overflow_layout->AddChild(std::move(button_spacer));

  std::vector<std::tuple<UiElementName, int>> menu_items = {
      {kOverflowMenuCloseAllIncognitoTabsItem,
       IDS_VR_MENU_CLOSE_INCOGNITO_TABS},
      {kOverflowMenuNewIncognitoTabItem, IDS_VR_MENU_NEW_INCOGNITO_TAB},
  };

  for (auto& item : menu_items) {
    auto layout = std::make_unique<LinearLayout>(LinearLayout::kRight);
    layout->SetType(kTypeOverflowMenuItem);
    layout->SetDrawPhase(kPhaseNone);
    layout->set_layout_length(kOverflowMenuMinimumWidth -
                              2 * kOverflowMenuItemXPadding);

    auto text =
        Create<Text>(kNone, kPhaseForeground, kSuggestionContentTextHeightDMM);
    text->SetDrawPhase(kPhaseForeground);
    text->SetText(l10n_util::GetStringUTF16(std::get<1>(item)));
    text->SetLayoutMode(TextLayoutMode::kSingleLine);
    text->SetAlignment(UiTexture::kTextAlignmentLeft);
    text->AddBinding(VR_BIND_FUNC(
        SkColor, Model, model_, model->color_scheme().url_bar_button.foreground,
        Text, text.get(), SetColor));
    layout->AddChild(std::move(text));

    auto spacer = Create<Rect>(kNone, kPhaseNone);
    spacer->SetType(kTypeSpacer);
    spacer->SetSize(0, kOverflowMenuItemHeight);
    spacer->set_resizable_by_layout(true);
    layout->AddChild(std::move(spacer));

    auto background = Create<Button>(std::get<0>(item), kPhaseForeground,
                                     base::DoNothing(), audio_delegate_);
    background->set_hit_testable(true);
    background->set_bounds_contain_children(true);
    background->set_hover_offset(0);
    background->set_padding(kOverflowMenuItemXPadding, 0);
    VR_BIND_BUTTON_COLORS(model_, background.get(),
                          &ColorScheme::url_bar_button,
                          &Button::SetButtonColors);
    background->AddChild(std::move(layout));

    switch (std::get<0>(item)) {
      case kOverflowMenuCloseAllIncognitoTabsItem:
        background->set_click_handler(base::BindRepeating(
            [](Model* model, UiBrowserInterface* browser) {
              model->overflow_menu_enabled = false;
              browser->CloseAllIncognitoTabs();
            },
            base::Unretained(model_), base::Unretained(browser_)));
        VR_BIND_VISIBILITY(background, model->incognito_tabs_open);
        break;
      case kOverflowMenuNewIncognitoTabItem:
        background->set_click_handler(base::BindRepeating(
            [](Model* model, UiBrowserInterface* browser) {
              model->overflow_menu_enabled = false;
              browser->OpenNewTab(true);
            },
            base::Unretained(model_), base::Unretained(browser_)));
        break;
      default:
        break;
    }

    overflow_layout->AddChild(std::move(background));
  }

  auto top_cap = Create<Rect>(kNone, kPhaseNone);
  top_cap->SetType(kTypeSpacer);
  top_cap->SetSize(kOverflowMenuMinimumWidth, kOverflowMenuYPadding);
  overflow_layout->AddChild(std::move(top_cap));

  overflow_menu->AddChild(std::move(overflow_layout));
  overflow_backplane->AddChild(std::move(overflow_menu));
  scene_->AddUiElement(kUrlBarOverflowButton, std::move(overflow_backplane));
}

void UiSceneCreator::CreateSnackbars() {
  auto snackbar = CreateSnackbar(
      kDownloadedSnackbar, model_, kFileDownloadDoneIcon,
      l10n_util::GetStringUTF16(IDS_VR_COMPONENT_UPDATE_READY),
      base::i18n::ToUpper(l10n_util::GetStringUTF16(IDS_VR_COMPONENT_APPLY)),
      base::DoNothing(), audio_delegate_);
  snackbar->SetVisible(false);
  snackbar->SetRotate(1, 0, 0, kSnackbarMoveInAngle);
  snackbar->SetTransitionedProperties({OPACITY, TRANSFORM});
  snackbar->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kSnackbarTransitionDurationMs));
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(snackbar));
}

void UiSceneCreator::CreateOmnibox() {
  auto visibility_control_root =
      Create<UiElement>(kOmniboxVisibiltyControlForVoice, kPhaseNone);
  visibility_control_root->set_contributes_to_parent_bounds(false);

  auto scaler =
      Create<ScaledDepthAdjuster>(kOmniboxDmmRoot, kPhaseNone, kUrlBarDistance);

  auto visibility_toggle_for_audio_permission = Create<UiElement>(
      kOmniboxVisibilityControlForAudioPermissionPrompt, kPhaseNone);
  // Note that when the audio permissions prompt is triggered from the omnibox
  // editing mode, we don't change the opacity of the background like we do in
  // the default browsing case.
  visibility_toggle_for_audio_permission->AddBinding(VR_BIND_FUNC(
      bool, Model, model_,
      model->active_modal_prompt_type !=
          kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission,
      UiElement, visibility_toggle_for_audio_permission.get(), SetVisible));

  auto omnibox_root = Create<UiElement>(kOmniboxRoot, kPhaseNone);
  omnibox_root->SetTransitionedProperties({OPACITY});
  omnibox_root->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kOmniboxTransitionMs));
  VR_BIND_VISIBILITY(omnibox_root, model->omnibox_editing_enabled());

  // The shadow also controls omnibox Y offset.
  auto shadow = Create<Shadow>(kOmniboxShadow, kPhaseForeground);
  shadow->set_intensity(kOmniboxShadowIntensity);
  shadow->set_y_anchoring(TOP);
  shadow->set_y_centering(BOTTOM);
  shadow->set_corner_radius(kOmniboxCornerRadiusDMM);
  shadow->SetTransitionedProperties({TRANSFORM});
  shadow->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kOmniboxTransitionMs));
  shadow->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA([](Model* m) { return m->omnibox_editing_enabled(); },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& v) {
            float y_offset =
                v ? kOmniboxVerticalOffsetDMM : kUrlBarVerticalOffsetDMM;
            y_offset -= 0.5 * kOmniboxHeightDMM;
            e->SetTranslate(0, y_offset, kOmniboxShadowOffset);
          },
          shadow.get())));

  auto omnibox_outer_layout =
      Create<LinearLayout>(kOmniboxOuterLayout, kPhaseNone, LinearLayout::kUp);

  auto omnibox_suggestion_divider = Create<Rect>(kNone, kPhaseForeground);
  omnibox_suggestion_divider->SetType(kTypeSpacer);
  omnibox_suggestion_divider->SetSize(kOmniboxWidthDMM, kSuggestionGapDMM);
  VR_BIND_COLOR(model_, omnibox_suggestion_divider.get(),
                &ColorScheme::url_bar_separator, &Rect::SetColor);

  auto omnibox_text_field = Create<OmniboxTextField>(
      kOmniboxTextField, kPhaseNone, kOmniboxTextHeightDMM,
      base::BindRepeating(
          [](Model* model, const EditedText& text_input_info) {
            model->omnibox_text_field_info = text_input_info;
          },
          base::Unretained(model_)),
      base::BindRepeating(
          [](UiBrowserInterface* browser, const AutocompleteRequest& request) {
            browser->StartAutocomplete(request);
          },
          base::Unretained(browser_)),
      base::BindRepeating(
          [](UiBrowserInterface* browser) { browser->StopAutocomplete(); },
          base::Unretained(browser_)));
  omnibox_text_field->SetTextInputDelegate(text_input_delegate_);
  omnibox_text_field->set_hit_testable(false);
  omnibox_text_field->SetHintText(
      l10n_util::GetStringUTF16(IDS_SEARCH_OR_TYPE_WEB_ADDRESS));
  omnibox_text_field->SetSize(kOmniboxWidthDMM - 2 * kOmniboxTextMarginDMM -
                                  kOmniboxTextFieldIconButtonSizeDMM -
                                  kOmniboxTextFieldRightMargin,
                              0);

  EventHandlers event_handlers;
  event_handlers.focus_change = base::BindRepeating(
      [](Model* model, TextInput* text_input, bool focused) {
        if (focused) {
          model->editing_input = true;
          text_input->UpdateInput(model->omnibox_text_field_info);
        } else {
          model->editing_input = false;
        }
      },
      base::Unretained(model_), base::Unretained(omnibox_text_field.get()));
  omnibox_text_field->set_event_handlers(event_handlers);

  omnibox_text_field->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, model->has_mode_in_stack(kModeEditingOmnibox),
      OmniboxTextField, omnibox_text_field.get(), SetEnabled));
  omnibox_text_field->AddBinding(std::make_unique<Binding<EditedText>>(
      VR_BIND_LAMBDA(
          [](Model* model) { return model->omnibox_text_field_info; },
          base::Unretained(model_)),
      VR_BIND_LAMBDA([](OmniboxTextField* e,
                        const EditedText& value) { e->UpdateInput(value); },
                     base::Unretained(omnibox_text_field.get()))));
  omnibox_text_field->set_input_committed_callback(base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser, Ui* ui,
         const EditedText& text) {
        if (!model->omnibox_suggestions.empty()) {
          browser->Navigate(model->omnibox_suggestions.front().destination,
                            NavigationMethod::kOmniboxUrlEntry);
          ui->OnUiRequestedNavigation();
        }
      },
      base::Unretained(model_), base::Unretained(browser_),
      base::Unretained(ui_)));
  omnibox_text_field->AddBinding(std::make_unique<Binding<Autocompletion>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            if (!m->omnibox_suggestions.empty()) {
              return m->omnibox_suggestions.front().autocompletion;
            } else {
              return Autocompletion();
            }
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA([](OmniboxTextField* e,
                        const Autocompletion& v) { e->SetAutocompletion(v); },
                     base::Unretained(omnibox_text_field.get()))));
  omnibox_text_field->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return m->omnibox_editing_enabled() &&
                   m->active_modal_prompt_type == kModalPromptTypeNone;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TextInput* e, Model* m, const bool& v) {
            if (v) {
              e->RequestFocus();
            } else {
              e->RequestUnfocus();
            }
          },
          base::Unretained(omnibox_text_field.get()),
          base::Unretained(model_))));
  omnibox_text_field->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return m->has_mode_in_stack(kModeEditingOmnibox); },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TextInput* e, Model* m, const bool& unused) {
            m->omnibox_text_field_info = EditedText();
          },
          base::Unretained(omnibox_text_field.get()),
          base::Unretained(model_))));
  omnibox_text_field->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, model->supports_selection, OmniboxTextField,
      omnibox_text_field.get(), set_allow_inline_autocomplete));

  VR_BIND_COLOR(model_, omnibox_text_field.get(), &ColorScheme::url_bar_text,
                &TextInput::SetTextColor);
  VR_BIND_COLOR(model_, omnibox_text_field.get(),
                &ColorScheme::url_bar_hint_text, &TextInput::SetHintColor);
  omnibox_text_field->AddBinding(std::make_unique<Binding<TextSelectionColors>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return m->color_scheme().omnibox_text_selection; },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TextInput* e, const TextSelectionColors& colors) {
            e->SetSelectionColors(colors);
          },
          base::Unretained(omnibox_text_field.get()))));

  auto mic_button = Create<VectorIconButton>(
      kOmniboxVoiceSearchButton, kPhaseForeground,
      base::BindRepeating(
          [](UiBrowserInterface* b, Ui* ui) { b->SetVoiceSearchActive(true); },
          base::Unretained(browser_), base::Unretained(ui_)),
      vector_icons::kMicIcon, audio_delegate_);
  mic_button->SetSize(kUrlBarButtonSizeDMM, kUrlBarButtonSizeDMM);
  mic_button->SetIconScaleFactor(kUrlBarButtonIconScaleFactor);
  mic_button->set_hover_offset(kUrlBarButtonHoverOffsetDMM);
  mic_button->set_corner_radius(kUrlBarItemCornerRadiusDMM);
  VR_BIND_VISIBILITY(mic_button,
                     model->speech.has_or_can_request_audio_permission &&
                         !model->incognito &&
                         !model->capturing_state.audio_capture_enabled);
  VR_BIND_BUTTON_COLORS(model_, mic_button.get(), &ColorScheme::url_bar_button,
                        &Button::SetButtonColors);

  auto text_field_layout = Create<LinearLayout>(
      kOmniboxTextFieldLayout, kPhaseNone, LinearLayout::kRight);
  text_field_layout->AddChild(
      CreateSpacer(kOmniboxTextMarginDMM, kOmniboxHeightDMM));
  text_field_layout->AddChild(std::move(omnibox_text_field));
  text_field_layout->AddChild(
      CreateSpacer(kOmniboxTextMarginDMM, kOmniboxHeightDMM));
  text_field_layout->AddChild(std::move(mic_button));
  text_field_layout->AddChild(
      CreateSpacer(kOmniboxTextFieldRightMargin, kOmniboxHeightDMM));

  // Set up the vector binding to manage suggestions dynamically.
  SuggestionSetBinding::ModelAddedCallback added_callback = base::BindRepeating(
      &OnSuggestionModelAdded, base::Unretained(scene_),
      base::Unretained(browser_), base::Unretained(ui_),
      base::Unretained(model_), base::Unretained(audio_delegate_));
  SuggestionSetBinding::ModelRemovedCallback removed_callback =
      base::BindRepeating(&OnSuggestionModelRemoved, base::Unretained(scene_));

  auto suggestions_layout =
      Create<LinearLayout>(kOmniboxSuggestions, kPhaseNone, LinearLayout::kUp);
  suggestions_layout->AddBinding(std::make_unique<SuggestionSetBinding>(
      &model_->omnibox_suggestions, added_callback, removed_callback));

  auto button_scaler = Create<ScaledDepthAdjuster>(
      kNone, kPhaseNone, kOmniboxCloseButtonDepthOffset);

  auto close_button = Create<DiscButton>(
      kOmniboxCloseButton, kPhaseForeground,
      base::BindRepeating(
          [](Model* model) { model->pop_mode(kModeEditingOmnibox); },
          base::Unretained(model_)),
      vector_icons::kBackArrowIcon, audio_delegate_);
  close_button->SetSize(kOmniboxCloseButtonDiameterDMM,
                        kOmniboxCloseButtonDiameterDMM);
  close_button->SetTranslate(0, kOmniboxCloseButtonVerticalOffsetDMM, 0);
  close_button->SetRotate(1, 0, 0, atan(kOmniboxCloseButtonVerticalOffsetDMM));
  close_button->set_hover_offset(kButtonZOffsetHoverDMM);
  VR_BIND_BUTTON_COLORS(model_, close_button.get(),
                        &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);

  auto suggestions_outer_layout = Create<LinearLayout>(
      kOmniboxSuggestionsOuterLayout, kPhaseNone, LinearLayout::kUp);
  VR_BIND_VISIBILITY(suggestions_outer_layout,
                     !model->omnibox_suggestions.empty());
  suggestions_outer_layout->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, !model->omnibox_suggestions.empty(), LinearLayout,
      suggestions_outer_layout.get(), set_requires_layout));
  suggestions_outer_layout->AddChild(std::move(omnibox_suggestion_divider));
  suggestions_outer_layout->AddChild(
      CreateSpacer(kOmniboxWidthDMM, kSuggestionVerticalPaddingDMM));
  suggestions_outer_layout->AddChild(std::move(suggestions_layout));
  suggestions_outer_layout->AddChild(
      CreateSpacer(kOmniboxWidthDMM, kSuggestionVerticalPaddingDMM));

  omnibox_outer_layout->AddChild(std::move(text_field_layout));
  omnibox_outer_layout->AddChild(std::move(suggestions_outer_layout));

  // Rounded-corner background of all omnibox and suggestion elements.
  auto omnibox_background = Create<Rect>(kOmniboxBackground, kPhaseForeground);
  omnibox_background->set_bounds_contain_children(true);
  omnibox_background->set_hit_testable(true);
  omnibox_background->set_focusable(false);
  omnibox_background->set_corner_radius(kOmniboxCornerRadiusDMM);
  VR_BIND_COLOR(model_, omnibox_background.get(),
                &ColorScheme::omnibox_background, &Rect::SetColor);
  omnibox_background->AddChild(std::move(omnibox_outer_layout));

  shadow->AddChild(std::move(omnibox_background));

  button_scaler->AddChild(std::move(close_button));

  omnibox_root->AddChild(std::move(shadow));
  omnibox_root->AddChild(std::move(button_scaler));

  visibility_toggle_for_audio_permission->AddChild(std::move(omnibox_root));
  scaler->AddChild(std::move(visibility_toggle_for_audio_permission));

  visibility_control_root->AddChild(std::move(scaler));

  scene_->AddUiElement(k2dBrowsingRepositioner,
                       std::move(visibility_control_root));
}

void UiSceneCreator::CreateCloseButton() {
  base::RepeatingCallback<void()> click_handler = base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser) {
        if (model->fullscreen_enabled()) {
          browser->ExitFullscreen();
        }
        if (model->in_cct) {
          browser->ExitCct();
        }
      },
      base::Unretained(model_), base::Unretained(browser_));
  std::unique_ptr<DiscButton> element =
      Create<DiscButton>(kCloseButton, kPhaseForeground, click_handler,
                         vector_icons::kClose16Icon, audio_delegate_);
  element->set_contributes_to_parent_bounds(false);
  element->SetSize(kCloseButtonDiameter, kCloseButtonDiameter);
  element->set_hover_offset(kButtonZOffsetHoverDMM * kCloseButtonDistance);
  element->set_y_anchoring(BOTTOM);
  element->SetTranslate(0, kCloseButtonRelativeOffset, -kCloseButtonDistance);
  VR_BIND_BUTTON_COLORS(model_, element.get(), &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);

  // Close button is a special control element that needs to be hidden when
  // in WebVR, but it needs to be visible when in cct or fullscreen.
  VR_BIND_VISIBILITY(element, model->fullscreen_enabled() || model->in_cct);
  element->AddBinding(
      VR_BIND(bool, Model, model_, model->fullscreen_enabled(), UiElement,
              element.get(),
              view->SetTranslate(0, kCloseButtonRelativeOffset,
                                 value ? -kCloseButtonFullscreenDistance
                                       : -kCloseButtonDistance)));
  element->AddBinding(VR_BIND(
      bool, Model, model_, model->fullscreen_enabled(), UiElement,
      element.get(),
      view->SetRotate(
          1, 0, 0,
          atan(value ? kCloseButtonFullscreenVerticalOffset /
                           kCloseButtonFullscreenDistance
                     : kCloseButtonVerticalOffset / kCloseButtonDistance))));
  element->AddBinding(VR_BIND(
      bool, Model, model_, model->fullscreen_enabled(), UiElement,
      element.get(),
      view->SetSize(
          value ? kCloseButtonFullscreenDiameter : kCloseButtonDiameter,
          value ? kCloseButtonFullscreenDiameter : kCloseButtonDiameter)));

  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
}

void UiSceneCreator::CreatePrompts() {
  auto prompt_callback = base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser, Prompt::Button button,
         UiUnsupportedMode mode) {
        ExitVrPromptChoice choice = CHOICE_NONE;
        switch (button) {
          case Prompt::NONE:
            choice = CHOICE_NONE;
            break;
          case Prompt::PRIMARY:
            choice = CHOICE_EXIT;
            break;
          case Prompt::SECONDARY:
            choice = CHOICE_STAY;
            break;
        }
        browser->OnExitVrPromptResult(choice, mode);
        model->active_modal_prompt_type = kModalPromptTypeNone;
      },
      base::Unretained(model_), base::Unretained(browser_));
  // Create audio permission prompt.
  auto prompt = CreatePrompt(
      kAudioPermissionPrompt, kAudioPermissionPromptBackplane, model_,
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_DESCRIPTION, vector_icons::kMicIcon,
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_CONTINUE_BUTTON,
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_ABORT_BUTTON, prompt_callback);
  VR_BIND_VISIBILITY(
      prompt.first,
      model->active_modal_prompt_type ==
          kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission);
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(prompt.first));

  // Create keyboard update prompt. Note that we re-use the same button texts as
  // the audio permission prompt.
  prompt = CreatePrompt(
      kUpdateKeyboardPrompt, kNone, model_, IDS_VR_UPDATE_KEYBOARD_PROMPT,
      vector_icons::kInfoOutlineIcon,
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_CONTINUE_BUTTON,
      IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_ABORT_BUTTON, prompt_callback);
  VR_BIND_VISIBILITY(prompt.first, model->active_modal_prompt_type ==
                                       kModalPromptTypeUpdateKeyboard);
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(prompt.first));

  // Create generic unsupported UI exit prompt.
  prompt_callback = base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser, Prompt::Button button,
         UiUnsupportedMode mode) {
        ExitVrPromptChoice choice = CHOICE_NONE;
        switch (button) {
          case Prompt::NONE:
            choice = CHOICE_NONE;
            break;
          case Prompt::PRIMARY:
            choice = CHOICE_STAY;
            break;
          case Prompt::SECONDARY:
            choice = CHOICE_EXIT;
            break;
        }
        browser->OnExitVrPromptResult(choice, mode);
        model->active_modal_prompt_type = kModalPromptTypeNone;
      },
      base::Unretained(model_), base::Unretained(browser_));
  prompt = CreatePrompt(
      kExitPrompt, kNone, model_, IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION,
      vector_icons::kInfoOutlineIcon, IDS_OK,
      IDS_VR_SHELL_EXIT_PROMPT_EXIT_VR_BUTTON, prompt_callback);
  VR_BIND_VISIBILITY(
      prompt.first,
      model->active_modal_prompt_type != kModalPromptTypeNone &&
          model->active_modal_prompt_type !=
              kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission &&
          model->active_modal_prompt_type != kModalPromptTypeUpdateKeyboard);
  prompt.second->AddBinding(std::make_unique<Binding<ModalPromptType>>(
      VR_BIND_LAMBDA([](Model* m) { return m->active_modal_prompt_type; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Prompt* e, const ModalPromptType& p) {
            e->set_reason(GetReasonForPrompt(p));
            switch (p) {
              case kModalPromptTypeExitVRForSiteInfo:
                e->SetContentMessageId(
                    IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION_SITE_INFO);
                break;
              case kModalPromptTypeGenericUnsupportedFeature:  // Fall through.
              default:
                e->SetContentMessageId(IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION);
                break;
            }
          },
          base::Unretained(prompt.second))));
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(prompt.first));
}

void UiSceneCreator::CreateWebVrOverlayElements() {
  // Create url toast shown when WebVR is auto-presented.
  auto parent = CreateTransientParent(kWebVrUrlToastTransientParent,
                                      kWebVrUrlToastTimeoutSeconds, true);
  parent->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* model, UiElement* splash_screen) {
            // The WebVR indicators should only be visible when the splash
            // screen is not visible.
            return model->web_vr_autopresentation_enabled() &&
                   model->web_vr.presenting_web_vr() &&
                   splash_screen->GetTargetOpacity() == 0.f;
          },
          base::Unretained(model_),
          base::Unretained(
              scene_->GetUiElementByName(kSplashScreenTransientParent))),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          base::Unretained(parent.get()))));
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(parent));

  // Create transient WebVR elements.
  auto indicators = Create<LinearLayout>(kWebVrIndicatorLayout, kPhaseNone,
                                         LinearLayout::kDown);
  indicators->SetTranslate(0, 0, kWebVrPermissionDepth);
  indicators->set_margin(kWebVrPermissionOuterMargin);

  IndicatorSpec app_button_spec = {kNone,
                                   kWebVrExclusiveScreenToast,
                                   kRemoveCircleOutlineIcon,
                                   IDS_PRESS_APP_TO_EXIT,
                                   0,
                                   0,
                                   nullptr,
                                   nullptr,
                                   nullptr,
                                   false};
  indicators->AddChild(CreateWebVrIndicator(model_, browser_, app_button_spec));

  IndicatorSpec url_indicator_spec = {kNone,
                                      kWebVrUrlToast,
                                      toolbar::kHttpsInvalidIcon,
                                      0,
                                      0,
                                      0,
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      true};
  indicators->AddChild(
      CreateWebVrIndicator(model_, browser_, url_indicator_spec));

  auto specs = GetIndicatorSpecs();
  for (const auto& spec : specs) {
    indicators->AddChild(CreateWebVrIndicator(model_, browser_, spec));
  }

  parent = CreateTransientParent(kWebVrIndicatorTransience,
                                 kToastTimeoutSeconds, true);
  parent->AddBinding(std::make_unique<Binding<std::tuple<bool, bool, bool>>>(
      VR_BIND_LAMBDA(
          [](Model* model, UiElement* splash_screen) {
            return std::tuple<bool, bool, bool>(
                model->web_vr_enabled() && model->web_vr.presenting_web_vr() &&
                    model->web_vr.has_received_permissions &&
                    splash_screen->GetTargetOpacity() == 0.f,
                model->controller.app_button_long_pressed,
                model->web_vr.showing_hosted_ui);
          },
          base::Unretained(model_),
          base::Unretained(
              scene_->GetUiElementByName(kSplashScreenTransientParent))),
      VR_BIND_LAMBDA(
          [](TransientElement* e, Model* model, UiScene* scene,
             const base::Optional<std::tuple<bool, bool, bool>>& last_value,
             const std::tuple<bool, bool, bool>& value) {
            const bool in_web_vr_presentation = std::get<0>(value);
            const bool in_long_press = std::get<1>(value);
            const bool showing_hosted_ui = std::get<2>(value);
            const bool was_in_long_press =
                last_value && std::get<1>(last_value.value());
            const bool was_showing_hosted_ui =
                last_value && std::get<2>(last_value.value());

            if (!in_web_vr_presentation) {
              e->SetVisibleImmediately(false);
              return;
            }

            // The reason we need the previous state is to disguish the
            // situation where the app button has been released after a long
            // press, and the situation when we want to initially show the
            // indicators.
            if (was_in_long_press && !in_long_press)
              return;

            // Similarly, we need to know when we've finished presenting hosted
            // ui because we should not show indicators then.
            if (was_showing_hosted_ui && !showing_hosted_ui)
              return;

            e->SetVisible(true);
            e->RefreshVisible();
            SetVisibleInLayout(
                scene->GetUiElementByName(kWebVrExclusiveScreenToast),
                !model->web_vr_autopresentation_enabled() &&
                    !model->browsing_disabled && !in_long_press);
            SetVisibleInLayout(scene->GetUiElementByName(kWebVrUrlToast),
                               model->web_vr_autopresentation_enabled() &&
                                   model->toolbar_state.should_display_url);

            auto specs = GetIndicatorSpecs();
            for (const auto& spec : specs) {
              SetVisibleInLayout(
                  scene->GetUiElementByName(spec.webvr_name),
                  model->capturing_state.*spec.signal ||
                      model->capturing_state.*spec.potential_signal ||
                      model->capturing_state.*spec.background_signal);
            }

            e->RemoveKeyframeModels(TRANSFORM);
            if (in_long_press) {
              // We do not do a translation animation for long press.
              e->SetTranslate(0, 0, 0);
              return;
            }

            e->SetTranslate(0, kWebVrPermissionOffsetStart, 0);

            // Build up a keyframe model for the initial transition.
            std::unique_ptr<cc::KeyframedTransformAnimationCurve> curve(
                cc::KeyframedTransformAnimationCurve::Create());

            cc::TransformOperations value_1;
            value_1.AppendTranslate(0, kWebVrPermissionOffsetStart, 0);
            curve->AddKeyframe(cc::TransformKeyframe::Create(
                base::TimeDelta(), value_1,
                cc::CubicBezierTimingFunction::CreatePreset(
                    cc::CubicBezierTimingFunction::EaseType::EASE)));

            cc::TransformOperations value_2;
            value_2.AppendTranslate(0, kWebVrPermissionOffsetOvershoot, 0);
            curve->AddKeyframe(cc::TransformKeyframe::Create(
                base::TimeDelta::FromMilliseconds(kWebVrPermissionOffsetMs),
                value_2,
                cc::CubicBezierTimingFunction::CreatePreset(
                    cc::CubicBezierTimingFunction::EaseType::EASE)));

            cc::TransformOperations value_3;
            value_3.AppendTranslate(0, kWebVrPermissionOffsetFinal, 0);
            curve->AddKeyframe(cc::TransformKeyframe::Create(
                base::TimeDelta::FromMilliseconds(
                    kWebVrPermissionAnimationDurationMs),
                value_3,
                cc::CubicBezierTimingFunction::CreatePreset(
                    cc::CubicBezierTimingFunction::EaseType::EASE)));

            e->AddKeyframeModel(cc::KeyframeModel::Create(
                std::move(curve), Animation::GetNextKeyframeModelId(),
                Animation::GetNextGroupId(), TRANSFORM));
          },
          base::Unretained(parent.get()), base::Unretained(model_),
          base::Unretained(scene_))));

  auto scaler = std::make_unique<ScaledDepthAdjuster>(kWebVrToastDistance);
  scaler->AddChild(std::move(indicators));
  parent->AddChild(std::move(scaler));

  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(parent));
}

void UiSceneCreator::CreateToasts() {
  auto layout = Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kLeft);
  layout->set_contributes_to_parent_bounds(false);
  layout->set_y_anchoring(TOP);
  layout->SetTranslate(0, kIndicatorVerticalOffset, kIndicatorDistanceOffset);
  layout->set_margin(kWebVrPermissionMargin);

  auto fullscreen_toast = CreateTextToast(
      kExclusiveScreenToastTransientParent, kExclusiveScreenToast, model_,
      l10n_util::GetStringUTF16(IDS_PRESS_APP_TO_EXIT_FULLSCREEN));
  fullscreen_toast->AddBinding(
      VR_BIND(bool, Model, model_, model->fullscreen_enabled(), UiElement,
              fullscreen_toast.get(), SetVisibleInLayout(view, value)));
  layout->AddChild(std::move(fullscreen_toast));

  auto platform_toast = CreateTextToast(
      kPlatformToastTransientParent, kPlatformToast, model_, base::string16());
  platform_toast->AddBinding(std::make_unique<Binding<const PlatformToast*>>(
      VR_BIND_LAMBDA([](Model* m) { return m->platform_toast.get(); },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TransientElement* t, const PlatformToast* const& value) {
            SetVisibleInLayout(t, value);
            if (value) {
              t->RefreshVisible();
            }
          },
          base::Unretained(platform_toast.get()))));
  Text* text_element =
      static_cast<Text*>(platform_toast->GetDescendantByType(kTypeToastText));
  DCHECK(text_element);
  text_element->AddBinding(std::make_unique<Binding<const PlatformToast*>>(
      VR_BIND_LAMBDA([](Model* m) { return m->platform_toast.get(); },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Text* t, const PlatformToast* const& value) {
            if (value) {
              t->SetText(value->text);
            }
          },
          base::Unretained(text_element))));
  layout->AddChild(std::move(platform_toast));

  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(layout));
}

}  // namespace vr
