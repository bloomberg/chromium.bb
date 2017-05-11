// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/once_condition_validator.h"

#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/internal/model.h"

namespace feature_engagement_tracker {

OnceConditionValidator::OnceConditionValidator() = default;

OnceConditionValidator::~OnceConditionValidator() = default;

ConditionValidator::Result OnceConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const Model& model,
    uint32_t current_day) {
  ConditionValidator::Result result(true);
  result.model_ready_ok = model.IsReady();

  result.currently_showing_ok = !model.IsCurrentlyShowing();

  const FeatureConfig& config = model.GetFeatureConfig(feature);
  result.config_ok = config.valid;

  result.session_rate_ok =
      shown_features_.find(&feature) == shown_features_.end();

  // Only show if there are no other errors.
  if (result.NoErrors() && result.session_rate_ok) {
    shown_features_.insert(&feature);
  }

  return result;
}

}  // namespace feature_engagement_tracker
