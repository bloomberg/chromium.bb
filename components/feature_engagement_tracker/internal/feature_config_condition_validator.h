// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_CONFIG_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_CONFIG_CONDITION_VALIDATOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/condition_validator.h"

namespace feature_engagement_tracker {
struct EventConfig;
class Model;

// A ConditionValidator that uses the FeatureConfigs as the source of truth.
class FeatureConfigConditionValidator : public ConditionValidator {
 public:
  FeatureConfigConditionValidator();
  ~FeatureConfigConditionValidator() override;

  // ConditionValidator implementation.
  ConditionValidator::Result MeetsConditions(
      const base::Feature& feature,
      const FeatureConfig& config,
      const Model& model,
      uint32_t current_day) const override;
  void NotifyIsShowing(const base::Feature& feature) override;
  void NotifyDismissed(const base::Feature& feature) override;

 private:
  bool EventConfigMeetsConditions(const EventConfig& event_config,
                                  const Model& model,
                                  uint32_t current_day) const;

  // Whether in-product help is currently being shown.
  bool currently_showing_;

  // Number of times in-product help has been shown within the current session.
  uint32_t times_shown_;

  DISALLOW_COPY_AND_ASSIGN(FeatureConfigConditionValidator);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_CONFIG_CONDITION_VALIDATOR_H_
