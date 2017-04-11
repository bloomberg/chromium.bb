// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_NEVER_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_NEVER_CONDITION_VALIDATOR_H_

#include <unordered_set>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/condition_validator.h"
#include "components/feature_engagement_tracker/internal/model.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement_tracker {

// An ConditionValidator that never acknowledges that a feature has met its
// conditions.
class NeverConditionValidator : public ConditionValidator {
 public:
  NeverConditionValidator();
  ~NeverConditionValidator() override;

  // ConditionValidator implementation.
  bool MeetsConditions(const base::Feature& feature,
                       const Model& model) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverConditionValidator);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_NEVER_CONDITION_VALIDATOR_H_
