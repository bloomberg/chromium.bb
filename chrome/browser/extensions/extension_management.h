// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/url_pattern_set.h"

class GURL;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Tracks the management policies that affect extensions and provides interfaces
// for observing and obtaining the global settings for all extensions, as well
// as per-extension settings.
class ExtensionManagement : public KeyedService {
 public:
  // Observer class for extension management settings changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Will be called when an extension management preference changes.
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

  // Class to hold extension management settings for one or a group of
  // extensions. Settings can be applied to an individual extension identified
  // by an ID, a group of extensions with specific |update_url| or all
  // extensions at once.
  struct IndividualSettings {
    IndividualSettings();
    ~IndividualSettings();

    void Reset();

    // Extension installation mode. Setting this to INSTALLATION_FORCED or
    // INSTALLATION_RECOMMENDED will enable extension auto-loading (only
    // applicable to single extension), and in this case the |update_url| must
    // be specified, containing the update URL for this extension.
    // Note that |update_url| will be ignored for INSTALLATION_ALLOWED and
    // INSTALLATION_BLOCKED installation mode.
    // These settings will override the default settings, and unspecified
    // settings will take value from default settings.
    InstallationMode installation_mode;
    std::string update_url;
  };

  // Global extension management settings, applicable to all extensions.
  struct GlobalSettings {
    GlobalSettings();
    ~GlobalSettings();

    void Reset();

    // Settings specifying which URLs are allowed to install extensions, will be
    // enforced only if |has_restricted_install_sources| is set to true.
    URLPatternSet install_sources;
    bool has_restricted_install_sources;

    // Settings specifying all allowed app/extension types, will be enforced
    // only of |has_restricted_allowed_types| is set to true.
    std::vector<Manifest::Type> allowed_types;
    bool has_restricted_allowed_types;
  };

  typedef std::map<ExtensionId, IndividualSettings> SettingsIdMap;

  explicit ExtensionManagement(PrefService* pref_service);
  virtual ~ExtensionManagement();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the ManagementPolicy::Provider controlled by extension management
  // policy settings.
  ManagementPolicy::Provider* GetProvider();

  // Checks if extensions are blacklisted by default, by policy. When true,
  // this means that even extensions without an ID should be blacklisted (e.g.
  // from the command line, or when loaded as an unpacked extension).
  bool BlacklistedByDefault();

  // Returns the force install list, in format specified by
  // ExternalPolicyLoader::AddExtension().
  scoped_ptr<base::DictionaryValue> GetForceInstallList() const;

  // Returns if an extension with id |id| is allowed to install or not.
  bool IsInstallationAllowed(const ExtensionId& id) const;

  // Returns true if an extension download should be allowed to proceed.
  bool IsOffstoreInstallAllowed(const GURL& url, const GURL& referrer_url);

  // Helper function to read |settings_by_id_| with |id| as key. Returns a
  // constant reference to default settings if |id| does not exist.
  const IndividualSettings& ReadById(const ExtensionId& id) const;

  // Returns a constant reference to |global_settings_|.
  const GlobalSettings& ReadGlobalSettings() const;

 private:
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
  IndividualSettings* AccessById(const ExtensionId& id);

  // A map containing all IndividualSettings applied to an individual extension
  // identified by extension ID. The extension ID is used as index key of the
  // map.
  // TODO(binjin): Add |settings_by_update_url_|, and implement mechanism for
  // it.
  SettingsIdMap settings_by_id_;

  // The default IndividualSettings.
  // For extension settings applied to an individual extension (identified by
  // extension ID) or a group of extension (with specified extension update
  // URL), all unspecified part will take value from |default_settings_|.
  // For all other extensions, all settings from |default_settings_| will be
  // enforced.
  IndividualSettings default_settings_;

  // Extension settings applicable to all extensions.
  GlobalSettings global_settings_;

  PrefService* pref_service_;

  ObserverList<Observer, true> observer_list_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_ptr<ManagementPolicy::Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagement);
};

class ExtensionManagementFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionManagement* GetForBrowserContext(
      content::BrowserContext* context);
  static ExtensionManagementFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionManagementFactory>;

  ExtensionManagementFactory();
  virtual ~ExtensionManagementFactory();

  // BrowserContextKeyedServiceExtensionManagementFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ExtensionManagementFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MANAGEMENT_H_
