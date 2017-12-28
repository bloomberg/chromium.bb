// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"

#include <string>

#include "base/logging.h"

namespace chromeos {
namespace {

constexpr const char kUserActionButtonClicked[] = "save-and-continue";

}  // namespace

SyncConsentScreen::SyncConsentScreen(BaseScreenDelegate* base_screen_delegate,
                                     SyncConsentScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_SYNC_CONSENT),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

SyncConsentScreen::~SyncConsentScreen() {
  view_->Bind(NULL);
}

void SyncConsentScreen::Show() {
  // Show the screen.
  view_->Show();
}

void SyncConsentScreen::Hide() {
  view_->Hide();
}

void SyncConsentScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionButtonClicked) {
    Finish(ScreenExitCode::SYNC_CONSENT_FINISHED);
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
