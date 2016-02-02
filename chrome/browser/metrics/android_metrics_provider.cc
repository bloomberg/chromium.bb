// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/android_metrics_provider.h"

#include "base/metrics/histogram.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/android/feature_utilities.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// Increments a particular entry in the ListValue.
void IncrementListValue(base::ListValue* counts, int index) {
  int current_count = 0;
  counts->GetInteger(index, &current_count);
  counts->Set(index, new base::FundamentalValue(current_count + 1));
}

// Takes an int corresponding to a Type and returns the corresponding flag.
int GetActivityFlag(int type_id) {
  ActivityTypeIds::Type type = ActivityTypeIds::GetActivityType(type_id);
  DCHECK_LT(type, ActivityTypeIds::ACTIVITY_MAX_VALUE);
  return (1 << type);
}

}  // namespace

AndroidMetricsProvider::AndroidMetricsProvider(PrefService* local_state)
    : local_state_(local_state) {
  LogStabilityToPrefs();
}

AndroidMetricsProvider::~AndroidMetricsProvider() {
}

void AndroidMetricsProvider::ProvideStabilityMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  ConvertStabilityPrefsToHistograms();
}

void AndroidMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  UMA_HISTOGRAM_ENUMERATION(
      "DocumentActivity.Enabled",
      chrome::android::GetDocumentModeValue(),
      chrome::android::RUNNING_MODE_MAX);
  UMA_HISTOGRAM_ENUMERATION(
      "CustomTabs.Visible",
      chrome::android::GetCustomTabsVisibleValue(),
      chrome::android::CUSTOM_TABS_VISIBILITY_MAX);
  UMA_HISTOGRAM_BOOLEAN(
      "MemoryAndroid.LowRamDevice",
      base::SysInfo::IsLowEndDevice());
}

void AndroidMetricsProvider::OnForegroundActivityChanged(
    ActivityTypeIds::Type type) {
  DCHECK_LT(type, ActivityTypeIds::ACTIVITY_MAX_VALUE);

  if (type == local_state_->GetInteger(prefs::kStabilityForegroundActivityType))
    return;

  // Record that the Activity is now in the foreground.
  local_state_->SetInteger(prefs::kStabilityForegroundActivityType, type);

  // Record that the Activity was launched this sesaion.
  // The pref stores a set of flags ORed together, where each set flag
  // corresponds to a launched Activity type.
  int launched =
      local_state_->GetInteger(prefs::kStabilityLaunchedActivityFlags);
  if (type != ActivityTypeIds::ACTIVITY_NONE) {
    launched |= GetActivityFlag(type);
    local_state_->SetInteger(prefs::kStabilityLaunchedActivityFlags, launched);
  }

  local_state_->CommitPendingWrite();
}

// static
void AndroidMetricsProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kStabilityForegroundActivityType,
                                ActivityTypeIds::ACTIVITY_NONE);
  registry->RegisterIntegerPref(prefs::kStabilityLaunchedActivityFlags, 0);
  registry->RegisterListPref(prefs::kStabilityLaunchedActivityCounts);
  registry->RegisterListPref(prefs::kStabilityCrashedActivityCounts);
}

void AndroidMetricsProvider::LogStabilityToPrefs() {
  // Track which Activities were launched by the user.
  // A 'launch' is defined as starting the Activity at least once during a
  // UMA session.  Multiple launches are counted only once since it is possible
  // for users to hop between Activities (e.g. entering and leaving Settings).
  const int launched =
      local_state_->GetInteger(prefs::kStabilityLaunchedActivityFlags);
  ListPrefUpdate update_launches(local_state_,
                                 prefs::kStabilityLaunchedActivityCounts);
  base::ListValue* launch_counts = update_launches.Get();
  for (int activity_type = ActivityTypeIds::ACTIVITY_NONE;
       activity_type < ActivityTypeIds::ACTIVITY_MAX_VALUE;
       ++activity_type) {
    if (launched & GetActivityFlag(activity_type))
      IncrementListValue(launch_counts, activity_type);
  }
  local_state_->SetInteger(prefs::kStabilityLaunchedActivityFlags, 0);

  // Track any Activities that were in the foreground when Chrome died.
  // These Activities failed to be recorded as leaving the foreground, so Chrome
  // couldn't have ended the UMA session cleanly.  Record them as crashing.
  const int foreground =
      local_state_->GetInteger(prefs::kStabilityForegroundActivityType);
  if (foreground != ActivityTypeIds::ACTIVITY_NONE) {
    ListPrefUpdate update_crashes(local_state_,
                                  prefs::kStabilityCrashedActivityCounts);
    base::ListValue* crash_counts = update_crashes.Get();
    IncrementListValue(crash_counts, foreground);
    local_state_->SetInteger(prefs::kStabilityForegroundActivityType,
                             ActivityTypeIds::ACTIVITY_NONE);
  }

  local_state_->CommitPendingWrite();
}

void AndroidMetricsProvider::ConvertStabilityPrefsToHistograms() {
  ListPrefUpdate launch_updater(local_state_,
                                prefs::kStabilityLaunchedActivityCounts);
  ListPrefUpdate crash_updater(local_state_,
                               prefs::kStabilityCrashedActivityCounts);

  base::ListValue* launch_counts = launch_updater.Get();
  base::ListValue* crash_counts = crash_updater.Get();

  for (int activity_type = ActivityTypeIds::ACTIVITY_NONE;
       activity_type < ActivityTypeIds::ACTIVITY_MAX_VALUE;
       ++activity_type) {
    int launch_count = 0;
    int crash_count = 0;

    launch_counts->GetInteger(activity_type, &launch_count);
    crash_counts->GetInteger(activity_type, &crash_count);

    for (int count = 0; count < launch_count; ++count) {
      UMA_STABILITY_HISTOGRAM_ENUMERATION(
          "Chrome.Android.Activity.LaunchCounts",
          activity_type,
          ActivityTypeIds::ACTIVITY_MAX_VALUE);
    }

    for (int count = 0; count < crash_count; ++count) {
      UMA_STABILITY_HISTOGRAM_ENUMERATION("Chrome.Android.Activity.CrashCounts",
                                          activity_type,
                                          ActivityTypeIds::ACTIVITY_MAX_VALUE);
    }
  }

  launch_counts->Clear();
  crash_counts->Clear();
}
