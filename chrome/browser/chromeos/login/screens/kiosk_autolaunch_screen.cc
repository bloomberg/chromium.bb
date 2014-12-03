// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

KioskAutolaunchScreen::KioskAutolaunchScreen(
    BaseScreenDelegate* base_screen_delegate,
    KioskAutolaunchScreenActor* actor)
    : BaseScreen(base_screen_delegate), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

KioskAutolaunchScreen::~KioskAutolaunchScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void KioskAutolaunchScreen::Show() {
  if (actor_)
    actor_->Show();
}

std::string KioskAutolaunchScreen::GetName() const {
  return WizardController::kKioskAutolaunchScreenName;
}

void KioskAutolaunchScreen::OnExit(bool confirmed) {
  Finish(confirmed ? BaseScreenDelegate::KIOSK_AUTOLAUNCH_CONFIRMED
                   : BaseScreenDelegate::KIOSK_AUTOLAUNCH_CANCELED);
}

void KioskAutolaunchScreen::OnActorDestroyed(
    KioskAutolaunchScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
