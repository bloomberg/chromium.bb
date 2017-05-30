// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/condition_validator.h"

namespace feature_engagement_tracker {

ConditionValidator::Result::Result(bool initial_values)
    : event_model_ready_ok(initial_values),
      currently_showing_ok(initial_values),
      feature_enabled_ok(initial_values),
      config_ok(initial_values),
      used_ok(initial_values),
      trigger_ok(initial_values),
      preconditions_ok(initial_values),
      session_rate_ok(initial_values),
      availability_model_ready_ok(initial_values),
      availability_ok(initial_values) {}

ConditionValidator::Result::Result(const Result& other) {
  event_model_ready_ok = other.event_model_ready_ok;
  currently_showing_ok = other.currently_showing_ok;
  feature_enabled_ok = other.feature_enabled_ok;
  config_ok = other.config_ok;
  used_ok = other.used_ok;
  trigger_ok = other.trigger_ok;
  preconditions_ok = other.preconditions_ok;
  session_rate_ok = other.session_rate_ok;
  availability_model_ready_ok = other.availability_model_ready_ok;
  availability_ok = other.availability_ok;
}

bool ConditionValidator::Result::NoErrors() const {
  return event_model_ready_ok && currently_showing_ok && feature_enabled_ok &&
         config_ok && used_ok && trigger_ok && preconditions_ok &&
         session_rate_ok && availability_model_ready_ok && availability_ok;
}

}  // namespace feature_engagement_tracker
