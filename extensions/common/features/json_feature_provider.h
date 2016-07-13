// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_JSON_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_JSON_FEATURE_PROVIDER_H_

#include "extensions/common/features/base_feature_provider.h"

namespace base {
class DictionaryValue;
}

namespace extensions {
class SimpleFeature;

// Reads Features out of a simple JSON file description.
class JSONFeatureProvider : public BaseFeatureProvider {
 public:
  typedef SimpleFeature* (*FeatureFactory)();

  // Creates a new JSONFeatureProvider. Pass null to |factory| to have the
  // provider create plain old Feature instances.
  JSONFeatureProvider(const base::DictionaryValue& root,
                      FeatureFactory factory);
  ~JSONFeatureProvider() override;

 private:
  FeatureFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(JSONFeatureProvider);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_JSON_FEATURE_PROVIDER_H_
