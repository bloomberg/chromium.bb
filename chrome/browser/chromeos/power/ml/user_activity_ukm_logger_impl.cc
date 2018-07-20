// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "chrome/browser/chromeos/power/ml/user_activity_event.pb.h"
#include "chrome/browser/chromeos/power/ml/user_activity_manager.h"
#include "chrome/browser/chromeos/power/ml/user_activity_ukm_logger_impl.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/gfx/native_widget_types.h"

namespace chromeos {
namespace power {
namespace ml {

namespace {

constexpr UserActivityUkmLoggerImpl::Bucket kBatteryPercentBuckets[] = {
    {100, 5}};

constexpr UserActivityUkmLoggerImpl::Bucket kEventLogDurationBuckets[] = {
    {600, 1},
    {1200, 10},
    {1800, 20}};

constexpr UserActivityUkmLoggerImpl::Bucket kUserInputEventBuckets[] = {
    {100, 1},
    {1000, 100},
    {10000, 1000}};

constexpr UserActivityUkmLoggerImpl::Bucket kRecentVideoPlayingTimeBuckets[] = {
    {60, 1},
    {1200, 300},
    {3600, 600},
    {18000, 1800}};

constexpr UserActivityUkmLoggerImpl::Bucket kTimeSinceLastVideoEndedBuckets[] =
    {{60, 1}, {600, 60}, {1200, 300}, {3600, 600}, {18000, 1800}};

}  // namespace

int UserActivityUkmLoggerImpl::Bucketize(int original_value,
                                         const Bucket* buckets,
                                         size_t num_buckets) {
  DCHECK_GE(original_value, 0);
  DCHECK(buckets);
  for (size_t i = 0; i < num_buckets; ++i) {
    const Bucket& bucket = buckets[i];
    if (original_value < bucket.boundary_end) {
      return bucket.rounding * (original_value / bucket.rounding);
    }
  }
  return buckets[num_buckets - 1].boundary_end;
}

UserActivityUkmLoggerImpl::UserActivityUkmLoggerImpl()
    : ukm_recorder_(ukm::UkmRecorder::Get()) {}

UserActivityUkmLoggerImpl::~UserActivityUkmLoggerImpl() = default;

void UserActivityUkmLoggerImpl::LogActivity(const UserActivityEvent& event) {
  DCHECK(ukm_recorder_);
  ukm::SourceId source_id = ukm_recorder_->GetNewSourceID();
  ukm::builders::UserActivity user_activity(source_id);

  const UserActivityEvent::Features& features = event.features();

  user_activity.SetSequenceId(next_sequence_id_++)
      .SetDeviceMode(features.device_mode())
      .SetDeviceType(features.device_type())
      .SetEventLogDuration(Bucketize(event.event().log_duration_sec(),
                                     kEventLogDurationBuckets,
                                     base::size(kEventLogDurationBuckets)))
      .SetEventReason(event.event().reason())
      .SetEventType(event.event().type())
      .SetLastActivityDay(features.last_activity_day())
      .SetLastActivityTime(std::floor(features.last_activity_time_sec() / 3600))
      .SetRecentTimeActive(features.recent_time_active_sec())
      .SetRecentVideoPlayingTime(Bucketize(
          features.video_playing_time_sec(), kRecentVideoPlayingTimeBuckets,
          base::size(kRecentVideoPlayingTimeBuckets)))
      .SetScreenDimmedInitially(features.screen_dimmed_initially())
      .SetScreenDimOccurred(event.event().screen_dim_occurred())
      .SetScreenLockedInitially(features.screen_locked_initially())
      .SetScreenLockOccurred(event.event().screen_lock_occurred())
      .SetScreenOffInitially(features.screen_off_initially())
      .SetScreenOffOccurred(event.event().screen_off_occurred());

  if (features.has_on_to_dim_sec()) {
    user_activity.SetScreenDimDelay(features.on_to_dim_sec());
  }
  if (features.has_dim_to_screen_off_sec()) {
    user_activity.SetScreenDimToOffDelay(features.dim_to_screen_off_sec());
  }

  if (features.has_last_user_activity_time_sec()) {
    user_activity.SetLastUserActivityTime(
        std::floor(features.last_user_activity_time_sec() / 3600));
  }
  if (features.has_time_since_last_key_sec()) {
    user_activity.SetTimeSinceLastKey(features.time_since_last_key_sec());
  }
  if (features.has_time_since_last_mouse_sec()) {
    user_activity.SetTimeSinceLastMouse(features.time_since_last_mouse_sec());
  }
  if (features.has_time_since_last_touch_sec()) {
    user_activity.SetTimeSinceLastTouch(features.time_since_last_touch_sec());
  }

  if (features.has_on_battery()) {
    user_activity.SetOnBattery(features.on_battery());
  }

  if (features.has_battery_percent()) {
    user_activity.SetBatteryPercent(
        Bucketize(std::floor(features.battery_percent()),
                  kBatteryPercentBuckets, base::size(kBatteryPercentBuckets)));
  }

  if (features.has_device_management()) {
    user_activity.SetDeviceManagement(features.device_management());
  }

  if (features.has_time_since_video_ended_sec()) {
    user_activity.SetTimeSinceLastVideoEnded(Bucketize(
        features.time_since_video_ended_sec(), kTimeSinceLastVideoEndedBuckets,
        base::size(kTimeSinceLastVideoEndedBuckets)));
  }

  if (features.has_key_events_in_last_hour()) {
    user_activity.SetKeyEventsInLastHour(
        Bucketize(features.key_events_in_last_hour(), kUserInputEventBuckets,
                  base::size(kUserInputEventBuckets)));
  }

  if (features.has_mouse_events_in_last_hour()) {
    user_activity.SetMouseEventsInLastHour(
        Bucketize(features.mouse_events_in_last_hour(), kUserInputEventBuckets,
                  base::size(kUserInputEventBuckets)));
  }

  if (features.has_touch_events_in_last_hour()) {
    user_activity.SetTouchEventsInLastHour(
        Bucketize(features.touch_events_in_last_hour(), kUserInputEventBuckets,
                  base::size(kUserInputEventBuckets)));
  }

  user_activity
      .SetPreviousNegativeActionsCount(
          features.previous_negative_actions_count())
      .SetPreviousPositiveActionsCount(
          features.previous_positive_actions_count());

  if (event.has_model_prediction()) {
    const UserActivityEvent::ModelPrediction& model_prediction =
        event.model_prediction();
    user_activity.SetModelApplied(model_prediction.model_applied())
        .SetModelDecisionThreshold(model_prediction.decision_threshold())
        .SetModelInactivityScore(model_prediction.inactivity_score());
  }

  user_activity.Record(ukm_recorder_);

  if (features.has_source_id()) {
    ukm::builders::UserActivityId user_activity_id(features.source_id());
    user_activity_id.SetActivityId(source_id).SetHasFormEntry(
        features.has_form_entry());

    if (features.has_engagement_score()) {
      user_activity_id.SetSiteEngagementScore(features.engagement_score());
    }
    user_activity_id.Record(ukm_recorder_);
  }
}

}  // namespace ml
}  // namespace power
}  // namespace chromeos
