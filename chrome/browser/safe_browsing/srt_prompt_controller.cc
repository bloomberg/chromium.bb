// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_prompt_controller.h"

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

SRTPromptController::SRTPromptController() {}

SRTPromptController::~SRTPromptController() = default;

base::string16 SRTPromptController::GetWindowTitle() const {
  return base::UTF8ToUTF16(kWindowTitle);
}

base::string16 SRTPromptController::GetMainText() const {
  return base::UTF8ToUTF16(kMainText);
}

base::string16 SRTPromptController::GetAcceptButtonLabel() const {
  return base::UTF8ToUTF16(kAcceptButtonLabel);
}

base::string16 SRTPromptController::GetAdvancedButtonLabel() const {
  return base::UTF8ToUTF16(kAdvancedButtonLabel);
}

void SRTPromptController::DialogShown() {}

void SRTPromptController::Accept() {
  OnInteractionDone();
}

void SRTPromptController::Cancel() {
  OnInteractionDone();
}

void SRTPromptController::Close() {
  OnInteractionDone();
}

void SRTPromptController::AdvancedButtonClicked() {
  OnInteractionDone();
}

void SRTPromptController::OnInteractionDone() {
  delete this;
}

}  // namespace safe_browsing
