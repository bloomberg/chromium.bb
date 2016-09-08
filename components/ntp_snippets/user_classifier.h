// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_USER_CLASSIFIER_H_
#define COMPONENTS_NTP_SNIPPETS_USER_CLASSIFIER_H_

#include "base/macros.h"

class PrefRegistrySimple;
class PrefService;

namespace ntp_snippets {

// Collects data about user usage patterns of content suggestions, computes
// long-term user metrics locally using pref, and reports the metrics to UMA.
// TODO(jkrcal): Add classification of users based on the metrics and getters
// for the classification as well as for the metrics.
class UserClassifier {
 public:
  // The provided |pref_service| may be nullptr in unit-tests.
  explicit UserClassifier(PrefService* pref_service);
  ~UserClassifier();

  // Registers profile prefs for all metrics. Called from browser_prefs.cc.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // When the user opens a new NTP - this indicates potential use of content
  // suggestions.
  void OnNTPOpened();

  // When the content suggestions are shown to the user - in the current
  // implementation when the user scrolls below the fold.
  void OnSuggestionsShown();

  // When the user clicks on some suggestions or on some "More" button.
  void OnSuggestionsUsed();

 private:
  // The event has happened, recompute and store the metric accordingly.
  void UpdateMetricOnEvent(const char* metric_pref_name,
                           const char* last_time_pref_name);

  // Compute the number of hours between two events for the given metric value
  // assuming the events were equally distributed.
  double GetEstimateHoursBetweenEvents(const char* metric_pref_name);

  // Returns the number of hours since the last event of the same type or
  // DBL_MAX if there is no last event of that type.
  double GetHoursSinceLastTime(const char* last_time_pref_name);
  void SetLastTimeToNow(const char* last_time_pref_name);

  PrefService* pref_service_;
  double const discount_rate_per_hour_;

  DISALLOW_COPY_AND_ASSIGN(UserClassifier);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_USER_CLASSIFIER_H_
