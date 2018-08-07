// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate/dialog_plate.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/callback_layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kPreferredHeightDip = 48;

// Animation.
constexpr base::TimeDelta kAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(100);
constexpr base::TimeDelta kAnimationFadeOutDuration =
    base::TimeDelta::FromMilliseconds(83);
constexpr base::TimeDelta kAnimationTransformInDuration =
    base::TimeDelta::FromMilliseconds(333);
constexpr int kAnimationTranslationDip = 30;

}  // namespace

// DialogPlate -----------------------------------------------------------------

DialogPlate::DialogPlate(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      animation_observer_(std::make_unique<ui::CallbackLayerAnimationObserver>(
          /*start_animation_callback=*/base::BindRepeating(
              &DialogPlate::OnAnimationStarted,
              base::Unretained(this)),
          /*end_animation_callback=*/base::BindRepeating(
              &DialogPlate::OnAnimationEnded,
              base::Unretained(this)))) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // DialogPlate belongs so is guaranteed to outlive it.
  assistant_controller_->interaction_controller()->AddModelObserver(this);
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

DialogPlate::~DialogPlate() {
  assistant_controller_->ui_controller()->RemoveModelObserver(this);
  assistant_controller_->interaction_controller()->RemoveModelObserver(this);
}

void DialogPlate::AddObserver(DialogPlateObserver* observer) {
  observers_.AddObserver(observer);
}

void DialogPlate::RemoveObserver(DialogPlateObserver* observer) {
  observers_.RemoveObserver(observer);
}

gfx::Size DialogPlate::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

int DialogPlate::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void DialogPlate::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void DialogPlate::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

void DialogPlate::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, 0, 0, kPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Input modality layout container.
  views::View* input_modality_layout_container = new views::View();
  input_modality_layout_container->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  input_modality_layout_container->SetPaintToLayer();
  input_modality_layout_container->layer()->SetFillsBoundsOpaquely(false);
  input_modality_layout_container->layer()->SetMasksToBounds(true);
  AddChildView(input_modality_layout_container);

  layout_manager->SetFlexForView(input_modality_layout_container, 1);

  InitKeyboardLayoutContainer(input_modality_layout_container);
  InitVoiceLayoutContainer(input_modality_layout_container);

  // Settings.
  views::ImageButton* settings_button = new views::ImageButton(this);
  settings_button->set_id(static_cast<int>(DialogPlateButtonId::kSettings));
  settings_button->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kNotificationSettingsIcon, kIconSizeDip,
                            gfx::kGoogleGrey600));
  settings_button->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(settings_button);

  // Artificially trigger event to set initial state.
  OnInputModalityChanged(assistant_controller_->interaction_controller()
                             ->model()
                             ->input_modality());
}

void DialogPlate::InitKeyboardLayoutContainer(
    views::View* input_modality_layout_container) {
  keyboard_layout_container_ = new views::View();
  keyboard_layout_container_->SetPaintToLayer();
  keyboard_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  keyboard_layout_container_->layer()->SetOpacity(0.f);

  views::BoxLayout* layout_manager =
      keyboard_layout_container_->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::Orientation::kHorizontal,
              gfx::Insets(0, kPaddingDip), kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  gfx::FontList font_list =
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(4);

  // Textfield.
  textfield_ = new views::Textfield();
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->SetBorder(views::NullBorder());
  textfield_->set_controller(this);
  textfield_->SetFontList(font_list);
  textfield_->set_placeholder_font_list(font_list);
  textfield_->set_placeholder_text(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_DIALOG_PLATE_HINT));
  textfield_->set_placeholder_text_color(kTextColorHint);
  textfield_->SetTextColor(kTextColorPrimary);
  keyboard_layout_container_->AddChildView(textfield_);

  layout_manager->SetFlexForView(textfield_, 1);

  // Voice input toggle.
  views::ImageButton* voice_input_toggle = new views::ImageButton(this);
  voice_input_toggle->set_id(
      static_cast<int>(DialogPlateButtonId::kVoiceInputToggle));
  voice_input_toggle->SetImage(views::Button::ButtonState::STATE_NORMAL,
                               gfx::CreateVectorIcon(kMicIcon, kIconSizeDip));
  voice_input_toggle->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  keyboard_layout_container_->AddChildView(voice_input_toggle);

  input_modality_layout_container->AddChildView(keyboard_layout_container_);
}

void DialogPlate::InitVoiceLayoutContainer(
    views::View* input_modality_layout_container) {
  voice_layout_container_ = new views::View();
  voice_layout_container_->SetPaintToLayer();
  voice_layout_container_->layer()->SetFillsBoundsOpaquely(false);
  voice_layout_container_->layer()->SetOpacity(0.f);

  views::BoxLayout* layout_manager = voice_layout_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip, 0, 0)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Keyboard input toggle.
  views::ImageButton* keyboard_input_toggle = new views::ImageButton(this);
  keyboard_input_toggle->set_id(
      static_cast<int>(DialogPlateButtonId::kKeyboardInputToggle));
  keyboard_input_toggle->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kKeyboardIcon, kIconSizeDip, gfx::kGoogleGrey600));
  keyboard_input_toggle->SetPreferredSize(
      gfx::Size(kIconSizeDip, kIconSizeDip));
  voice_layout_container_->AddChildView(keyboard_input_toggle);

  // Spacer.
  views::View* spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  // Animated voice input toggle.
  voice_layout_container_->AddChildView(
      new ActionView(assistant_controller_, this));

  // Spacer.
  spacer = new views::View();
  voice_layout_container_->AddChildView(spacer);

  layout_manager->SetFlexForView(spacer, 1);

  input_modality_layout_container->AddChildView(voice_layout_container_);
}

