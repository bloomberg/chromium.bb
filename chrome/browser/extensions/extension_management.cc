// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_management.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_management_constants.h"
#include "chrome/browser/extensions/extension_management_internal.h"
#include "chrome/browser/extensions/external_policy_loader.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/extensions/permissions_based_management_policy_provider.h"
#include "chrome/browser/extensions/standard_management_policy_provider.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/crx_file/id_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "url/gurl.h"

namespace extensions {

ExtensionManagement::ExtensionManagement(PrefService* pref_service)
    : pref_service_(pref_service) {
  TRACE_EVENT0("browser,startup",
               "ExtensionManagement::ExtensionManagement::ctor");
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
  // Note that both |global_settings_| and |default_settings_| will be null
  // before first call to Refresh(), so in order to resolve this, Refresh() must
  // be called in the initialization of ExtensionManagement.
  Refresh();
  providers_.push_back(
      base::MakeUnique<StandardManagementPolicyProvider>(this));
  providers_.push_back(
      base::MakeUnique<PermissionsBasedManagementPolicyProvider>(this));
}

ExtensionManagement::~ExtensionManagement() {
}

void ExtensionManagement::Shutdown() {
  pref_change_registrar_.RemoveAll();
  pref_service_ = nullptr;
}

void ExtensionManagement::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ExtensionManagement::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

const std::vector<std::unique_ptr<ManagementPolicy::Provider>>&
ExtensionManagement::GetProviders() const {
  return providers_;
}

bool ExtensionManagement::BlacklistedByDefault() const {
  return default_settings_->installation_mode == INSTALLATION_BLOCKED;
}

ExtensionManagement::InstallationMode ExtensionManagement::GetInstallationMode(
    const Extension* extension) const {
  // Check per-extension installation mode setting first.
  auto iter_id = settings_by_id_.find(extension->id());
  if (iter_id != settings_by_id_.end())
    return iter_id->second->installation_mode;
  std::string update_url;
  // Check per-update-url installation mode setting.
  if (extension->manifest()->GetString(manifest_keys::kUpdateURL,
                                       &update_url)) {
    auto iter_update_url = settings_by_update_url_.find(update_url);
    if (iter_update_url != settings_by_update_url_.end())
      return iter_update_url->second->installation_mode;
  }
  // Fall back to default installation mode setting.
  return default_settings_->installation_mode;
}

std::unique_ptr<base::DictionaryValue>
ExtensionManagement::GetForceInstallList() const {
  std::unique_ptr<base::DictionaryValue> install_list(
      new base::DictionaryValue());
  for (const auto& entry : settings_by_id_) {
    if (entry.second->installation_mode == INSTALLATION_FORCED) {
      ExternalPolicyLoader::AddExtension(install_list.get(), entry.first,
                                         entry.second->update_url);
    }
  }
  return install_list;
}

std::unique_ptr<base::DictionaryValue>
ExtensionManagement::GetRecommendedInstallList() const {
  std::unique_ptr<base::DictionaryValue> install_list(
      new base::DictionaryValue());
  for (const auto& entry : settings_by_id_) {
    if (entry.second->installation_mode == INSTALLATION_RECOMMENDED) {
      ExternalPolicyLoader::AddExtension(install_list.get(), entry.first,
                                         entry.second->update_url);
    }
  }
  return install_list;
}

bool ExtensionManagement::IsInstallationExplicitlyAllowed(
    const ExtensionId& id) const {
  auto it = settings_by_id_.find(id);
  // No settings explicitly specified for |id|.
  if (it == settings_by_id_.end())
    return false;
  // Checks if the extension is on the automatically installed list or
  // install white-list.
  InstallationMode mode = it->second->installation_mode;
  return mode == INSTALLATION_FORCED || mode == INSTALLATION_RECOMMENDED ||
         mode == INSTALLATION_ALLOWED;
}

bool ExtensionManagement::IsOffstoreInstallAllowed(
    const GURL& url,
    const GURL& referrer_url) const {
  // No allowed install sites specified, disallow by default.
  if (!global_settings_->has_restricted_install_sources)
    return false;

  const URLPatternSet& url_patterns = global_settings_->install_sources;

  if (!url_patterns.MatchesURL(url))
    return false;

  // The referrer URL must also be whitelisted, unless the URL has the file
  // scheme (there's no referrer for those URLs).
  return url.SchemeIsFile() || url_patterns.MatchesURL(referrer_url);
}

bool ExtensionManagement::IsAllowedManifestType(
    Manifest::Type manifest_type) const {
  if (!global_settings_->has_restricted_allowed_types)
    return true;
  const std::vector<Manifest::Type>& allowed_types =
      global_settings_->allowed_types;
  return std::find(allowed_types.begin(), allowed_types.end(), manifest_type) !=
         allowed_types.end();
}

APIPermissionSet ExtensionManagement::GetBlockedAPIPermissions(
    const Extension* extension) const {
  // Fetch per-extension blocked permissions setting.
  auto iter_id = settings_by_id_.find(extension->id());

  // Fetch per-update-url blocked permissions setting.
  std::string update_url;
  auto iter_update_url = settings_by_update_url_.end();
  if (extension->manifest()->GetString(manifest_keys::kUpdateURL,
                                       &update_url)) {
    iter_update_url = settings_by_update_url_.find(update_url);
  }

  if (iter_id != settings_by_id_.end() &&
      iter_update_url != settings_by_update_url_.end()) {
    // Blocked permissions setting are specified in both per-extension and
    // per-update-url settings, try to merge them.
    APIPermissionSet merged;
    APIPermissionSet::Union(iter_id->second->blocked_permissions,
                            iter_update_url->second->blocked_permissions,
                            &merged);
    return merged;
  }
  // Check whether if in one of them, setting is specified.
  if (iter_id != settings_by_id_.end())
    return iter_id->second->blocked_permissions;
  if (iter_update_url != settings_by_update_url_.end())
    return iter_update_url->second->blocked_permissions;
  // Fall back to the default blocked permissions setting.
  return default_settings_->blocked_permissions;
}

std::unique_ptr<const PermissionSet> ExtensionManagement::GetBlockedPermissions(
    const Extension* extension) const {
  // Only api permissions are supported currently.
  return std::unique_ptr<const PermissionSet>(new PermissionSet(
      GetBlockedAPIPermissions(extension), ManifestPermissionSet(),
      URLPatternSet(), URLPatternSet()));
}

bool ExtensionManagement::IsPermissionSetAllowed(
    const Extension* extension,
    const PermissionSet& perms) const {
  for (auto* blocked_api : GetBlockedAPIPermissions(extension)) {
    if (perms.HasAPIPermission(blocked_api->id()))
      return false;
  }
  return true;
}

bool ExtensionManagement::CheckMinimumVersion(
    const Extension* extension,
    std::string* required_version) const {
  auto iter = settings_by_id_.find(extension->id());
  // If there are no minimum version required for |extension|, return true.
  if (iter == settings_by_id_.end() || !iter->second->minimum_version_required)
    return true;
  bool meets_requirement = extension->version()->CompareTo(
                             *iter->second->minimum_version_required) >= 0;
  // Output a human readable version string for prompting if necessary.
  if (!meets_requirement && required_version)
    *required_version = iter->second->minimum_version_required->GetString();
  return meets_requirement;
}

void ExtensionManagement::Refresh() {
  TRACE_EVENT0("browser,startup", "ExtensionManagement::Refresh");
  SCOPED_UMA_HISTOGRAM_TIMER("Extensions.ExtensionManagement_RefreshTime");
  // Load all extension management settings preferences.
  const base::ListValue* allowed_list_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kInstallAllowList, true, base::Value::Type::LIST));
  // Allow user to use preference to block certain extensions. Note that policy
  // managed forcelist or whitelist will always override this.
  const base::ListValue* denied_list_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kInstallDenyList, false, base::Value::Type::LIST));
  const base::DictionaryValue* forced_list_pref =
      static_cast<const base::DictionaryValue*>(LoadPreference(
          pref_names::kInstallForceList, true, base::Value::Type::DICTIONARY));
  const base::ListValue* install_sources_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kAllowedInstallSites, true, base::Value::Type::LIST));
  const base::ListValue* allowed_types_pref =
      static_cast<const base::ListValue*>(LoadPreference(
          pref_names::kAllowedTypes, true, base::Value::Type::LIST));
  const base::DictionaryValue* dict_pref =
      static_cast<const base::DictionaryValue*>(
          LoadPreference(pref_names::kExtensionManagement,
                         true,
                         base::Value::Type::DICTIONARY));

  // Reset all settings.
  global_settings_.reset(new internal::GlobalSettings());
  settings_by_id_.clear();
  default_settings_.reset(new internal::IndividualSettings());

  // Parse default settings.
  const base::StringValue wildcard("*");
  if (denied_list_pref &&
      denied_list_pref->Find(wildcard) != denied_list_pref->end()) {
    default_settings_->installation_mode = INSTALLATION_BLOCKED;
  }

  const base::DictionaryValue* subdict = NULL;
  if (dict_pref &&
      dict_pref->GetDictionary(schema_constants::kWildcard, &subdict)) {
    if (!default_settings_->Parse(
            subdict, internal::IndividualSettings::SCOPE_DEFAULT)) {
      LOG(WARNING) << "Default extension management settings parsing error.";
      default_settings_->Reset();
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
        internal::IndividualSettings* by_id = AccessById(it.key());
        by_id->installation_mode = INSTALLATION_FORCED;
        by_id->update_url = update_url;
      }
    }
  }

  if (install_sources_pref) {
    global_settings_->has_restricted_install_sources = true;
    for (base::ListValue::const_iterator it = install_sources_pref->begin();
         it != install_sources_pref->end(); ++it) {
      std::string url_pattern;
      if ((*it)->GetAsString(&url_pattern)) {
        URLPattern entry(URLPattern::SCHEME_ALL);
        if (entry.Parse(url_pattern) == URLPattern::PARSE_SUCCESS) {
          global_settings_->install_sources.AddPattern(entry);
        } else {
          LOG(WARNING) << "Invalid URL pattern in for preference "
                       << pref_names::kAllowedInstallSites << ": "
                       << url_pattern << ".";
        }
      }
    }
  }

  if (allowed_types_pref) {
    global_settings_->has_restricted_allowed_types = true;
    for (base::ListValue::const_iterator it = allowed_types_pref->begin();
         it != allowed_types_pref->end(); ++it) {
      int int_value;
      std::string string_value;
      if ((*it)->GetAsInteger(&int_value) && int_value >= 0 &&
          int_value < Manifest::Type::NUM_LOAD_TYPES) {
        global_settings_->allowed_types.push_back(
            static_cast<Manifest::Type>(int_value));
      } else if ((*it)->GetAsString(&string_value)) {
        Manifest::Type manifest_type =
            schema_constants::GetManifestType(string_value);
        if (manifest_type != Manifest::TYPE_UNKNOWN)
          global_settings_->allowed_types.push_back(manifest_type);
      }
    }
  }

  if (dict_pref) {
    // Parse new extension management preference.
    for (base::DictionaryValue::Iterator iter(*dict_pref); !iter.IsAtEnd();
         iter.Advance()) {
      if (iter.key() == schema_constants::kWildcard)
        continue;
      if (!iter.value().GetAsDictionary(&subdict))
        continue;
      if (base::StartsWith(iter.key(), schema_constants::kUpdateUrlPrefix,
                           base::CompareCase::SENSITIVE)) {
        const std::string& update_url =
            iter.key().substr(strlen(schema_constants::kUpdateUrlPrefix));
        if (!GURL(update_url).is_valid()) {
          LOG(WARNING) << "Invalid update URL: " << update_url << ".";
          continue;
        }
        internal::IndividualSettings* by_update_url =
            AccessByUpdateUrl(update_url);
        if (!by_update_url->Parse(
                subdict, internal::IndividualSettings::SCOPE_UPDATE_URL)) {
          settings_by_update_url_.erase(update_url);
          LOG(WARNING) << "Malformed Extension Management settings for "
                          "extensions with update url: " << update_url << ".";
        }
      } else {
        const std::string& extension_id = iter.key();
        if (!crx_file::id_util::IdIsValid(extension_id)) {
          LOG(WARNING) << "Invalid extension ID : " << extension_id << ".";
          continue;
        }
        internal::IndividualSettings* by_id = AccessById(extension_id);
        if (!by_id->Parse(subdict,
                          internal::IndividualSettings::SCOPE_INDIVIDUAL)) {
          settings_by_id_.erase(extension_id);
          LOG(WARNING) << "Malformed Extension Management settings for "
                       << extension_id << ".";
        }
      }
    }
  }
}

