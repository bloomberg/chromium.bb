// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/password_generation_popup_view_views.h"

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/password_generation_popup_controller.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// The amount of whitespace that is present when there is no padding. Used
// to get the proper spacing in the help section.
const int kHelpVerticalOffset = 5;

// Wrapper around just the text portions of the generation UI (password and
// prompting text).
class PasswordTextBox : public views::View {
 public:
  PasswordTextBox() {}
  virtual ~PasswordTextBox() {}

  // |suggestion_text| prompts the user to select the password,
  // |generated_password| is the generated password, and |font_list| is the font
  // used for all text in this class.
  void Init(const base::string16& suggestion_text,
            const base::string16& generated_password,
            const gfx::FontList& font_list) {
    views::BoxLayout* box_layout = new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 12, 5);
    box_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    SetLayoutManager(box_layout);

    views::Label* suggestion_label = new views::Label(
        suggestion_text, font_list.DeriveWithStyle(gfx::Font::BOLD));
    suggestion_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    suggestion_label->SetEnabledColor(
        PasswordGenerationPopupView::kPasswordTextColor);
    AddChildView(suggestion_label);

    views::Label* password_label =
        new views::Label(generated_password, font_list);
    password_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    password_label->SetEnabledColor(
        PasswordGenerationPopupView::kPasswordTextColor);
    AddChildView(password_label);
  }

  // views::View:
  virtual bool CanProcessEventsWithinSubtree() const OVERRIDE {
    // Send events to the parent view for handling.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordTextBox);
};

}  // namespace

// Class that shows the generated password and associated UI (currently a key
// image and some explanatory text).
class PasswordGenerationPopupViewViews::PasswordBox : public views::View {
 public:
  PasswordBox() {}
  virtual ~PasswordBox() {}

  // |password| is the generated password, |suggestion| is the text prompting
  // the user to select the password, and |font_list| is the font used for all
  // the text.
  void Init(const base::string16& password,
            const base::string16& suggestion,
            const gfx::FontList& font_list) {
    views::BoxLayout* box_layout = new views::BoxLayout(
        views::BoxLayout::kHorizontal,
        PasswordGenerationPopupController::kHorizontalPadding,
        0,
        15);
    box_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
    SetLayoutManager(box_layout);

    views::ImageView* key_image = new views::ImageView();
    key_image->SetImage(
        ui::ResourceBundle::GetSharedInstance().GetImageNamed(
            IDR_GENERATE_PASSWORD_KEY).ToImageSkia());
    AddChildView(key_image);

    PasswordTextBox* password_text_box = new PasswordTextBox();
    password_text_box->Init(suggestion, password, font_list);
    AddChildView(password_text_box);
  }

  // views::View:
  virtual bool CanProcessEventsWithinSubtree() const OVERRIDE {
    // Send events to the parent view for handling.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordBox);
};

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    PasswordGenerationPopupController* controller,
    views::Widget* observing_widget)
    : AutofillPopupBaseView(controller, observing_widget),
      password_view_(NULL),
      font_list_(ResourceBundle::GetSharedInstance().GetFontList(
          ResourceBundle::SmallFont)),
      controller_(controller) {
  if (controller_->display_password())
    CreatePasswordView();

  help_label_ = new views::StyledLabel(controller_->HelpText(), this);
  help_label_->SetBaseFontList(font_list_);
  help_label_->SetLineHeight(20);
  views::StyledLabel::RangeStyleInfo default_style;
  default_style.color = kExplanatoryTextColor;
  help_label_->SetDefaultStyle(default_style);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.disable_line_wrapping = false;
  help_label_->AddStyleRange(controller_->HelpTextLinkRange(), link_style);

  help_label_->set_background(
      views::Background::CreateSolidBackground(
          kExplanatoryTextBackgroundColor));
  help_label_->SetBorder(views::Border::CreateEmptyBorder(
      PasswordGenerationPopupController::kHelpVerticalPadding -
      kHelpVerticalOffset,
      PasswordGenerationPopupController::kHorizontalPadding,
      PasswordGenerationPopupController::kHelpVerticalPadding -
      kHelpVerticalOffset,
      PasswordGenerationPopupController::kHorizontalPadding));
  AddChildView(help_label_);

  set_background(views::Background::CreateSolidBackground(kPopupBackground));
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() {}

void PasswordGenerationPopupViewViews::CreatePasswordView() {
  if (password_view_)
    return;

  password_view_ = new PasswordBox();
  password_view_->Init(controller_->password(),
                       controller_->SuggestedText(),
                       font_list_);
  password_view_->SetPosition(gfx::Point(kPopupBorderThickness,
                                         kPopupBorderThickness));
  password_view_->SizeToPreferredSize();
  AddChildView(password_view_);
}

gfx::Size PasswordGenerationPopupViewViews::GetPreferredSizeOfPasswordView() {
  int height = kPopupBorderThickness;
  if (controller_->display_password()) {
    // Add divider height as well.
    height +=
        PasswordGenerationPopupController::kPopupPasswordSectionHeight + 1;
  }
  int width = controller_->GetMinimumWidth();
  int popup_width = width - 2 * kPopupBorderThickness;
  height += help_label_->GetHeightForWidth(popup_width);
  return gfx::Size(width, height + kPopupBorderThickness);
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
  // Need to leave room for the border.
  int y = kPopupBorderThickness;
  int popup_width = bounds().width() - 2 * kPopupBorderThickness;
  if (controller_->display_password()) {
    // Currently the UI can change from not offering a password to offering
    // a password (e.g. the user is editing a generated password and deletes
    // it), but it can't change the other way around.
    CreatePasswordView();
    password_view_->SetBounds(
        kPopupBorderThickness,
        y,
        popup_width,
        PasswordGenerationPopupController::kPopupPasswordSectionHeight);
    divider_bounds_ =
        gfx::Rect(kPopupBorderThickness, password_view_->bounds().bottom(),
                  popup_width, 1);
    y = divider_bounds_.bottom();
  }

  help_label_->SetBounds(kPopupBorderThickness, y, popup_width,
                         help_label_->GetHeightForWidth(popup_width));
}

void PasswordGenerationPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  // Draw border and background.
  views::View::OnPaint(canvas);

  // Divider line needs to be drawn after OnPaint() otherwise the background
  // will overwrite the divider.
  if (password_view_)
    canvas->FillRect(divider_bounds_, kDividerColor);
}

void PasswordGenerationPopupViewViews::StyledLabelLinkClicked(
    const gfx::Range& range, int event_flags) {
  controller_->OnSavedPasswordsLinkClicked();
}

bool PasswordGenerationPopupViewViews::IsPointInPasswordBounds(
    const gfx::Point& point) {
  if (password_view_)
    return password_view_->bounds().Contains(point);
  return false;
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
