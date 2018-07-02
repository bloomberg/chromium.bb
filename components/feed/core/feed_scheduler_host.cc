// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <utility>

#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/time_serialization.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace feed {

namespace {

// Run the given closure if it is valid.
void TryRun(base::OnceClosure closure) {
  if (closure) {
    std::move(closure).Run();
  }
}

}  // namespace

enum class FeedSchedulerHost::TriggerType {
  NTP_SHOWN = 0,
  FOREGROUNDED = 1,
  FIXED_TIMER = 2,
  COUNT
};

FeedSchedulerHost::FeedSchedulerHost(PrefService* pref_service,
                                     base::Clock* clock)
    : pref_service_(pref_service), clock_(clock) {}

FeedSchedulerHost::~FeedSchedulerHost() {}

// static
void FeedSchedulerHost::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterTimePref(prefs::kLastFetchAttemptTime, base::Time());
  registry->RegisterTimeDeltaPref(prefs::kBackgroundRefreshPeriod,
                                  base::TimeDelta());
}

void FeedSchedulerHost::Initialize(
    base::RepeatingClosure refresh_callback,
    ScheduleBackgroundTaskCallback schedule_background_task_callback) {
  // There should only ever be one scheduler host and bridge created. Neither
  // are ever destroyed before shutdown, and this method should only be called
  // once as the bridge is constructed.
  DCHECK(!refresh_callback_);
  DCHECK(!schedule_background_task_callback_);

  refresh_callback_ = std::move(refresh_callback);
  schedule_background_task_callback_ =
      std::move(schedule_background_task_callback);

  base::TimeDelta old_period =
      pref_service_->GetTimeDelta(prefs::kBackgroundRefreshPeriod);
  base::TimeDelta new_period = GetTriggerThreshold(TriggerType::FIXED_TIMER);
  if (old_period != new_period) {
    ScheduleFixedTimerWakeUp(new_period);
  }
}

NativeRequestBehavior FeedSchedulerHost::ShouldSessionRequestData(
    bool has_content,
    base::Time content_creation_date_time,
    bool has_outstanding_request) {
  // TODO(skym): Record requested behavior into histogram.
  if (!has_outstanding_request && ShouldRefresh(TriggerType::NTP_SHOWN)) {
    if (!has_content) {
      return REQUEST_WITH_WAIT;
    } else if (IsContentStale(content_creation_date_time)) {
      return REQUEST_WITH_TIMEOUT;
    } else {
      return REQUEST_WITH_CONTENT;
    }
  } else {
    if (!has_content) {
      return NO_REQUEST_WITH_WAIT;
    } else if (IsContentStale(content_creation_date_time)) {
      return NO_REQUEST_WITH_TIMEOUT;
    } else {
      return NO_REQUEST_WITH_CONTENT;
    }
  }
}

void FeedSchedulerHost::OnReceiveNewContent(
    base::Time content_creation_date_time) {
  pref_service_->SetTime(prefs::kLastFetchAttemptTime, clock_->Now());
  TryRun(std::move(fixed_timer_completion_));
  ScheduleFixedTimerWakeUp(GetTriggerThreshold(TriggerType::FIXED_TIMER));
}

void FeedSchedulerHost::OnRequestError(int network_response_code) {
  pref_service_->SetTime(prefs::kLastFetchAttemptTime, clock_->Now());
  TryRun(std::move(fixed_timer_completion_));
}

void FeedSchedulerHost::OnForegrounded() {
  DCHECK(refresh_callback_);
  if (ShouldRefresh(TriggerType::FOREGROUNDED)) {
    refresh_callback_.Run();
  }
}

void FeedSchedulerHost::OnFixedTimer(base::OnceClosure on_completion) {
  DCHECK(refresh_callback_);
  if (ShouldRefresh(TriggerType::FIXED_TIMER)) {
    // There shouldn't typically be anything in |fixed_timer_completion_| right
    // now, but if there was, run it before we replace it.
    TryRun(std::move(fixed_timer_completion_));

    fixed_timer_completion_ = std::move(on_completion);
    refresh_callback_.Run();
  } else {
    // The task driving this doesn't need to stay around, since no work is being
    // done on its behalf.
    TryRun(std::move(on_completion));
  }
}

void FeedSchedulerHost::OnTaskReschedule() {
  ScheduleFixedTimerWakeUp(GetTriggerThreshold(TriggerType::FIXED_TIMER));
}

bool FeedSchedulerHost::ShouldRefresh(TriggerType trigger) {
  // TODO(skym): Check various criteria are met, record metrics.
  return true;
}

bool FeedSchedulerHost::IsContentStale(base::Time content_creation_date_time) {
  // TODO(skym): Compare |content_creation_date_time| to foregrounded trigger's
  // threshold.
  return false;
}

base::TimeDelta FeedSchedulerHost::GetTriggerThreshold(TriggerType trigger) {
  // TODO(skym): Select Finch param based on trigger and user classification.
  return base::TimeDelta::FromMinutes(1);
}

void FeedSchedulerHost::ScheduleFixedTimerWakeUp(base::TimeDelta period) {
  pref_service_->SetTimeDelta(prefs::kBackgroundRefreshPeriod, period);
  schedule_background_task_callback_.Run(period);
}

}  // namespace feed
