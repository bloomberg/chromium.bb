// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_CONFIG_STORAGE_VALIDATOR_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_CONFIG_STORAGE_VALIDATOR_H_

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "base/macros.h"
#include "components/feature_engagement_tracker/internal/feature_list.h"
#include "components/feature_engagement_tracker/internal/storage_validator.h"

namespace feature_engagement_tracker {
class Configuration;
struct EventConfig;
struct FeatureConfig;

// A StorageValidator that uses the FeatureConfig as the source of truth.
class FeatureConfigStorageValidator : public StorageValidator {
 public:
  FeatureConfigStorageValidator();
  ~FeatureConfigStorageValidator() override;

  // StorageValidator implementation.
  bool ShouldStore(const std::string& event_name) override;
  bool ShouldKeep(const std::string& event_name,
                  uint32_t event_day,
                  uint32_t current_day) override;

  // Set up internal configuration required for the given |features|.
  void InitializeFeatures(FeatureVector features,
                          const Configuration& configuration);

  // Resets the full state of this StorageValidator. After calling this method
  // it is valid to call InitializeFeatures() again.
  void ClearForTesting();

 private:
  // Updates the internal configuration with conditions from the given
  // |feature_config|.
  void InitializeFeatureConfig(const FeatureConfig& feature_config);

  // Updates the internal configuration with conditions from the given
  // |event_config|.
  void InitializeEventConfig(const EventConfig& event_config);

  // Contains an entry for each of the events that any EventConfig required to
  // be stored.
  std::unordered_set<std::string> should_store_event_names_;

  // Contains the longest time to store each event across all EventConfigs,
  // as a number of days.
  std::unordered_map<std::string, uint32_t> longest_storage_times_;

  DISALLOW_COPY_AND_ASSIGN(FeatureConfigStorageValidator);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_FEATURE_CONFIG_STORAGE_VALIDATOR_H_
