// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_FEATURE_PROVIDER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_FEATURE_PROVIDER_H_
#pragma once

#include <set>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/feature.h"

namespace extensions {

// Wrapper for the _manifest_features.json file, which vends Feature objects
// describing the features declared there.
class ManifestFeatureProvider {
 public:
  // Create an instance for an arbitrary hunk of JSON. This is typically used
  // during tests.
  explicit ManifestFeatureProvider(scoped_ptr<DictionaryValue> root);
  ~ManifestFeatureProvider();

  // Gets an instance for the _manifest_features.json file that is baked into
  // Chrome as a resource.
  static ManifestFeatureProvider* GetDefaultInstance();

  // Returns all features described by this instance.
  std::set<std::string> GetAllFeatureNames() const;

  // Gets the feature |feature_name|, if it exists.
  scoped_ptr<Feature> GetFeature(const std::string& feature_name) const;

 private:
  scoped_ptr<DictionaryValue> root_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_FEATURE_PROVIDER_H_
