// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/locally_managed_user_creation_screen.h"

#include "chrome/browser/chromeos/login/managed/locally_managed_user_controller.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

LocallyManagedUserCreationScreen::LocallyManagedUserCreationScreen(
    ScreenObserver* observer,
    LocallyManagedUserCreationScreenHandler* actor)
    : WizardScreen(observer), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

LocallyManagedUserCreationScreen::~LocallyManagedUserCreationScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void LocallyManagedUserCreationScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void LocallyManagedUserCreationScreen::SetParameters(string16 name,
    std::string password) {
  name_ = name;
  password_ = password;
}
void LocallyManagedUserCreationScreen::Show() {
  if (actor_)
    actor_->Show();
  // Make sure no two controllers exist at the same time.
  controller_.reset();
  controller_.reset(new LocallyManagedUserController());

  controller_->StartCreation(name_, password_);
  if (actor_)
    actor_->ShowSuccessMessage();
}

void LocallyManagedUserCreationScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string LocallyManagedUserCreationScreen::GetName() const {
  return WizardController::kLocallyManagedUserCreationScreenName;
}

void LocallyManagedUserCreationScreen::AbortFlow() {
  controller_->FinishCreation();
}

void LocallyManagedUserCreationScreen::FinishFlow() {
  controller_->FinishCreation();
}

void LocallyManagedUserCreationScreen::OnExit() {
}

void LocallyManagedUserCreationScreen::OnActorDestroyed(
    LocallyManagedUserCreationScreenHandler* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
