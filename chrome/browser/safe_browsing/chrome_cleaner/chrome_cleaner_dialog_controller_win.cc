// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_win.h"

#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

ChromeCleanerDialogController::ChromeCleanerDialogController() {}

ChromeCleanerDialogController::~ChromeCleanerDialogController() = default;

base::string16 ChromeCleanerDialogController::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CHROME_CLEANUP_PROMPT_TITLE);
}

base::string16 ChromeCleanerDialogController::GetMainText() const {
  return l10n_util::GetStringUTF16(IDS_CHROME_CLEANUP_PROMPT_EXPLANATION);
}

base::string16 ChromeCleanerDialogController::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_CHROME_CLEANUP_PROMPT_REMOVE_BUTTON_LABEL);
}

base::string16 ChromeCleanerDialogController::GetDetailsButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_CHROME_CLEANUP_PROMPT_DETAILS_BUTTON_LABEL);
}

void ChromeCleanerDialogController::DialogShown() {}

void ChromeCleanerDialogController::Accept() {
  OnInteractionDone();
}

void ChromeCleanerDialogController::Cancel() {
  OnInteractionDone();
}

void ChromeCleanerDialogController::Close() {
  OnInteractionDone();
}

void ChromeCleanerDialogController::DetailsButtonClicked() {
  OnInteractionDone();
}

void ChromeCleanerDialogController::OnInteractionDone() {
  delete this;
}

}  // namespace safe_browsing
