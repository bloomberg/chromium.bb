// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_bubble_experiment.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"

namespace password_bubble_experiment {
namespace {

const char kBrandingExperimentName[] = "PasswordBubbleBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";

} // namespace

void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  // TODO(vasilii): store the statistics.
}

bool IsEnabledSmartLockBranding(Profile* profile) {
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  return (signin && signin->IsAuthenticated() &&
          base::FieldTrialList::FindFullName(kBrandingExperimentName) ==
              kSmartLockBrandingGroupName);
}


}  // namespace password_bubble_experiment
