// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/machine_intelligence/ranker_example_util.h"

namespace machine_intelligence {

bool HasFeature(const std::string& key, const RankerExample& example) {
  return (example.features().find(key) != example.features().end());
}

bool SafeGetFeature(const std::string& key,
                    const RankerExample& example,
                    Feature* feature) {
  auto p_feature = example.features().find(key);
  if (p_feature != example.features().end()) {
    *feature = p_feature->second;
    return true;
  }
  return false;
}

bool GetFeatureValueAsFloat(const std::string& key,
                            const RankerExample& example,
                            float* value) {
  Feature feature;
  if (!SafeGetFeature(key, example, &feature)) {
    return false;
  }
  switch (feature.feature_type_case()) {
    case Feature::kBoolValue:
      *value = static_cast<float>(feature.bool_value());
      break;
    case Feature::kInt64Value:
      *value = static_cast<float>(feature.int64_value());
      break;
    case Feature::kFloatValue:
      *value = feature.float_value();
      break;
    default:
      return false;
  }
  return true;
}

bool GetOneHotValue(const std::string& key,
                    const RankerExample& example,
                    std::string* value) {
  Feature feature;
  if (!SafeGetFeature(key, example, &feature)) {
    return false;
  }
  if (feature.feature_type_case() != Feature::kStringValue) {
    return false;
  }
  *value = feature.string_value();
  return true;
}

}  // namespace machine_intelligence