void DialogPlate::OnActionPressed() {
  OnButtonPressed(DialogPlateButtonId::kVoiceInputToggle);
}

void DialogPlate::ButtonPressed(views::Button* sender, const ui::Event& event) {
  OnButtonPressed(static_cast<DialogPlateButtonId>(sender->id()));
}

void DialogPlate::OnButtonPressed(DialogPlateButtonId id) {
  for (DialogPlateObserver& observer : observers_)
    observer.OnDialogPlateButtonPressed(id);

  textfield_->SetText(base::string16());
}

void DialogPlate::OnInputModalityChanged(InputModality input_modality) {
  using namespace assistant::util;

  switch (input_modality) {
    case InputModality::kKeyboard: {
      // Animate voice layout container opacity to 0%.
      voice_layout_container_->layer()->GetAnimator()->StartAnimation(
          CreateLayerAnimationSequence(
              CreateOpacityElement(0.f, kAnimationFadeOutDuration,
                                   gfx::Tween::Type::FAST_OUT_LINEAR_IN)));

      // Apply a pre-transformation on the keyboard layout container so that it
      // can be animated into place.
      gfx::Transform transform;
      transform.Translate(-kAnimationTranslationDip, 0);
      keyboard_layout_container_->layer()->SetTransform(transform);

      // Animate keyboard layout container.
      StartLayerAnimationSequencesTogether(
          keyboard_layout_container_->layer()->GetAnimator(),
          {// Animate transformation.
           CreateLayerAnimationSequence(CreateTransformElement(
               gfx::Transform(), kAnimationTransformInDuration,
               gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
           // Animate opacity to 100% with delay.
           CreateLayerAnimationSequence(
               ui::LayerAnimationElement::CreatePauseElement(
                   ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                   kAnimationFadeInDelay),
               CreateOpacityElement(1.f, kAnimationFadeInDuration,
                                    gfx::Tween::Type::FAST_OUT_LINEAR_IN))},
          // Observe this animation.
          animation_observer_.get());

      // Activate the animation observer to receive start/end events.
      animation_observer_->SetActive();
      break;
    }
    case InputModality::kVoice: {
      // Animate keyboard layout container opacity to 0%.
      keyboard_layout_container_->layer()->GetAnimator()->StartAnimation(
          CreateLayerAnimationSequence(
              CreateOpacityElement(0.f, kAnimationFadeOutDuration,
                                   gfx::Tween::Type::FAST_OUT_LINEAR_IN)));

      // Apply a pre-transformation on the voice layout container so that it can
      // be animated into place.
      gfx::Transform transform;
      transform.Translate(kAnimationTranslationDip, 0);
      voice_layout_container_->layer()->SetTransform(transform);

      // Animate voice layout container.
      StartLayerAnimationSequencesTogether(
          voice_layout_container_->layer()->GetAnimator(),
          {// Animate transformation.
           CreateLayerAnimationSequence(CreateTransformElement(
               gfx::Transform(), kAnimationTransformInDuration,
               gfx::Tween::Type::FAST_OUT_SLOW_IN_2)),
           // Animate opacity to 100% with delay.
           CreateLayerAnimationSequence(
               ui::LayerAnimationElement::CreatePauseElement(
                   ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                   kAnimationFadeInDelay),
               CreateOpacityElement(1.f, kAnimationFadeInDuration,
                                    gfx::Tween::Type::FAST_OUT_LINEAR_IN))},
          // Observe this animation.
          animation_observer_.get());

      // Activate the animation observer to receive start/end events.
      animation_observer_->SetActive();
      break;
    }
    case InputModality::kStylus:
      // No action necessary.
      break;
  }
}

void DialogPlate::OnAnimationStarted(
    const ui::CallbackLayerAnimationObserver& observer) {
  keyboard_layout_container_->set_can_process_events_within_subtree(false);
  voice_layout_container_->set_can_process_events_within_subtree(false);
}

bool DialogPlate::OnAnimationEnded(
    const ui::CallbackLayerAnimationObserver& observer) {
  InputModality input_modality = assistant_controller_->interaction_controller()
                                     ->model()
                                     ->input_modality();
  switch (input_modality) {
    case InputModality::kKeyboard:
      keyboard_layout_container_->set_can_process_events_within_subtree(true);
      textfield_->RequestFocus();
      break;
    case InputModality::kVoice:
      voice_layout_container_->set_can_process_events_within_subtree(true);
      break;
    case InputModality::kStylus:
      // No action necessary.
      break;
  }

  // We return false so that the animation observer will not destroy itself.
  return false;
}

void DialogPlate::OnUiVisibilityChanged(bool visible, AssistantSource source) {
  // When the Assistant UI is hidden we need to clear the dialog plate so that
  // text does not persist across Assistant entries.
  if (!visible)
    textfield_->SetText(base::string16());
}

bool DialogPlate::HandleKeyEvent(views::Textfield* textfield,
                                 const ui::KeyEvent& key_event) {
  if (key_event.key_code() != ui::KeyboardCode::VKEY_RETURN)
    return false;

  if (key_event.type() != ui::EventType::ET_KEY_PRESSED)
    return false;

  const base::StringPiece16& trimmed_text =
      base::TrimWhitespace(textfield_->text(), base::TrimPositions::TRIM_ALL);

  // Only non-empty trimmed text is consider a valid contents commit. Anything
  // else will simply result in the DialogPlate being cleared.
  if (!trimmed_text.empty()) {
    for (DialogPlateObserver& observer : observers_)
      observer.OnDialogPlateContentsCommitted(base::UTF16ToUTF8(trimmed_text));
  }

  textfield_->SetText(base::string16());

  return true;
}

void DialogPlate::RequestFocus() {
  textfield_->RequestFocus();
}

}  // namespace ash
