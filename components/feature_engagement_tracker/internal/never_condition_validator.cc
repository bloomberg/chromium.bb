// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_condition_validator.h"

namespace feature_engagement_tracker {

NeverConditionValidator::NeverConditionValidator() = default;

NeverConditionValidator::~NeverConditionValidator() = default;

ConditionValidator::Result NeverConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const Model& model,
    uint32_t current_day) const {
  return ConditionValidator::Result(false);
}

void NeverConditionValidator::NotifyIsShowing(const base::Feature& feature) {}

void NeverConditionValidator::NotifyDismissed(const base::Feature& feature) {}

}  // namespace feature_engagement_tracker
