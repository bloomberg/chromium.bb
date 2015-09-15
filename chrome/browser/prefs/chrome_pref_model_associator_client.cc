// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/chrome_pref_model_associator_client.h"

#include "base/memory/singleton.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"

namespace {
// List of migrated preference name pairs. If a preference is migrated
// (meaning renamed) adding the old and new preference names here will ensure
// that the sync engine knows how to deal with the synced values coming in
// with the old name. Preference migration itself doesn't happen here. It may
// happen in session_startup_pref.cc.
const struct MigratedPreferences {
  const char* const old_name;
  const char* const new_name;
} kMigratedPreferences[] = {
    {prefs::kURLsToRestoreOnStartupOld, prefs::kURLsToRestoreOnStartup},
};
}  // namespace

// static
ChromePrefModelAssociatorClient*
ChromePrefModelAssociatorClient::GetInstance() {
  return base::Singleton<ChromePrefModelAssociatorClient>::get();
}

ChromePrefModelAssociatorClient::ChromePrefModelAssociatorClient() {}

ChromePrefModelAssociatorClient::~ChromePrefModelAssociatorClient() {}

bool ChromePrefModelAssociatorClient::IsMergeableListPreference(
    const std::string& pref_name) const {
  return pref_name == prefs::kURLsToRestoreOnStartup;
}

bool ChromePrefModelAssociatorClient::IsMergeableDictionaryPreference(
    const std::string& pref_name) const {
  const content_settings::WebsiteSettingsRegistry& registry =
      *content_settings::WebsiteSettingsRegistry::GetInstance();
  for (const content_settings::WebsiteSettingsInfo* info : registry) {
    if (info->pref_name() == pref_name)
      return true;
  }
  return false;
}

bool ChromePrefModelAssociatorClient::IsMigratedPreference(
    const std::string& new_pref_name,
    std::string* old_pref_name) const {
  for (size_t i = 0; i < arraysize(kMigratedPreferences); ++i) {
    if (new_pref_name == kMigratedPreferences[i].new_name) {
      old_pref_name->assign(kMigratedPreferences[i].old_name);
      return true;
    }
  }
  return false;
}

bool ChromePrefModelAssociatorClient::IsOldMigratedPreference(
    const std::string& old_pref_name,
    std::string* new_pref_name) const {
  for (size_t i = 0; i < arraysize(kMigratedPreferences); ++i) {
    if (old_pref_name == kMigratedPreferences[i].old_name) {
      new_pref_name->assign(kMigratedPreferences[i].new_name);
      return true;
    }
  }
  return false;
}
