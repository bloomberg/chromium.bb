// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/simple_feature_provider.h"

#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "chrome/common/extensions/manifest_feature.h"
#include "chrome/common/extensions/permission_feature.h"
#include "grit/common_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

namespace {

const bool kAllowTrailingComma = false;

template<class FeatureClass>
Feature* CreateFeature() {
  return new FeatureClass();
}

struct Static {
  Static()
      : manifest_features(
            LoadProvider("manifest",
                         &CreateFeature<ManifestFeature>,
                         IDR_EXTENSION_MANIFEST_FEATURES)),
        permission_features(
            LoadProvider("permissions",
                         &CreateFeature<PermissionFeature>,
                         IDR_EXTENSION_PERMISSION_FEATURES)) {
  }

  scoped_ptr<SimpleFeatureProvider> manifest_features;
  scoped_ptr<SimpleFeatureProvider> permission_features;

 private:
  scoped_ptr<SimpleFeatureProvider> LoadProvider(
      const std::string& debug_string,
      SimpleFeatureProvider::FeatureFactory factory,
      int resource_id) {
    std::string manifest_features =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            resource_id).as_string();
    int error_code = 0;
    std::string error_message;
    Value* value = base::JSONReader::ReadAndReturnError(
        manifest_features, kAllowTrailingComma, &error_code, &error_message);
    CHECK(value) << "Could not load features: " << debug_string << " "
                 << error_message;
    CHECK(value->IsType(Value::TYPE_DICTIONARY)) << debug_string;
    scoped_ptr<DictionaryValue> dictionary_value(
        static_cast<DictionaryValue*>(value));
    return scoped_ptr<SimpleFeatureProvider>(
        new SimpleFeatureProvider(dictionary_value.Pass(), factory));
  }
};

base::LazyInstance<Static> g_static = LAZY_INSTANCE_INITIALIZER;

}  // namespace

SimpleFeatureProvider::SimpleFeatureProvider(scoped_ptr<DictionaryValue> root,
                                             FeatureFactory factory)
    : root_(root.release()),
      factory_(factory ? factory :
               static_cast<FeatureFactory>(&CreateFeature<Feature>)) {
}

SimpleFeatureProvider::~SimpleFeatureProvider() {
}

// static
SimpleFeatureProvider* SimpleFeatureProvider::GetManifestFeatures() {
  return g_static.Get().manifest_features.get();
}

// static
SimpleFeatureProvider* SimpleFeatureProvider::GetPermissionFeatures() {
  return g_static.Get().permission_features.get();
}

std::set<std::string> SimpleFeatureProvider::GetAllFeatureNames() const {
  std::set<std::string> result;
  for (DictionaryValue::key_iterator iter = root_->begin_keys();
       iter != root_->end_keys(); ++iter) {
    result.insert(*iter);
  }
  return result;
}

scoped_ptr<Feature> SimpleFeatureProvider::GetFeature(const std::string& name) {
  DictionaryValue* description = NULL;
  if (!root_->GetDictionary(name, &description)) {
    LOG(ERROR) << name << ": Definition not found.";
    return scoped_ptr<Feature>();
  }

  scoped_ptr<Feature> feature(new Feature());
  feature.reset((*factory_)());
  feature->set_name(name);
  feature->Parse(description);

  if (feature->extension_types()->empty()) {
    LOG(ERROR) << name << ": Simple features must specify atleast one value "
               << "for extension_types.";
    return scoped_ptr<Feature>();
  }

  if (!feature->contexts()->empty()) {
    LOG(ERROR) << name << ": Simple features do not support contexts.";
    return scoped_ptr<Feature>();
  }

  return feature.Pass();
}

}  // namespace
