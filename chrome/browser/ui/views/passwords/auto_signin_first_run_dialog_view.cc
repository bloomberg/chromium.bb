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
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

AutoSigninFirstRunDialogView::AutoSigninFirstRunDialogView(
    PasswordDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents), text_(nullptr) {
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

bool AutoSigninFirstRunDialogView::ShouldShowCloseButton() const {
  return false;
}

void AutoSigninFirstRunDialogView::WindowClosing() {
  if (controller_)
    controller_->OnCloseDialog();
}

bool AutoSigninFirstRunDialogView::Cancel() {
  controller_->OnAutoSigninTurnOff();
  return true;
}

bool AutoSigninFirstRunDialogView::Accept() {
  controller_->OnAutoSigninOK();
  return true;
}

bool AutoSigninFirstRunDialogView::Close() {
  // Do nothing rather than running Cancel(), which would turn off auto-signin.
  return true;
}

base::string16 AutoSigninFirstRunDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_AUTO_SIGNIN_FIRST_RUN_OK
                                       : IDS_AUTO_SIGNIN_FIRST_RUN_TURN_OFF);
}

void AutoSigninFirstRunDialogView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  controller_->OnSmartLockLinkClicked();
}

void AutoSigninFirstRunDialogView::InitWindow() {
  SetBorder(views::CreateEmptyBorder(
      ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(views::TEXT,
                                                                 views::TEXT)));
  SetLayoutManager(new views::FillLayout());

  std::pair<base::string16, gfx::Range> text_content =
      controller_->GetAutoSigninText();
  text_ = new views::StyledLabel(text_content.first, this);
  text_->SetTextContext(CONTEXT_BODY_TEXT_LARGE);
  text_->SetDefaultTextStyle(STYLE_SECONDARY);
  if (!text_content.second.is_empty()) {
    text_->AddStyleRange(text_content.second,
                         views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  AddChildView(text_);
}

AutoSigninFirstRunPrompt* CreateAutoSigninPromptView(
    PasswordDialogController* controller, content::WebContents* web_contents) {
  return new AutoSigninFirstRunDialogView(controller, web_contents);
}
