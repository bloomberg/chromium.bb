// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_bubble_experiment.h"

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/variations/variations_associated_data.h"

namespace password_bubble_experiment {
namespace {

bool IsNegativeEvent(password_manager::metrics_util::UIDismissalReason reason) {
  return (reason == password_manager::metrics_util::NO_DIRECT_INTERACTION ||
          reason == password_manager::metrics_util::CLICKED_NOPE ||
          reason == password_manager::metrics_util::CLICKED_NEVER);
}

// "TimeSpan" experiment -----------------------------------------------------

bool ExtractTimeSpanParams(base::TimeDelta* time_delta, int* nopes_limit) {
  std::map<std::string, std::string> params;
  bool retrieved = variations::GetVariationParams(kExperimentName, &params);
  if (!retrieved)
    return false;
  int days = 0;
  if (!base::StringToInt(params[kParamTimeSpan], &days) ||
      !base::StringToInt(params[kParamTimeSpanNopeThreshold], nopes_limit))
    return false;
  *time_delta = base::TimeDelta::FromDays(days);
  return true;
}

bool OverwriteTimeSpanPrefsIfNeeded(PrefService* prefs,
                                    base::TimeDelta time_span) {
  base::Time beginning = base::Time::FromInternalValue(
      prefs->GetInt64(prefs::kPasswordBubbleTimeStamp));
  base::Time now = base::Time::Now();
  if (beginning + time_span < now) {
    prefs->SetInt64(prefs::kPasswordBubbleTimeStamp, now.ToInternalValue());
    prefs->SetInteger(prefs::kPasswordBubbleNopesCount, 0);
    return true;
  }
  return false;
}

// If user dismisses the bubble >= kParamTimeSpanNopeThreshold times during
// kParamTimeSpan days then the bubble isn't shown until the end of this time
// span.
bool ShouldShowBubbleTimeSpanExperiment(PrefService* prefs) {
  base::TimeDelta time_span;
  int nopes_limit = 0;
  if (!ExtractTimeSpanParams(&time_span, &nopes_limit)) {
    VLOG(2) << "Can't read parameters for "
        << kExperimentName  << " experiment";
    return true;
  }
  // Check if the new time span has started.
  if (OverwriteTimeSpanPrefsIfNeeded(prefs, time_span))
    return true;
  int current_nopes = prefs->GetInteger(prefs::kPasswordBubbleNopesCount);
  return current_nopes < nopes_limit;
}

// Increase the "Nope" counter in prefs and start a new time span if needed.
void UpdateTimeSpanPrefs(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  if (!IsNegativeEvent(reason))
    return;
  base::TimeDelta time_span;
  int nopes_limit = 0;
  if (!ExtractTimeSpanParams(&time_span, &nopes_limit)) {
    VLOG(2) << "Can't read parameters for "
        << kExperimentName << " experiment";
    return;
  }
  OverwriteTimeSpanPrefsIfNeeded(prefs, time_span);
  int current_nopes = prefs->GetInteger(prefs::kPasswordBubbleNopesCount);
  prefs->SetInteger(prefs::kPasswordBubbleNopesCount, current_nopes + 1);
}

// "Probability" experiment --------------------------------------------------

bool ExtractProbabilityParams(unsigned* history_length, unsigned* saves) {
  std::map<std::string, std::string> params;
  bool retrieved = variations::GetVariationParams(kExperimentName, &params);
  if (!retrieved)
    return false;
  return base::StringToUint(params[kParamProbabilityInteractionsCount],
                            history_length) &&
      base::StringToUint(params[kParamProbabilityFakeSaves], saves);
}

std::vector<int> ReadInteractionHistory(PrefService* prefs) {
  std::vector<int> interactions;
  const base::ListValue* list =
      prefs->GetList(prefs::kPasswordBubbleLastInteractions);
  if (!list)
    return interactions;
  for (const base::Value* value : *list) {
    int out_value;
    if (value->GetAsInteger(&out_value))
      interactions.push_back(out_value);
  }
  return interactions;
}

// We keep the history of last kParamProbabilityInteractionsCount interactions
// with the bubble. We implicitly add kParamProbabilityFakeSaves "Save" clicks.
// If there are x "Save" clicks among those kParamProbabilityInteractionsCount
// then the bubble is shown with probability (x + kParamProbabilityFakeSaves)/
// (kParamProbabilityInteractionsCount + kParamProbabilityFakeSaves).
bool ShouldShowBubbleProbabilityExperiment(PrefService* prefs) {
  unsigned history_length = 0, fake_saves = 0;
  if (!ExtractProbabilityParams(&history_length, &fake_saves)) {
    VLOG(2) << "Can't read parameters for "
        << kExperimentName << " experiment";
    return true;
  }
  std::vector<int> interactions = ReadInteractionHistory(prefs);
  unsigned real_saves =
      std::count(interactions.begin(), interactions.end(),
                 password_manager::metrics_util::CLICKED_SAVE);
  return (interactions.size() + fake_saves) * base::RandDouble() <=
      real_saves + fake_saves;
}

void UpdateProbabilityPrefs(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  if (!IsNegativeEvent(reason) &&
      reason != password_manager::metrics_util::CLICKED_SAVE)
    return;
  unsigned history_length = 0, fake_saves = 0;
  if (!ExtractProbabilityParams(&history_length, &fake_saves)) {
    VLOG(2) << "Can't read parameters for "
        << kExperimentName << " experiment";
    return;
  }
  std::vector<int> interactions = ReadInteractionHistory(prefs);
  interactions.push_back(reason);
  size_t history_beginning = interactions.size() > history_length ?
      interactions.size() - history_length : 0;
  base::ListValue value;
  for (size_t i = history_beginning; i < interactions.size(); ++i)
    value.AppendInteger(interactions[i]);
  prefs->Set(prefs::kPasswordBubbleLastInteractions, value);
}

} // namespace

const char kExperimentName[] = "PasswordBubbleAlgorithm";
const char kGroupTimeSpanBased[] = "TimeSpan";
const char kGroupProbabilityBased[] = "Probability";
const char kParamProbabilityFakeSaves[] = "saves_count";
const char kParamProbabilityInteractionsCount[] = "last_interactions_count";
const char kParamTimeSpan[] = "time_span";
const char kParamTimeSpanNopeThreshold[] = "nope_threshold";

void RegisterPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterInt64Pref(
      prefs::kPasswordBubbleTimeStamp,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kPasswordBubbleNopesCount,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kPasswordBubbleLastInteractions,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool ShouldShowBubble(PrefService* prefs) {
  if (!base::FieldTrialList::TrialExists(kExperimentName))
    return true;
  std::string group_name =
      base::FieldTrialList::FindFullName(kExperimentName);

  if (group_name == kGroupTimeSpanBased) {
    return ShouldShowBubbleTimeSpanExperiment(prefs);
  }
  if (group_name == kGroupProbabilityBased) {
    return ShouldShowBubbleProbabilityExperiment(prefs);
  }

  // The "Show Always" should be the default case.
  return true;
}

void RecordBubbleClosed(
    PrefService* prefs,
    password_manager::metrics_util::UIDismissalReason reason) {
  UpdateTimeSpanPrefs(prefs, reason);
  UpdateProbabilityPrefs(prefs, reason);
}

}  // namespace password_bubble_experiment
