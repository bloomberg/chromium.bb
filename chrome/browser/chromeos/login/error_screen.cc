// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/error_screen.h"

#include "chrome/browser/chromeos/login/error_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

ErrorScreen::ErrorScreen(ScreenObserver* screen_observer,
                         ErrorScreenActor* actor)
    : WizardScreen(screen_observer),
      actor_(actor),
      parent_screen_(OobeDisplay::SCREEN_UNKNOWN) {
  DCHECK(actor_);
}

ErrorScreen::~ErrorScreen() {
}

void ErrorScreen::PrepareToShow() {
}

void ErrorScreen::Show() {
  DCHECK(actor_);
  actor_->Show(parent_screen(), NULL);
}

void ErrorScreen::Hide() {
  DCHECK(actor_);
  actor_->Hide();
}

std::string ErrorScreen::GetName() const {
  return WizardController::kErrorScreenName;
}

void ErrorScreen::FixCaptivePortal() {
  DCHECK(actor_);
  actor_->FixCaptivePortal();
}

void ErrorScreen::ShowCaptivePortal() {
  DCHECK(actor_);
  actor_->ShowCaptivePortal();
}

void ErrorScreen::HideCaptivePortal() {
  DCHECK(actor_);
  actor_->HideCaptivePortal();
}

void ErrorScreen::SetUIState(UIState ui_state) {
  DCHECK(actor_);
  actor_->SetUIState(ui_state);
}

void ErrorScreen::SetErrorState(ErrorState error_state,
                                const std::string& network) {
  DCHECK(actor_);
  actor_->SetErrorState(error_state, network);
}

}  // namespace chromeos
