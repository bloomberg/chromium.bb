// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/kiosk_next_flow_observer.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace supervision {

KioskNextFlowObserver::KioskNextFlowObserver(Profile* profile,
                                             OnboardingFlowModel* flow_model)
    : profile_(profile), flow_model_(flow_model) {
  flow_model_->AddObserver(this);
}

KioskNextFlowObserver::~KioskNextFlowObserver() {
  flow_model_->RemoveObserver(this);
}

void KioskNextFlowObserver::StepFinishedLoading(
    OnboardingFlowModel::Step step) {
  if (step == OnboardingFlowModel::Step::kStart)
    profile_->GetPrefs()->SetBoolean(ash::prefs::kKioskNextShellEligible, true);
}

void KioskNextFlowObserver::WillExitFlow(
    OnboardingFlowModel::Step step,
    OnboardingFlowModel::ExitReason reason) {
  if (reason == OnboardingFlowModel::ExitReason::kUserReachedEnd)
    profile_->GetPrefs()->SetBoolean(ash::prefs::kKioskNextShellEnabled, true);
}

}  // namespace supervision
}  // namespace chromeos
