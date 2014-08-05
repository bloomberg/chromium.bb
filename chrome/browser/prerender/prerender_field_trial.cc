// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/variations_associated_data.h"

using base::FieldTrial;
using base::FieldTrialList;
using base::StringToInt;
using std::string;
using std::vector;

namespace prerender {

namespace {

const char kOmniboxTrialName[] = "PrerenderFromOmnibox";
int g_omnibox_trial_default_group_number = kint32min;

const char kDisabledGroup[] = "Disabled";
const char kEnabledGroup[] = "Enabled";

const char kLocalPredictorSpecTrialName[] = "PrerenderLocalPredictorSpec";
const char kLocalPredictorKeyName[] = "LocalPredictor";
const char kLocalPredictorUnencryptedSyncOnlyKeyName[] =
    "LocalPredictorUnencryptedSyncOnly";
const char kSideEffectFreeWhitelistKeyName[] = "SideEffectFreeWhitelist";
const char kPrerenderLaunchKeyName[] = "PrerenderLaunch";
const char kPrerenderAlwaysControlKeyName[] = "PrerenderAlwaysControl";
const char kPrerenderPrefetchKeyName[] = "PrerenderPrefetch";
const char kPrerenderQueryPrerenderServiceKeyName[] =
    "PrerenderQueryPrerenderService";
const char kPrerenderQueryPrerenderServiceCurrentURLKeyName[] =
    "PrerenderQueryPrerenderServiceCurrentURL";
const char kPrerenderQueryPrerenderServiceCandidateURLsKeyName[] =
    "PrerenderQueryPrerenderServiceCandidateURLs";
const char kPrerenderServiceBehaviorIDKeyName[] = "PrerenderServiceBehaviorID";
const char kPrerenderServiceFetchTimeoutKeyName[] =
    "PrerenderServiceFetchTimeoutMs";
const char kPrefetchListTimeoutKeyName[] = "PrefetchListTimeoutSeconds";
const char kPrerenderTTLKeyName[] = "PrerenderTTLSeconds";
const char kPrerenderPriorityHalfLifeTimeKeyName[] =
    "PrerenderPriorityHalfLifeTimeSeconds";
const char kMaxConcurrentPrerenderKeyName[] = "MaxConcurrentPrerenders";
const char kMaxLaunchPrerenderKeyName[] = "MaxLaunchPrerenders";
const char kSkipFragment[] = "SkipFragment";
const char kSkipHTTPS[] = "SkipHTTPS";
const char kSkipWhitelist[] = "SkipWhitelist";
const char kSkipServiceWhitelist[] = "SkipServiceWhitelist";
const char kSkipLoggedIn[] = "SkipLoggedIn";
const char kSkipDefaultNoPrerender[] = "SkipDefaultNoPrerender";
const char kPrerenderServiceURLPrefixParameterName[] =
    "PrerenderServiceURLPrefix";
const char kDefaultPrerenderServiceURLPrefix[] =
    "https://clients4.google.com/prerenderservice/?q=";
const int kMinPrerenderServiceTimeoutMs = 1;
const int kMaxPrerenderServiceTimeoutMs = 10000;
const int kDefaultPrerenderServiceTimeoutMs = 1000;
const int kMinPrefetchListTimeoutSeconds = 1;
const int kMaxPrefetchListTimeoutSeconds = 1800;
const int kDefaultPrefetchListTimeoutSeconds = 300;
const char kSkipPrerenderLocalCanadidates[] = "SkipPrerenderLocalCandidates";
const char kSkipPrerenderServiceCanadidates[] =
    "SkipPrerenderServiceCandidates";
const char kDisableSessionStorageNamespaceMerging[] =
    "DisableSessionStorageNamespaceMerging";
const char kPrerenderCookieStore[] = "PrerenderCookieStore";

void SetupPrerenderFieldTrial() {
  const FieldTrial::Probability divisor = 1000;

  FieldTrial::Probability control_probability;
  FieldTrial::Probability experiment_multi_prerender_probability;
  FieldTrial::Probability experiment_15min_ttl_probability;
  FieldTrial::Probability experiment_no_use_probability;

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    // Use very conservatives and stable settings in beta and stable.
    const FieldTrial::Probability release_prerender_enabled_probability = 980;
    const FieldTrial::Probability release_control_probability = 10;
    const FieldTrial::Probability
        release_experiment_multi_prerender_probability = 0;
    const FieldTrial::Probability release_experiment_15min_ttl_probability = 10;
    const FieldTrial::Probability release_experiment_no_use_probability = 0;
    COMPILE_ASSERT(
        release_prerender_enabled_probability + release_control_probability +
        release_experiment_multi_prerender_probability +
        release_experiment_15min_ttl_probability +
        release_experiment_no_use_probability == divisor,
        release_experiment_probabilities_must_equal_divisor);

    control_probability = release_control_probability;
    experiment_multi_prerender_probability =
        release_experiment_multi_prerender_probability;
    experiment_15min_ttl_probability = release_experiment_15min_ttl_probability;
    experiment_no_use_probability = release_experiment_no_use_probability;
  } else {
    // In testing channels, use more experiments and a larger control group to
    // improve quality of data.
    const FieldTrial::Probability dev_prerender_enabled_probability = 250;
    const FieldTrial::Probability dev_control_probability = 250;
    const FieldTrial::Probability
        dev_experiment_multi_prerender_probability = 250;
    const FieldTrial::Probability dev_experiment_15min_ttl_probability = 125;
    const FieldTrial::Probability dev_experiment_no_use_probability = 125;
    COMPILE_ASSERT(dev_prerender_enabled_probability + dev_control_probability +
                   dev_experiment_multi_prerender_probability +
                   dev_experiment_15min_ttl_probability +
                   dev_experiment_no_use_probability == divisor,
                   dev_experiment_probabilities_must_equal_divisor);

    control_probability = dev_control_probability;
    experiment_multi_prerender_probability =
        dev_experiment_multi_prerender_probability;
    experiment_15min_ttl_probability = dev_experiment_15min_ttl_probability;
    experiment_no_use_probability = dev_experiment_no_use_probability;
  }

