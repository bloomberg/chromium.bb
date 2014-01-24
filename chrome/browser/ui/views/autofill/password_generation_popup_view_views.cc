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
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

const SkColor kExplanatoryTextBackground = SkColorSetRGB(0xF5, 0xF5, 0xF5);
const SkColor kExplanatoryTextColor = SkColorSetRGB(0x93, 0x93, 0x93);
const SkColor kDividerColor = SkColorSetRGB(0xE7, 0xE7, 0xE7);

// This is the amount of vertical whitespace that is left above and below the
// password when it is highlighted.
const int kPasswordVerticalInset = 7;

// The amount of whitespace that is present when there is no padding. Used
// to get the proper spacing in the help section.
const int kHelpVerticalOffset = 3;

}  // namespace

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    PasswordGenerationPopupController* controller,
    views::Widget* observing_widget)
    : AutofillPopupBaseView(controller, observing_widget),
      controller_(controller) {
  password_label_ = new views::Label(controller->password());
  password_label_->SetBoundsRect(controller->password_bounds());
  password_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  password_label_->set_clip_insets(gfx::Insets(
      kPasswordVerticalInset, 0, kPasswordVerticalInset, 0));
  password_label_->SetBorder(views::Border::CreateEmptyBorder(
      0, controller_->kHorizontalPadding, 0, controller_->kHorizontalPadding));
  AddChildView(password_label_);

  base::string16 help_text = controller->HelpText();
  base::string16 learn_more_link_text = controller->LearnMoreLink();
  views::StyledLabel* help_label =
      new views::StyledLabel(help_text + learn_more_link_text, this);
  views::StyledLabel::RangeStyleInfo default_style;
  default_style.color = kExplanatoryTextColor;
  help_label->SetDefaultStyle(default_style);
  help_label->AddStyleRange(
      gfx::Range(help_text.size(),
                 help_text.size() + learn_more_link_text.size()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  help_label->SetBoundsRect(controller->help_bounds());
  help_label->set_background(
      views::Background::CreateSolidBackground(kExplanatoryTextBackground));
  help_label->SetBorder(views::Border::CreateEmptyBorder(
      controller_->kHelpVerticalPadding - kHelpVerticalOffset,
      controller_->kHorizontalPadding,
      0,
      controller_->kHorizontalPadding));
  AddChildView(help_label);

  set_background(views::Background::CreateSolidBackground(kPopupBackground));
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() {}

void PasswordGenerationPopupViewViews::Show() {
  DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = NULL;

  DoHide();
}

void PasswordGenerationPopupViewViews::UpdateBoundsAndRedrawPopup() {
  DoUpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  if (controller_->password_selected()) {
    password_label_->set_background(
        views::Background::CreateSolidBackground(kHoveredBackgroundColor));
  } else {
    password_label_->set_background(
        views::Background::CreateSolidBackground(kPopupBackground));
  }

  // Draw border and background.
  views::View::OnPaint(canvas);

  // Divider line needs to be drawn after OnPaint() otherwise the background
  // will overwrite the divider.
  canvas->FillRect(controller_->divider_bounds(), kDividerColor);
}

void PasswordGenerationPopupViewViews::StyledLabelLinkClicked(
    const gfx::Range& range, int event_flags) {
  controller_->OnHelpLinkClicked();
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
