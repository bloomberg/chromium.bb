// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/simple_feature_provider.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {

Manifest::Manifest(Extension::Location location,
                   scoped_ptr<DictionaryValue> value)
    : location_(location),
      value_(value.Pass()),
      type_(Extension::TYPE_UNKNOWN) {
  if (value_->HasKey(keys::kTheme)) {
    type_ = Extension::TYPE_THEME;
  } else {
    bool is_platform_app = false;
    if (value_->GetBoolean(keys::kPlatformApp, &is_platform_app) &&
        is_platform_app) {
      type_ = Extension::TYPE_PLATFORM_APP;
    } else if (value_->HasKey(keys::kApp)) {
      if (value_->Get(keys::kWebURLs, NULL) ||
          value_->Get(keys::kLaunchWebURL, NULL)) {
        type_ = Extension::TYPE_HOSTED_APP;
      } else {
        type_ = Extension::TYPE_PACKAGED_APP;
      }
    } else {
      type_ = Extension::TYPE_EXTENSION;
    }
  }
  CHECK_NE(type_, Extension::TYPE_UNKNOWN);
}

Manifest::~Manifest() {
}

void Manifest::ValidateManifest(std::string* error,
                                std::vector<std::string>* warnings) const {
  *error = "";
  if (type_ == Extension::TYPE_PLATFORM_APP && GetManifestVersion() < 2) {
    *error = errors::kPlatformAppNeedsManifestVersion2;
    return;
  }

  // Check every feature to see if its in the manifest. Note that this means
  // we will ignore keys that are not features; we do this for forward
  // compatibility.
  // TODO(aa): Consider having an error here in the case of strict error
  // checking to let developers know when they screw up.

  std::set<std::string> feature_names =
      SimpleFeatureProvider::GetManifestFeatures()->GetAllFeatureNames();
  for (std::set<std::string>::iterator feature_name = feature_names.begin();
       feature_name != feature_names.end(); ++feature_name) {
    // Use Get instead of HasKey because the former uses path expansion.
    if (!value_->Get(*feature_name, NULL))
      continue;

    Feature* feature =
        SimpleFeatureProvider::GetManifestFeatures()->GetFeature(*feature_name);
    Feature::Availability result = feature->IsAvailableToManifest(
        extension_id_, type_, Feature::ConvertLocation(location_),
        GetManifestVersion());
    if (result != Feature::IS_AVAILABLE)
      warnings->push_back(feature->GetErrorMessage(result));
  }

  // Also generate warnings for keys that are not features.
  for (DictionaryValue::key_iterator key = value_->begin_keys();
      key != value_->end_keys(); ++key) {
    if (!SimpleFeatureProvider::GetManifestFeatures()->GetFeature(*key)) {
      warnings->push_back(base::StringPrintf("Unrecognized manifest key '%s'.",
          (*key).c_str()));
    }
  }
}

bool Manifest::HasKey(const std::string& key) const {
  return CanAccessKey(key) && value_->HasKey(key);
}

bool Manifest::Get(
    const std::string& path, Value** out_value) const {
  return CanAccessPath(path) && value_->Get(path, out_value);
}

bool Manifest::GetBoolean(
    const std::string& path, bool* out_value) const {
  return CanAccessPath(path) && value_->GetBoolean(path, out_value);
}

bool Manifest::GetInteger(
    const std::string& path, int* out_value) const {
  return CanAccessPath(path) && value_->GetInteger(path, out_value);
}

bool Manifest::GetString(
    const std::string& path, std::string* out_value) const {
  return CanAccessPath(path) && value_->GetString(path, out_value);
}

bool Manifest::GetString(
    const std::string& path, string16* out_value) const {
  return CanAccessPath(path) && value_->GetString(path, out_value);
}

bool Manifest::GetDictionary(
    const std::string& path, DictionaryValue** out_value) const {
  return CanAccessPath(path) && value_->GetDictionary(path, out_value);
}

bool Manifest::GetList(
    const std::string& path, ListValue** out_value) const {
  return CanAccessPath(path) && value_->GetList(path, out_value);
}

Manifest* Manifest::DeepCopy() const {
  Manifest* manifest = new Manifest(
      location_, scoped_ptr<DictionaryValue>(value_->DeepCopy()));
  manifest->set_extension_id(extension_id_);
  return manifest;
}

bool Manifest::Equals(const Manifest* other) const {
  return other && value_->Equals(other->value());
}

int Manifest::GetManifestVersion() const {
  int manifest_version = 1;  // default to version 1 if no version is specified
  value_->GetInteger(keys::kManifestVersion, &manifest_version);
  return manifest_version;
}

bool Manifest::CanAccessPath(const std::string& path) const {
  std::vector<std::string> components;
  base::SplitString(path, '.', &components);
  std::string key;
  for (size_t i = 0; i < components.size(); ++i) {
    key += components[i];
    if (!CanAccessKey(key))
      return false;
    key += '.';
  }
  return true;
}

bool Manifest::CanAccessKey(const std::string& key) const {
  Feature* feature =
      SimpleFeatureProvider::GetManifestFeatures()->GetFeature(key);
  if (!feature)
    return true;

  return Feature::IS_AVAILABLE == feature->IsAvailableToManifest(
      extension_id_, type_, Feature::ConvertLocation(location_),
      GetManifestVersion());
}

}  // namespace extensions