  int prerender_enabled_group = -1;
  scoped_refptr<FieldTrial> trial(
      FieldTrialList::FactoryGetFieldTrial(
          "Prerender", divisor, "PrerenderEnabled",
          2014, 12, 31, FieldTrial::SESSION_RANDOMIZED,
          &prerender_enabled_group));
  const int control_group =
      trial->AppendGroup("PrerenderControl",
                         control_probability);
  const int experiment_multi_prerender_group =
      trial->AppendGroup("PrerenderMulti",
                         experiment_multi_prerender_probability);
  const int experiment_15_min_TTL_group =
      trial->AppendGroup("Prerender15minTTL",
                         experiment_15min_ttl_probability);
  const int experiment_no_use_group =
      trial->AppendGroup("PrerenderNoUse",
                         experiment_no_use_probability);

  const int trial_group = trial->group();
  if (trial_group == prerender_enabled_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP);
  } else if (trial_group == control_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  } else if (trial_group == experiment_multi_prerender_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP);
  } else if (trial_group == experiment_15_min_TTL_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_15MIN_TTL_GROUP);
  } else if (trial_group == experiment_no_use_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP);
  } else {
    NOTREACHED();
  }
}

}  // end namespace

void ConfigureOmniboxPrerender();

void ConfigurePrerender(const CommandLine& command_line) {
  enum PrerenderOption {
    PRERENDER_OPTION_AUTO,
    PRERENDER_OPTION_DISABLED,
    PRERENDER_OPTION_ENABLED,
  };

  PrerenderOption prerender_option = PRERENDER_OPTION_AUTO;
  if (command_line.HasSwitch(switches::kPrerenderMode)) {
    const string switch_value =
        command_line.GetSwitchValueASCII(switches::kPrerenderMode);

    if (switch_value == switches::kPrerenderModeSwitchValueAuto) {
      prerender_option = PRERENDER_OPTION_AUTO;
    } else if (switch_value == switches::kPrerenderModeSwitchValueDisabled) {
      prerender_option = PRERENDER_OPTION_DISABLED;
    } else if (switch_value.empty() ||
               switch_value == switches::kPrerenderModeSwitchValueEnabled) {
      // The empty string means the option was provided with no value, and that
      // means enable.
      prerender_option = PRERENDER_OPTION_ENABLED;
    } else {
      prerender_option = PRERENDER_OPTION_DISABLED;
      LOG(ERROR) << "Invalid --prerender option received on command line: "
                 << switch_value;
      LOG(ERROR) << "Disabling prerendering!";
    }
  }

  switch (prerender_option) {
    case PRERENDER_OPTION_AUTO:
      SetupPrerenderFieldTrial();
      break;
    case PRERENDER_OPTION_DISABLED:
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_DISABLED);
      break;
    case PRERENDER_OPTION_ENABLED:
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_ENABLED);
      break;
    default:
      NOTREACHED();
  }

  ConfigureOmniboxPrerender();
}

