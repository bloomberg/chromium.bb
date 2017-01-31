// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

ArcTermsOfServiceScreen::ArcTermsOfServiceScreen(
    BaseScreenDelegate* base_screen_delegate,
    ArcTermsOfServiceScreenActor* actor)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE),
      actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->AddObserver(this);
}

ArcTermsOfServiceScreen::~ArcTermsOfServiceScreen() {
  if (actor_)
    actor_->RemoveObserver(this);
}

void ArcTermsOfServiceScreen::Show() {
  if (!actor_)
    return;

  // Show the screen.
  actor_->Show();
}

void ArcTermsOfServiceScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

void ArcTermsOfServiceScreen::OnSkip() {
  Finish(BaseScreenDelegate::ARC_TERMS_OF_SERVICE_FINISHED);
}

void ArcTermsOfServiceScreen::OnAccept() {
  Finish(BaseScreenDelegate::ARC_TERMS_OF_SERVICE_FINISHED);
}

void ArcTermsOfServiceScreen::OnActorDestroyed(
    ArcTermsOfServiceScreenActor* actor) {
  DCHECK_EQ(actor, actor_);
  actor_ = nullptr;
}

}  // namespace chromeos
