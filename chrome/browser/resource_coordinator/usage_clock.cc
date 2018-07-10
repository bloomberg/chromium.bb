// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/usage_clock.h"

#include "chrome/browser/resource_coordinator/time.h"

namespace resource_coordinator {

UsageClock::UsageClock() : current_usage_session_start_time_(NowTicks()) {
#if !defined(OS_CHROMEOS)
  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    auto* tracker = metrics::DesktopSessionDurationTracker::Get();
    tracker->AddObserver(this);
    if (!tracker->in_session())
      current_usage_session_start_time_ = base::TimeTicks();
  }
#endif  // defined(OS_CHROMEOS)
}

UsageClock::~UsageClock() {
#if !defined(OS_CHROMEOS)
  if (metrics::DesktopSessionDurationTracker::IsInitialized())
    metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
#endif
}

base::TimeDelta UsageClock::GetTotalUsageTime() const {
  base::TimeDelta elapsed_time_in_session = usage_time_in_completed_sessions_;
  if (!current_usage_session_start_time_.is_null())
    elapsed_time_in_session += NowTicks() - current_usage_session_start_time_;
  return elapsed_time_in_session;
}

#if !defined(OS_CHROMEOS)
void UsageClock::OnSessionStarted(base::TimeTicks session_start) {
  // Ignore |session_start| because it doesn't come from the resource
  // coordinator clock.
  DCHECK(current_usage_session_start_time_.is_null());
  current_usage_session_start_time_ = NowTicks();
}

void UsageClock::OnSessionEnded(base::TimeDelta session_length) {
  // Ignore |session_length| because it wasn't measured using the resource
  // coordinator clock.
  DCHECK(!current_usage_session_start_time_.is_null());
  usage_time_in_completed_sessions_ +=
      NowTicks() - current_usage_session_start_time_;
  current_usage_session_start_time_ = base::TimeTicks();
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace resource_coordinator
