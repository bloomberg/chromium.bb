// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_template.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

AutofillDialogController::AutofillDialogController(
    content::WebContents* contents)
    : contents_(contents) {}

AutofillDialogController::~AutofillDialogController() {}

void AutofillDialogController::Show() {
  view_.reset(AutofillDialogView::Create(this));
  view_->Show();
}

string16 AutofillDialogController::DialogTitle() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("PaY"));
}

string16 AutofillDialogController::IntroText() const {
  // TODO(estade): real strings and l10n.
  return string16(
      ASCIIToUTF16("random.com has requested the following deets:"));
}

string16 AutofillDialogController::EmailSectionLabel() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("Email address fixme"));
}

string16 AutofillDialogController::BillingSectionLabel() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("Billing details fixme"));
}

string16 AutofillDialogController::WalletOptionText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("I love lamp."));
}

bool AutofillDialogController::ShouldShowInput(const DetailInput& input) const {
  // TODO(estade): filter fields that aren't part of this autofill request.
  return true;
}

string16 AutofillDialogController::CancelButtonText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("CaNceL"));
}

string16 AutofillDialogController::ConfirmButtonText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("SuBMiT"));
}

bool AutofillDialogController::ConfirmButtonEnabled() const {
  return false;
}

void AutofillDialogController::ViewClosed(Action action) {
  // TODO(estade): pass the result along to the page.
  delete this;
}

}  // namespace autofill
