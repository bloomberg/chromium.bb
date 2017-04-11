// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/configuration.h"

#include <string>

namespace feature_engagement_tracker {

FeatureConfig::FeatureConfig() : valid(false) {}

FeatureConfig::~FeatureConfig() = default;

bool operator==(const FeatureConfig& lhs, const FeatureConfig& rhs) {
  return lhs.valid == rhs.valid &&
         lhs.feature_used_event == rhs.feature_used_event;
}

}  // namespace feature_engagement_tracker
