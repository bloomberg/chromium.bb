// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"

#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

WrongHWIDScreen::WrongHWIDScreen(ScreenObserver* observer,
                                 WrongHWIDScreenActor* actor)
    : WizardScreen(observer), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

WrongHWIDScreen::~WrongHWIDScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void WrongHWIDScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void WrongHWIDScreen::Show() {
  if (actor_)
    actor_->Show();
}

void WrongHWIDScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string WrongHWIDScreen::GetName() const {
  return WizardController::kWrongHWIDScreenName;
}

void WrongHWIDScreen::OnExit() {
  get_screen_observer()->OnExit(ScreenObserver::WRONG_HWID_WARNING_SKIPPED);
}

void WrongHWIDScreen::OnActorDestroyed(WrongHWIDScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos

