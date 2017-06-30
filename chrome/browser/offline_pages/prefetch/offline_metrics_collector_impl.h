// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_
#define CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/time/clock.h"
#include "components/offline_pages/core/prefetch/offline_metrics_collector.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace offline_pages {

// Tracks the types of usage (started/offline/online etc.) of Chrome observed
// throughout a day. The objective of this tracking is to count and report:
// - Days on which the browser was not started.
// - Days on which the browser was started but no navigations happened.
// - Days on which the browser observed only successful online navigations.
// - Days on which the browser observed only successful offline navigations.
// - Days on which the browser observed both successful online and offline
//   navigations.
//
// For each 'type' of the day there is a separate counter which accumulates the
// number of those days over time and the counters are reported via UMA when
// network connectivity is likely given.
//
// When Chrome is being used (navigating to URLs etc), one of the
// OfflineMetricsCollector interface methods is called (normally from a tab
// helper or some other observer of relevant activity) and the 'current' usage
// is updated.
//
// When transition from one day to another is detected (by
// comparing the midnight timestamp of when tracking was initialized and current
// time when usage sample arrives), the counters that correspond to the
// end-of-day state are updated. If more then one day passes between samples,
// that means Chrome was not used in the days in between and they have to be
// counted as 'Chrome was not used' days.
//
// To persist tracking data and counters across Chrome restarts, they are backed
// up in Prefs.
class OfflineMetricsCollectorImpl : public OfflineMetricsCollector {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // The kind of Chrome usage during a day.
  // This class is used in UMA reporting. Keep it in sync with
  // OfflinePagesOfflineUsage enum in histograms. Public for testing.
  enum class DailyUsageType {
    UNUSED = 0,   // Chrome was not used the whole day.
    STARTED = 1,  // Started, but no successful navigations happened during the
                  // day (Error pages, Dine etc)
    OFFLINE = 2,  // Successfully navigated to at least one offline page, no
                  // online navigations(however device may be connected).
    ONLINE = 3,   // Only online navigations happened during the day.
    MIXED = 4,    // Both offline and online navigations happened during the
                  // day.
    MAX_USAGE = 5,
  };

  explicit OfflineMetricsCollectorImpl(PrefService* prefs);
  ~OfflineMetricsCollectorImpl() override;

  // OfflineMetricsCollector implementation.
  void OnAppStartupOrResume() override;
  void OnSuccessfulNavigationOnline() override;
  void OnSuccessfulNavigationOffline() override;
  void ReportAccumulatedStats() override;

  void SetClockForTesting(base::Clock* clock);

 private:
  void EnsureLoaded();
  void SaveToPrefs();

  // Sets the specified flag to 'true', saves new tracking state to Prefs if
  // the there was a change.
  void SetTrackingFlag(bool* flag);

  // Moves tracking_day_midnight_ to the midnight that starts the current day if
  // necessary and updates the usage counters accordingly. Returns 'true' if the
  // past days counters changed and need to be updated on disk.
  bool UpdatePastDaysIfNeeded();

  // Reports to UMA 1 day of specified usage.
  void ReportUsageForOneDayToUma(DailyUsageType usage_type);

  // Used to retrieve current time, overrideable in tests.
  base::Time Now() const;

  // Tracking flags. They reflect the current usage of Chrome through the day,
  // starting from 'false' at start of the day and then eventually set to 'true'
  // when corresponding usage is observed.
  bool chrome_start_observed_ = false;
  bool offline_navigation_observed_ = false;
  bool online_navigation_observed_ = false;

  // The midnight that starts the day for which current tracking is happening.
  // It is used to determine if the time of the observable usage is still during
  // the same local day (between two consecutive midnights).
  // If a usage is observed outside of that range, the accumulated counters
  // should be incremented and tracking state initialized for the current day.
  base::Time tracking_day_midnight_;

  // Accumulated usage counters. There is a counter for each DailyUsageType
  // enum value. Count the number of days with specific Chrome usage.
  int unused_days_count_ = 0;
  int started_days_count_ = 0;
  int offline_days_count_ = 0;
  int online_days_count_ = 0;
  int mixed_days_count_ = 0;

  // Has the same lifetime as profile, so should outlive this subcomponent
  // of profile's PrefetchService.
  PrefService* prefs_ = nullptr;

  // Used in tests, managed by the test, outlives this object.
  base::Clock* testing_clock_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(OfflineMetricsCollectorImpl);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_PREFETCH_OFFLINE_METRICS_COLLECTOR_IMPL_H_
