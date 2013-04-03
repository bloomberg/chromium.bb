// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/base_feature_provider.h"

#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "chrome/common/extensions/features/api_feature.h"
#include "chrome/common/extensions/features/complex_feature.h"
#include "chrome/common/extensions/features/manifest_feature.h"
#include "chrome/common/extensions/features/permission_feature.h"
#include "grit/common_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

namespace {

template<class FeatureClass>
SimpleFeature* CreateFeature() {
  return new FeatureClass();
}

struct Static {
  Static()
      : api_features(
            LoadProvider("api",
                         &CreateFeature<APIFeature>,
                         IDR_EXTENSION_API_FEATURES)),
        manifest_features(
            LoadProvider("manifest",
                         &CreateFeature<ManifestFeature>,
                         IDR_EXTENSION_MANIFEST_FEATURES)),
        permission_features(
            LoadProvider("permissions",
                         &CreateFeature<PermissionFeature>,
                         IDR_EXTENSION_PERMISSION_FEATURES)) {
  }

  scoped_ptr<BaseFeatureProvider> api_features;
  scoped_ptr<BaseFeatureProvider> manifest_features;
  scoped_ptr<BaseFeatureProvider> permission_features;

 private:
  scoped_ptr<BaseFeatureProvider> LoadProvider(
      const std::string& debug_string,
      BaseFeatureProvider::FeatureFactory factory,
      int resource_id) {
    const std::string& features_file =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            resource_id).as_string();
    int error_code = 0;
    std::string error_message;
    scoped_ptr<Value> value(base::JSONReader::ReadAndReturnError(
        features_file, base::JSON_PARSE_RFC,
        &error_code, &error_message));
    DCHECK(value) << "Could not load features: " << debug_string << " "
                  << error_message;
    scoped_ptr<DictionaryValue> value_as_dict;
    if (value) {
      CHECK(value->IsType(Value::TYPE_DICTIONARY)) << debug_string;
      value_as_dict.reset(static_cast<DictionaryValue*>(value.release()));
    } else {
      // http://crbug.com/176381
      value_as_dict.reset(new DictionaryValue());
    }
    return make_scoped_ptr(new BaseFeatureProvider(*value_as_dict, factory));
  }
};

bool ParseFeature(const DictionaryValue* value,
                  const std::string& name,
                  SimpleFeature* feature) {
  feature->set_name(name);
  std::string error = feature->Parse(value);
  if (!error.empty())
    LOG(ERROR) << error;
  return error.empty();
}

base::LazyInstance<Static> g_static = LAZY_INSTANCE_INITIALIZER;

}  // namespace

BaseFeatureProvider::BaseFeatureProvider(const DictionaryValue& root,
                                         FeatureFactory factory)
    : factory_(factory ? factory :
               static_cast<FeatureFactory>(&CreateFeature<SimpleFeature>)) {
  for (DictionaryValue::Iterator iter(root); iter.HasNext(); iter.Advance()) {
    if (iter.value().GetType() == Value::TYPE_DICTIONARY) {
      linked_ptr<SimpleFeature> feature((*factory_)());

      if (!ParseFeature(static_cast<const DictionaryValue*>(&iter.value()),
                        iter.key(),
                        feature.get()))
        continue;

      features_[iter.key()] = feature;
    } else if (iter.value().GetType() == Value::TYPE_LIST) {
      // This is a complex feature.
      const ListValue* list = static_cast<const ListValue*>(&iter.value());
      CHECK_GT(list->GetSize(), 0UL);

      scoped_ptr<ComplexFeature::FeatureList> features(
          new ComplexFeature::FeatureList());

      // Parse and add all SimpleFeatures from the list.
      for (ListValue::const_iterator list_iter = list->begin();
           list_iter != list->end(); ++list_iter) {
        if ((*list_iter)->GetType() != Value::TYPE_DICTIONARY) {
          LOG(ERROR) << iter.key() << ": Feature rules must be dictionaries.";
          continue;
        }

        scoped_ptr<SimpleFeature> feature((*factory_)());
        if (!ParseFeature(static_cast<const DictionaryValue*>(*list_iter),
                          iter.key(),
                          feature.get()))
          continue;

        features->push_back(feature.release());
      }

      linked_ptr<ComplexFeature> feature(new ComplexFeature(features.Pass()));
      feature->set_name(iter.key());

      features_[iter.key()] = feature;
    } else {
      LOG(ERROR) << iter.key() << ": Feature description must be dictionary or"
                 << " list of dictionaries.";
    }
  }
}

BaseFeatureProvider::~BaseFeatureProvider() {
}

// static
BaseFeatureProvider* BaseFeatureProvider::GetAPIFeatures() {
  return g_static.Get().api_features.get();
}

// static
BaseFeatureProvider* BaseFeatureProvider::GetManifestFeatures() {
  return g_static.Get().manifest_features.get();
}

// static
BaseFeatureProvider* BaseFeatureProvider::GetPermissionFeatures() {
  return g_static.Get().permission_features.get();
}

std::set<std::string> BaseFeatureProvider::GetAllFeatureNames() const {
  std::set<std::string> result;
  for (FeatureMap::const_iterator iter = features_.begin();
       iter != features_.end(); ++iter) {
    result.insert(iter->first);
  }
  return result;
}

Feature* BaseFeatureProvider::GetFeature(const std::string& name) {
  FeatureMap::iterator iter = features_.find(name);
  if (iter != features_.end())
    return iter->second.get();
  else
    return NULL;
}

}  // namespace
