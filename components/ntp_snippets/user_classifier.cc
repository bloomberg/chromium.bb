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

namespace ntp_snippets {

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

// Classification constants.
const double kFrequentUserScrollsAtLeastOncePerHours = 24;
const double kOccasionalUserOpensNTPAtMostOncePerHours = 72;

const char kHistogramAverageHoursToOpenNTP[] =
    "NewTabPage.UserClassifier.AverageHoursToOpenNTP";
const char kHistogramAverageHoursToShowSuggestions[] =
    "NewTabPage.UserClassifier.AverageHoursToShowSuggestions";
const char kHistogramAverageHoursToUseSuggestions[] =
    "NewTabPage.UserClassifier.AverageHoursToUseSuggestions";

// The enum used for iteration.
const UserClassifier::Metric kMetrics[] = {
    UserClassifier::Metric::NTP_OPENED,
    UserClassifier::Metric::SUGGESTIONS_SHOWN,
    UserClassifier::Metric::SUGGESTIONS_USED};

// The summary of the prefs.
const char* kMetricKeys[] = {
    prefs::kUserClassifierAverageNTPOpenedPerHour,
    prefs::kUserClassifierAverageSuggestionsShownPerHour,
    prefs::kUserClassifierAverageSuggestionsUsedPerHour};
const char* kLastTimeKeys[] = {prefs::kUserClassifierLastTimeToOpenNTP,
                               prefs::kUserClassifierLastTimeToShowSuggestions,
                               prefs::kUserClassifierLastTimeToUseSuggestions};

// Default lengths of the intervals for new users for the metrics.
const double kDefaults[] = {24, 36, 48};

static_assert(arraysize(kMetrics) ==
                      static_cast<int>(UserClassifier::Metric::COUNT) &&
                  arraysize(kMetricKeys) ==
                      static_cast<int>(UserClassifier::Metric::COUNT) &&
                  arraysize(kLastTimeKeys) ==
                      static_cast<int>(UserClassifier::Metric::COUNT) &&
                  arraysize(kDefaults) ==
                      static_cast<int>(UserClassifier::Metric::COUNT),
              "Fill in info for all metrics.");

// Computes the discount rate.
double GetDiscountRatePerHour() {
  // Compute discount_rate_per_hour such that
  //   kDiscountFactorPerDay = 1 - e^{-discount_rate_per_hour * 24}.
  return std::log(1.0 / (1.0 - kDiscountFactorPerDay)) / 24.0;
}

// Returns the new value of the metric using its |old_value|, assuming
// |hours_since_last_time| hours have passed since it was last discounted.
double DiscountMetric(double old_value,
                      double hours_since_last_time,
                      double discount_rate_per_hour) {
  // Compute the new discounted average according to the formula
  //   avg_events := e^{-discount_rate_per_hour * hours_since} * avg_events
  return std::exp(-discount_rate_per_hour * hours_since_last_time) * old_value;
}

// Compute the number of hours between two events for the given metric value
// assuming the events were equally distributed.
double GetEstimateHoursBetweenEvents(double metric_value,
                                     double discount_rate_per_hour) {
  // The computation below is well-defined only for |metric_value| > 1 (log of
  // negative value or division by zero). When |metric_value| -> 1, the estimate
  // below -> infinity, so kMaxHours is a natural result, here.
  if (metric_value <= 1)
    return kMaxHours;

  // This is the estimate with the assumption that last event happened right
  // now and the system is in the steady-state. Solve estimate_hours in the
  // steady-state equation:
  //   metric_value = 1 + e^{-discount_rate * estimate_hours} * metric_value,
  // i.e.
  //   -discount_rate * estimate_hours = log((metric_value - 1) / metric_value),
  //   discount_rate * estimate_hours = log(metric_value / (metric_value - 1)),
  //   estimate_hours = log(metric_value / (metric_value - 1)) / discount_rate.
  double estimate_hours =
      std::log(metric_value / (metric_value - 1)) / discount_rate_per_hour;
  return std::max(kMinHours, std::min(kMaxHours, estimate_hours));
}

// The inverse of GetEstimateHoursBetweenEvents().
double GetMetricValueForEstimateHoursBetweenEvents(
    double estimate_hours,
    double discount_rate_per_hour) {
  // Keep the input value within [kMinHours, kMaxHours].
  estimate_hours = std::max(kMinHours, std::min(kMaxHours, estimate_hours));

  // Return |metric_value| such that GetEstimateHoursBetweenEvents for
  // |metric_value| returns |estimate_hours|. Thus, solve |metric_value| in
  //   metric_value = 1 + e^{-discount_rate * estimate_hours} * metric_value,
  // i.e.
  //   metric_value * (1 - e^{-discount_rate * estimate_hours}) = 1,
  //   metric_value = 1 / (1 - e^{-discount_rate * estimate_hours}).
  return 1.0 / (1.0 - std::exp(-discount_rate_per_hour * estimate_hours));
}

}  // namespace

UserClassifier::UserClassifier(PrefService* pref_service)
    : pref_service_(pref_service),
      discount_rate_per_hour_(GetDiscountRatePerHour()) {
  // The pref_service_ can be null in tests.
  if (!pref_service_)
    return;

  // Initialize the prefs storing the last time: the counter has just started!
  for (const Metric metric : kMetrics) {
    if (!HasLastTime(metric))
      SetLastTimeToNow(metric);
  }
}

