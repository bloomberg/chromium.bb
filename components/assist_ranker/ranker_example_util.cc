// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/ranker_example_util.h"
#include "base/logging.h"

namespace assist_ranker {

bool SafeGetFeature(const std::string& key,
                    const RankerExample& example,
                    Feature* feature) {
  auto p_feature = example.features().find(key);
  if (p_feature != example.features().end()) {
    if (feature)
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
    case Feature::kInt32Value:
      *value = static_cast<float>(feature.int32_value());
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
    DVLOG(1) << "Feature " << key
             << " exists, but is not the right type (Expected: "
             << Feature::kStringValue
             << " vs. Actual: " << feature.feature_type_case() << ")";
    return false;
  }
  *value = feature.string_value();
  return true;
}

}  // namespace assist_ranker
