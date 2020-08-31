// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_JSON_FEATURE_PROVIDER_SOURCE_H_
#define EXTENSIONS_COMMON_FEATURES_JSON_FEATURE_PROVIDER_SOURCE_H_

#include <string>

#include "base/macros.h"
#include "base/values.h"

namespace extensions {

// A JSONFeatureProviderSource loads JSON dictionary files that
// define features.
class JSONFeatureProviderSource {
 public:
  explicit JSONFeatureProviderSource(const std::string& name);
  ~JSONFeatureProviderSource();

  // Adds the JSON dictionary file to this provider, merging its values with
  // the current dictionary. Key collisions are treated as errors.
  void LoadJSON(int resource_id);

  // Returns the parsed dictionary.
  const base::DictionaryValue& dictionary() { return dictionary_; }

 private:
  // The name of this feature type; only used for debugging.
  const std::string name_;

  base::DictionaryValue dictionary_;

  DISALLOW_COPY_AND_ASSIGN(JSONFeatureProviderSource);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_JSON_FEATURE_PROVIDER_SOURCE_H_
