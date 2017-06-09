// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/single_invalid_configuration.h"

#include "components/feature_engagement_tracker/internal/configuration.h"

namespace feature_engagement_tracker {

SingleInvalidConfiguration::SingleInvalidConfiguration() {
  invalid_feature_config_.valid = false;
  invalid_feature_config_.used.name = "nothing_to_see_here";
}

SingleInvalidConfiguration::~SingleInvalidConfiguration() = default;

const FeatureConfig& SingleInvalidConfiguration::GetFeatureConfig(
    const base::Feature& feature) const {
  return invalid_feature_config_;
}

const FeatureConfig& SingleInvalidConfiguration::GetFeatureConfigByName(
    const std::string& feature_name) const {
  return invalid_feature_config_;
}

const Configuration::ConfigMap&
SingleInvalidConfiguration::GetRegisteredFeatures() const {
  return configs_;
}

}  // namespace feature_engagement_tracker
