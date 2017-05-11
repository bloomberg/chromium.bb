// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_ONCE_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_ONCE_CONDITION_VALIDATOR_H_

#include <unordered_set>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/condition_validator.h"
#include "components/feature_engagement_tracker/public/feature_list.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement_tracker {
class Model;

// An ConditionValidator that will ensure that each base::Feature will meet
// conditions maximum one time for any given session.
// It has the following requirements:
// - The Model is ready.
// - No other in-product help is currently showing.
// - FeatureConfig for the feature is valid.
// - This is the first time the given base::Feature meets all above stated
//   conditions.
//
// NOTE: This ConditionValidator fully ignores whether the base::Feature is
// enabled or not and any other configuration specified in the FeatureConfig.
// In practice this leads this ConditionValidator to be well suited for a
// demonstration mode of in-product help.
class OnceConditionValidator : public ConditionValidator {
 public:
  OnceConditionValidator();
  ~OnceConditionValidator() override;

  // ConditionValidator implementation.
  ConditionValidator::Result MeetsConditions(const base::Feature& feature,
                                             const Model& model,
                                             uint32_t current_day) override;

 private:
  // Contains all features that have met conditions within the current session.
  std::unordered_set<const base::Feature*> shown_features_;

  DISALLOW_COPY_AND_ASSIGN(OnceConditionValidator);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_ONCE_CONDITION_VALIDATOR_H_
