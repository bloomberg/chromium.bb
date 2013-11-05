// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/decorated_textfield.h"

#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/views/autofill/tooltip_icon.h"
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
      invalid_(false),
      editable_(true) {
  UpdateBackground();

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
  if (!editable_)
    return;

  if (invalid)
    border_->SetColor(kWarningColor);
  else
    border_->UseDefaultColor();
  SchedulePaint();
}

void DecoratedTextfield::SetEditable(bool editable) {
  if (editable_ == editable)
    return;

  editable_ = editable;
  if (editable) {
    SetInvalid(invalid_);
    UseDefaultBackgroundColor();
  } else {
    border_->SetColor(SK_ColorTRANSPARENT);
    SetBackgroundColor(SK_ColorTRANSPARENT);
  }

  UpdateBackground();
  SetEnabled(editable);
  IconChanged();
}

void DecoratedTextfield::SetIcon(const gfx::Image& icon) {
  if (!icon_view_ && icon.IsEmpty())
    return;

  if (icon_view_)
    RemoveChildView(icon_view_.get());

  if (!icon.IsEmpty()) {
    icon_view_.reset(new views::ImageView());
    icon_view_->set_owned_by_client();
    icon_view_->SetImage(icon.ToImageSkia());
    AddChildView(icon_view_.get());
  }

  IconChanged();
}

void DecoratedTextfield::SetTooltipIcon(const base::string16& text) {
  if (!icon_view_ && text.empty())
    return;

  if (icon_view_)
    RemoveChildView(icon_view_.get());

  if (!text.empty()) {
    icon_view_.reset(new TooltipIcon(text));
    AddChildView(icon_view_.get());
  }

  IconChanged();
}

base::string16 DecoratedTextfield::GetPlaceholderText() const {
  if (!editable_)
    return base::string16();

  return views::Textfield::GetPlaceholderText();
}

const char* DecoratedTextfield::GetClassName() const {
  return kViewClassName;
}

views::View* DecoratedTextfield::GetEventHandlerForRect(const gfx::Rect& rect) {
  return native_wrapper_->GetView();
}

void DecoratedTextfield::OnFocus() {
  border_->set_has_focus(true);
  views::Textfield::OnFocus();
}

void DecoratedTextfield::OnBlur() {
  border_->set_has_focus(false);
  views::Textfield::OnBlur();
}

gfx::Size DecoratedTextfield::GetPreferredSize() {
  int w = views::Textfield::GetPreferredSize().width();
  views::LabelButton button(NULL, string16());
  button.SetStyle(views::Button::STYLE_BUTTON);
  int h = button.GetPreferredSize().height();
  return gfx::Size(w, h - kMagicInsetNumber);
}

void DecoratedTextfield::Layout() {
  views::Textfield::Layout();

  if (icon_view_ && icon_view_->visible()) {
    gfx::Rect bounds = GetContentsBounds();
    gfx::Size icon_size = icon_view_->GetPreferredSize();
    int x = base::i18n::IsRTL() ?
        kTextfieldIconPadding :
        bounds.right() - icon_size.width() - kTextfieldIconPadding;
    // Vertically centered.
    int y = bounds.y() + (bounds.height() - icon_size.height()) / 2;
    icon_view_->SetBounds(x,
                          y,
                          icon_size.width(),
                          icon_size.height());
  }
}

void DecoratedTextfield::UpdateBackground() {
  set_background(
      views::Background::CreateSolidBackground(GetBackgroundColor()));
}

void DecoratedTextfield::IconChanged() {
  // Don't show the icon if nothing else is showing.
  icon_view_->SetVisible(editable_ || !text().empty());

  int icon_space = icon_view_ ?
      icon_view_->GetPreferredSize().width() + 2 * kTextfieldIconPadding : 0;

  bool is_rtl = base::i18n::IsRTL();
  SetHorizontalMargins(is_rtl ? icon_space : 0, is_rtl ? 0 : icon_space);

  Layout();
  SchedulePaint();
}

} // namespace autofill
