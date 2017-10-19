// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MACHINE_INTELLIGENCE_RANKER_EXAMPLE_UTIL_H_
#define COMPONENTS_MACHINE_INTELLIGENCE_RANKER_EXAMPLE_UTIL_H_

#include "base/compiler_specific.h"
#include "components/machine_intelligence/proto/ranker_example.pb.h"

namespace machine_intelligence {

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

// Extract category from one-hot feature. Returns true and fills
// in |value| if the feature is found and is of type string_value. Returns false
// otherwise.
bool GetOneHotValue(const std::string& key,
                    const RankerExample& example,
                    std::string* value) WARN_UNUSED_RESULT;

}  // namespace machine_intelligence

#endif  // COMPONENTS_MACHINE_INTELLIGENCE_RANKER_EXAMPLE_UTIL_H_
