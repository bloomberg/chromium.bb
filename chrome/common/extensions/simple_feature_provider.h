// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_SIMPLE_FEATURE_PROVIDER_H_
#define CHROME_COMMON_EXTENSIONS_SIMPLE_FEATURE_PROVIDER_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/feature.h"

namespace extensions {

// Reads Features out of a simple JSON file description.
class SimpleFeatureProvider {
 public:
  // Create an instance for an arbitrary hunk of JSON. This is typically used
  // during tests.
  explicit SimpleFeatureProvider(scoped_ptr<DictionaryValue> root);
  ~SimpleFeatureProvider();

  // Gets an instance for the _manifest_features.json file that is baked into
  // Chrome as a resource.
  static SimpleFeatureProvider* GetManifestFeatures();

  // Gets an instance for the _permission_features.json file that is baked into
  // Chrome as a resource.
  static SimpleFeatureProvider* GetPermissionFeatures();

  // Returns all features described by this instance.
  std::set<std::string> GetAllFeatureNames() const;

  // Gets the feature |feature_name|, if it exists.
  scoped_ptr<Feature> GetFeature(const std::string& feature_name) const;

 private:
  scoped_ptr<DictionaryValue> root_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_SIMPLE_FEATURE_PROVIDER_H_
