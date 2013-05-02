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

class LazyFeatureProvider : public FeatureProvider {
 public:
  LazyFeatureProvider(const std::string& name,
                      BaseFeatureProvider::FeatureFactory factory,
                      int resource_id)
      : name_(name),
        factory_(factory),
        resource_id_(resource_id) {
  }

  virtual Feature* GetFeature(const std::string& name) OVERRIDE {
    return GetBaseFeatureProvider()->GetFeature(name);
  }

  virtual std::set<std::string> GetAllFeatureNames() OVERRIDE {
    return GetBaseFeatureProvider()->GetAllFeatureNames();
  }

 private:
  BaseFeatureProvider* GetBaseFeatureProvider() {
    if (!features_)
      features_ = LoadProvider();
    return features_.get();
  }

  scoped_ptr<BaseFeatureProvider> LoadProvider() {
    const std::string& features_file =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            resource_id_).as_string();
    int error_code = 0;
    std::string error_message;
    scoped_ptr<Value> value(base::JSONReader::ReadAndReturnError(
        features_file, base::JSON_PARSE_RFC,
        &error_code, &error_message));
    DCHECK(value) << "Could not load features: " << name_ << " "
        << error_message;
    scoped_ptr<DictionaryValue> value_as_dict;
    if (value) {
      CHECK(value->IsType(Value::TYPE_DICTIONARY)) << name_;
      value_as_dict.reset(static_cast<DictionaryValue*>(value.release()));
    } else {
      // http://crbug.com/176381
      value_as_dict.reset(new DictionaryValue());
    }
    return make_scoped_ptr(new BaseFeatureProvider(*value_as_dict, factory_));
  }

  std::string name_;
  BaseFeatureProvider::FeatureFactory factory_;
  int resource_id_;
  scoped_ptr<BaseFeatureProvider> features_;
};

struct Static {
  Static() {
    lazy_feature_providers["api"] = make_linked_ptr(
        new LazyFeatureProvider("api",
                                &CreateFeature<APIFeature>,
                                IDR_EXTENSION_API_FEATURES));
    lazy_feature_providers["permission"] = make_linked_ptr(
        new LazyFeatureProvider("permission",
                                &CreateFeature<PermissionFeature>,
                                IDR_EXTENSION_PERMISSION_FEATURES));
    lazy_feature_providers["manifest"] = make_linked_ptr(
        new LazyFeatureProvider("manifest",
                                &CreateFeature<ManifestFeature>,
                                IDR_EXTENSION_MANIFEST_FEATURES));
  }

  typedef std::map<std::string, linked_ptr<LazyFeatureProvider> >
      LazyFeatureProviderMap;

  LazyFeatureProvider* LazyGetFeatures(const std::string& name) {
    LazyFeatureProviderMap::iterator it = lazy_feature_providers.find(name);
    CHECK(it != lazy_feature_providers.end());
    return it->second.get();
  }

  LazyFeatureProviderMap lazy_feature_providers;
};

base::LazyInstance<Static> g_static = LAZY_INSTANCE_INITIALIZER;

bool ParseFeature(const DictionaryValue* value,
                  const std::string& name,
                  SimpleFeature* feature) {
  feature->set_name(name);
  std::string error = feature->Parse(value);
  if (!error.empty())
    LOG(ERROR) << error;
  return error.empty();
}

}  // namespace

BaseFeatureProvider::BaseFeatureProvider(const DictionaryValue& root,
                                         FeatureFactory factory)
    : factory_(factory ? factory :
               static_cast<FeatureFactory>(&CreateFeature<SimpleFeature>)) {
  for (DictionaryValue::Iterator iter(root); !iter.IsAtEnd(); iter.Advance()) {
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
FeatureProvider* BaseFeatureProvider::GetByName(
    const std::string& name) {
  return g_static.Get().LazyGetFeatures(name);
}

std::set<std::string> BaseFeatureProvider::GetAllFeatureNames() {
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
