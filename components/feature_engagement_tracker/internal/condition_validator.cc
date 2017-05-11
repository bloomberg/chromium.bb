// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/condition_validator.h"

namespace feature_engagement_tracker {

ConditionValidator::Result::Result(bool initial_values)
    : model_ready_ok(initial_values),
      currently_showing_ok(initial_values),
      config_ok(initial_values),
      used_ok(initial_values),
      trigger_ok(initial_values),
      preconditions_ok(initial_values),
      session_rate_ok(initial_values) {}

bool ConditionValidator::Result::NoErrors() {
  return model_ready_ok && currently_showing_ok && config_ok && used_ok &&
         trigger_ok && preconditions_ok && session_rate_ok;
}

}  // namespace feature_engagement_tracker
