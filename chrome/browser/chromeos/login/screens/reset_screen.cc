// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/reset_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

ResetScreen::ResetScreen(ScreenObserver* observer, ResetScreenActor* actor)
    : WizardScreen(observer), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

ResetScreen::~ResetScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void ResetScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void ResetScreen::Show() {
  if (actor_)
    actor_->Show();
}

void ResetScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string ResetScreen::GetName() const {
  return WizardController::kResetScreenName;
}

void ResetScreen::OnExit() {
  get_screen_observer()->OnExit(ScreenObserver::RESET_CANCELED);
}

void ResetScreen::OnActorDestroyed(ResetScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
