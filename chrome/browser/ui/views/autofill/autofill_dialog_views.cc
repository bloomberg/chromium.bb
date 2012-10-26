// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/views/controls/label.h"

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViews(controller);
}

AutofillDialogViews::AutofillDialogViews(AutofillDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(NULL) {
  DCHECK(controller);
}

AutofillDialogViews::~AutofillDialogViews() {
  DCHECK(!window_);
}

void AutofillDialogViews::Show() {
  InitChildViews();

  // Ownership of |contents_| is handed off by this call. The ConstrainedWindow
  // will take care of deleting itself after calling DeleteDelegate().
  window_ = new ConstrainedWindowViews(
      controller_->web_contents(), this,
      true, ConstrainedWindowViews::DEFAULT_INSETS);
}

string16 AutofillDialogViews::GetWindowTitle() const {
  return controller_->DialogTitle();
}

void AutofillDialogViews::DeleteDelegate() {
  window_ = NULL;
  // |this| belongs to |controller_|.
  controller_->ViewClosed(AutofillDialogController::AUTOFILL_ACTION_ABORT);
}

views::Widget* AutofillDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* AutofillDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* AutofillDialogViews::GetContentsView() {
  return contents_;
}

string16 AutofillDialogViews::GetDialogButtonLabel(ui::DialogButton button)
    const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonText() : controller_->CancelButtonText();
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonEnabled() : true;
}

bool AutofillDialogViews::UseChromeStyle() const {
  return true;
}

bool AutofillDialogViews::Cancel() {
  return true;
}

bool AutofillDialogViews::Accept() {
  NOTREACHED();
  return true;
}

void AutofillDialogViews::InitChildViews() {
  contents_ = new views::View();
  contents_->AddChildView(new views::Label(ASCIIToUTF16("Hello, world.")));
}
