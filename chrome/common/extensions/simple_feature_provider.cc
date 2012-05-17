// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/simple_feature_provider.h"

#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "chrome/common/extensions/manifest_feature.h"
#include "chrome/common/extensions/permission_feature.h"
#include "grit/common_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

namespace {

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
            resource_id, ui::SCALE_FACTOR_NONE).as_string();
    int error_code = 0;
    std::string error_message;
    Value* value = base::JSONReader::ReadAndReturnError(
        manifest_features, base::JSON_PARSE_RFC,
        &error_code, &error_message);
    CHECK(value) << "Could not load features: " << debug_string << " "
                 << error_message;
    CHECK(value->IsType(Value::TYPE_DICTIONARY)) << debug_string;
    scoped_ptr<DictionaryValue> dictionary_value(
        static_cast<DictionaryValue*>(value));
    return scoped_ptr<SimpleFeatureProvider>(
        new SimpleFeatureProvider(dictionary_value.get(), factory));
  }
};

base::LazyInstance<Static> g_static = LAZY_INSTANCE_INITIALIZER;

}  // namespace

SimpleFeatureProvider::SimpleFeatureProvider(DictionaryValue* root,
                                             FeatureFactory factory)
    : factory_(factory ? factory :
               static_cast<FeatureFactory>(&CreateFeature<Feature>)) {
  for (DictionaryValue::Iterator iter(*root); iter.HasNext(); iter.Advance()) {
    if (iter.value().GetType() != Value::TYPE_DICTIONARY) {
      LOG(ERROR) << iter.key() << ": Feature description must be dictionary.";
      continue;
    }

    linked_ptr<Feature> feature((*factory_)());
    feature->set_name(iter.key());
    feature->Parse(static_cast<const DictionaryValue*>(&iter.value()));

    if (feature->extension_types()->empty()) {
      LOG(ERROR) << iter.key() << ": Simple features must specify atleast one "
                 << "value for extension_types.";
      continue;
    }

    if (!feature->contexts()->empty()) {
      LOG(ERROR) << iter.key() << ": Simple features do not support contexts.";
      continue;
    }

    features_[iter.key()] = feature;
  }
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
  for (FeatureMap::const_iterator iter = features_.begin();
       iter != features_.end(); ++iter) {
    result.insert(iter->first);
  }
  return result;
}

Feature* SimpleFeatureProvider::GetFeature(const std::string& name) {
  FeatureMap::iterator iter = features_.find(name);
  if (iter != features_.end())
    return iter->second.get();
  else
    return NULL;
}

}  // namespace
