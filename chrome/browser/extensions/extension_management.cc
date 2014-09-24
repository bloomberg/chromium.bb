// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char kMalformedPreferenceWarning[] =
    "Malformed extension management preference.";

enum Scope {
  // Parses the default settings.
  SCOPE_DEFAULT = 0,
  // Parses the settings for an extension with specified extension ID.
  SCOPE_INDIVIDUAL,
};

// Parse the individual settings for |settings|. |dict| is the a
// sub-dictionary in extension management preference and |scope| represents
// the applicable range of the settings, a single extension, a group of
// extensions or default settings.
// Note that in case of parsing errors, |settings| will NOT be left untouched.
bool ParseIndividualSettings(
    const base::DictionaryValue* dict,
    Scope scope,
    ExtensionManagement::IndividualSettings* settings) {
  std::string installation_mode;
  if (dict->GetStringWithoutPathExpansion(schema_constants::kInstallationMode,
                                          &installation_mode)) {
    if (installation_mode == schema_constants::kAllowed) {
      settings->installation_mode = ExtensionManagement::INSTALLATION_ALLOWED;
    } else if (installation_mode == schema_constants::kBlocked) {
      settings->installation_mode = ExtensionManagement::INSTALLATION_BLOCKED;
    } else if (installation_mode == schema_constants::kForceInstalled) {
      settings->installation_mode = ExtensionManagement::INSTALLATION_FORCED;
    } else if (installation_mode == schema_constants::kNormalInstalled) {
      settings->installation_mode =
          ExtensionManagement::INSTALLATION_RECOMMENDED;
    } else {
      // Invalid value for 'installation_mode'.
      LOG(WARNING) << kMalformedPreferenceWarning;
      return false;
    }
  }

  if (settings->installation_mode == ExtensionManagement::INSTALLATION_FORCED ||
      settings->installation_mode ==
          ExtensionManagement::INSTALLATION_RECOMMENDED) {
    if (scope != SCOPE_INDIVIDUAL) {
      // Only individual extensions are allowed to be automatically installed.
      LOG(WARNING) << kMalformedPreferenceWarning;
      return false;
    }
    std::string update_url;
    if (dict->GetStringWithoutPathExpansion(schema_constants::kUpdateUrl,
                                            &update_url) &&
        GURL(update_url).is_valid()) {
      settings->update_url = update_url;
    } else {
      // No valid update URL for extension.
      LOG(WARNING) << kMalformedPreferenceWarning;
      return false;
    }
  }

  return true;
}

}  // namespace

ExtensionManagement::IndividualSettings::IndividualSettings() {
  Reset();
}

ExtensionManagement::IndividualSettings::~IndividualSettings() {
}

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
  pref_change_registrar_.Init(pref_service_);
  base::Closure pref_change_callback = base::Bind(
      &ExtensionManagement::OnExtensionPrefChanged, base::Unretained(this));
  pref_change_registrar_.Add(pref_names::kInstallAllowList,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kInstallDenyList,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kInstallForceList,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kAllowedInstallSites,
                             pref_change_callback);
  pref_change_registrar_.Add(pref_names::kAllowedTypes, pref_change_callback);
  pref_change_registrar_.Add(pref_names::kExtensionManagement,
                             pref_change_callback);
  Refresh();
  provider_.reset(new StandardManagementPolicyProvider(this));
}

ExtensionManagement::~ExtensionManagement() {
}

void ExtensionManagement::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionManagement::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

ManagementPolicy::Provider* ExtensionManagement::GetProvider() {
  return provider_.get();
}

bool ExtensionManagement::BlacklistedByDefault() {
  return default_settings_.installation_mode == INSTALLATION_BLOCKED;
}

scoped_ptr<base::DictionaryValue> ExtensionManagement::GetForceInstallList()
    const {
  scoped_ptr<base::DictionaryValue> forcelist(new base::DictionaryValue());
  for (SettingsIdMap::const_iterator it = settings_by_id_.begin();
       it != settings_by_id_.end();
       ++it) {
    if (it->second.installation_mode == INSTALLATION_FORCED) {
      ExternalPolicyLoader::AddExtension(
          forcelist.get(), it->first, it->second.update_url);
    }
  }
  return forcelist.Pass();
}

bool ExtensionManagement::IsInstallationAllowed(const ExtensionId& id) const {
  return ReadById(id).installation_mode != INSTALLATION_BLOCKED;
}

