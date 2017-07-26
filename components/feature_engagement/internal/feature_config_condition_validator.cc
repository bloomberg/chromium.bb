// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/feature_config_condition_validator.h"

#include "base/feature_list.h"
#include "components/feature_engagement/internal/availability_model.h"
#include "components/feature_engagement/internal/configuration.h"
#include "components/feature_engagement/internal/event_model.h"
#include "components/feature_engagement/internal/proto/event.pb.h"
#include "components/feature_engagement/public/feature_list.h"

namespace feature_engagement {

FeatureConfigConditionValidator::FeatureConfigConditionValidator()
    : currently_showing_(false), times_shown_(0u) {}

FeatureConfigConditionValidator::~FeatureConfigConditionValidator() = default;

ConditionValidator::Result FeatureConfigConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const EventModel& event_model,
    const AvailabilityModel& availability_model,
    uint32_t current_day) const {
  ConditionValidator::Result result(true);
  result.event_model_ready_ok = event_model.IsReady();
  result.currently_showing_ok = !currently_showing_;
  result.feature_enabled_ok = base::FeatureList::IsEnabled(feature);
  result.config_ok = config.valid;
  result.used_ok =
      EventConfigMeetsConditions(config.used, event_model, current_day);
  result.trigger_ok =
      EventConfigMeetsConditions(config.trigger, event_model, current_day);

  for (const auto& event_config : config.event_configs) {
    result.preconditions_ok &=
        EventConfigMeetsConditions(event_config, event_model, current_day);
  }

  result.session_rate_ok = config.session_rate.MeetsCriteria(times_shown_);

  result.availability_model_ready_ok = availability_model.IsReady();

  result.availability_ok = AvailabilityMeetsConditions(
      feature, config.availability, availability_model, current_day);

  return result;
}

void FeatureConfigConditionValidator::NotifyIsShowing(
    const base::Feature& feature) {
  DCHECK(!currently_showing_);
  DCHECK(base::FeatureList::IsEnabled(feature));

  currently_showing_ = true;
  ++times_shown_;
}

void FeatureConfigConditionValidator::NotifyDismissed(
    const base::Feature& feature) {
  currently_showing_ = false;
}

bool FeatureConfigConditionValidator::EventConfigMeetsConditions(
    const EventConfig& event_config,
    const EventModel& event_model,
    uint32_t current_day) const {
  const Event* event = event_model.GetEvent(event_config.name);

  // If no events are found, the requirement must be met with 0 elements.
  // Also, if the window is 0 days, there will never be any events.
  if (event == nullptr || event_config.window == 0u)
    return event_config.comparator.MeetsCriteria(0u);

  DCHECK(event_config.window >= 0);

  // A window of N=0:  Nothing should be counted.
  // A window of N=1:  |current_day| should be counted.
  // A window of N=2+: |current_day| plus |N-1| more days should be counted.
  uint32_t oldest_accepted_day = current_day - event_config.window + 1;

  // Cap |oldest_accepted_day| to UNIX epoch.
  if (event_config.window > current_day)
    oldest_accepted_day = 0u;

  // Calculate the number of events within the window.
  uint32_t event_count = 0;
  for (const auto& event_day : event->events()) {
    if (event_day.day() < oldest_accepted_day)
      continue;

    event_count += event_day.count();
  }

  return event_config.comparator.MeetsCriteria(event_count);
}

bool FeatureConfigConditionValidator::AvailabilityMeetsConditions(
    const base::Feature& feature,
    Comparator comparator,
    const AvailabilityModel& availability_model,
    uint32_t current_day) const {
  if (comparator.type == ANY)
    return true;

  base::Optional<uint32_t> availability_day =
      availability_model.GetAvailability(feature);
  if (!availability_day.has_value())
    return false;

  uint32_t days_available = current_day - availability_day.value();

  // Ensure that availability days never wrap around.
  if (availability_day.value() > current_day)
    days_available = 0u;

  return comparator.MeetsCriteria(days_available);
}

}  // namespace feature_engagement
