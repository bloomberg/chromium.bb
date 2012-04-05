// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/simple_feature_provider.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {

Manifest::Manifest(Extension::Location location,
                   scoped_ptr<DictionaryValue> value)
    : location_(location), value_(value.Pass()) {
}

Manifest::~Manifest() {
}

bool Manifest::ValidateManifest(string16* error) const {
  for (DictionaryValue::key_iterator key = value_->begin_keys();
       key != value_->end_keys(); ++key) {
    Feature* feature =
        SimpleFeatureProvider::GetManifestFeatures()->GetFeature(*key);
    if (!feature) {
      // When validating the extension manifests, we ignore keys that are not
      // recognized for forward compatibility.
      // TODO(aa): Consider having an error here in the case of strict error
      // checking to let developers know when they screw up.
      continue;
    }

    Feature::Availability result = feature->IsAvailableToManifest(
        extension_id_, GetType(), Feature::ConvertLocation(location_),
        GetManifestVersion());
    if (result != Feature::IS_AVAILABLE) {
      *error = ExtensionErrorUtils::FormatErrorMessageUTF16(
          errors::kFeatureNotAllowed,
          *key,
          feature->GetErrorMessage(result));
      return false;
    }
  }

  return true;
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

Extension::Type Manifest::GetType() const {
  if (value_->HasKey(keys::kTheme))
    return Extension::TYPE_THEME;
  bool is_platform_app = false;
  if (value_->GetBoolean(keys::kPlatformApp, &is_platform_app) &&
      is_platform_app)
    return Extension::TYPE_PLATFORM_APP;
  if (value_->HasKey(keys::kApp)) {
    if (value_->Get(keys::kWebURLs, NULL) ||
        value_->Get(keys::kLaunchWebURL, NULL))
      return Extension::TYPE_HOSTED_APP;
    else
      return Extension::TYPE_PACKAGED_APP;
  } else {
    return Extension::TYPE_EXTENSION;
  }
}

bool Manifest::IsTheme() const {
  return GetType() == Extension::TYPE_THEME;
}

bool Manifest::IsPlatformApp() const {
  return GetType() == Extension::TYPE_PLATFORM_APP;
}

bool Manifest::IsPackagedApp() const {
  return GetType() == Extension::TYPE_PACKAGED_APP;
}

bool Manifest::IsHostedApp() const {
  return GetType() == Extension::TYPE_HOSTED_APP;
}

bool Manifest::CanAccessPath(const std::string& path) const {
  std::vector<std::string> components;
  base::SplitString(path, '.', &components);
  return CanAccessKey(components[0]);
}

bool Manifest::CanAccessKey(const std::string& key) const {
  Feature* feature =
      SimpleFeatureProvider::GetManifestFeatures()->GetFeature(key);
  if (!feature)
    return false;

  return Feature::IS_AVAILABLE == feature->IsAvailableToManifest(
      extension_id_, GetType(), Feature::ConvertLocation(location_),
      GetManifestVersion());
}

}  // namespace extensions
