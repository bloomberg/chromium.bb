// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/password_generation_popup_view_views.h"

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/password_generation_popup_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

const SkColor kExplanatoryTextBackground = SkColorSetRGB(0xF5, 0xF5, 0xF5);
const SkColor kExplanatoryTextColor = SkColorSetRGB(0x66, 0x66, 0x66);
const SkColor kDividerColor = SkColorSetRGB(0xE9, 0xE9, 0xE9);
const SkColor kLinkColor = SkColorSetRGB(0x55, 0x59, 0xFE);

// The amount of whitespace that is present when there is no padding. Used
// to get the proper spacing in the help section.
const int kHelpVerticalOffset = 3;

// Class that shows the password and the suggestion side-by-side.
class PasswordRow : public views::View {
 public:
  PasswordRow(const base::string16& password,
              const base::string16& suggestion,
              const gfx::FontList& font_list,
              int horizontal_border) {
    set_clip_insets(gfx::Insets(
        PasswordGenerationPopupView::kPasswordVerticalInset, 0,
        PasswordGenerationPopupView::kPasswordVerticalInset, 0));
    views::BoxLayout* box_layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal, horizontal_border, 0, 0);
    box_layout->set_spread_blank_space(true);
    SetLayoutManager(box_layout);

    password_label_ = new views::Label(password, font_list);
    password_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(password_label_);

    suggestion_label_ = new views::Label(suggestion, font_list);
    suggestion_label_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    suggestion_label_->SetEnabledColor(kExplanatoryTextColor);
    AddChildView(suggestion_label_);
  }
  virtual ~PasswordRow() {}

  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE {
    // Have parent do event handling.
    return false;
  }

 private:
  // Child views. Not owned.
  views::Label* suggestion_label_;
  views::Label* password_label_;

  DISALLOW_COPY_AND_ASSIGN(PasswordRow);
};

}  // namespace

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    PasswordGenerationPopupController* controller,
    views::Widget* observing_widget)
    : AutofillPopupBaseView(controller, observing_widget),
      password_view_(NULL),
      controller_(controller) {
  if (controller_->display_password())
    CreatePasswordView();

  help_label_ = new views::StyledLabel(controller_->HelpText(), this);
  help_label_->SetBaseFontList(controller_->font_list());
  views::StyledLabel::RangeStyleInfo default_style;
  default_style.color = kExplanatoryTextColor;
  help_label_->SetDefaultStyle(default_style);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.color = kLinkColor;
  help_label_->AddStyleRange(controller_->HelpTextLinkRange(), link_style);

  help_label_->SetBoundsRect(controller_->help_bounds());
  help_label_->set_background(
      views::Background::CreateSolidBackground(kExplanatoryTextBackground));
  help_label_->SetBorder(views::Border::CreateEmptyBorder(
      controller_->kHelpVerticalPadding - kHelpVerticalOffset,
      controller_->kHorizontalPadding,
      0,
      controller_->kHorizontalPadding));
  AddChildView(help_label_);

  set_background(views::Background::CreateSolidBackground(kPopupBackground));
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() {}

void PasswordGenerationPopupViewViews::CreatePasswordView() {
  if (password_view_)
    return;

  password_view_ = new PasswordRow(controller_->password(),
                                   controller_->SuggestedText(),
                                   controller_->font_list(),
                                   controller_->kHorizontalPadding);
  AddChildView(password_view_);
}

void PasswordGenerationPopupViewViews::Show() {
  DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = NULL;

  DoHide();
}

void PasswordGenerationPopupViewViews::UpdateBoundsAndRedrawPopup() {
  // Currently the UI can change from not offering a password to offering
  // a password (e.g. the user is editing a generated password and deletes it),
  // but it can't change the other way around.
  if (controller_->display_password())
    CreatePasswordView();

  DoUpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewViews::PasswordSelectionUpdated() {
  if (!password_view_)
    return;

  password_view_->set_background(
      views::Background::CreateSolidBackground(
          controller_->password_selected() ?
          kHoveredBackgroundColor :
          kPopupBackground));
}

void PasswordGenerationPopupViewViews::Layout() {
  if (password_view_)
    password_view_->SetBoundsRect(controller_->password_bounds());

  help_label_->SetBoundsRect(controller_->help_bounds());
}

void PasswordGenerationPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  // Draw border and background.
  views::View::OnPaint(canvas);

  // Divider line needs to be drawn after OnPaint() otherwise the background
  // will overwrite the divider.
  if (password_view_)
    canvas->FillRect(controller_->divider_bounds(), kDividerColor);
}

void PasswordGenerationPopupViewViews::StyledLabelLinkClicked(
    const gfx::Range& range, int event_flags) {
  controller_->OnSavedPasswordsLinkClicked();
}

PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    PasswordGenerationPopupController* controller) {
  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

  // If the top level widget can't be found, cancel the popup since we can't
  // fully set it up.
  if (!observing_widget)
    return NULL;

  return new PasswordGenerationPopupViewViews(controller, observing_widget);
}

}  // namespace autofill
