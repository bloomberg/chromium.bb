// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_

#include <unordered_set>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/configuration.h"

namespace base {
struct Feature;
}  // namespace base

namespace feature_engagement_tracker {

// An Configuration that always returns the same single invalid configuration,
// regardless of which feature.
class SingleInvalidConfiguration : public Configuration {
 public:
  SingleInvalidConfiguration();
  ~SingleInvalidConfiguration() override;

  // Configuration implementation.
  const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const override;

 private:
  // The invalid configuration to always return.
  FeatureConfig invalid_feature_config_;

  DISALLOW_COPY_AND_ASSIGN(SingleInvalidConfiguration);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_SINGLE_INVALID_CONFIGURATION_H_
