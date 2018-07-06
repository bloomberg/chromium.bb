// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/feed_scheduler_host.h"

#include <map>
#include <string>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "base/stl_util.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/feed/core/pref_names.h"
#include "components/feed/core/time_serialization.h"
#include "components/feed/feed_feature_list.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace feed {

namespace {

using TriggerType = FeedSchedulerHost::TriggerType;
using UserClass = UserClassifier::UserClass;

struct ParamPair {
  std::string name;
  double default_value;
};

// The Cartesian product of TriggerType and UserClass each need a different
// param name in case we decide to change it via a config change. This nested
// switch lookup ensures that all combinations are defined, along with a
// default value.
ParamPair LookupParam(UserClass user_class, TriggerType trigger) {
  switch (user_class) {
    case UserClass::RARE_NTP_USER:
      switch (trigger) {
        case TriggerType::NTP_SHOWN:
          return {"ntp_shown_hours_rare_ntp_user", 4.0};
        case TriggerType::FOREGROUNDED:
          return {"foregrounded_hours_rare_ntp_user", 24.0};
        case TriggerType::FIXED_TIMER:
          return {"fixed_timer_hours_rare_ntp_user", 96.0};
      }
    case UserClass::ACTIVE_NTP_USER:
      switch (trigger) {
        case TriggerType::NTP_SHOWN:
          return {"ntp_shown_hours_active_ntp_user", 4.0};
        case TriggerType::FOREGROUNDED:
          return {"foregrounded_hours_active_ntp_user", 24.0};
        case TriggerType::FIXED_TIMER:
          return {"fixed_timer_hours_active_ntp_user", 48.0};
      }
    case UserClass::ACTIVE_SUGGESTIONS_CONSUMER:
      switch (trigger) {
        case TriggerType::NTP_SHOWN:
          return {"ntp_shown_hours_active_suggestions_consumer", 1.0};
        case TriggerType::FOREGROUNDED:
          return {"foregrounded_hours_active_suggestions_consumer", 12.0};
        case TriggerType::FIXED_TIMER:
          return {"fixed_timer_hours_active_suggestions_consumer", 24.0};
      }
  }
}

// Run the given closure if it is valid.
void TryRun(base::OnceClosure closure) {
  if (closure) {
    std::move(closure).Run();
  }
}

}  // namespace

FeedSchedulerHost::FeedSchedulerHost(PrefService* pref_service,
                                     base::Clock* clock)
    : pref_service_(pref_service),
      clock_(clock),
      user_classifier_(pref_service, clock) {}

FeedSchedulerHost::~FeedSchedulerHost() = default;

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
  NativeRequestBehavior behavior;
  if (!has_outstanding_request && ShouldRefresh(TriggerType::NTP_SHOWN)) {
    if (!has_content) {
      behavior = REQUEST_WITH_WAIT;
    } else if (IsContentStale(content_creation_date_time)) {
      behavior = REQUEST_WITH_TIMEOUT;
    } else {
      behavior = REQUEST_WITH_CONTENT;
    }
  } else {
    // Note that NO_REQUEST_WITH_WAIT is used to show a blank article section
    // even when no request is being made. The user will be given the ability to
    // force a refresh but this scheduler is not driving it.
    if (!has_content) {
      behavior = NO_REQUEST_WITH_WAIT;
    } else if (IsContentStale(content_creation_date_time) &&
               has_outstanding_request) {
      // This needs to check |has_outstanding_request|, it does not make sense
      // to use a timeout when no request is being made. Just show the stale
      // content, since nothing better is on the way.
      behavior = NO_REQUEST_WITH_TIMEOUT;
    } else {
      behavior = NO_REQUEST_WITH_CONTENT;
    }
  }

  // TODO(skym): Record requested behavior into histogram.
  user_classifier_.OnEvent(UserClassifier::Event::NTP_OPENED);
  return behavior;
}

void FeedSchedulerHost::OnReceiveNewContent(
    base::Time content_creation_date_time) {
  pref_service_->SetTime(prefs::kLastFetchAttemptTime,
                         content_creation_date_time);
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

void FeedSchedulerHost::OnSuggestionConsumed() {
  user_classifier_.OnEvent(UserClassifier::Event::SUGGESTIONS_USED);
}

bool FeedSchedulerHost::ShouldRefresh(TriggerType trigger) {
  // TODO(skym): Check various other criteria are met, record metrics.
  return (clock_->Now() -
          pref_service_->GetTime(prefs::kLastFetchAttemptTime)) >
         GetTriggerThreshold(trigger);
}

bool FeedSchedulerHost::IsContentStale(base::Time content_creation_date_time) {
  return (clock_->Now() - content_creation_date_time) >
         GetTriggerThreshold(TriggerType::FOREGROUNDED);
}

base::TimeDelta FeedSchedulerHost::GetTriggerThreshold(TriggerType trigger) {
  UserClass user_class = user_classifier_.GetUserClass();
  ParamPair param = LookupParam(user_class, trigger);
  double value_hours = base::GetFieldTrialParamByFeatureAsDouble(
      kInterestFeedContentSuggestions, param.name, param.default_value);

  // Use FromSecondsD in case one of the values contained a decimal.
  return base::TimeDelta::FromSecondsD(value_hours * 3600.0);
}

void FeedSchedulerHost::ScheduleFixedTimerWakeUp(base::TimeDelta period) {
  pref_service_->SetTimeDelta(prefs::kBackgroundRefreshPeriod, period);
  schedule_background_task_callback_.Run(period);
}

}  // namespace feed