void ConfigureOmniboxPrerender() {
  // Field trial to see if we're enabled.
  const FieldTrial::Probability kDivisor = 100;

  FieldTrial::Probability kDisabledProbability = 10;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    kDisabledProbability = 1;
  }
  scoped_refptr<FieldTrial> omnibox_prerender_trial(
      FieldTrialList::FactoryGetFieldTrial(
          kOmniboxTrialName, kDivisor, "OmniboxPrerenderEnabled",
          2014, 12, 31, FieldTrial::SESSION_RANDOMIZED,
          &g_omnibox_trial_default_group_number));
  omnibox_prerender_trial->AppendGroup("OmniboxPrerenderDisabled",
                                       kDisabledProbability);
}

bool IsOmniboxEnabled(Profile* profile) {
  if (!profile)
    return false;

  if (!PrerenderManager::IsPrerenderingPossible())
    return false;

  // Override any field trial groups if the user has set a command line flag.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrerenderFromOmnibox)) {
    const string switch_value =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPrerenderFromOmnibox);

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueEnabled)
      return true;

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueDisabled)
      return false;

    DCHECK_EQ(switches::kPrerenderFromOmniboxSwitchValueAuto, switch_value);
  }

  const int group = FieldTrialList::FindValue(kOmniboxTrialName);
  return group == FieldTrial::kNotFinalized ||
         group == g_omnibox_trial_default_group_number;
}

/*
PrerenderLocalPredictorSpec is a field trial, and its value must have the
following format:
key1=value1:key2=value2:key3=value3
eg "LocalPredictor=Enabled:SideEffectFreeWhitelist=Enabled"
The function below extracts the value corresponding to a key provided from the
LocalPredictorSpec.
*/
string GetLocalPredictorSpecValue(string spec_key) {
  vector<string> elements;
  base::SplitString(FieldTrialList::FindFullName(kLocalPredictorSpecTrialName),
                    ':', &elements);
  for (int i = 0; i < static_cast<int>(elements.size()); i++) {
    vector<string> key_value;
    base::SplitString(elements[i], '=', &key_value);
    if (key_value.size() == 2 && key_value[0] == spec_key)
      return key_value[1];
  }
  return string();
}

bool IsUnencryptedSyncEnabled(Profile* profile) {
  ProfileSyncService* service = ProfileSyncServiceFactory::GetInstance()->
      GetForProfile(profile);
  return service && service->GetOpenTabsUIDelegate() &&
      !service->EncryptEverythingEnabled();
}

// Indicates whether the Local Predictor is enabled based on field trial
// selection.
bool IsLocalPredictorEnabled() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  return false;
#endif
  return
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePrerenderLocalPredictor) &&
      GetLocalPredictorSpecValue(kLocalPredictorKeyName) == kEnabledGroup;
}

bool DisableLocalPredictorBasedOnSyncAndConfiguration(Profile* profile) {
  return
      GetLocalPredictorSpecValue(kLocalPredictorUnencryptedSyncOnlyKeyName) ==
          kEnabledGroup &&
      !IsUnencryptedSyncEnabled(profile);
}

bool IsLoggedInPredictorEnabled() {
  return IsLocalPredictorEnabled();
}

bool IsSideEffectFreeWhitelistEnabled() {
  return IsLocalPredictorEnabled() &&
      GetLocalPredictorSpecValue(kSideEffectFreeWhitelistKeyName) !=
      kDisabledGroup;
}

bool IsLocalPredictorPrerenderLaunchEnabled() {
  return GetLocalPredictorSpecValue(kPrerenderLaunchKeyName) != kDisabledGroup;
}

bool IsLocalPredictorPrerenderAlwaysControlEnabled() {
  // If we prefetch rather than prerender, we automatically also prerender
  // as a control group only.
  return (GetLocalPredictorSpecValue(kPrerenderAlwaysControlKeyName) ==
          kEnabledGroup) || IsLocalPredictorPrerenderPrefetchEnabled();
}

bool IsLocalPredictorPrerenderPrefetchEnabled() {
  return GetLocalPredictorSpecValue(kPrerenderPrefetchKeyName) ==
      kEnabledGroup;
}

bool ShouldQueryPrerenderService(Profile* profile) {
  return IsUnencryptedSyncEnabled(profile) &&
      GetLocalPredictorSpecValue(kPrerenderQueryPrerenderServiceKeyName) ==
      kEnabledGroup;
}

bool ShouldQueryPrerenderServiceForCurrentURL() {
  return GetLocalPredictorSpecValue(
      kPrerenderQueryPrerenderServiceCurrentURLKeyName) != kDisabledGroup;
}

bool ShouldQueryPrerenderServiceForCandidateURLs() {
  return GetLocalPredictorSpecValue(
      kPrerenderQueryPrerenderServiceCandidateURLsKeyName) != kDisabledGroup;
}

