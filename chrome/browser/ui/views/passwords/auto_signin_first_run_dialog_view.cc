// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/auto_signin_first_run_dialog_view.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {
// An identifier for views::ColumnSet.
enum ColumnSetType {
  // | | (FILL, FILL) | |
  SINGLE_VIEW_COLUMN_SET,

  // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the dialog which should nest at the
  // bottom-right corner.
  DOUBLE_BUTTON_COLUMN_SET,
};

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void BuildColumnSet(views::GridLayout* layout, ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_CONTENTS);
  column_set->AddPaddingColumn(0, dialog_insets.left());
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(
          0, layout_provider->GetDistanceMetric(
                 views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
  }
  column_set->AddPaddingColumn(0, dialog_insets.right());
}

}  // namespace

AutoSigninFirstRunDialogView::AutoSigninFirstRunDialogView(
    PasswordDialogController* controller,
    content::WebContents* web_contents)
    : ok_button_(nullptr),
      turn_off_button_(nullptr),
      controller_(controller),
      web_contents_(web_contents) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::AUTO_SIGNIN_FIRST_RUN);
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

void AutoSigninFirstRunDialogView::WindowClosing() {
  if (controller_)
    controller_->OnCloseDialog();
}

int AutoSigninFirstRunDialogView::GetDialogButtons() const {
  // None because ESC is equivalent to Cancel. It shouldn't turn off the auto
  // signin.
  return ui::DIALOG_BUTTON_NONE;
}

gfx::Size AutoSigninFirstRunDialogView::CalculatePreferredSize() const {
  return gfx::Size(kDesiredWidth, GetHeightForWidth(kDesiredWidth));
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
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);

  // Title.
  views::Label* title_label =
      new views::Label(GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // Content.
  std::pair<base::string16, gfx::Range> text_content =
      controller_->GetAutoSigninText();
  views::StyledLabel* content_label =
      new views::StyledLabel(text_content.first, this);
  content_label->SetBaseFontList(views::style::GetFont(
      CONTEXT_DEPRECATED_SMALL, views::style::STYLE_PRIMARY));
  views::StyledLabel::RangeStyleInfo default_style;
  default_style.color = kAutoSigninTextColor;
  content_label->SetDefaultStyle(default_style);
  if (!text_content.second.is_empty()) {
    content_label->AddStyleRange(
        text_content.second,
        views::StyledLabel::RangeStyleInfo::CreateForLink());
  }

  // Buttons.
  ok_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_OK));
  turn_off_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_AUTO_SIGNIN_FIRST_RUN_TURN_OFF));

  // Layout.
  layout->StartRowWithPadding(
      0, SINGLE_VIEW_COLUMN_SET, 0,
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_TITLE).top());
  layout->AddView(title_label);
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_CONTENTS);
  layout->StartRowWithPadding(0, SINGLE_VIEW_COLUMN_SET, 0,
                              dialog_insets.top());
  layout->AddView(content_label);
  layout->AddPaddingRow(0, dialog_insets.bottom());

  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  const gfx::Insets button_insets =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG_BUTTON_ROW);
  layout->StartRowWithPadding(0, DOUBLE_BUTTON_COLUMN_SET, 0,
                              button_insets.top());
  layout->AddView(ok_button_);
  layout->AddView(turn_off_button_);
  layout->AddPaddingRow(0, button_insets.bottom());
}

AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new AutoSigninFirstRunDialogView(controller, web_contents);
}
