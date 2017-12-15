// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_
#define COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_

#include "base/compiler_specific.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"

namespace assist_ranker {

// If |key| feature is found in |example|, fills in |feature| and return true.
// Returns false if the feature is not found. |feature| can be nullptr. In such
// a case, the return value is not changed, but |feature| will not be filled in.
// This can be used to check for the presence of a key.
bool SafeGetFeature(const std::string& key,
                    const RankerExample& example,
                    Feature* feature) WARN_UNUSED_RESULT;

// Extract value from |feature| for scalar feature types. Returns true and fills
// in |value| if the feature is found and has a float, int32 or bool value.
// Returns false otherwise.
bool GetFeatureValueAsFloat(const std::string& key,
                            const RankerExample& example,
                            float* value) WARN_UNUSED_RESULT;

// Returns a int value for this feature. Float values are multiplied by a given
// constant that defines the precision. String features are hashed.
bool FeatureToInt(const Feature& feature, int* int_value) WARN_UNUSED_RESULT;

// Extract category from one-hot feature. Returns true and fills
// in |value| if the feature is found and is of type string_value. Returns false
// otherwise.
bool GetOneHotValue(const std::string& key,
                    const RankerExample& example,
                    std::string* value) WARN_UNUSED_RESULT;

// Converts a string to a hex ahsh string.
std::string HashFeatureName(const std::string& feature_name);

// Hashes feature names to an hex string.
// Features logged through UKM will apply this transformation when logging
// features, so models trained on UKM data are expected to have hashed input
// feature names.
RankerExample HashExampleFeatureNames(const RankerExample& example);

}  // namespace assist_ranker

#endif  // COMPONENTS_ASSIST_RANKER_RANKER_EXAMPLE_UTIL_H_
