// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/install_warning.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {

namespace {

// Rank extension locations in a way that allows
// Manifest::GetHigherPriorityLocation() to compare locations.
// An extension installed from two locations will have the location
// with the higher rank, as returned by this function. The actual
// integer values may change, and should never be persisted.
int GetLocationRank(Manifest::Location location) {
  const int kInvalidRank = -1;
  int rank = kInvalidRank;  // Will CHECK that rank is not kInvalidRank.

  switch (location) {
    // Component extensions can not be overriden by any other type.
    case Manifest::COMPONENT:
      rank = 7;
      break;

    // Policy controlled extensions may not be overridden by any type
    // that is not part of chrome.
    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
      rank = 6;
      break;

    // A developer-loaded extension should override any installed type
    // that a user can disable. Anything specified on the command-line should
    // override one loaded via the extensions UI.
    case Manifest::COMMAND_LINE:
      rank = 5;
      break;

    case Manifest::UNPACKED:
      rank = 4;
      break;

    // The relative priority of various external sources is not important,
    // but having some order ensures deterministic behavior.
    case Manifest::EXTERNAL_REGISTRY:
      rank = 3;
      break;

    case Manifest::EXTERNAL_PREF:
      rank = 2;
      break;

    case Manifest::EXTERNAL_PREF_DOWNLOAD:
      rank = 1;
      break;

    // User installed extensions are overridden by any external type.
    case Manifest::INTERNAL:
      rank = 0;
      break;

    default:
      NOTREACHED() << "Need to add new extension location " << location;
  }

  CHECK(rank != kInvalidRank);
  return rank;
}

}  // namespace

// static
Manifest::Location Manifest::GetHigherPriorityLocation(
    Location loc1, Location loc2) {
  if (loc1 == loc2)
    return loc1;

  int loc1_rank = GetLocationRank(loc1);
  int loc2_rank = GetLocationRank(loc2);

  // If two different locations have the same rank, then we can not
  // deterministicly choose a location.
  CHECK(loc1_rank != loc2_rank);

  // Highest rank has highest priority.
  return (loc1_rank > loc2_rank ? loc1 : loc2 );
}

Manifest::Manifest(Location location, scoped_ptr<base::DictionaryValue> value)
    : location_(location),
      value_(value.Pass()),
      type_(TYPE_UNKNOWN) {
  if (value_->HasKey(keys::kTheme)) {
    type_ = TYPE_THEME;
  } else if (value_->HasKey(keys::kApp)) {
    if (value_->Get(keys::kWebURLs, NULL) ||
        value_->Get(keys::kLaunchWebURL, NULL)) {
      type_ = TYPE_HOSTED_APP;
    } else if (value_->Get(keys::kPlatformAppBackground, NULL)) {
      type_ = TYPE_PLATFORM_APP;
    } else {
      type_ = TYPE_LEGACY_PACKAGED_APP;
    }
  } else {
    type_ = TYPE_EXTENSION;
  }
  CHECK_NE(type_, TYPE_UNKNOWN);
}

Manifest::~Manifest() {
}

void Manifest::ValidateManifest(
    std::string* error,
    std::vector<InstallWarning>* warnings) const {
  *error = "";
  if (type_ == Manifest::TYPE_PLATFORM_APP && GetManifestVersion() < 2) {
    *error = errors::kPlatformAppNeedsManifestVersion2;
    return;
  }

  // Check every feature to see if its in the manifest. Note that this means
  // we will ignore keys that are not features; we do this for forward
  // compatibility.
  // TODO(aa): Consider having an error here in the case of strict error
  // checking to let developers know when they screw up.

  std::set<std::string> feature_names =
      BaseFeatureProvider::GetManifestFeatures()->GetAllFeatureNames();
  for (std::set<std::string>::iterator feature_name = feature_names.begin();
       feature_name != feature_names.end(); ++feature_name) {
    // Use Get instead of HasKey because the former uses path expansion.
    if (!value_->Get(*feature_name, NULL))
      continue;

    Feature* feature =
        BaseFeatureProvider::GetManifestFeatures()->GetFeature(*feature_name);
    Feature::Availability result = feature->IsAvailableToManifest(
        extension_id_, type_, Feature::ConvertLocation(location_),
        GetManifestVersion());
    if (!result.is_available())
      warnings->push_back(InstallWarning(
          InstallWarning::FORMAT_TEXT, result.message()));
  }

  // Also generate warnings for keys that are not features.
  for (base::DictionaryValue::key_iterator key = value_->begin_keys();
      key != value_->end_keys(); ++key) {
    if (!BaseFeatureProvider::GetManifestFeatures()->GetFeature(*key)) {
      warnings->push_back(InstallWarning(
          InstallWarning::FORMAT_TEXT,
          base::StringPrintf("Unrecognized manifest key '%s'.",
                             (*key).c_str())));
    }
  }
}

bool Manifest::HasKey(const std::string& key) const {
  return CanAccessKey(key) && value_->HasKey(key);
}

bool Manifest::HasPath(const std::string& path) const {
  base::Value* ignored = NULL;
  return CanAccessPath(path) && value_->Get(path, &ignored);
}

bool Manifest::Get(
    const std::string& path, const base::Value** out_value) const {
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
    const std::string& path, const base::DictionaryValue** out_value) const {
  return CanAccessPath(path) && value_->GetDictionary(path, out_value);
}

bool Manifest::GetList(
    const std::string& path, const base::ListValue** out_value) const {
  return CanAccessPath(path) && value_->GetList(path, out_value);
}

Manifest* Manifest::DeepCopy() const {
  Manifest* manifest = new Manifest(
      location_, scoped_ptr<base::DictionaryValue>(value_->DeepCopy()));
  manifest->set_extension_id(extension_id_);
  return manifest;
}

bool Manifest::Equals(const Manifest* other) const {
  return other && value_->Equals(other->value());
}

int Manifest::GetManifestVersion() const {
  // Platform apps were launched after manifest version 2 was the preferred
  // version, so they default to that.
  int manifest_version = type_ == TYPE_PLATFORM_APP ? 2 : 1;
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
      BaseFeatureProvider::GetManifestFeatures()->GetFeature(key);
  if (!feature)
    return true;

  return feature->IsAvailableToManifest(
      extension_id_, type_, Feature::ConvertLocation(location_),
      GetManifestVersion()).is_available();
}

}  // namespace extensions