UserClassifier::~UserClassifier() {}

// static
void UserClassifier::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  for (Metric metric : kMetrics) {
    double default_metric_value = GetMetricValueForEstimateHoursBetweenEvents(
        kDefaults[static_cast<int>(metric)], GetDiscountRatePerHour());
    registry->RegisterDoublePref(kMetricKeys[static_cast<int>(metric)],
                                 default_metric_value);
    registry->RegisterInt64Pref(kLastTimeKeys[static_cast<int>(metric)], 0);
  }
}

void UserClassifier::OnEvent(Metric metric) {
  DCHECK_NE(metric, Metric::COUNT);
  double metric_value = UpdateMetricOnEvent(metric);

  double avg =
      GetEstimateHoursBetweenEvents(metric_value, discount_rate_per_hour_);
  switch (metric) {
    case Metric::NTP_OPENED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToOpenNTP, avg, 1,
                                  kMaxHours, 50);
      break;
    case Metric::SUGGESTIONS_SHOWN:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToShowSuggestions, avg,
                                  1, kMaxHours, 50);
      break;
    case Metric::SUGGESTIONS_USED:
      UMA_HISTOGRAM_CUSTOM_COUNTS(kHistogramAverageHoursToUseSuggestions, avg,
                                  1, kMaxHours, 50);
      break;
    case Metric::COUNT:
      NOTREACHED();
      break;
  }
}

double UserClassifier::GetEstimatedAvgTime(Metric metric) const {
  DCHECK_NE(metric, Metric::COUNT);
  double metric_value = GetUpToDateMetricValue(metric);
  return GetEstimateHoursBetweenEvents(metric_value, discount_rate_per_hour_);
}

UserClassifier::UserClass UserClassifier::GetUserClass() const {
  if (GetEstimatedAvgTime(Metric::NTP_OPENED) >=
      kOccasionalUserOpensNTPAtMostOncePerHours) {
    return UserClass::RARE_NTP_USER;
  }

  if (GetEstimatedAvgTime(Metric::SUGGESTIONS_SHOWN) <=
      kFrequentUserScrollsAtLeastOncePerHours) {
    return UserClass::ACTIVE_SUGGESTIONS_CONSUMER;
  }

  return UserClass::ACTIVE_NTP_USER;
}

std::string UserClassifier::GetUserClassDescriptionForDebugging() const {
  switch (GetUserClass()) {
    case UserClass::RARE_NTP_USER:
      return "Rare user of the NTP";
    case UserClass::ACTIVE_NTP_USER:
      return "Active user of the NTP";
    case UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      return "Active consumer of NTP suggestions";
  }
  NOTREACHED();
  return std::string();
}

void UserClassifier::ClearClassificationForDebugging() {
  // The pref_service_ can be null in tests.
  if (!pref_service_)
    return;

  for (const Metric& metric : kMetrics) {
    ClearMetricValue(metric);
    SetLastTimeToNow(metric);
  }
}

double UserClassifier::UpdateMetricOnEvent(Metric metric) {
  // The pref_service_ can be null in tests.
  if (!pref_service_)
    return 0;

  double hours_since_last_time =
      std::min(kMaxHours, GetHoursSinceLastTime(metric));
  // Ignore events within the same "browsing session".
  if (hours_since_last_time < kMinHours)
    return GetUpToDateMetricValue(metric);

  SetLastTimeToNow(metric);

  double metric_value = GetMetricValue(metric);
  // Add 1 to the discounted metric as the event has happened right now.
  double new_metric_value =
      1 + DiscountMetric(metric_value, hours_since_last_time,
                          discount_rate_per_hour_);
  SetMetricValue(metric, new_metric_value);
  return new_metric_value;
}

double UserClassifier::GetUpToDateMetricValue(Metric metric) const {
  // The pref_service_ can be null in tests.
  if (!pref_service_)
    return 0;

  double hours_since_last_time =
      std::min(kMaxHours, GetHoursSinceLastTime(metric));

  double metric_value = GetMetricValue(metric);
  return DiscountMetric(metric_value, hours_since_last_time,
                        discount_rate_per_hour_);
}

double UserClassifier::GetHoursSinceLastTime(Metric metric) const {
  if (!HasLastTime(metric))
    return 0;

  base::TimeDelta since_last_time =
      base::Time::Now() - base::Time::FromInternalValue(pref_service_->GetInt64(
                              kLastTimeKeys[static_cast<int>(metric)]));
  return since_last_time.InSecondsF() / 3600;
}

bool UserClassifier::HasLastTime(Metric metric) const {
  return pref_service_->HasPrefPath(kLastTimeKeys[static_cast<int>(metric)]);
}

void UserClassifier::SetLastTimeToNow(Metric metric) {
  pref_service_->SetInt64(kLastTimeKeys[static_cast<int>(metric)],
                          base::Time::Now().ToInternalValue());
}

double UserClassifier::GetMetricValue(Metric metric) const {
  return pref_service_->GetDouble(kMetricKeys[static_cast<int>(metric)]);
}

void UserClassifier::SetMetricValue(Metric metric, double metric_value) {
  pref_service_->SetDouble(kMetricKeys[static_cast<int>(metric)], metric_value);
}

void UserClassifier::ClearMetricValue(Metric metric) {
  pref_service_->ClearPref(kMetricKeys[static_cast<int>(metric)]);
}

}  // namespace ntp_snippets
