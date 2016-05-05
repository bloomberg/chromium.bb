// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/decorated_textfield.h"

#include <utility>

#include "chrome/browser/ui/views/autofill/tooltip_icon.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focusable_border.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view_targeter.h"

namespace {

// Padding around icons inside DecoratedTextfields.
const int kTextfieldIconPadding = 3;

}  // namespace

namespace autofill {

// static
const char DecoratedTextfield::kViewClassName[] = "autofill/DecoratedTextfield";

DecoratedTextfield::DecoratedTextfield(
    const base::string16& default_value,
    const base::string16& placeholder,
    views::TextfieldController* controller)
    : invalid_(false),
      editable_(true) {
  UpdateBackground();
  UpdateBorder();

  set_placeholder_text(placeholder);
  SetText(default_value);
  set_controller(controller);

  SetEventTargeter(
      std::unique_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

DecoratedTextfield::~DecoratedTextfield() {}

void DecoratedTextfield::SetInvalid(bool invalid) {
  if (invalid_ == invalid)
    return;

  invalid_ = invalid;
  UpdateBorder();
  SchedulePaint();
}

void DecoratedTextfield::SetEditable(bool editable) {
  if (editable_ == editable)
    return;

  editable_ = editable;
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
  return editable_ ? views::Textfield::GetPlaceholderText() : base::string16();
}

const char* DecoratedTextfield::GetClassName() const {
  return kViewClassName;
}

gfx::Size DecoratedTextfield::GetPreferredSize() const {
  static const int height =
      views::LabelButton(NULL, base::string16()).GetPreferredSize().height();
  const gfx::Size size = views::Textfield::GetPreferredSize();
  return gfx::Size(size.width(), std::max(size.height(), height));
}

void DecoratedTextfield::Layout() {
  views::Textfield::Layout();

  if (icon_view_ && icon_view_->visible()) {
    gfx::Rect bounds = GetContentsBounds();
    gfx::Size icon_size = icon_view_->GetPreferredSize();
    int x = base::i18n::IsRTL() ?
        bounds.x() - icon_size.width() - kTextfieldIconPadding :
        bounds.right() + kTextfieldIconPadding;
    // Vertically centered.
    int y = bounds.y() + (bounds.height() - icon_size.height()) / 2;
    gfx::Rect icon_bounds(x, y, icon_size.width(), icon_size.height());
    icon_bounds.set_x(GetMirroredXForRect(icon_bounds));
    icon_view_->SetBoundsRect(icon_bounds);
  }
}

views::View* DecoratedTextfield::TargetForRect(views::View* root,
                                               const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  views::View* handler = views::ViewTargeterDelegate::TargetForRect(root, rect);
  if (handler->GetClassName() == TooltipIcon::kViewClassName)
    return handler;
  return this;
}

void DecoratedTextfield::UpdateBackground() {
  if (editable_)
    UseDefaultBackgroundColor();
  else
    SetBackgroundColor(SK_ColorTRANSPARENT);
  set_background(
      views::Background::CreateSolidBackground(GetBackgroundColor()));
}

void DecoratedTextfield::UpdateBorder() {
  std::unique_ptr<views::FocusableBorder> border(new views::FocusableBorder());
  if (invalid_)
    border->SetColor(gfx::kGoogleRed700);
  else if (!editable_)
    border->SetColor(SK_ColorTRANSPARENT);

  // Adjust the border insets to include the icon and its padding.
  if (icon_view_ && icon_view_->visible()) {
    int w = icon_view_->GetPreferredSize().width() + 2 * kTextfieldIconPadding;
    gfx::Insets insets = border->GetInsets();
    int left = insets.left() + (base::i18n::IsRTL() ? w : 0);
    int right = insets.right() + (base::i18n::IsRTL() ? 0 : w);
    border->SetInsets(insets.top(), left, insets.bottom(), right);
  }

  SetBorder(std::move(border));
}

void DecoratedTextfield::IconChanged() {
  // Don't show the icon if nothing else is showing.
  icon_view_->SetVisible(editable_ || !text().empty());
  UpdateBorder();
  Layout();
}

} // namespace autofill
