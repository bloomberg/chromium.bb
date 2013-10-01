// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/decorated_textfield.h"

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/textfield/textfield_controller.h"

namespace {

// Padding around icons inside DecoratedTextfields.
const int kTextfieldIconPadding = 3;

}  // namespace

namespace autofill {

// static
const char DecoratedTextfield::kViewClassName[] = "autofill/DecoratedTextfield";

const int DecoratedTextfield::kMagicInsetNumber = 6;

DecoratedTextfield::DecoratedTextfield(
    const base::string16& default_value,
    const base::string16& placeholder,
    views::TextfieldController* controller)
    : border_(new views::FocusableBorder()),
      invalid_(false) {
  set_background(
      views::Background::CreateSolidBackground(GetBackgroundColor()));

  set_border(border_);
  // Removes the border from |native_wrapper_|.
  RemoveBorder();

  set_placeholder_text(placeholder);
  SetText(default_value);
  SetController(controller);
  SetHorizontalMargins(0, 0);
}

DecoratedTextfield::~DecoratedTextfield() {}

void DecoratedTextfield::SetInvalid(bool invalid) {
  invalid_ = invalid;
  if (invalid)
    border_->SetColor(kWarningColor);
  else
    border_->UseDefaultColor();
  SchedulePaint();
}

void DecoratedTextfield::SetIcon(const gfx::Image& icon) {
  int icon_space = icon.IsEmpty() ? 0 :
                                    icon.Width() + 2 * kTextfieldIconPadding;
  // Extra indent inside of textfield before text starts, in px.
  const int kTextIndent = 6;
  int left = base::i18n::IsRTL() ? icon_space : kTextIndent;
  int right = base::i18n::IsRTL() ? kTextIndent : icon_space;
  SetHorizontalMargins(left, right);
  icon_ = icon;

  PreferredSizeChanged();
  SchedulePaint();
}

const char* DecoratedTextfield::GetClassName() const {
  return kViewClassName;
}

void DecoratedTextfield::PaintChildren(gfx::Canvas* canvas) {}

void DecoratedTextfield::OnPaint(gfx::Canvas* canvas) {
  // Draw the border and background.
  border_->set_has_focus(HasFocus());
  views::View::OnPaint(canvas);

  // Then the textfield.
  views::View::PaintChildren(canvas);

  // Then the icon.
  if (!icon_.IsEmpty()) {
    gfx::Rect bounds = GetContentsBounds();
    int x = base::i18n::IsRTL() ?
        kTextfieldIconPadding :
        bounds.right() - icon_.Width() - kTextfieldIconPadding;
    canvas->DrawImageInt(icon_.AsImageSkia(), x,
                         bounds.y() + (bounds.height() - icon_.Height()) / 2);
  }
}

gfx::Size DecoratedTextfield::GetPreferredSize() {
  int w = views::Textfield::GetPreferredSize().width();
  views::LabelButton button(NULL, string16());
  button.SetStyle(views::Button::STYLE_BUTTON);
  int h = button.GetPreferredSize().height();
  return gfx::Size(w, h - kMagicInsetNumber);
}

} // namespace autofill
