// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "content/public/browser/web_contents.h"

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
