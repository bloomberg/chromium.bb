// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/once_condition_validator.h"

#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/internal/model.h"

namespace feature_engagement_tracker {

OnceConditionValidator::OnceConditionValidator()
    : currently_showing_feature_(nullptr) {}

OnceConditionValidator::~OnceConditionValidator() = default;

ConditionValidator::Result OnceConditionValidator::MeetsConditions(
    const base::Feature& feature,
    const FeatureConfig& config,
    const Model& model,
    uint32_t current_day) const {
  ConditionValidator::Result result(true);
  result.model_ready_ok = model.IsReady();

  result.currently_showing_ok = currently_showing_feature_ == nullptr;

  result.config_ok = config.valid;

  result.session_rate_ok =
      shown_features_.find(&feature) == shown_features_.end();

  return result;
}

void OnceConditionValidator::NotifyIsShowing(const base::Feature& feature) {
  DCHECK(currently_showing_feature_ == nullptr);
  DCHECK(shown_features_.find(&feature) == shown_features_.end());
  shown_features_.insert(&feature);
  currently_showing_feature_ = &feature;
}

void OnceConditionValidator::NotifyDismissed(const base::Feature& feature) {
  DCHECK(&feature == currently_showing_feature_);
  currently_showing_feature_ = nullptr;
}

}  // namespace feature_engagement_tracker
