// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_feature_provider.h"

#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "grit/common_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const bool kAllowTrailingComma = false;

struct Static {
  Static() {
    std::string manifest_features =
        ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_EXTENSION_MANIFEST_FEATURES).as_string();
    int error_code = 0;
    std::string error_message;
    Value* value = base::JSONReader::ReadAndReturnError(
        manifest_features, kAllowTrailingComma, &error_code, &error_message);
    CHECK(value) << error_message;
    CHECK(value->IsType(Value::TYPE_DICTIONARY));
    scoped_ptr<DictionaryValue> dictionary_value(
        static_cast<DictionaryValue*>(value));
    provider.reset(new extensions::ManifestFeatureProvider(
        dictionary_value.Pass()));
  }

  scoped_ptr<extensions::ManifestFeatureProvider> provider;
};

base::LazyInstance<Static> g_static = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace extensions {

ManifestFeatureProvider::ManifestFeatureProvider(
    scoped_ptr<DictionaryValue> root)
    : root_(root.release()) {
}

ManifestFeatureProvider::~ManifestFeatureProvider() {
}

// static
ManifestFeatureProvider* ManifestFeatureProvider::GetDefaultInstance() {
  return g_static.Get().provider.get();
}

std::set<std::string> ManifestFeatureProvider::GetAllFeatureNames() const {
  std::set<std::string> result;
  for (DictionaryValue::key_iterator iter = root_->begin_keys();
       iter != root_->end_keys(); ++iter) {
    result.insert(*iter);
  }
  return result;
}

scoped_ptr<Feature> ManifestFeatureProvider::GetFeature(
    const std::string& name) const {
  scoped_ptr<Feature> feature;

  DictionaryValue* description = NULL;
  if (root_->GetDictionary(name, &description))
    feature = Feature::Parse(description);

  if (!feature.get()) {
    // Have to use DLOG here because this happens in a lot of unit tests that
    // use ancient compiled crx files with unknown keys.
    DLOG(ERROR) << name;
    return scoped_ptr<Feature>();
  }

  if (feature->extension_types()->empty()) {
    LOG(ERROR) << name << ": Manifest features must specify atleast one value "
               << "for extension_types.";
    return scoped_ptr<Feature>();
  }

  if (!feature->contexts()->empty()) {
    LOG(ERROR) << name << ": Manifest features do not support contexts.";
    return scoped_ptr<Feature>();
  }

  return feature.Pass();
}

}  // namespace
