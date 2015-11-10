// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/tab_data_use_entry.h"

#include "base/gtest_prod_util.h"
#include "base/macros.h"

namespace {

// Indicates the maximum number of tracking session history to maintain per tab.
const size_t kMaxSessionsPerTab = 5;

// Indicates the expiration duration in seconds for a closed tab entry, after
// which it can be sweeped from history.
const unsigned int kClosedTabExpirationDurationSeconds = 30;  // 30 seconds.

// Indicates the expiration duration in seconds for an open tab entry, after
// which it can be sweeped from history.
const unsigned int kOpenTabExpirationDurationSeconds =
    60 * 60 * 24 * 5;  // 5 days.

}  // namespace

namespace chrome {

namespace android {

size_t TabDataUseEntry::GetMaxSessionsPerTabForTests() {
  return kMaxSessionsPerTab;
}

unsigned int TabDataUseEntry::GetClosedTabExpirationDurationSecondsForTests() {
  return kClosedTabExpirationDurationSeconds;
}

unsigned int TabDataUseEntry::GetOpenTabExpirationDurationSecondsForTests() {
  return kOpenTabExpirationDurationSeconds;
}

TabDataUseEntry::TabDataUseEntry() {}

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
  return true;
}

void TabDataUseEntry::OnTabCloseEvent() {
  DCHECK(!IsTrackingDataUse());
  tab_close_time_ = Now();
}

bool TabDataUseEntry::IsExpired() const {
  const base::TimeTicks now = Now();

  if (!tab_close_time_.is_null()) {
    // Closed tab entry.
    return ((now - tab_close_time_) >
            base::TimeDelta::FromSeconds(kClosedTabExpirationDurationSeconds));
  }

  const base::TimeTicks latest_session_time = GetLatestStartOrEndTime();
  if (latest_session_time.is_null() ||
      ((now - latest_session_time) >
       base::TimeDelta::FromSeconds(kOpenTabExpirationDurationSeconds))) {
    // TODO(rajendrant): Add UMA to track deletion of entries corresponding to
    // existing tabs.
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
  // TODO(rajendrant): Add UMA to track how often old sessions are lost.
  while (sessions_.size() > kMaxSessionsPerTab)
    sessions_.pop_front();
}

}  // namespace android

}  // namespace chrome
