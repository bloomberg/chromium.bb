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

// TODO(dmblack): Remove after removing placeholders.
// Placeholder.
constexpr SkColor kPlaceholderColor = SkColorSetA(SK_ColorBLACK, 0x1F);

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
    : assistant_controller_(assistant_controller) {
  InitLayout();
}

DialogPlate::~DialogPlate() = default;

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
  views::Textfield* textfield = new views::Textfield();
  textfield->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield->SetBorder(views::NullBorder());
  textfield->set_controller(this);
  textfield->SetFontList(font_list);
  textfield->set_placeholder_font_list(font_list);
  textfield->set_placeholder_text(base::UTF8ToUTF16(kHint));
  textfield->set_placeholder_text_color(kTextColorHint);
  textfield->SetTextColor(kTextColorPrimary);
  AddChildView(textfield);

  layout->SetFlexForView(textfield, 1);

  // TODO(dmblack): Replace w/ stateful icon.
  // Icon placeholder.
  views::View* icon_placeholder = new views::View();
  icon_placeholder->SetBackground(std::make_unique<RoundRectBackground>(
      kPlaceholderColor, kIconSizeDip / 2));
  icon_placeholder->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  AddChildView(icon_placeholder);
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

}  // namespace ash
