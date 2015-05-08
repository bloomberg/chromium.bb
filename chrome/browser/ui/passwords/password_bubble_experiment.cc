// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_bubble_experiment.h"

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/variations/variations_associated_data.h"

namespace password_bubble_experiment {
namespace {

const char kBrandingExperimentName[] = "PasswordBubbleBranding";
const char kSmartLockBrandingGroupName[] = "SmartLockBranding";

// Return the number of consecutive times the user clicked "Not now" in the
// "Save password" bubble.
int GetCurrentNopesCount(PrefService* prefs) {
  return prefs->GetInteger(prefs::kPasswordBubbleNopesCount);
}

} // namespace

const char kExperimentName[] = "PasswordBubbleAlgorithm";
const char kParamNopeThreshold[] = "consecutive_nope_threshold";

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kPasswordBubbleNopesCount, 0);
}

bool ShouldShowNeverForThisSiteDefault(PrefService* prefs) {
  if (!base::FieldTrialList::TrialExists(kExperimentName))
    return false;
  std::string param =
      variations::GetVariationParamValue(kExperimentName, kParamNopeThreshold);

  int threshold = 0;
  return base::StringToInt(param, &threshold) &&
      GetCurrentNopesCount(prefs) >= threshold;
}

void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  // Clicking "Never" doesn't change the experiment state.
  if (reason == password_manager::metrics_util::CLICKED_NEVER)
    return;
  int nopes_count = GetCurrentNopesCount(prefs);
  if (reason == password_manager::metrics_util::CLICKED_NOPE)
    nopes_count++;
  else
    nopes_count = 0;
  prefs->SetInteger(prefs::kPasswordBubbleNopesCount, nopes_count);
}

bool IsEnabledSmartLockBranding(Profile* profile) {
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(profile);
  return (signin && signin->IsAuthenticated() &&
          base::FieldTrialList::FindFullName(kBrandingExperimentName) ==
              kSmartLockBrandingGroupName);
}


}  // namespace password_bubble_experiment
