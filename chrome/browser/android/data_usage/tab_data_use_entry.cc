// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/tab_data_use_entry.h"

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Indicates the default maximum number of tracking session history to maintain
// per tab. May be overridden by the field trial.
const size_t kDefaultMaxSessionsPerTab = 5;

// Indicates the default expiration duration in seconds, after which it can be
// sweeped from history for a closed tab entry and an open tab entry
// respectively. May be overridden by the field trial.
const unsigned int kDefaultClosedTabExpirationDurationSeconds =
    30;  // 30 seconds.
const unsigned int kDefaultOpenTabExpirationDurationSeconds =
    60 * 60 * 24 * 5;  // 5 days.

const char kUMATrackingSessionLifetimeSecondsHistogram[] =
    "DataUse.TabModel.TrackingSessionLifetime";
const char kUMAOldInactiveSessionRemovalDurationSecondsHistogram[] =
    "DataUse.TabModel.OldInactiveSessionRemovalDuration";

// Returns various parameters from the values specified in the field trial.
size_t GetMaxSessionsPerTab() {
  size_t max_sessions_per_tab = -1;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "max_sessions_per_tab");
  if (!variation_value.empty() &&
      base::StringToSizeT(variation_value, &max_sessions_per_tab)) {
    return max_sessions_per_tab;
  }
  return kDefaultMaxSessionsPerTab;
}

base::TimeDelta GetClosedTabExpirationDuration() {
  uint32_t duration_seconds = -1;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "closed_tab_expiration_duration_seconds");
  if (!variation_value.empty() &&
      base::StringToUint(variation_value, &duration_seconds)) {
    return base::TimeDelta::FromSeconds(duration_seconds);
  }
  return base::TimeDelta::FromSeconds(
      kDefaultClosedTabExpirationDurationSeconds);
}

base::TimeDelta GetOpenTabExpirationDuration() {
  uint32_t duration_seconds = -1;
  std::string variation_value = variations::GetVariationParamValue(
      chrome::android::ExternalDataUseObserver::
          kExternalDataUseObserverFieldTrial,
      "open_tab_expiration_duration_seconds");
  if (!variation_value.empty() &&
      base::StringToUint(variation_value, &duration_seconds)) {
    return base::TimeDelta::FromSeconds(duration_seconds);
  }
  return base::TimeDelta::FromSeconds(kDefaultOpenTabExpirationDurationSeconds);
}

}  // namespace

namespace chrome {

namespace android {

TabDataUseEntry::TabDataUseEntry()
    : max_sessions_per_tab_(GetMaxSessionsPerTab()),
      closed_tab_expiration_duration_(GetClosedTabExpirationDuration()),
      open_tab_expiration_duration_(GetOpenTabExpirationDuration()) {}

TabDataUseEntry::TabDataUseEntry(const TabDataUseEntry& other) = default;

TabDataUseEntry::~TabDataUseEntry() {}

base::TimeTicks TabDataUseEntry::Now() const {
  return base::TimeTicks::Now();
}

bool TabDataUseEntry::StartTracking(const std::string& label) {
  DCHECK(!label.empty());
  DCHECK(tab_close_time_.is_null());

  if (IsTrackingDataUse())
    return false;  // Duplicate start events.

  // TODO(rajendrant): Explore ability to handle changes in label for current
  // session.

  sessions_.push_back(TabDataUseTrackingSession(label, Now()));

  CompactSessionHistory();
  return true;
}

bool TabDataUseEntry::EndTracking() {
  DCHECK(tab_close_time_.is_null());
  if (!IsTrackingDataUse())
    return false;  // Duplicate end events.

  TabSessions::reverse_iterator back_iterator = sessions_.rbegin();
  if (back_iterator == sessions_.rend())
    return false;

  back_iterator->end_time = Now();

  UMA_HISTOGRAM_CUSTOM_TIMES(
      kUMATrackingSessionLifetimeSecondsHistogram,
      back_iterator->end_time - back_iterator->start_time,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromHours(1), 50);

  return true;
}

bool TabDataUseEntry::EndTrackingWithLabel(const std::string& label) {
  if (!sessions_.empty() && sessions_.back().label == label)
    return EndTracking();
  return false;
}

void TabDataUseEntry::OnTabCloseEvent() {
  DCHECK(!IsTrackingDataUse());
  tab_close_time_ = Now();
}

bool TabDataUseEntry::IsExpired() const {
  const base::TimeTicks now = Now();

  if (!tab_close_time_.is_null()) {
    // Closed tab entry.
    return ((now - tab_close_time_) > closed_tab_expiration_duration_);
  }

  const base::TimeTicks latest_session_time = GetLatestStartOrEndTime();
  if (latest_session_time.is_null() ||
      ((now - latest_session_time) > open_tab_expiration_duration_)) {
    return true;
  }
  return false;
}

bool TabDataUseEntry::GetLabel(const base::TimeTicks& data_use_time,
                               std::string* output_label) const {
  *output_label = "";

  // Find a tracking session in history that was active at |data_use_time|.
  for (TabSessions::const_reverse_iterator session_iterator =
           sessions_.rbegin();
       session_iterator != sessions_.rend(); ++session_iterator) {
    if (session_iterator->start_time <= data_use_time &&
        (session_iterator->end_time.is_null() ||
         session_iterator->end_time >= data_use_time)) {
      *output_label = session_iterator->label;
      return true;
    }
    if (!session_iterator->end_time.is_null() &&
        session_iterator->end_time < data_use_time) {
      // Older sessions in history will end before |data_use_time| and will not
      // match.
      break;
    }
  }
  return false;
}

bool TabDataUseEntry::IsTrackingDataUse() const {
  TabSessions::const_reverse_iterator back_iterator = sessions_.rbegin();
  if (back_iterator == sessions_.rend())
    return false;
  return back_iterator->end_time.is_null();
}

const base::TimeTicks TabDataUseEntry::GetLatestStartOrEndTime() const {
  TabSessions::const_reverse_iterator back_iterator = sessions_.rbegin();
  if (back_iterator == sessions_.rend())
    return base::TimeTicks();  // No tab session found.
  if (!back_iterator->end_time.is_null())
    return back_iterator->end_time;

  DCHECK(!back_iterator->start_time.is_null());
  return back_iterator->start_time;
}

void TabDataUseEntry::CompactSessionHistory() {
  while (sessions_.size() > max_sessions_per_tab_) {
    const auto& front = sessions_.front();
    DCHECK(!front.end_time.is_null());
    // Track how often old sessions are lost.
    UMA_HISTOGRAM_CUSTOM_TIMES(
        kUMAOldInactiveSessionRemovalDurationSecondsHistogram,
        Now() - front.end_time, base::TimeDelta::FromSeconds(1),
        base::TimeDelta::FromHours(1), 50);
    sessions_.pop_front();
  }
}

}  // namespace android

}  // namespace chrome
