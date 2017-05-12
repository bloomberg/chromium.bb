// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/feature_config_storage_validator.h"

#include <unordered_map>
#include <unordered_set>

#include "base/feature_list.h"
#include "components/feature_engagement_tracker/internal/configuration.h"
#include "components/feature_engagement_tracker/public/feature_list.h"

namespace feature_engagement_tracker {

FeatureConfigStorageValidator::FeatureConfigStorageValidator() = default;

FeatureConfigStorageValidator::~FeatureConfigStorageValidator() = default;

bool FeatureConfigStorageValidator::ShouldStore(
    const std::string& event_name) const {
  return should_store_event_names_.find(event_name) !=
         should_store_event_names_.end();
}

bool FeatureConfigStorageValidator::ShouldKeep(const std::string& event_name,
                                               uint32_t event_day,
                                               uint32_t current_day) const {
  // Should not keep events that will happen in the future.
  if (event_day > current_day)
    return false;

  // If no feature configuration mentioned the event, it should not be kept.
  auto it = longest_storage_times_.find(event_name);
  if (it == longest_storage_times_.end())
    return false;

  // Too old events should not be kept.
  uint32_t longest_storage_time = it->second;
  uint32_t age = current_day - event_day;
  if (longest_storage_time <= age)
    return false;

  return true;
}

void FeatureConfigStorageValidator::InitializeFeatures(
    FeatureVector features,
    const Configuration& configuration) {
  for (const auto* feature : features) {
    if (!base::FeatureList::IsEnabled(*feature))
      continue;

    InitializeFeatureConfig(configuration.GetFeatureConfig(*feature));
  }
}

void FeatureConfigStorageValidator::ClearForTesting() {
  should_store_event_names_.clear();
  longest_storage_times_.clear();
}

void FeatureConfigStorageValidator::InitializeFeatureConfig(
    const FeatureConfig& feature_config) {
  InitializeEventConfig(feature_config.used);
  InitializeEventConfig(feature_config.trigger);

  for (const auto& event_config : feature_config.event_configs)
    InitializeEventConfig(event_config);
}

void FeatureConfigStorageValidator::InitializeEventConfig(
    const EventConfig& event_config) {
  // Minimum storage time is 1 day.
  if (event_config.storage < 1u)
    return;

  // When minimum storage time is met, new events should always be stored.
  should_store_event_names_.insert(event_config.name);

  // Track the longest time any configuration wants to store a particular event.
  uint32_t current_longest_time = longest_storage_times_[event_config.name];
  if (event_config.storage > current_longest_time)
    longest_storage_times_[event_config.name] = event_config.storage;
}

}  // namespace feature_engagement_tracker
