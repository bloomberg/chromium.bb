// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/internal/never_condition_validator.h"

namespace feature_engagement {

NeverConditionValidator::NeverConditionValidator() = default;

NeverConditionValidator::~NeverConditionValidator() = default;

ConditionValidator::Result NeverConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const EventModel& event_model,
    const AvailabilityModel& availability_model,
    const DisplayLockController& display_lock_controller,
    uint32_t current_day) const {
  return ConditionValidator::Result(false);
}

void NeverConditionValidator::NotifyIsShowing(
    const base::Feature& feature,
    const FeatureConfig& config,
    const std::vector<std::string>& all_feature_names) {}

void NeverConditionValidator::NotifyDismissed(const base::Feature& feature) {}

}  // namespace feature_engagement