const base::Value* ExtensionManagement::LoadPreference(
    const char* pref_name,
    bool force_managed,
    base::Value::Type expected_type) {
  if (!pref_service_)
    return nullptr;
  const PrefService::Preference* pref =
      pref_service_->FindPreference(pref_name);
  if (pref && !pref->IsDefaultValue() &&
      (!force_managed || pref->IsManaged())) {
    const base::Value* value = pref->GetValue();
    if (value && value->IsType(expected_type))
      return value;
  }
  return nullptr;
}

void ExtensionManagement::OnExtensionPrefChanged() {
  Refresh();
  NotifyExtensionManagementPrefChanged();
}

void ExtensionManagement::NotifyExtensionManagementPrefChanged() {
  for (auto& observer : observer_list_)
    observer.OnExtensionManagementSettingsChanged();
}

internal::IndividualSettings* ExtensionManagement::AccessById(
    const ExtensionId& id) {
  DCHECK(crx_file::id_util::IdIsValid(id)) << "Invalid ID: " << id;
  std::unique_ptr<internal::IndividualSettings>& settings = settings_by_id_[id];
  if (!settings) {
    settings =
        base::MakeUnique<internal::IndividualSettings>(default_settings_.get());
  }
  return settings.get();
}

internal::IndividualSettings* ExtensionManagement::AccessByUpdateUrl(
    const std::string& update_url) {
  DCHECK(GURL(update_url).is_valid()) << "Invalid update URL: " << update_url;
  std::unique_ptr<internal::IndividualSettings>& settings =
      settings_by_update_url_[update_url];
  if (!settings) {
    settings =
        base::MakeUnique<internal::IndividualSettings>(default_settings_.get());
  }
  return settings.get();
}

ExtensionManagement* ExtensionManagementFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionManagement*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ExtensionManagementFactory* ExtensionManagementFactory::GetInstance() {
  return base::Singleton<ExtensionManagementFactory>::get();
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
  TRACE_EVENT0("browser,startup",
               "ExtensionManagementFactory::BuildServiceInstanceFor");
  return new ExtensionManagement(
      Profile::FromBrowserContext(context)->GetPrefs());
}

content::BrowserContext* ExtensionManagementFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

void ExtensionManagementFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
  user_prefs->RegisterDictionaryPref(pref_names::kExtensionManagement);
}

}  // namespace extensions
