// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux_helper.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/welcome/nux/constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace nux {
// This feature flag is used to force the feature to be turned on for non-win
// and non-branded builds, like with tests or development on other platforms.
const base::Feature kNuxOnboardingForceEnabled = {
    "NuxOnboardingForceEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

// The value of these FeatureParam values should be a comma-delimited list
// of element names whitelisted in the MODULES_WHITELIST list, defined in
// chrome/browser/resources/welcome/onboarding_welcome/welcome_app.js
const base::FeatureParam<std::string> kNuxOnboardingForceEnabledNewUserModules =
    {&kNuxOnboardingForceEnabled, "new-user-modules",
     "nux-email,nux-google-apps,nux-set-as-default,signin-view"};
const base::FeatureParam<std::string>
    kNuxOnboardingForceEnabledReturningUserModules = {
        &kNuxOnboardingForceEnabled, "returning-user-modules",
        "nux-set-as-default"};
const base::FeatureParam<bool> kNuxOnboardingForceEnabledShowEmailInterstitial =
    {&kNuxOnboardingForceEnabled, "show-email-interstitial", true};

int GetOnboardingGroup() {
  // Preppend a 0 to avoid issues with empty string.
  return std::stoi("0" + base::GetFieldTrialParamValue(
                             /* Must match finch study name */ "NaviOnboarding",
                             "onboarding-group"));
}

bool IsNuxOnboardingEnabled(Profile* profile) {
  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    return true;
  } else {
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
    // To avoid diluting data collection, existing users should not be assigned
    // an NUX group. So, the kNuxOnboardGroup integer is used to short-circuit
    // the feature checks below.
    PrefService* prefs = profile->GetPrefs();
    if (!prefs)
      return false;

    int onboard_group = prefs->GetInteger(prefs::kNuxOnboardGroup);

    if (onboard_group == 0)
      return false;

    int current_group = GetOnboardingGroup();
    if (onboard_group != current_group) {
      // Remove user from group if they're not part of the current experiment.
      prefs->SetInteger(prefs::kNuxOnboardGroup, 0);
      return false;
    }

    return base::FeatureList::IsEnabled(nux::kNuxOnboardingFeature);
#else
    return false;
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  }
}

base::DictionaryValue GetNuxOnboardingModules(Profile* profile) {
  // This function should not be called when nux onboarding feature is not on.
  DCHECK(nux::IsNuxOnboardingEnabled(profile));

  base::DictionaryValue modules;

  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    modules.SetString("new-user",
                      kNuxOnboardingForceEnabledNewUserModules.Get());
    modules.SetString("returning-user",
                      kNuxOnboardingForceEnabledReturningUserModules.Get());
    modules.SetBoolean("show-email-interstitial",
                       kNuxOnboardingForceEnabledShowEmailInterstitial.Get());
  } else {  // This means nux::kNuxOnboardingFeature is enabled.
    modules.SetString("new-user", kNuxOnboardingNewUserModules.Get());
    modules.SetString("returning-user",
                      kNuxOnboardingReturningUserModules.Get());
    modules.SetBoolean("show-email-interstitial",
                       kNuxOnboardingShowEmailInterstitial.Get());
  }

  return modules;
}
}  // namespace nux
