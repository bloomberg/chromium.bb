// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management_internal.h"

#include "base/logging.h"
#include "base/values.h"
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

  return true;
}

void IndividualSettings::Reset() {
  installation_mode = ExtensionManagement::INSTALLATION_ALLOWED;
  update_url.clear();
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
