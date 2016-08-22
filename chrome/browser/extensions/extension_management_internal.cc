// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_internal.h"

#include <utility>

#include "base/logging.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "extensions/common/url_pattern_set.h"
#include "url/gurl.h"

namespace extensions {

namespace internal {

namespace {
const char kMalformedPreferenceWarning[] =
    "Malformed extension management preference.";
}  // namespace

IndividualSettings::IndividualSettings() {
  Reset();
}

// Initializes from default settings.
IndividualSettings::IndividualSettings(
    const IndividualSettings* default_settings) {
  installation_mode = default_settings->installation_mode;
  update_url = default_settings->installation_mode;
  blocked_permissions = default_settings->blocked_permissions;
  // We are not initializing |minimum_version_required| from |default_settings|
  // here since it's not applicable to default settings.
}

IndividualSettings::~IndividualSettings() {
}

bool IndividualSettings::Parse(const base::DictionaryValue* dict,
                               ParsingScope scope) {
  std::string installation_mode_str;
  if (dict->GetStringWithoutPathExpansion(schema_constants::kInstallationMode,
                                          &installation_mode_str)) {
    if (installation_mode_str == schema_constants::kAllowed) {
      installation_mode = ExtensionManagement::INSTALLATION_ALLOWED;
    } else if (installation_mode_str == schema_constants::kBlocked) {
      installation_mode = ExtensionManagement::INSTALLATION_BLOCKED;
    } else if (installation_mode_str == schema_constants::kForceInstalled) {
      installation_mode = ExtensionManagement::INSTALLATION_FORCED;
    } else if (installation_mode_str == schema_constants::kNormalInstalled) {
      installation_mode = ExtensionManagement::INSTALLATION_RECOMMENDED;
    } else {
      // Invalid value for 'installation_mode'.
      LOG(WARNING) << kMalformedPreferenceWarning;
      return false;
    }

    // Only proceed to fetch update url if force or recommended install mode
    // is set.
    if (installation_mode == ExtensionManagement::INSTALLATION_FORCED ||
        installation_mode == ExtensionManagement::INSTALLATION_RECOMMENDED) {
      if (scope != SCOPE_INDIVIDUAL) {
        // Only individual extensions are allowed to be automatically installed.
        LOG(WARNING) << kMalformedPreferenceWarning;
        return false;
      }
      std::string update_url_str;
      if (dict->GetStringWithoutPathExpansion(schema_constants::kUpdateUrl,
                                              &update_url_str) &&
          GURL(update_url_str).is_valid()) {
        update_url = update_url_str;
      } else {
        // No valid update URL for extension.
        LOG(WARNING) << kMalformedPreferenceWarning;
        return false;
      }
    }
  }

  // Parses the blocked permission settings.
  const base::ListValue* list_value = nullptr;
  base::string16 error;

  // If applicable, inherit from global block list and remove all explicitly
  // allowed permissions.
  if (scope != SCOPE_DEFAULT &&
      dict->GetListWithoutPathExpansion(schema_constants::kAllowedPermissions,
                                        &list_value)) {
    // It is assumed that Parse() is already called for SCOPE_DEFAULT and
    // settings specified for |this| is initialized by copying from default
    // settings, including the |blocked_permissions| setting here.
    // That is, |blocked_permissions| should be the default block permissions
    // list settings here.
    APIPermissionSet globally_blocked_permissions = blocked_permissions;
    APIPermissionSet explicitly_allowed_permissions;
    // Reuses code for parsing API permissions from manifest. But note that we
    // only support list of strings type.
    if (!APIPermissionSet::ParseFromJSON(
            list_value,
            APIPermissionSet::kDisallowInternalPermissions,
            &explicitly_allowed_permissions,
            &error,
            nullptr)) {
      // There might be unknown permissions, warn and just ignore them;
      LOG(WARNING) << error;
    }
    APIPermissionSet::Difference(globally_blocked_permissions,
                                 explicitly_allowed_permissions,
                                 &blocked_permissions);
  }

  // Then add all newly blocked permissions to the list.
  if (dict->GetListWithoutPathExpansion(schema_constants::kBlockedPermissions,
                                        &list_value)) {
    // The |blocked_permissions| might be the result of the routines above,
    // or remains the same as default block permissions settings.
    APIPermissionSet permissions_to_merge_from = blocked_permissions;
    APIPermissionSet permissions_parsed;
    if (!APIPermissionSet::ParseFromJSON(
            list_value,
            APIPermissionSet::kDisallowInternalPermissions,
            &permissions_parsed,
            &error,
            nullptr)) {
      LOG(WARNING) << error;
    }
    APIPermissionSet::Union(
        permissions_to_merge_from, permissions_parsed, &blocked_permissions);
  }

  // Parses the minimum version settings.
  std::string minimum_version_required_str;
  if (scope == SCOPE_INDIVIDUAL &&
      dict->GetStringWithoutPathExpansion(
          schema_constants::kMinimumVersionRequired,
          &minimum_version_required_str)) {
    std::unique_ptr<base::Version> version(
        new base::Version(minimum_version_required_str));
    // We accept a general version string here. Note that count of components in
    // version string of extensions is limited to 4.
    if (!version->IsValid())
      LOG(WARNING) << kMalformedPreferenceWarning;
    else
      minimum_version_required = std::move(version);
  }

  return true;
}

void IndividualSettings::Reset() {
  installation_mode = ExtensionManagement::INSTALLATION_ALLOWED;
  update_url.clear();
  blocked_permissions.clear();
}

GlobalSettings::GlobalSettings() {
  Reset();
}

GlobalSettings::~GlobalSettings() {
}

void GlobalSettings::Reset() {
  has_restricted_install_sources = false;
  install_sources.ClearPatterns();
  has_restricted_allowed_types = false;
  allowed_types.clear();
}

}  // namespace internal

}  // namespace extensions