bool ExtensionManagement::IsOffstoreInstallAllowed(const GURL& url,
                                                   const GURL& referrer_url) {
  // No allowed install sites specified, disallow by default.
  if (!global_settings_.has_restricted_install_sources)
    return false;

  const extensions::URLPatternSet& url_patterns =
      global_settings_.install_sources;

  if (!url_patterns.MatchesURL(url))
    return false;

  // The referrer URL must also be whitelisted, unless the URL has the file
  // scheme (there's no referrer for those URLs).
  return url.SchemeIsFile() || url_patterns.MatchesURL(referrer_url);
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
  const base::DictionaryValue* dict_pref =
      static_cast<const base::DictionaryValue*>(
          LoadPreference(pref_names::kExtensionManagement,
                         true,
                         base::Value::TYPE_DICTIONARY));

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

  const base::DictionaryValue* subdict = NULL;
  if (dict_pref &&
      dict_pref->GetDictionary(schema_constants::kWildcard, &subdict)) {
    if (!ParseIndividualSettings(subdict, SCOPE_DEFAULT, &default_settings_)) {
      LOG(WARNING) << "Default extension management settings parsing error.";
      default_settings_.Reset();
    }

    // Settings from new preference have higher priority over legacy ones.
    const base::ListValue* list_value = NULL;
    if (subdict->GetList(schema_constants::kInstallSources, &list_value))
      install_sources_pref = list_value;
    if (subdict->GetList(schema_constants::kAllowedTypes, &list_value))
      allowed_types_pref = list_value;
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
    for (base::ListValue::const_iterator it = install_sources_pref->begin();
         it != install_sources_pref->end(); ++it) {
      std::string url_pattern;
      if ((*it)->GetAsString(&url_pattern)) {
        URLPattern entry(URLPattern::SCHEME_ALL);
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
      std::string string_value;
      if ((*it)->GetAsInteger(&int_value) && int_value >= 0 &&
          int_value < Manifest::Type::NUM_LOAD_TYPES) {
        global_settings_.allowed_types.push_back(
            static_cast<Manifest::Type>(int_value));
      } else if ((*it)->GetAsString(&string_value)) {
        Manifest::Type manifest_type =
            schema_constants::GetManifestType(string_value);
        if (manifest_type != Manifest::TYPE_UNKNOWN)
          global_settings_.allowed_types.push_back(manifest_type);
      }
    }
  }

  if (dict_pref) {
    // Parse new extension management preference.
    for (base::DictionaryValue::Iterator iter(*dict_pref); !iter.IsAtEnd();
         iter.Advance()) {
      if (iter.key() == schema_constants::kWildcard)
        continue;
      if (!iter.value().GetAsDictionary(&subdict)) {
        LOG(WARNING) << kMalformedPreferenceWarning;
        continue;
      }
      if (StartsWithASCII(iter.key(), schema_constants::kUpdateUrlPrefix, true))
        continue;
      const std::string& extension_id = iter.key();
      if (!crx_file::id_util::IdIsValid(extension_id)) {
        LOG(WARNING) << kMalformedPreferenceWarning;
        continue;
      }
      IndividualSettings* by_id = AccessById(extension_id);
      if (!ParseIndividualSettings(subdict, SCOPE_INDIVIDUAL, by_id)) {
        settings_by_id_.erase(settings_by_id_.find(extension_id));
        LOG(WARNING) << "Malformed Extension Management settings for "
                     << extension_id << ".";
      }
    }
  }
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

void ExtensionManagement::OnExtensionPrefChanged() {
  Refresh();
  NotifyExtensionManagementPrefChanged();
}

void ExtensionManagement::NotifyExtensionManagementPrefChanged() {
  FOR_EACH_OBSERVER(
      Observer, observer_list_, OnExtensionManagementSettingsChanged());
}

ExtensionManagement::IndividualSettings* ExtensionManagement::AccessById(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id)) << "Invalid ID: " << id;
  SettingsIdMap::iterator it = settings_by_id_.find(id);
  if (it == settings_by_id_.end())
    it = settings_by_id_.insert(std::make_pair(id, default_settings_)).first;
  return &it->second;
}

ExtensionManagement* ExtensionManagementFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionManagement*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ExtensionManagementFactory* ExtensionManagementFactory::GetInstance() {
  return Singleton<ExtensionManagementFactory>::get();
}

ExtensionManagementFactory::ExtensionManagementFactory()
    : BrowserContextKeyedServiceFactory(
          "ExtensionManagement",
          BrowserContextDependencyManager::GetInstance()) {
}

ExtensionManagementFactory::~ExtensionManagementFactory() {
}

KeyedService* ExtensionManagementFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionManagement(
      Profile::FromBrowserContext(context)->GetPrefs());
}

content::BrowserContext* ExtensionManagementFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

void ExtensionManagementFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterDictionaryPref(
      pref_names::kExtensionManagement,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace extensions
