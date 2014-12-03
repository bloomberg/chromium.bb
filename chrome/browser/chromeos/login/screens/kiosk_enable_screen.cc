// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

KioskEnableScreen::KioskEnableScreen(BaseScreenDelegate* base_screen_delegate,
                                     KioskEnableScreenActor* actor)
    : BaseScreen(base_screen_delegate), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

KioskEnableScreen::~KioskEnableScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void KioskEnableScreen::Show() {
  if (actor_)
    actor_->Show();
}

std::string KioskEnableScreen::GetName() const {
  return WizardController::kKioskEnableScreenName;
}

void KioskEnableScreen::OnExit() {
  Finish(BaseScreenDelegate::KIOSK_ENABLE_COMPLETED);
}

void KioskEnableScreen::OnActorDestroyed(KioskEnableScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
