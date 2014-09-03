// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/url_pattern.h"

namespace extensions {

void ExtensionManagement::IndividualSettings::Reset() {
  installation_mode = ExtensionManagement::INSTALLATION_ALLOWED;
  update_url.clear();
}

ExtensionManagement::GlobalSettings::GlobalSettings() {
  Reset();
}

ExtensionManagement::GlobalSettings::~GlobalSettings() {
}

void ExtensionManagement::GlobalSettings::Reset() {
  has_restricted_install_sources = false;
  install_sources.ClearPatterns();
  has_restricted_allowed_types = false;
  allowed_types.clear();
}

ExtensionManagement::ExtensionManagement(PrefService* pref_service)
    : pref_service_(pref_service) {
}

ExtensionManagement::~ExtensionManagement() {
}

void ExtensionManagement::Refresh() {
  // Load all extension management settings preferences.
  const base::ListValue* allowed_list_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kInstallAllowList, true, base::Value::TYPE_LIST));
  // Allow user to use preference to block certain extensions. Note that policy
  // managed forcelist or whitelist will always override this.
  const base::ListValue* denied_list_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kInstallDenyList, false, base::Value::TYPE_LIST));
  const base::DictionaryValue* forced_list_pref =
      static_cast<const base::DictionaryValue*>(LoadPreference(
          pref_names::kInstallForceList, true, base::Value::TYPE_DICTIONARY));
  const base::ListValue* install_sources_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kAllowedInstallSites, true, base::Value::TYPE_LIST));
  const base::ListValue* allowed_types_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kAllowedTypes, true, base::Value::TYPE_LIST));

  // Reset all settings.
  global_settings_.Reset();
  settings_by_id_.clear();
  default_settings_.Reset();

  // Parse default settings.
  const base::StringValue wildcard("*");
  if (denied_list_pref &&
      denied_list_pref->Find(wildcard) != denied_list_pref->end()) {
    default_settings_.installation_mode = INSTALLATION_BLOCKED;
  }

  // Parse legacy preferences.
  ExtensionId id;

  if (allowed_list_pref) {
    for (base::ListValue::const_iterator it = allowed_list_pref->begin();
         it != allowed_list_pref->end(); ++it) {
      if ((*it)->GetAsString(&id) && crx_file::id_util::IdIsValid(id))
        AccessById(id)->installation_mode = INSTALLATION_ALLOWED;
    }
  }

  if (denied_list_pref) {
    for (base::ListValue::const_iterator it = denied_list_pref->begin();
         it != denied_list_pref->end(); ++it) {
      if ((*it)->GetAsString(&id) && crx_file::id_util::IdIsValid(id))
        AccessById(id)->installation_mode = INSTALLATION_BLOCKED;
    }
  }

  if (forced_list_pref) {
    std::string update_url;
    for (base::DictionaryValue::Iterator it(*forced_list_pref); !it.IsAtEnd();
         it.Advance()) {
      if (!crx_file::id_util::IdIsValid(it.key()))
        continue;
      const base::DictionaryValue* dict_value = NULL;
      if (it.value().GetAsDictionary(&dict_value) &&
          dict_value->GetStringWithoutPathExpansion(
              ExternalProviderImpl::kExternalUpdateUrl, &update_url)) {
        IndividualSettings* by_id = AccessById(it.key());
        by_id->installation_mode = INSTALLATION_FORCED;
        by_id->update_url = update_url;
      }
    }
  }

  if (install_sources_pref) {
    global_settings_.has_restricted_install_sources = true;
    std::string url_pattern;
    for (base::ListValue::const_iterator it = install_sources_pref->begin();
         it != install_sources_pref->end(); ++it) {
      URLPattern entry(URLPattern::SCHEME_ALL);
      if ((*it)->GetAsString(&url_pattern)) {
        if (entry.Parse(url_pattern) == URLPattern::PARSE_SUCCESS) {
          global_settings_.install_sources.AddPattern(entry);
        } else {
          LOG(WARNING) << "Invalid URL pattern in for preference "
                       << pref_names::kAllowedInstallSites << ": "
                       << url_pattern << ".";
        }
      }
    }
  }

  if (allowed_types_pref) {
    global_settings_.has_restricted_allowed_types = true;
    for (base::ListValue::const_iterator it = allowed_types_pref->begin();
         it != allowed_types_pref->end(); ++it) {
      int int_value;
      if ((*it)->GetAsInteger(&int_value) && int_value >= 0 &&
          int_value < Manifest::Type::NUM_LOAD_TYPES) {
        global_settings_.allowed_types.push_back(
            static_cast<Manifest::Type>(int_value));
      }
    }
  }

  // TODO(binjin): Add parsing of new ExtensionManagement preference after the
  // new ExtensionManagement policy is added.
}

const ExtensionManagement::IndividualSettings& ExtensionManagement::ReadById(
    const ExtensionId& id) const {
  DCHECK(crx_file::id_util::IdIsValid(id)) << "Invalid ID: " << id;
  SettingsIdMap::const_iterator it = settings_by_id_.find(id);
  if (it != settings_by_id_.end())
    return it->second;
  return default_settings_;
}

const ExtensionManagement::GlobalSettings&
ExtensionManagement::ReadGlobalSettings() const {
  return global_settings_;
}

const base::Value* ExtensionManagement::LoadPreference(
    const char* pref_name,
    bool force_managed,
    base::Value::Type expected_type) {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(pref_name);
  if (pref && !pref->IsDefaultValue() &&
      (!force_managed || pref->IsManaged())) {
    const base::Value* value = pref->GetValue();
    if (value && value->IsType(expected_type))
      return value;
  }
  return NULL;
}

ExtensionManagement::IndividualSettings* ExtensionManagement::AccessById(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id)) << "Invalid ID: " << id;
  SettingsIdMap::iterator it = settings_by_id_.find(id);
  if (it == settings_by_id_.end())
    it = settings_by_id_.insert(std::make_pair(id, default_settings_)).first;
  return &it->second;
}

}  // namespace extensions
