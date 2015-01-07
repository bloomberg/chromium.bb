// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_url_collection_experiment.h"

#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/variations/variations_associated_data.h"

namespace password_manager {
namespace urls_collection_experiment {

namespace {

const char kExperimentStartDate[] = "Mon, 12 Jan 00:00:00 2015 GMT";

// A safe default value. Using this will not show the bubble.
const int kDefaultValueForStartingDayFactor = -1;

bool ExtractExperimentParams(
    int* experiment_length_in_days,
    base::TimeDelta* experiment_active_for_user_period) {
  std::map<std::string, std::string> params;
  if (!variations::GetVariationParams(kExperimentName, &params))
    return false;
  int days = 0;
  if (!base::StringToInt(params[kParamExperimentLengthInDays],
                         experiment_length_in_days) ||
      !base::StringToInt(params[kParamActivePeriodInDays], &days)) {
    return false;
  }
  *experiment_active_for_user_period = base::TimeDelta::FromDays(days);
  return true;
}

bool ShouldShowBubbleInternal(PrefService* prefs, base::Clock* clock) {
  if (prefs->GetBoolean(prefs::kAllowToCollectURLBubbleWasShown))
    return false;
  int experiment_length_in_days = 0;
  base::TimeDelta experiment_active_for_user_period;
  if (!ExtractExperimentParams(&experiment_length_in_days,
                               &experiment_active_for_user_period)) {
    return false;
  }
  base::Time now = clock->Now();
  base::Time experiment_active_for_user_start =
      DetermineStartOfActivityPeriod(prefs, experiment_length_in_days);
  return (now >= experiment_active_for_user_start &&
          now < experiment_active_for_user_start +
                    experiment_active_for_user_period);
}

}  // namespace

const char kExperimentName[] = "AskToSubmitURLBubble";
const char kParamExperimentLengthInDays[] = "experiment_length_in_days";
const char kParamActivePeriodInDays[] = "experiment_active_for_user_period";
const char kParamBubbleStatus[] = "show_bubble_status";
const char kParamBubbleStatusValueWhenShouldShow[] = "show";

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      password_manager::prefs::kAllowToCollectURLBubbleWasShown,
      false,  // bubble hasn't been shown yet
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      password_manager::prefs::kAllowToCollectURLBubbleActivePeriodStartFactor,
      kDefaultValueForStartingDayFactor,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

base::Time DetermineStartOfActivityPeriod(PrefService* prefs,
                                          int experiment_length_in_days) {
  double active_period_start_second_factor =
      prefs->GetDouble(prefs::kAllowToCollectURLBubbleActivePeriodStartFactor);
  if (active_period_start_second_factor == kDefaultValueForStartingDayFactor) {
    active_period_start_second_factor = base::RandDouble();
    prefs->SetDouble(prefs::kAllowToCollectURLBubbleActivePeriodStartFactor,
                     active_period_start_second_factor);
  }
  base::Time beginning;
  base::Time::FromString(kExperimentStartDate, &beginning);
  const int kSecondsInDay = 24 * 60 * 60;
  return beginning + base::TimeDelta::FromSeconds(
                         kSecondsInDay * experiment_length_in_days *
                         active_period_start_second_factor);
}

bool ShouldShowBubbleWithClock(PrefService* prefs, base::Clock* clock) {
  std::string show_bubble =
      variations::GetVariationParamValue(kExperimentName, kParamBubbleStatus);
  if (show_bubble == kParamBubbleStatusValueWhenShouldShow)
    return ShouldShowBubbleInternal(prefs, clock);
  // "Do not show" is the default case.
  return false;
}

bool ShouldShowBubble(PrefService* prefs) {
  base::DefaultClock clock;
  return ShouldShowBubbleWithClock(prefs, &clock);
}

void RecordBubbleOpened(PrefService* prefs) {
  prefs->SetBoolean(password_manager::prefs::kAllowToCollectURLBubbleWasShown,
                    true);
}

}  // namespace urls_collection_experiment
}  // namespace password_manager
