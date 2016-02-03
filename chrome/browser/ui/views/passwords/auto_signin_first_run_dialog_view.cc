// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"

#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {
// An identifier for views::ColumnSet.
enum ColumnSetType {
  // | | (FILL, FILL) | |
  SINGLE_VIEW_COLUMN_SET,

  // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should nest at the
  // bottom-right corner.
  DOUBLE_BUTTON_COLUMN_SET,
};

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void BuildColumnSet(views::GridLayout* layout, ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  column_set->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                            views::GridLayout::USE_PREF, 0, 0);
      break;
    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
  }
  column_set->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
}

}  // namespace

AutoSigninFirstRunDialogView::AutoSigninFirstRunDialogView(
    PasswordDialogController* controller,
    content::WebContents* web_contents)
    : ok_button_(nullptr),
      turn_off_button_(nullptr),
      controller_(controller),
      web_contents_(web_contents) {
}

AutoSigninFirstRunDialogView::~AutoSigninFirstRunDialogView() {
}

void AutoSigninFirstRunDialogView::ShowAutoSigninPrompt() {
  InitWindow();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void AutoSigninFirstRunDialogView::ControllerGone() {
  controller_ = nullptr;
  GetWidget()->Close();
}

ui::ModalType AutoSigninFirstRunDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 AutoSigninFirstRunDialogView::GetWindowTitle() const {
  return controller_->GetAutoSigninPromoTitle();
}

bool AutoSigninFirstRunDialogView::ShouldShowWindowTitle() const {
  // The framework trims the title instead of resizing the dialog.
  return false;
}

bool AutoSigninFirstRunDialogView::ShouldShowCloseButton() const {
  return false;
}

views::View* AutoSigninFirstRunDialogView::GetInitiallyFocusedView() {
  return ok_button_;
}

int AutoSigninFirstRunDialogView::GetDialogButtons() const {
  // None because ESC is equivalent to Cancel. It shouldn't turn off the auto
  // signin.
  return ui::DIALOG_BUTTON_NONE;
}

void AutoSigninFirstRunDialogView::OnClosed() {
  if (controller_)
    controller_->OnCloseDialog();
  // This method is called twice. crbug.com/583330
  controller_ = nullptr;
}

void AutoSigninFirstRunDialogView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == ok_button_)
    controller_->OnAutoSigninOK();
  else if (sender == turn_off_button_)
    controller_->OnAutoSigninTurnOff();
  else
    NOTREACHED();
}

void AutoSigninFirstRunDialogView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  controller_->OnSmartLockLinkClicked();
}

void AutoSigninFirstRunDialogView::InitWindow() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);

  // Title.
  views::Label* title_label = new views::Label(GetWindowTitle());
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  layout->StartRowWithPadding(0, SINGLE_VIEW_COLUMN_SET, 0, kTitleTopInset);
  layout->AddView(title_label);

  // Content.
  std::pair<base::string16, gfx::Range> text_content =
      controller_->GetAutoSigninText();
  views::StyledLabel* content_label =
      new views::StyledLabel(text_content.first, this);
  content_label->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  if (!text_content.second.is_empty()) {
    content_label->AddStyleRange(
        text_content.second,
        views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  layout->StartRowWithPadding(0, SINGLE_VIEW_COLUMN_SET, 0,
                              2 * views::kRelatedControlVerticalSpacing);
  layout->AddView(content_label);

  // Buttons.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(0, DOUBLE_BUTTON_COLUMN_SET, 0,
                              3 * views::kRelatedControlVerticalSpacing);
  ok_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_OK));
  ok_button_->SetStyle(views::Button::STYLE_BUTTON);
  turn_off_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_TURN_OFF));
  turn_off_button_->SetStyle(views::Button::STYLE_BUTTON);
  layout->AddView(ok_button_);
  layout->AddView(turn_off_button_);
  layout->AddPaddingRow(0, views::kButtonVEdgeMarginNew);
}

AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new AutoSigninFirstRunDialogView(controller, web_contents);
}