string GetPrerenderServiceURLPrefix() {
  string prefix = variations::GetVariationParamValue(
      kLocalPredictorSpecTrialName,
      kPrerenderServiceURLPrefixParameterName);
  return prefix.empty() ? kDefaultPrerenderServiceURLPrefix : prefix;
}

int GetPrerenderServiceBehaviorID() {
  int id;
  StringToInt(GetLocalPredictorSpecValue(kPrerenderServiceBehaviorIDKeyName),
              &id);
  // The behavior ID must be non-negative.
  return std::max(id, 0);
}

int GetPrerenderServiceFetchTimeoutMs() {
  int result;
  StringToInt(GetLocalPredictorSpecValue(kPrerenderServiceFetchTimeoutKeyName),
              &result);
  // If the value is outside the valid range, use the default value.
  return (result < kMinPrerenderServiceTimeoutMs ||
          result > kMaxPrerenderServiceTimeoutMs) ?
      kDefaultPrerenderServiceTimeoutMs : result;
}

int GetPrerenderPrefetchListTimeoutSeconds() {
  int result;
  StringToInt(GetLocalPredictorSpecValue(kPrefetchListTimeoutKeyName), &result);
  // If the value is outside the valid range, use the default value.
  return (result < kMinPrefetchListTimeoutSeconds ||
          result > kMaxPrefetchListTimeoutSeconds) ?
      kDefaultPrefetchListTimeoutSeconds : result;
}

int GetLocalPredictorTTLSeconds() {
  int ttl;
  StringToInt(GetLocalPredictorSpecValue(kPrerenderTTLKeyName), &ttl);
  // If the value is outside of 10s or 600s, use a default value of 180s.
  return (ttl < 10 || ttl > 600) ? 180 : ttl;
}

int GetLocalPredictorPrerenderPriorityHalfLifeTimeSeconds() {
  int half_life_time;
  StringToInt(GetLocalPredictorSpecValue(kPrerenderPriorityHalfLifeTimeKeyName),
              &half_life_time);
  // Sanity check: Ensure the half life time is non-negative.
  return std::max(half_life_time, 0);
}

int GetLocalPredictorMaxConcurrentPrerenders() {
  int num_prerenders;
  StringToInt(GetLocalPredictorSpecValue(kMaxConcurrentPrerenderKeyName),
              &num_prerenders);
  // Sanity check: Ensure the number of prerenders is between 1 and 10.
  return std::min(std::max(num_prerenders, 1), 10);
}

int GetLocalPredictorMaxLaunchPrerenders() {
  int num_prerenders;
  StringToInt(GetLocalPredictorSpecValue(kMaxLaunchPrerenderKeyName),
              &num_prerenders);
  // Sanity check: Ensure the number of prerenders is between 1 and 10.
  return std::min(std::max(num_prerenders, 1), 10);
}

bool SkipLocalPredictorFragment() {
  return GetLocalPredictorSpecValue(kSkipFragment) == kEnabledGroup;
}

bool SkipLocalPredictorHTTPS() {
  return GetLocalPredictorSpecValue(kSkipHTTPS) == kEnabledGroup;
}

bool SkipLocalPredictorWhitelist() {
  return GetLocalPredictorSpecValue(kSkipWhitelist) == kEnabledGroup;
}

bool SkipLocalPredictorServiceWhitelist() {
  return GetLocalPredictorSpecValue(kSkipServiceWhitelist) == kEnabledGroup;
}

bool SkipLocalPredictorLoggedIn() {
  return GetLocalPredictorSpecValue(kSkipLoggedIn) == kEnabledGroup;
}

bool SkipLocalPredictorDefaultNoPrerender() {
  return GetLocalPredictorSpecValue(kSkipDefaultNoPrerender) == kEnabledGroup;
}

bool SkipLocalPredictorLocalCandidates() {
  return GetLocalPredictorSpecValue(kSkipPrerenderLocalCanadidates) ==
      kEnabledGroup;
}

bool SkipLocalPredictorServiceCandidates() {
  return GetLocalPredictorSpecValue(kSkipPrerenderServiceCanadidates) ==
      kEnabledGroup;
}

bool ShouldMergeSessionStorageNamespaces() {
  return GetLocalPredictorSpecValue(kDisableSessionStorageNamespaceMerging) !=
      kDisabledGroup;
}

bool IsPrerenderCookieStoreEnabled() {
  return GetLocalPredictorSpecValue(kPrerenderCookieStore) != kDisabledGroup &&
      FieldTrialList::FindFullName(kPrerenderCookieStore) != kDisabledGroup;
}

}  // namespace prerender
