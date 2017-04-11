// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/editable_configuration.h"

#include <map>

#include "base/logging.h"
#include "components/feature_engagement_tracker/internal/configuration.h"

namespace feature_engagement_tracker {

EditableConfiguration::EditableConfiguration() = default;

EditableConfiguration::~EditableConfiguration() = default;

void EditableConfiguration::SetConfiguration(
    const base::Feature* feature,
    const FeatureConfig& feature_config) {
  configs_[feature] = feature_config;
}

const FeatureConfig& EditableConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  auto it = configs_.find(&feature);
  DCHECK(it != configs_.end());
  return it->second;
}

}  // namespace feature_engagement_tracker
