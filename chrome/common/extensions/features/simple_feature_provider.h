// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_SIMPLE_FEATURE_PROVIDER_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_SIMPLE_FEATURE_PROVIDER_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/features/feature_provider.h"

namespace extensions {

// Reads Features out of a simple JSON file description.
class SimpleFeatureProvider : public FeatureProvider {
 public:
  typedef Feature*(*FeatureFactory)();

  // Creates a new SimpleFeatureProvider. Pass null to |factory| to have the
  // provider create plain old Feature instances.
  SimpleFeatureProvider(DictionaryValue* root, FeatureFactory factory);
  virtual ~SimpleFeatureProvider();

  // Gets an instance for the _manifest_features.json file that is baked into
  // Chrome as a resource.
  static SimpleFeatureProvider* GetManifestFeatures();

  // Gets an instance for the _permission_features.json file that is baked into
  // Chrome as a resource.
  static SimpleFeatureProvider* GetPermissionFeatures();

  // Returns all features described by this instance.
  std::set<std::string> GetAllFeatureNames() const;

  // Gets the feature |feature_name|, if it exists.
  virtual Feature* GetFeature(const std::string& feature_name) OVERRIDE;

 private:
  typedef std::map<std::string, linked_ptr<Feature> > FeatureMap;
  FeatureMap features_;

  FeatureFactory factory_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_SIMPLE_FEATURE_PROVIDER_H_
