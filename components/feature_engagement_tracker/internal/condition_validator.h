// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONDITION_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONDITION_VALIDATOR_H_

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/model.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement_tracker {

// A ConditionValidator checks the requred conditions for a given feature,
// and checks if all conditions are met.
class ConditionValidator {
 public:
  virtual ~ConditionValidator() = default;

  // Returns true iff all conditions for the given feature have been met.
  virtual bool MeetsConditions(const base::Feature& feature,
                               const Model& model) = 0;

 protected:
  ConditionValidator() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConditionValidator);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONDITION_VALIDATOR_H_
