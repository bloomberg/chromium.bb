// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_
#define EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "base/values.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"

namespace extensions {

// Reads Features out of a simple JSON file description.
class BaseFeatureProvider : public FeatureProvider {
 public:
  typedef SimpleFeature*(*FeatureFactory)();

  // Creates a new BaseFeatureProvider. Pass null to |factory| to have the
  // provider create plain old Feature instances.
  BaseFeatureProvider(const base::DictionaryValue& root,
                      FeatureFactory factory);
  virtual ~BaseFeatureProvider();

  // Gets the feature |feature_name|, if it exists.
  virtual Feature* GetFeature(const std::string& feature_name) const OVERRIDE;
  virtual Feature* GetParent(Feature* feature) const OVERRIDE;
  virtual std::vector<Feature*> GetChildren(const Feature& parent) const
      OVERRIDE;

  virtual const std::vector<std::string>& GetAllFeatureNames() const OVERRIDE;

 private:
  typedef std::map<std::string, linked_ptr<Feature> > FeatureMap;
  FeatureMap features_;

  // Populated on first use.
  mutable std::vector<std::string> feature_names_;

  FeatureFactory factory_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_BASE_FEATURE_PROVIDER_H_
