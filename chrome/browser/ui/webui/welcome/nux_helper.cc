// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux_helper.h"

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/policy/browser_signin_policy_handler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/webui/welcome/nux/constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

#if defined(OS_MACOSX)
#include "base/enterprise_util.h"
#endif  // defined(OS_MACOSX)

namespace nux {

bool CanShowGoogleAppModule(const policy::PolicyMap& policies) {
  const base::Value* bookmark_bar_enabled_value =
      policies.GetValue(policy::key::kBookmarkBarEnabled);

  if (bookmark_bar_enabled_value && !bookmark_bar_enabled_value->GetBool()) {
    return false;
  }

  const base::Value* edit_bookmarks_value =
      policies.GetValue(policy::key::kEditBookmarksEnabled);

  if (edit_bookmarks_value && !edit_bookmarks_value->GetBool()) {
    return false;
  }

  return true;
}

bool CanShowNTPBackgroundModule(const policy::PolicyMap& policies) {
  // We shouldn't show this module if any policy is set that overrides the NTP.
  return !policies.GetValue(policy::key::kNewTabPageLocation);
}

bool CanShowSetDefaultModule(const policy::PolicyMap& policies) {
  const base::Value* set_default_value =
      policies.GetValue(policy::key::kDefaultBrowserSettingEnabled);

  return !set_default_value || set_default_value->GetBool();
}

bool CanShowSigninModule(const policy::PolicyMap& policies) {
  const base::Value* browser_signin_value =
      policies.GetValue(policy::key::kBrowserSignin);

  if (!browser_signin_value)
    return true;

  int int_browser_signin_value;
  bool success = browser_signin_value->GetAsInteger(&int_browser_signin_value);
  DCHECK(success);

  return static_cast<policy::BrowserSigninMode>(int_browser_signin_value) !=
         policy::BrowserSigninMode::kDisabled;
}

#if defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)
// These feature flags are used to tie our experiment to specific studies.
// go/navi-app-variation for details.
// TODO(hcarmona): find a solution that scales better.
const base::Feature kNaviControlEnabled = {"NaviControlEnabled",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNaviAppVariationEnabled = {
    "NaviAppVariationEnabled", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNaviNTPVariationEnabled = {
    "NaviNTPVariationEnabled", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kNaviShortcutVariationEnabled = {
    "NaviShortcutVariationEnabled", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)

// This feature flag is used to force the feature to be turned on for non-win
// and non-branded builds, like with tests or development on other platforms.
const base::Feature kNuxOnboardingForceEnabled = {
    "NuxOnboardingForceEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

// The value of these FeatureParam values should be a comma-delimited list
// of element names whitelisted in the MODULES_WHITELIST list, defined in
// chrome/browser/resources/welcome/onboarding_welcome/welcome_app.js
const base::FeatureParam<std::string> kNuxOnboardingForceEnabledNewUserModules =
    {&kNuxOnboardingForceEnabled, "new-user-modules",
     "nux-google-apps,nux-ntp-background,nux-set-as-default,"
     "signin-view"};
const base::FeatureParam<std::string>
    kNuxOnboardingForceEnabledReturningUserModules = {
        &kNuxOnboardingForceEnabled, "returning-user-modules",
        "nux-set-as-default"};
const base::FeatureParam<bool> kNuxOnboardingForceEnabledShowGoogleApp = {
    &kNuxOnboardingForceEnabled, "app-variation-enabled", false};

// Our current running experiment of testing the nux-ntp-background module
// depends on the Local NTP feature/experiment being enabled. To avoid polluting
// our data with users who cannot use the nux-ntp-background module, we need
// to check to make sure the Local NTP feature is enabled before running
// any experiment or even reading any feature params from our experiment.
bool CanExperimentWithVariations(Profile* profile) {
  return (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kForceLocalNtp) ||
          base::FeatureList::IsEnabled(features::kUseGoogleLocalNtp)) &&
         search::DefaultSearchProviderIsGoogle(profile);
}

// Must match study name in configs.
const char kNuxOnboardingStudyName[] = "NaviOnboarding";

std::string GetOnboardingGroup(Profile* profile) {
  if (!CanExperimentWithVariations(profile)) {
    // If we cannot run any variations, we bucket the users into a separate
    // synthetic group that we will ignore data for.
    return "NaviNoVariationSynthetic";
  }

  // We need to use |base::GetFieldTrialParamValue| instead of
  // |base::FeatureParam| because our control group needs a custom value for
  // this param.
  return base::GetFieldTrialParamValue(kNuxOnboardingStudyName,
                                       "onboarding-group");
}

bool IsNuxOnboardingEnabled(Profile* profile) {
  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    return true;
  }

#if defined(GOOGLE_CHROME_BUILD)

#if defined(OS_MACOSX)
  return !base::IsMachineExternallyManaged();
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
  // To avoid diluting data collection, existing users should not be assigned
  // an onboarding group. So, |prefs::kNaviOnboardGroup| is used to
  // short-circuit the feature checks below.
  PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return false;
  }

  std::string onboard_group = prefs->GetString(prefs::kNaviOnboardGroup);

  if (onboard_group.empty()) {
    // Users who onboarded before Navi or are part of an enterprise.
    return false;
  }

  if (!CanExperimentWithVariations(profile)) {
    return true;  // Default Navi behavior.
  }

  // User will be tied to their original onboarding group, even after
  // experiment ends.
  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "NaviOnboardingSynthetic", onboard_group);

  // Check for feature based on onboarding group.
  // TODO(hcarmona): find a solution that scales better.
  if (onboard_group.compare("ControlSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviControlEnabled);
  else if (onboard_group.compare("AppVariationSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviAppVariationEnabled);
  else if (onboard_group.compare("NTPVariationSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviNTPVariationEnabled);
  else if (onboard_group.compare("ShortcutVariationSynthetic-008") == 0)
    base::FeatureList::IsEnabled(kNaviShortcutVariationEnabled);

  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingFeature)) {
    return true;
  }
#endif  // defined(OS_WIN)

#endif  // defined(GOOGLE_CHROME_BUILD)

  return false;
}

bool IsAppVariationEnabled() {
  return kNuxOnboardingForceEnabledShowGoogleApp.Get() ||
         kNuxOnboardingShowGoogleApp.Get();
}

const policy::PolicyMap& GetPoliciesFromProfile(Profile* profile) {
  policy::ProfilePolicyConnector* profile_connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile);
  DCHECK(profile_connector);
  return profile_connector->policy_service()->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
}

std::vector<std::string> GetAvailableModules(
    const policy::PolicyMap& policies) {
  std::vector<std::string> available_modules;

  if (CanShowGoogleAppModule(policies))
    available_modules.push_back("nux-google-apps");
  if (CanShowNTPBackgroundModule(policies))
    available_modules.push_back("nux-ntp-background");
  if (CanShowSetDefaultModule(policies))
    available_modules.push_back("nux-set-as-default");
  if (CanShowSigninModule(policies))
    available_modules.push_back("signin-view");

  return available_modules;
}

std::string FilterModules(const std::string& requested_modules,
                          const std::vector<std::string>& available_modules) {
  std::vector<std::string> requested_list = base::SplitString(
      requested_modules, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> filtered_modules;

  std::copy_if(requested_list.begin(), requested_list.end(),
               std::back_inserter(filtered_modules),
               [available_modules](std::string module) {
                 return !module.empty() &&
                        base::ContainsValue(available_modules, module);
               });

  return base::JoinString(filtered_modules, ",");
}

base::DictionaryValue GetNuxOnboardingModules(Profile* profile) {
  // This function should not be called when nux onboarding feature is not on.
  DCHECK(nux::IsNuxOnboardingEnabled(profile));

  std::string new_user_modules = kDefaultNewUserModules;
  std::string returning_user_modules = kDefaultReturningUserModules;

  if (base::FeatureList::IsEnabled(nux::kNuxOnboardingForceEnabled)) {
    new_user_modules = kNuxOnboardingForceEnabledNewUserModules.Get();
    returning_user_modules =
        kNuxOnboardingForceEnabledReturningUserModules.Get();
  } else if (CanExperimentWithVariations(profile)) {
    new_user_modules = kNuxOnboardingNewUserModules.Get();
    returning_user_modules = kNuxOnboardingReturningUserModules.Get();
  }

  const policy::PolicyMap& policies = GetPoliciesFromProfile(profile);
  std::vector<std::string> available_modules = GetAvailableModules(policies);

  base::DictionaryValue modules;
  modules.SetString("new-user",
                    FilterModules(new_user_modules, available_modules));
  modules.SetString("returning-user",
                    FilterModules(returning_user_modules, available_modules));
  return modules;
}
}  // namespace nux
