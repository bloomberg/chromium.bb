// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_

#include <string>
#include <vector>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"

class GURL;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

namespace internal {

struct IndividualSettings;
struct GlobalSettings;

}  // namespace internal

class APIPermissionSet;
class PermissionSet;

// Tracks the management policies that affect extensions and provides interfaces
// for observing and obtaining the global settings for all extensions, as well
// as per-extension settings.
class ExtensionManagement : public KeyedService {
 public:
  // Observer class for extension management settings changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the extension management settings change.
    virtual void OnExtensionManagementSettingsChanged() = 0;
  };

  // Installation mode for extensions, default is INSTALLATION_ALLOWED.
  // * INSTALLATION_ALLOWED: Extension can be installed.
  // * INSTALLATION_BLOCKED: Extension cannot be installed.
  // * INSTALLATION_FORCED: Extension will be installed automatically
  //                        and cannot be disabled.
  // * INSTALLATION_RECOMMENDED: Extension will be installed automatically but
  //                             can be disabled.
  enum InstallationMode {
    INSTALLATION_ALLOWED = 0,
    INSTALLATION_BLOCKED,
    INSTALLATION_FORCED,
    INSTALLATION_RECOMMENDED,
  };

  explicit ExtensionManagement(PrefService* pref_service);
  ~ExtensionManagement() override;

  // KeyedService implementations:
  void Shutdown() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the list of ManagementPolicy::Provider controlled by extension
  // management policy settings.
  std::vector<ManagementPolicy::Provider*> GetProviders() const;

  // Checks if extensions are blacklisted by default, by policy. When true,
  // this means that even extensions without an ID should be blacklisted (e.g.
  // from the command line, or when loaded as an unpacked extension).
  bool BlacklistedByDefault() const;

  // Returns installation mode for an extension.
  InstallationMode GetInstallationMode(const Extension* extension) const;

  // Returns the force install list, in format specified by
  // ExternalPolicyLoader::AddExtension().
  scoped_ptr<base::DictionaryValue> GetForceInstallList() const;

  // Like GetForceInstallList(), but returns recommended install list instead.
  scoped_ptr<base::DictionaryValue> GetRecommendedInstallList() const;

  // Returns if an extension with id |id| is explicitly allowed by enterprise
  // policy or not.
  bool IsInstallationExplicitlyAllowed(const ExtensionId& id) const;

  // Returns true if an extension download should be allowed to proceed.
  bool IsOffstoreInstallAllowed(const GURL& url,
                                const GURL& referrer_url) const;

  // Returns true if an extension with manifest type |manifest_type| is
  // allowed to be installed.
  bool IsAllowedManifestType(Manifest::Type manifest_type) const;

  // Returns the list of blocked API permissions for |extension|.
  APIPermissionSet GetBlockedAPIPermissions(const Extension* extension) const;

  // Returns blocked permission set for |extension|.
  scoped_ptr<const PermissionSet> GetBlockedPermissions(
      const Extension* extension) const;

  // Returns true if every permission in |perms| is allowed for |extension|.
  bool IsPermissionSetAllowed(const Extension* extension,
                              const PermissionSet& perms) const;

  // Returns true if |extension| meets the minimum required version set for it.
  // If there is no such requirement set for it, returns true as well.
  // If false is returned and |required_version| is not null, the minimum
  // required version is returned.
  bool CheckMinimumVersion(const Extension* extension,
                           std::string* required_version) const;

 private:
  typedef base::ScopedPtrHashMap<ExtensionId,
                                 scoped_ptr<internal::IndividualSettings>>
      SettingsIdMap;
  typedef base::ScopedPtrHashMap<std::string,
                                 scoped_ptr<internal::IndividualSettings>>
      SettingsUpdateUrlMap;
  friend class ExtensionManagementServiceTest;

  // Load all extension management preferences from |pref_service|, and
  // refresh the settings.
  void Refresh();

  // Load preference with name |pref_name| and expected type |expected_type|.
  // If |force_managed| is true, only loading from the managed preference store
  // is allowed. Returns NULL if the preference is not present, not allowed to
  // be loaded from or has the wrong type.
  const base::Value* LoadPreference(const char* pref_name,
                                    bool force_managed,
                                    base::Value::Type expected_type);

  void OnExtensionPrefChanged();
  void NotifyExtensionManagementPrefChanged();

  // Helper function to access |settings_by_id_| with |id| as key.
  // Adds a new IndividualSettings entry to |settings_by_id_| if none exists for
  // |id| yet.
  internal::IndividualSettings* AccessById(const ExtensionId& id);

  // Similar to AccessById(), but access |settings_by_update_url_| instead.
  internal::IndividualSettings* AccessByUpdateUrl(
      const std::string& update_url);

  // A map containing all IndividualSettings applied to an individual extension
  // identified by extension ID. The extension ID is used as index key of the
  // map.
  SettingsIdMap settings_by_id_;

  // Similar to |settings_by_id_|, but contains the settings for a group of
  // extensions with same update URL. The update url itself is used as index
  // key for the map.
  SettingsUpdateUrlMap settings_by_update_url_;

  // The default IndividualSettings.
  // For extension settings applied to an individual extension (identified by
  // extension ID) or a group of extension (with specified extension update
  // URL), all unspecified part will take value from |default_settings_|.
  // For all other extensions, all settings from |default_settings_| will be
  // enforced.
  scoped_ptr<internal::IndividualSettings> default_settings_;

  // Extension settings applicable to all extensions.
  scoped_ptr<internal::GlobalSettings> global_settings_;

  PrefService* pref_service_;

  base::ObserverList<Observer, true> observer_list_;
  PrefChangeRegistrar pref_change_registrar_;
  ScopedVector<ManagementPolicy::Provider> providers_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagement);
};

class ExtensionManagementFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionManagement* GetForBrowserContext(
      content::BrowserContext* context);
  static ExtensionManagementFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ExtensionManagementFactory>;

  ExtensionManagementFactory();
  ~ExtensionManagementFactory() override;

  // BrowserContextKeyedServiceExtensionManagementFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagementFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_
