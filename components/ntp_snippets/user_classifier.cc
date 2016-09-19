// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/user_classifier.h"

#include <float.h>

#include <algorithm>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {

// TODO(jkrcal): Make all of this configurable via variations_service.

// The discount factor for computing the discounted-average metrics. Must be
// strictly larger than 0 and strictly smaller than 1!
const double kDiscountFactorPerDay = 0.25;

// Never consider any larger interval than this (so that extreme situations such
// as losing your phone or going for a long offline vacation do not skew the
// average too much).
const double kMaxHours = 7 * 24;

// Ignore events within |kMinHours| hours since the last event (|kMinHours| is
// the length of the browsing session where subsequent events of the same type
// do not count again).
const double kMinHours = 0.5;

const char kHistogramAverageHoursToOpenNTP[] =
    "NewTabPage.UserClassifier.AverageHoursToOpenNTP";
const char kHistogramAverageHoursToShowSuggestions[] =
    "NewTabPage.UserClassifier.AverageHoursToShowSuggestions";
const char kHistogramAverageHoursToUseSuggestions[] =
    "NewTabPage.UserClassifier.AverageHoursToUseSuggestions";

}  // namespace

namespace ntp_snippets {

UserClassifier::UserClassifier(PrefService* pref_service)
    : pref_service_(pref_service),
      // Compute discount_rate_per_hour such that
      //   kDiscountFactorPerDay = 1 - e^{-discount_rate_per_hour * 24}.
      discount_rate_per_hour_(std::log(1 / (1 - kDiscountFactorPerDay)) / 24) {}

UserClassifier::~UserClassifier() {}

// static
void UserClassifier::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDoublePref(
      prefs::kUserClassifierAverageNTPOpenedPerHour, 1);
  registry->RegisterDoublePref(
      prefs::kUserClassifierAverageSuggestionsShownPerHour, 1);
  registry->RegisterDoublePref(
      prefs::kUserClassifierAverageSuggestionsUsedPerHour, 1);

  registry->RegisterInt64Pref(prefs::kUserClassifierLastTimeToOpenNTP, 0);
  registry->RegisterInt64Pref(prefs::kUserClassifierLastTimeToShowSuggestions,
                              0);
  registry->RegisterInt64Pref(prefs::kUserClassifierLastTimeToUseSuggestions,
                              0);
}

void UserClassifier::OnNTPOpened() {
  UpdateMetricOnEvent(prefs::kUserClassifierAverageNTPOpenedPerHour,
                      prefs::kUserClassifierLastTimeToOpenNTP);

  double avg = GetEstimateHoursBetweenEvents(
      prefs::kUserClassifierAverageNTPOpenedPerHour);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToOpenNTP, avg, 1,
                              kMaxHours, 50);
}

void UserClassifier::OnSuggestionsShown() {
  UpdateMetricOnEvent(prefs::kUserClassifierAverageSuggestionsShownPerHour,
                      prefs::kUserClassifierLastTimeToShowSuggestions);

  double avg = GetEstimateHoursBetweenEvents(
      prefs::kUserClassifierAverageSuggestionsShownPerHour);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToShowSuggestions, avg, 1,
                              kMaxHours, 50);
}

void UserClassifier::OnSuggestionsUsed() {
  UpdateMetricOnEvent(prefs::kUserClassifierAverageSuggestionsUsedPerHour,
                      prefs::kUserClassifierLastTimeToUseSuggestions);

  double avg = GetEstimateHoursBetweenEvents(
      prefs::kUserClassifierAverageSuggestionsUsedPerHour);
  UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToUseSuggestions, avg, 1,
                              kMaxHours, 50);
}

void UserClassifier::UpdateMetricOnEvent(const char* metric_pref_name,
                                         const char* last_time_pref_name) {
  if (!pref_service_)
    return;

  double hours_since_last_time =
      std::min(kMaxHours, GetHoursSinceLastTime(last_time_pref_name));
  // Ignore events within the same "browsing session".
  if (hours_since_last_time < kMinHours)
    return;
  SetLastTimeToNow(last_time_pref_name);

  double avg_events_per_hour = pref_service_->GetDouble(metric_pref_name);
  // Compute and store the new discounted average according to the formula
  //   avg_events := 1 + e^{-discount_rate_per_hour * hours_since} * avg_events.
  double new_avg_events_per_hour =
      1 +
      std::exp(-discount_rate_per_hour_ * hours_since_last_time) *
          avg_events_per_hour;
  pref_service_->SetDouble(metric_pref_name, new_avg_events_per_hour);
}

double UserClassifier::GetEstimateHoursBetweenEvents(
    const char* metric_pref_name) {
  double avg_events_per_hour = pref_service_->GetDouble(metric_pref_name);

  // Right after the first update, the metric is equal to 1.
  if (avg_events_per_hour <= 1)
    return kMaxHours;

  // This is the estimate with the assumption that last event happened right
  // now and the system is in the steady-state. Solve estimate_hours in the
  // steady-state equation:
  //   avg_events = 1 + e^{-discount_rate * estimate_hours} * avg_events,
  // i.e.
  //   -discount_rate * estimate_hours = log((avg_events - 1) / avg_events),
  //   discount_rate * estimate_hours = log(avg_events / (avg_events - 1)),
  //   estimate_hours = log(avg_events / (avg_events - 1)) / discount_rate.
  return std::min(kMaxHours,
                  std::log(avg_events_per_hour / (avg_events_per_hour - 1)) /
                      discount_rate_per_hour_);
}

double UserClassifier::GetHoursSinceLastTime(
    const char* last_time_pref_name) {
  if (!pref_service_->HasPrefPath(last_time_pref_name))
    return DBL_MAX;

  base::TimeDelta since_last_time =
      base::Time::Now() - base::Time::FromInternalValue(
                              pref_service_->GetInt64(last_time_pref_name));
  return since_last_time.InSecondsF() / 3600;
}

void UserClassifier::SetLastTimeToNow(const char* last_time_pref_name) {
  pref_service_->SetInt64(last_time_pref_name,
                          base::Time::Now().ToInternalValue());
}

}  // namespace ntp_snippets
