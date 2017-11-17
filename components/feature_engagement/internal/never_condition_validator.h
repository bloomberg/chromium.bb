// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_CONDITION_VALIDATOR_H_

#include "base/macros.h"
#include "components/feature_engagement/internal/condition_validator.h"
#include "components/feature_engagement/public/feature_list.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement {
class AvailabilityModel;
class DisplayLockController;
class EventModel;

// An ConditionValidator that never acknowledges that a feature has met its
// conditions.
class NeverConditionValidator : public ConditionValidator {
 public:
  NeverConditionValidator();
  ~NeverConditionValidator() override;

  // ConditionValidator implementation.
  ConditionValidator::Result MeetsConditions(
      const base::Feature& feature,
      const FeatureConfig& config,
      const EventModel& event_model,
      const AvailabilityModel& availability_model,
      const DisplayLockController& display_lock_controller,
      uint32_t current_day) const override;
  void NotifyIsShowing(
      const base::Feature& feature,
      const FeatureConfig& config,
      const std::vector<std::string>& all_feature_names) override;
  void NotifyDismissed(const base::Feature& feature) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverConditionValidator);
};

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_INTERNAL_NEVER_CONDITION_VALIDATOR_H_
