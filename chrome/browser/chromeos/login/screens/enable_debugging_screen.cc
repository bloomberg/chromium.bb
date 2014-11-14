// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/enable_debugging_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"

namespace chromeos {

EnableDebuggingScreen::EnableDebuggingScreen(
    BaseScreenDelegate* delegate,
    EnableDebuggingScreenActor* actor)
    : BaseScreen(delegate), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

EnableDebuggingScreen::~EnableDebuggingScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void EnableDebuggingScreen::PrepareToShow() {
  if (actor_)
    actor_->PrepareToShow();
}

void EnableDebuggingScreen::Show() {
  if (actor_)
    actor_->Show();
}

void EnableDebuggingScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string EnableDebuggingScreen::GetName() const {
  return WizardController::kEnableDebuggingScreenName;
}

void EnableDebuggingScreen::OnExit(bool success) {
  Finish(success ? BaseScreenDelegate::ENABLE_DEBUGGING_FINISHED :
                   BaseScreenDelegate::ENABLE_DEBUGGING_CANCELED);
}

void EnableDebuggingScreen::OnActorDestroyed(
    EnableDebuggingScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
