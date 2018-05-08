// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/dialog_plate.h"

#include <memory>

#include "ash/assistant/ash_assistant_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr SkColor kBackgroundColor = SkColorSetA(SK_ColorBLACK, 0x1F);
constexpr int kIconSizeDip = 24;
constexpr int kPaddingHorizontalDip = 12;
constexpr int kPaddingVerticalDip = 8;
constexpr int kSpacingDip = 8;

// Typography.
constexpr SkColor kTextColorHint = SkColorSetA(SK_ColorBLACK, 0x42);
constexpr SkColor kTextColorPrimary = SkColorSetA(SK_ColorBLACK, 0xDE);

// TODO(dmblack): Remove after implementing stateful icon.
// Background colors to represent icon states.
constexpr SkColor kKeyboardColor = SkColorSetRGB(0x4C, 0x8B, 0xF5);    // Blue
constexpr SkColor kMicOpenColor = SkColorSetRGB(0xDD, 0x51, 0x44);     // Red
constexpr SkColor kMicClosedColor = SkColorSetA(SK_ColorBLACK, 0x1F);  // Grey

// TODO(b/77638210): Replace with localized resource strings.
constexpr char kHint[] = "Type a message";

// TODO(dmblack): Remove after removing placeholders.
// RoundRectBackground ---------------------------------------------------------

class RoundRectBackground : public views::Background {
 public:
  RoundRectBackground(SkColor color, int corner_radius)
      : color_(color), corner_radius_(corner_radius) {}

  ~RoundRectBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_);
    canvas->DrawRoundRect(view->GetContentsBounds(), corner_radius_, flags);
  }

 private:
  const SkColor color_;
  const int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundRectBackground);
};

}  // namespace

// DialogPlate -----------------------------------------------------------------

DialogPlate::DialogPlate(AshAssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      textfield_(new views::Textfield()),
      icon_(new views::View()) {
  InitLayout();

  // The Assistant controller indirectly owns the view hierarchy to which
  // DialogPlate belongs, so is guaranteed to outlive it.
  assistant_controller_->AddInteractionModelObserver(this);
}

DialogPlate::~DialogPlate() {
  assistant_controller_->RemoveInteractionModelObserver(this);
}

void DialogPlate::InitLayout() {
  SetBackground(views::CreateSolidBackground(kBackgroundColor));

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(kPaddingVerticalDip, kPaddingHorizontalDip),
          kSpacingDip));

  gfx::FontList font_list =
      views::Textfield::GetDefaultFontList().DeriveWithSizeDelta(4);

  // Textfield.
  textfield_->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield_->SetBorder(views::NullBorder());
  textfield_->set_controller(this);
  textfield_->SetFontList(font_list);
  textfield_->set_placeholder_font_list(font_list);
  textfield_->set_placeholder_text(base::UTF8ToUTF16(kHint));
  textfield_->set_placeholder_text_color(kTextColorHint);
  textfield_->SetTextColor(kTextColorPrimary);
  AddChildView(textfield_);

  layout->SetFlexForView(textfield_, 1);

  // TODO(dmblack): Replace w/ stateful icon.
  // Icon.
  icon_->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon_);

  // TODO(dmblack): Remove once the icon has been replaced. Only needed to
  // force the background for the initial state.
  UpdateIcon();
}

void DialogPlate::OnInputModalityChanged(InputModality input_modality) {
  // TODO(dmblack): When the stylus is selected we will hide the dialog plate
  // and so should suspend any ongoing animations once the stateful icon is
  // implemented.
  if (input_modality == InputModality::kStylus)
    return;

  UpdateIcon();
}

void DialogPlate::OnMicStateChanged(MicState mic_state) {
  UpdateIcon();
}

void DialogPlate::ContentsChanged(views::Textfield* textfield,
                                  const base::string16& new_contents) {
  assistant_controller_->OnDialogPlateContentsChanged(
      base::UTF16ToUTF8(new_contents));
}

bool DialogPlate::HandleKeyEvent(views::Textfield* textfield,
                                 const ui::KeyEvent& key_event) {
  if (key_event.key_code() != ui::KeyboardCode::VKEY_RETURN)
    return false;

  if (key_event.type() != ui::EventType::ET_KEY_PRESSED)
    return false;

  const base::StringPiece16& text =
      base::TrimWhitespace(textfield->text(), base::TrimPositions::TRIM_ALL);

  if (text.empty())
    return false;

  assistant_controller_->OnDialogPlateContentsCommitted(
      base::UTF16ToUTF8(text));

  textfield->SetText(base::string16());

  return true;
}

// TODO(dmblack): Revise this method to update the state of the stateful icon
// once it has been implemented. For the time being, we represent state by
// modifying the background color of the placeholder icon.
void DialogPlate::UpdateIcon() {
  const AssistantInteractionModel* interaction_model =
      assistant_controller_->interaction_model();

  if (interaction_model->input_modality() == InputModality::kKeyboard) {
    icon_->SetBackground(std::make_unique<RoundRectBackground>(
        kKeyboardColor, kIconSizeDip / 2));
    SchedulePaint();
    return;
  }

  switch (interaction_model->mic_state()) {
    case MicState::kClosed:
      icon_->SetBackground(std::make_unique<RoundRectBackground>(
          kMicClosedColor, kIconSizeDip / 2));
      break;
    case MicState::kOpen:
      icon_->SetBackground(std::make_unique<RoundRectBackground>(
          kMicOpenColor, kIconSizeDip / 2));
      break;
  }

  SchedulePaint();
}

void DialogPlate::RequestFocus() {
  textfield_->RequestFocus();
}

}  // namespace ash
