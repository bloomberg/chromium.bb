// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/base_feature_provider.h"

#include <stack>

#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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

  virtual Feature* GetParent(Feature* feature) OVERRIDE {
    return GetBaseFeatureProvider()->GetParent(feature);
  }

  virtual const std::vector<std::string>& GetAllFeatureNames() OVERRIDE {
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
    scoped_ptr<base::Value> value(base::JSONReader::ReadAndReturnError(
        features_file, base::JSON_PARSE_RFC,
        &error_code, &error_message));
    DCHECK(value) << "Could not load features: " << name_ << " "
        << error_message;
    scoped_ptr<base::DictionaryValue> value_as_dict;
    if (value) {
      CHECK(value->IsType(base::Value::TYPE_DICTIONARY)) << name_;
      value_as_dict.reset(static_cast<base::DictionaryValue*>(value.release()));
    } else {
      // http://crbug.com/176381
      value_as_dict.reset(new base::DictionaryValue());
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

bool ParseFeature(const base::DictionaryValue* value,
                  const std::string& name,
                  SimpleFeature* feature) {
  feature->set_name(name);
  std::string error = feature->Parse(value);
  if (!error.empty())
    LOG(ERROR) << error;
  return error.empty();
}

}  // namespace

BaseFeatureProvider::BaseFeatureProvider(const base::DictionaryValue& root,
                                         FeatureFactory factory)
    : factory_(factory ? factory :
               static_cast<FeatureFactory>(&CreateFeature<SimpleFeature>)) {
  for (base::DictionaryValue::Iterator iter(root); !iter.IsAtEnd();
       iter.Advance()) {
    if (iter.value().GetType() == base::Value::TYPE_DICTIONARY) {
      linked_ptr<SimpleFeature> feature((*factory_)());

      std::vector<std::string> split;
      base::SplitString(iter.key(), '.', &split);

      // Push parent features on the stack, starting with the current feature.
      // If one of the features has "noparent" set, stop pushing features on
      // the stack. The features will then be parsed in order, starting with
      // the farthest parent that is either top level or has "noparent" set.
      std::stack<std::pair<std::string, const base::DictionaryValue*> >
          parse_stack;
      while (!split.empty()) {
        std::string parent_name = JoinString(split, '.');
        split.pop_back();
        if (root.HasKey(parent_name)) {
          const base::DictionaryValue* parent = NULL;
          CHECK(root.GetDictionaryWithoutPathExpansion(parent_name, &parent));
          parse_stack.push(std::make_pair(parent_name, parent));
          bool no_parent = false;
          parent->GetBoolean("noparent", &no_parent);
          if (no_parent)
            break;
        }
      }

      CHECK(!parse_stack.empty());
      // Parse all parent features.
      bool parse_error = false;
      while (!parse_stack.empty()) {
        if (!ParseFeature(parse_stack.top().second,
                          parse_stack.top().first,
                          feature.get())) {
          parse_error = true;
          break;
        }
        parse_stack.pop();
      }

      if (parse_error)
        continue;

      features_[iter.key()] = feature;
    } else if (iter.value().GetType() == base::Value::TYPE_LIST) {
      // This is a complex feature.
      const base::ListValue* list =
          static_cast<const base::ListValue*>(&iter.value());
      CHECK_GT(list->GetSize(), 0UL);

      scoped_ptr<ComplexFeature::FeatureList> features(
          new ComplexFeature::FeatureList());

      // Parse and add all SimpleFeatures from the list.
      for (base::ListValue::const_iterator list_iter = list->begin();
           list_iter != list->end(); ++list_iter) {
        if ((*list_iter)->GetType() != base::Value::TYPE_DICTIONARY) {
          LOG(ERROR) << iter.key() << ": Feature rules must be dictionaries.";
          continue;
        }

        scoped_ptr<SimpleFeature> feature((*factory_)());
        if (!ParseFeature(static_cast<const base::DictionaryValue*>(*list_iter),
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

const std::vector<std::string>& BaseFeatureProvider::GetAllFeatureNames() {
  if (feature_names_.empty()) {
    for (FeatureMap::const_iterator iter = features_.begin();
         iter != features_.end(); ++iter) {
      feature_names_.push_back(iter->first);
    }
  }
  return feature_names_;
}

Feature* BaseFeatureProvider::GetFeature(const std::string& name) {
  FeatureMap::iterator iter = features_.find(name);
  if (iter != features_.end())
    return iter->second.get();
  else
    return NULL;
}

Feature* BaseFeatureProvider::GetParent(Feature* feature) {
  CHECK(feature);
  if (feature->no_parent())
    return NULL;

  std::vector<std::string> split;
  base::SplitString(feature->name(), '.', &split);
  if (split.size() < 2)
    return NULL;
  split.pop_back();
  return GetFeature(JoinString(split, '.'));
}

} // namespace extensions
