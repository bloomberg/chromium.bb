// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/once_condition_validator.h"

#include "base/feature_list.h"
#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/internal/model.h"

namespace feature_engagement_tracker {

OnceConditionValidator::OnceConditionValidator() = default;

OnceConditionValidator::~OnceConditionValidator() = default;

bool OnceConditionValidator::MeetsConditions(const base::Feature& feature,
                                             const Model& model) {
  if (!model.IsReady())
    return false;

  if (model.IsCurrentlyShowing())
    return false;

  const FeatureConfig& config = model.GetFeatureConfig(feature);
  if (!config.valid)
    return false;

  if (shown_features_.find(&feature) == shown_features_.end()) {
    shown_features_.insert(&feature);
    return true;
  }

  return false;
}

}  // namespace feature_engagement_tracker
