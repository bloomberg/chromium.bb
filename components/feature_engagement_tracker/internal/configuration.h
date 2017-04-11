// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONFIGURATION_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONFIGURATION_H_

#include <map>
#include <string>

#include "base/macros.h"

namespace base {
struct Feature;
}

namespace feature_engagement_tracker {

// A FeatureConfig contains all the configuration for a given feature.
struct FeatureConfig {
 public:
  FeatureConfig();
  ~FeatureConfig();

  // Whether the configuration has been successfully parsed.
  bool valid;

  // The identifier for a particular event that will be searched for when
  // counting how many times a particular feature has been used.
  std::string feature_used_event;
};

bool operator==(const FeatureConfig& lhs, const FeatureConfig& rhs);

// A Configuration contains the current set of runtime configurations.
// It is up to each implementation of Configuration to provide a way to
// register features and their configurations.
class Configuration {
 public:
  // Convenience alias for typical implementatinos of Configuration.
  using ConfigMap = std::map<const base::Feature*, FeatureConfig>;

  virtual ~Configuration() = default;

  // Returns the FeatureConfig for the given |feature|. The |feature| must
  // be registered with the Configuration instance.
  virtual const FeatureConfig& GetFeatureConfig(
      const base::Feature& feature) const = 0;

 protected:
  Configuration() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(Configuration);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_CONFIGURATION_H_
