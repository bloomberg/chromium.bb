// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

KioskAutolaunchScreen::KioskAutolaunchScreen(
    KioskAutolaunchScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(OobeScreen::SCREEN_KIOSK_AUTOLAUNCH),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->SetDelegate(this);
}

KioskAutolaunchScreen::~KioskAutolaunchScreen() {
  if (view_)
    view_->SetDelegate(NULL);
}

void KioskAutolaunchScreen::Show() {
  if (view_)
    view_->Show();
}

void KioskAutolaunchScreen::OnExit(bool confirmed) {
  exit_callback_.Run(confirmed ? Result::COMPLETED : Result::CANCELED);
}

void KioskAutolaunchScreen::OnViewDestroyed(KioskAutolaunchScreenView* view) {
  if (view_ == view)
    view_ = NULL;
}

}  // namespace chromeos
