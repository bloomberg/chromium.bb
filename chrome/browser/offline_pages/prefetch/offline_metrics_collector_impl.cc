// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/offline_metrics_collector_impl.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/common/pref_names.h"

namespace offline_pages {

// static
void OfflineMetricsCollectorImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOfflineUsageStartObserved, false);
  registry->RegisterBooleanPref(prefs::kOfflineUsageOnlineObserved, false);
  registry->RegisterBooleanPref(prefs::kOfflineUsageOfflineObserved, false);
  registry->RegisterInt64Pref(prefs::kOfflineUsageTrackingDay, 0L);
  registry->RegisterIntegerPref(prefs::kOfflineUsageUnusedCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageStartedCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageOfflineCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageOnlineCount, 0);
  registry->RegisterIntegerPref(prefs::kOfflineUsageMixedCount, 0);
}

OfflineMetricsCollectorImpl::OfflineMetricsCollectorImpl(PrefService* prefs)
    : prefs_(prefs) {}

OfflineMetricsCollectorImpl::~OfflineMetricsCollectorImpl() {}

void OfflineMetricsCollectorImpl::OnAppStartupOrResume() {
  SetTrackingFlag(&chrome_start_observed_);
}

void OfflineMetricsCollectorImpl::OnSuccessfulNavigationOffline() {
  SetTrackingFlag(&offline_navigation_observed_);
}

void OfflineMetricsCollectorImpl::OnSuccessfulNavigationOnline() {
  SetTrackingFlag(&online_navigation_observed_);
}

void OfflineMetricsCollectorImpl::ReportAccumulatedStats() {
  int total_day_count = unused_days_count_ + started_days_count_ +
                        offline_days_count_ + online_days_count_ +
                        mixed_days_count_;
  // No accumulated daily usage, nothing to report.
  if (total_day_count == 0)
    return;

  for (int i = 0; i < unused_days_count_; ++i)
    ReportUsageForOneDayToUma(DailyUsageType::UNUSED);
  for (int i = 0; i < started_days_count_; ++i)
    ReportUsageForOneDayToUma(DailyUsageType::STARTED);
  for (int i = 0; i < offline_days_count_; ++i)
    ReportUsageForOneDayToUma(DailyUsageType::OFFLINE);
  for (int i = 0; i < online_days_count_; ++i)
    ReportUsageForOneDayToUma(DailyUsageType::ONLINE);
  for (int i = 0; i < mixed_days_count_; ++i)
    ReportUsageForOneDayToUma(DailyUsageType::MIXED);

  unused_days_count_ = started_days_count_ = offline_days_count_ =
      online_days_count_ = mixed_days_count_ = 0;
  SaveToPrefs();
}

void OfflineMetricsCollectorImpl::EnsureLoaded() {
  if (!tracking_day_midnight_.is_null())
    return;

  DCHECK(prefs_);
  chrome_start_observed_ =
      prefs_->GetBoolean(prefs::kOfflineUsageStartObserved);
  offline_navigation_observed_ =
      prefs_->GetBoolean(prefs::kOfflineUsageOfflineObserved);
  online_navigation_observed_ =
      prefs_->GetBoolean(prefs::kOfflineUsageOnlineObserved);
  int64_t time_value = prefs_->GetInt64(prefs::kOfflineUsageTrackingDay);
  // For the very first run, initialize to current time.
  tracking_day_midnight_ = time_value == 0L
                               ? Now().LocalMidnight()
                               : base::Time::FromInternalValue(time_value);
  unused_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageUnusedCount);
  started_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageStartedCount);
  offline_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageOfflineCount);
  online_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageOnlineCount);
  mixed_days_count_ = prefs_->GetInteger(prefs::kOfflineUsageMixedCount);
}

void OfflineMetricsCollectorImpl::SaveToPrefs() {
  prefs_->SetBoolean(prefs::kOfflineUsageStartObserved, chrome_start_observed_);
  prefs_->SetBoolean(prefs::kOfflineUsageOfflineObserved,
                     offline_navigation_observed_);
  prefs_->SetBoolean(prefs::kOfflineUsageOnlineObserved,
                     online_navigation_observed_);
  prefs_->SetInt64(prefs::kOfflineUsageTrackingDay,
                   tracking_day_midnight_.ToInternalValue());
  prefs_->SetInteger(prefs::kOfflineUsageUnusedCount, unused_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageStartedCount, started_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageOfflineCount, offline_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageOnlineCount, online_days_count_);
  prefs_->SetInteger(prefs::kOfflineUsageMixedCount, mixed_days_count_);
  prefs_->CommitPendingWrite();
}

void OfflineMetricsCollectorImpl::SetTrackingFlag(bool* flag) {
  EnsureLoaded();

  bool changed = UpdatePastDaysIfNeeded() || !(*flag);
  *flag = true;

  if (changed)
    SaveToPrefs();
}

bool OfflineMetricsCollectorImpl::UpdatePastDaysIfNeeded() {
  base::Time current_midnight = Now().LocalMidnight();
  // It is still the same day, or a day from the future (rarely may happen when
  // clock is reset), skip updating past days counters.
  if (tracking_day_midnight_ >= current_midnight)
    return false;

  // Increment the counter that corresponds to tracked usage.
  if (online_navigation_observed_ && offline_navigation_observed_)
    mixed_days_count_++;
  else if (online_navigation_observed_)
    online_days_count_++;
  else if (offline_navigation_observed_)
    offline_days_count_++;
  else if (chrome_start_observed_)
    started_days_count_++;
  else
    unused_days_count_++;

  // The days between the day when tracking was done and the current one are
  // 'unused'.
  // Calculation of the 'days in between' is as following:
  // 1. If current_midnight == tracking_day_midnight_, we returned earlier.
  // 2. If current_midnight is for the next day, days_in_between is 0,
  //    tracking is reset to start for the current day.
  // 3. If current_midnight is > 48hrs later, the days_in_between are added,
  //    tracking is reset to start for the current day.
  int days_in_between =
      (current_midnight - tracking_day_midnight_).InDays() - 1;
  DCHECK(days_in_between >= 0);
  unused_days_count_ += days_in_between;

  // Reset tracking
  chrome_start_observed_ = offline_navigation_observed_ =
      online_navigation_observed_ = false;
  tracking_day_midnight_ = current_midnight;

  return true;
}

void OfflineMetricsCollectorImpl::ReportUsageForOneDayToUma(
    DailyUsageType usage_type) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.OfflineUsage", usage_type,
                            DailyUsageType::MAX_USAGE);
}

base::Time OfflineMetricsCollectorImpl::Now() const {
  if (testing_clock_)
    return testing_clock_->Now();
  return base::Time::Now();
}

void OfflineMetricsCollectorImpl::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

}  // namespace offline_pages
