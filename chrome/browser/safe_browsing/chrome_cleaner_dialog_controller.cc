// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner_dialog_controller.h"

#include "base/strings/utf_string_conversions.h"

namespace safe_browsing {

namespace {

// Some dummy strings to be displayed in the Cleaner dialog while iterating on
// the dialog's UX design and work on the Chrome<->Cleaner IPC is ongoing.
constexpr char kWindowTitle[] = "Clean up your computer?";
constexpr char kMainText[] =
    "Chrome found software that harms your browsing experience. Remove related "
    "files from your computer and restore browser settings, including your "
    "search engine and home page.";
constexpr char kAcceptButtonLabel[] = "Cleanup";
constexpr char kAdvancedButtonLabel[] = "Advanced";

}  // namespace

ChromeCleanerDialogController::ChromeCleanerDialogController() {}

ChromeCleanerDialogController::~ChromeCleanerDialogController() = default;

base::string16 ChromeCleanerDialogController::GetWindowTitle() const {
  return base::UTF8ToUTF16(kWindowTitle);
}

base::string16 ChromeCleanerDialogController::GetMainText() const {
  return base::UTF8ToUTF16(kMainText);
}

base::string16 ChromeCleanerDialogController::GetAcceptButtonLabel() const {
  return base::UTF8ToUTF16(kAcceptButtonLabel);
}

base::string16 ChromeCleanerDialogController::GetAdvancedButtonLabel() const {
  return base::UTF8ToUTF16(kAdvancedButtonLabel);
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

void ChromeCleanerDialogController::AdvancedButtonClicked() {
  OnInteractionDone();
}

void ChromeCleanerDialogController::OnInteractionDone() {
  delete this;
}

}  // namespace safe_browsing
