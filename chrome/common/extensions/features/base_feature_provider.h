// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_FEATURES_BASE_FEATURE_PROVIDER_H_
#define CHROME_COMMON_EXTENSIONS_FEATURES_BASE_FEATURE_PROVIDER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/linked_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/features/feature_provider.h"
#include "chrome/common/extensions/features/simple_feature.h"

namespace extensions {

// Reads Features out of a simple JSON file description.
class BaseFeatureProvider : public FeatureProvider {
 public:
  typedef SimpleFeature*(*FeatureFactory)();

  // Creates a new BaseFeatureProvider. Pass null to |factory| to have the
  // provider create plain old Feature instances.
  BaseFeatureProvider(const DictionaryValue& root, FeatureFactory factory);
  virtual ~BaseFeatureProvider();

  // Gets a feature provider for a specific feature type, like "permission".
  static FeatureProvider* GetByName(const std::string& name);

  static FeatureProvider* GetPermissionFeatures() {
    return GetByName("permission");
  }

  // Gets the feature |feature_name|, if it exists.
  virtual Feature* GetFeature(const std::string& feature_name) OVERRIDE;

  virtual std::set<std::string> GetAllFeatureNames() OVERRIDE;

 private:
  typedef std::map<std::string, linked_ptr<Feature> > FeatureMap;
  FeatureMap features_;

  FeatureFactory factory_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_FEATURES_BASE_FEATURE_PROVIDER_H_
