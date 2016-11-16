// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"

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
    : BaseScreen(base_screen_delegate), actor_(actor) {
  DCHECK(actor_);
  if (actor_)
    actor_->SetDelegate(this);
}

ArcTermsOfServiceScreen::~ArcTermsOfServiceScreen() {
  if (actor_)
    actor_->SetDelegate(nullptr);
}

void ArcTermsOfServiceScreen::PrepareToShow() {}

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

std::string ArcTermsOfServiceScreen::GetName() const {
  return WizardController::kArcTermsOfServiceScreenName;
}

void ArcTermsOfServiceScreen::OnSkip() {
  ApplyTerms(false);
}

void ArcTermsOfServiceScreen::OnAccept() {
  ApplyTerms(true);
}

void ArcTermsOfServiceScreen::ApplyTerms(bool accepted) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  profile->GetPrefs()->SetBoolean(prefs::kArcTermsAccepted, accepted);
  profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, accepted);

  Finish(BaseScreenDelegate::ARC_TERMS_OF_SERVICE_FINISHED);
}

void ArcTermsOfServiceScreen::OnActorDestroyed(
    ArcTermsOfServiceScreenActor* actor) {
  DCHECK_EQ(actor, actor_);
  actor_ = nullptr;
}

}  // namespace chromeos
