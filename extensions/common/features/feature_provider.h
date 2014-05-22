// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_

#include <string>
#include <vector>

namespace extensions {

class Feature;

// Implemented by classes that can vend features.
class FeatureProvider {
 public:
  FeatureProvider() {}
  virtual ~FeatureProvider() {}

  // Returns the feature with the specified name.
  virtual Feature* GetFeature(const std::string& name) const = 0;

  // Returns the parent feature of |feature|, or NULL if there isn't one.
  virtual Feature* GetParent(Feature* feature) const = 0;

  // Returns the features inside the |parent| namespace, recursively.
  virtual std::vector<Feature*> GetChildren(const Feature& parent) const = 0;

  // Returns all features described by this instance, in asciibetical order.
  virtual const std::vector<std::string>& GetAllFeatureNames() const = 0;

  // Gets a feature provider for a specific feature type, like "permission".
  static const FeatureProvider* GetByName(const std::string& name);

  // Directly access the common feature types.
  static const FeatureProvider* GetAPIFeatures();
  static const FeatureProvider* GetManifestFeatures();
  static const FeatureProvider* GetPermissionFeatures();
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_PROVIDER_H_
