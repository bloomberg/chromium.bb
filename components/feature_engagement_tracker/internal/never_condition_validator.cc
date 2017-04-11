// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_condition_validator.h"

#include "components/feature_engagement_tracker/internal/model.h"

namespace feature_engagement_tracker {

NeverConditionValidator::NeverConditionValidator() = default;

NeverConditionValidator::~NeverConditionValidator() = default;

bool NeverConditionValidator::MeetsConditions(const base::Feature& feature,
                                              const Model& model) {
  return false;
}

}  // namespace feature_engagement_tracker
