// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest.h"

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {

namespace {

typedef std::map<std::string, int> RestrictionMap;

struct Restrictions {
  Restrictions() {
    // Base keys that all manifests can specify.
    map[keys::kName] = Manifest::kTypeAll;
    map[keys::kVersion] = Manifest::kTypeAll;
    map[keys::kManifestVersion] = Manifest::kTypeAll;
    map[keys::kDescription] = Manifest::kTypeAll;
    map[keys::kIcons] = Manifest::kTypeAll;
    map[keys::kCurrentLocale] = Manifest::kTypeAll;
    map[keys::kDefaultLocale] = Manifest::kTypeAll;
    map[keys::kSignature] = Manifest::kTypeAll;
    map[keys::kUpdateURL] = Manifest::kTypeAll;
    map[keys::kPublicKey] = Manifest::kTypeAll;

    // Type specific.
    map[keys::kApp] = Manifest::kTypeHostedApp | Manifest::kTypePackagedApp |
                      Manifest::kTypePlatformApp;
    map[keys::kTheme] = Manifest::kTypeTheme;

    // keys::kPlatformApp holds a boolean, so all types can define it.
    map[keys::kPlatformApp] = Manifest::kTypeAll;

    // Extensions only.
    map[keys::kBrowserAction] = Manifest::kTypeExtension;
    map[keys::kPageAction] = Manifest::kTypeExtension;
    map[keys::kPageActions] = Manifest::kTypeExtension;
    map[keys::kChromeURLOverrides] = Manifest::kTypeExtension;

    // Everything except themes.
    int all_but_themes = Manifest::kTypeAll - Manifest::kTypeTheme;
    map[keys::kPermissions] = all_but_themes;
    map[keys::kOptionalPermissions] = all_but_themes;
    map[keys::kOptionsPage] = all_but_themes;
    map[keys::kBackground] = all_but_themes;
    map[keys::kOfflineEnabled] = all_but_themes;
    map[keys::kMinimumChromeVersion] = all_but_themes;
    map[keys::kRequirements] = all_but_themes;
    map[keys::kConvertedFromUserScript] = all_but_themes;
    map[keys::kNaClModules] = all_but_themes;
    map[keys::kPlugins] = all_but_themes;

    // Extensions and packaged apps.
    int ext_and_packaged =
        Manifest::kTypeExtension | Manifest::kTypePackagedApp;
    map[keys::kContentScripts] = ext_and_packaged;
    map[keys::kOmnibox] = ext_and_packaged;
    map[keys::kDevToolsPage] = ext_and_packaged;
    map[keys::kSidebar] = ext_and_packaged;
    map[keys::kHomepageURL] = ext_and_packaged;

    // Extensions, packaged apps and platform apps.
    int local_apps_and_ext = ext_and_packaged | Manifest::kTypePlatformApp;
    map[keys::kContentSecurityPolicy] = local_apps_and_ext;
    map[keys::kFileBrowserHandlers] = local_apps_and_ext;
    map[keys::kIncognito] = local_apps_and_ext;
    map[keys::kInputComponents] = local_apps_and_ext;
    map[keys::kTtsEngine] = local_apps_and_ext;
    map[keys::kIntents] = local_apps_and_ext;
  }

  // Returns true if the |key| is recognized.
  bool IsKnownKey(const std::string& key) const {
    RestrictionMap::const_iterator i = map.find(key);
    return i != map.end();
  }

  // Returns true if the given |key| can be specified by the manifest |type|.
  bool CanAccessKey(const std::string& key, Manifest::Type type) const {
    RestrictionMap::const_iterator i = map.find(key);
    return (i != map.end() && (type & i->second) != 0);
  }

  RestrictionMap map;
};

base::LazyInstance<Restrictions> g_restrictions;

}  // namespace

// static
std::set<std::string> Manifest::GetAllKnownKeys() {
  std::set<std::string> keys;
  const RestrictionMap& map = g_restrictions.Get().map;
  for (RestrictionMap::const_iterator i = map.begin(); i != map.end(); i++)
    keys.insert(i->first);
  return keys;
}

Manifest::Manifest(DictionaryValue* value) : value_(value) {}
Manifest::~Manifest() {}

bool Manifest::ValidateManifest(std::string* error) const {
  Restrictions restrictions = g_restrictions.Get();
  Type type = GetType();

  for (DictionaryValue::key_iterator key = value_->begin_keys();
       key != value_->end_keys(); ++key) {
    // When validating the extension manifests, we ignore keys that are not
    // recognized for forward compatibility.
    if (!restrictions.IsKnownKey(*key)) {
      // TODO(aa): Consider having an error here in the case of strict error
      // checking to let developers know when they screw up.
      continue;
    }

    if (!restrictions.CanAccessKey(*key, type)) {
      *error = ExtensionErrorUtils::FormatErrorMessage(
          errors::kFeatureNotAllowed, *key);
      return false;
    }
  }

  return true;
}

bool Manifest::HasKey(const std::string& key) const {
  Restrictions restrictions = g_restrictions.Get();
  return restrictions.CanAccessKey(key, GetType()) && value_->HasKey(key);
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
  return new Manifest(value_->DeepCopy());
}

bool Manifest::Equals(const Manifest* other) const {
  return other && value_->Equals(other->value());
}

Manifest::Type Manifest::GetType() const {
  if (value_->HasKey(keys::kTheme))
    return kTypeTheme;
  bool is_platform_app = false;
  if (value_->GetBoolean(keys::kPlatformApp, &is_platform_app) &&
      is_platform_app)
    return kTypePlatformApp;
  if (value_->HasKey(keys::kApp)) {
    if (value_->Get(keys::kWebURLs, NULL) ||
        value_->Get(keys::kLaunchWebURL, NULL))
      return kTypeHostedApp;
    else
      return kTypePackagedApp;
  } else {
    return kTypeExtension;
  }
}

bool Manifest::IsTheme() const {
  return GetType() == kTypeTheme;
}

bool Manifest::IsPlatformApp() const {
  return GetType() == kTypePlatformApp;
}

bool Manifest::IsPackagedApp() const {
  return GetType() == kTypePackagedApp;
}

bool Manifest::IsHostedApp() const {
  return GetType() == kTypeHostedApp;
}

bool Manifest::CanAccessPath(const std::string& path) const {
  std::vector<std::string> components;
  base::SplitString(path, '.', &components);

  Restrictions restrictions = g_restrictions.Get();
  return restrictions.CanAccessKey(components[0], GetType());
}

}  // namespace extensions
