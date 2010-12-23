// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_pref_store.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#if defined(OS_WIN)
#include "chrome/browser/policy/configuration_policy_provider_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#elif defined(OS_POSIX)
#include "chrome/browser/policy/config_dir_policy_provider.h"
#endif
#include "chrome/browser/policy/device_management_policy_provider.h"
#include "chrome/browser/policy/dummy_configuration_policy_provider.h"
#include "chrome/browser/policy/profile_policy_context.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/policy_constants.h"
#include "chrome/common/pref_names.h"

namespace policy {

// Accepts policy settings from a ConfigurationPolicyProvider, converts them
// to preferences and caches the result.
class ConfigurationPolicyPrefKeeper
    : private ConfigurationPolicyStoreInterface {
 public:
  explicit ConfigurationPolicyPrefKeeper(ConfigurationPolicyProvider* provider);
  virtual ~ConfigurationPolicyPrefKeeper();

  // Get a preference value.
  PrefStore::ReadResult GetValue(const std::string& key, Value** result) const;

  // Compute the set of preference names that are different in |keeper|. This
  // includes preferences that are missing in either one.
  void GetDifferingPrefPaths(const ConfigurationPolicyPrefKeeper* other,
                             std::vector<std::string>* differing_prefs) const;

 private:
  // ConfigurationPolicyStore methods:
  virtual void Apply(ConfigurationPolicyType setting, Value* value);

  // Policies that map to a single preference are handled
  // by an automated converter. Each one of these policies
  // has an entry in |simple_policy_map_| with the following type.
  struct PolicyToPreferenceMapEntry {
    Value::ValueType value_type;
    ConfigurationPolicyType policy_type;
    const char* preference_path;  // A DictionaryValue path, not a file path.
  };

  // Returns the map entry that corresponds to |policy| in the map.
  const PolicyToPreferenceMapEntry* FindPolicyInMap(
      ConfigurationPolicyType policy,
      const PolicyToPreferenceMapEntry* map,
      int size) const;

  // Remove the preferences found in the map from |prefs_|. Returns true if any
  // such preferences were found and removed.
  bool RemovePreferencesOfMap(const PolicyToPreferenceMapEntry* map,
                              int table_size);

  bool ApplyPolicyFromMap(ConfigurationPolicyType policy,
                          Value* value,
                          const PolicyToPreferenceMapEntry* map,
                          int size);

  // Processes proxy-specific policies. Returns true if the specified policy
  // is a proxy-related policy. ApplyProxyPolicy assumes the ownership
  // of |value| in the case that the policy is proxy-specific.
  bool ApplyProxyPolicy(ConfigurationPolicyType policy, Value* value);

  // Handles sync-related policies. Returns true if the policy was handled.
  // Assumes ownership of |value| in that case.
  bool ApplySyncPolicy(ConfigurationPolicyType policy, Value* value);

  // Handles policies that affect AutoFill. Returns true if the policy was
  // handled and assumes ownership of |value| in that case.
  bool ApplyAutoFillPolicy(ConfigurationPolicyType policy, Value* value);

  // Make sure that the |path| if present in |prefs_|.  If not, set it to
  // a blank string.
  void EnsureStringPrefExists(const std::string& path);

  // If the required entries for default search are specified and valid,
  // finalizes the policy-specified configuration by initializing the
  // unspecified map entries.  Otherwise wipes all default search related
  // map entries from |prefs_|.
  void FinalizeDefaultSearchPolicySettings();

  // If the required entries for the proxy settings are specified and valid,
  // finalizes the policy-specified configuration by initializing the
  // respective values in |prefs_|.
  void FinalizeProxyPolicySettings();

  // Returns true if the policy values stored in proxy_* represent a valid
  // proxy configuration.
  bool CheckProxySettings();

  // Assumes CheckProxySettings returns true and applies the values stored
  // in proxy_*.
  void ApplyProxySettings();

  bool HasProxyPolicy(ConfigurationPolicyType policy) const;

  // Temporary cache that stores values until FinalizeProxyPolicySettings()
  // is called.
  std::map<ConfigurationPolicyType, Value*> proxy_policies_;

  // Set to false until the first proxy-relevant policy is applied. At that
  // time, default values are provided for all proxy-relevant prefs
  // to override any values set from stores with a lower priority.
  bool lower_priority_proxy_settings_overridden_;

  // The following are used to track what proxy-relevant policy has been applied
  // accross calls to Apply to provide a warning if a policy specifies a
  // contradictory proxy configuration. |proxy_disabled_| is set to true if and
  // only if the kPolicyNoProxyServer has been applied,
  // |proxy_configuration_specified_| is set to true if and only if any other
  // proxy policy other than kPolicyNoProxyServer has been applied.
  bool proxy_disabled_;
  bool proxy_configuration_specified_;

  // Set to true if a the proxy mode policy has been set to force Chrome
  // to use the system proxy.
  bool use_system_proxy_;

  PrefValueMap prefs_;

  static const PolicyToPreferenceMapEntry kSimplePolicyMap[];
  static const PolicyToPreferenceMapEntry kProxyPolicyMap[];
  static const PolicyToPreferenceMapEntry kDefaultSearchPolicyMap[];

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyPrefKeeper);
};

const ConfigurationPolicyPrefKeeper::PolicyToPreferenceMapEntry
    ConfigurationPolicyPrefKeeper::kSimplePolicyMap[] = {
  { Value::TYPE_STRING, kPolicyHomePage,  prefs::kHomePage },
  { Value::TYPE_BOOLEAN, kPolicyHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage },
  { Value::TYPE_INTEGER, kPolicyRestoreOnStartup,
    prefs::kRestoreOnStartup},
  { Value::TYPE_LIST, kPolicyURLsToRestoreOnStartup,
    prefs::kURLsToRestoreOnStartup },
  { Value::TYPE_BOOLEAN, kPolicyAlternateErrorPagesEnabled,
    prefs::kAlternateErrorPagesEnabled },
  { Value::TYPE_BOOLEAN, kPolicySearchSuggestEnabled,
    prefs::kSearchSuggestEnabled },
  { Value::TYPE_BOOLEAN, kPolicyDnsPrefetchingEnabled,
    prefs::kDnsPrefetchingEnabled },
  { Value::TYPE_BOOLEAN, kPolicyDisableSpdy,
    prefs::kDisableSpdy },
  { Value::TYPE_BOOLEAN, kPolicySafeBrowsingEnabled,
    prefs::kSafeBrowsingEnabled },
  { Value::TYPE_BOOLEAN, kPolicyPasswordManagerEnabled,
    prefs::kPasswordManagerEnabled },
  { Value::TYPE_BOOLEAN, kPolicyPasswordManagerAllowShowPasswords,
    prefs::kPasswordManagerAllowShowPasswords },
  { Value::TYPE_BOOLEAN, kPolicyPrintingEnabled,
    prefs::kPrintingEnabled },
  { Value::TYPE_BOOLEAN, kPolicyMetricsReportingEnabled,
    prefs::kMetricsReportingEnabled },
  { Value::TYPE_STRING, kPolicyApplicationLocale,
    prefs::kApplicationLocale},
  { Value::TYPE_LIST, kPolicyExtensionInstallAllowList,
    prefs::kExtensionInstallAllowList},
  { Value::TYPE_LIST, kPolicyExtensionInstallDenyList,
    prefs::kExtensionInstallDenyList},
  { Value::TYPE_LIST, kPolicyExtensionInstallForceList,
    prefs::kExtensionInstallForceList},
  { Value::TYPE_LIST, kPolicyDisabledPlugins,
    prefs::kPluginsPluginsBlacklist},
  { Value::TYPE_BOOLEAN, kPolicyShowHomeButton,
    prefs::kShowHomeButton },
  { Value::TYPE_BOOLEAN, kPolicyJavascriptEnabled,
    prefs::kWebKitJavascriptEnabled },
  { Value::TYPE_BOOLEAN, kPolicySavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled },
  { Value::TYPE_BOOLEAN, kPolicyDeveloperToolsDisabled,
    prefs::kDevToolsDisabled },
  { Value::TYPE_BOOLEAN, kPolicyBlockThirdPartyCookies,
    prefs::kBlockThirdPartyCookies },
  { Value::TYPE_INTEGER, kPolicyDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultJavaScriptSetting,
    prefs::kManagedDefaultJavaScriptSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultPluginsSetting,
    prefs::kManagedDefaultPluginsSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultNotificationSetting,
    prefs::kDesktopNotificationDefaultContentSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultGeolocationSetting,
    prefs::kGeolocationDefaultContentSetting },
  { Value::TYPE_STRING, kPolicyAuthSchemes,
    prefs::kAuthSchemes },
  { Value::TYPE_BOOLEAN, kPolicyDisableAuthNegotiateCnameLookup,
    prefs::kDisableAuthNegotiateCnameLookup },
  { Value::TYPE_BOOLEAN, kPolicyEnableAuthNegotiatePort,
    prefs::kEnableAuthNegotiatePort },
  { Value::TYPE_STRING, kPolicyAuthServerWhitelist,
    prefs::kAuthServerWhitelist },
  { Value::TYPE_STRING, kPolicyAuthNegotiateDelegateWhitelist,
    prefs::kAuthNegotiateDelegateWhitelist },
  { Value::TYPE_STRING, kPolicyGSSAPILibraryName,
    prefs::kGSSAPILibraryName },
  { Value::TYPE_BOOLEAN, kPolicyDisable3DAPIs,
    prefs::kDisable3DAPIs },

#if defined(OS_CHROMEOS)
  { Value::TYPE_BOOLEAN, kPolicyChromeOsLockOnIdleSuspend,
    prefs::kEnableScreenLock },
#endif
};

const ConfigurationPolicyPrefKeeper::PolicyToPreferenceMapEntry
    ConfigurationPolicyPrefKeeper::kDefaultSearchPolicyMap[] = {
  { Value::TYPE_BOOLEAN, kPolicyDefaultSearchProviderEnabled,
    prefs::kDefaultSearchProviderEnabled },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderName,
    prefs::kDefaultSearchProviderName },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderKeyword,
    prefs::kDefaultSearchProviderKeyword },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderSearchURL,
    prefs::kDefaultSearchProviderSearchURL },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderSuggestURL,
    prefs::kDefaultSearchProviderSuggestURL },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderIconURL,
    prefs::kDefaultSearchProviderIconURL },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderEncodings,
    prefs::kDefaultSearchProviderEncodings },
};

const ConfigurationPolicyPrefKeeper::PolicyToPreferenceMapEntry
    ConfigurationPolicyPrefKeeper::kProxyPolicyMap[] = {
  { Value::TYPE_STRING, kPolicyProxyServer, prefs::kProxyServer },
  { Value::TYPE_STRING, kPolicyProxyPacUrl, prefs::kProxyPacUrl },
  { Value::TYPE_STRING, kPolicyProxyBypassList, prefs::kProxyBypassList }
};

ConfigurationPolicyPrefKeeper::ConfigurationPolicyPrefKeeper(
    ConfigurationPolicyProvider* provider)
  : lower_priority_proxy_settings_overridden_(false),
    proxy_disabled_(false),
    proxy_configuration_specified_(false),
    use_system_proxy_(false) {
  if (!provider->Provide(this))
    LOG(WARNING) << "Failed to get policy from provider.";
  FinalizeProxyPolicySettings();
  FinalizeDefaultSearchPolicySettings();
}

ConfigurationPolicyPrefKeeper::~ConfigurationPolicyPrefKeeper() {
  DCHECK(proxy_policies_.empty());
}

PrefStore::ReadResult
ConfigurationPolicyPrefKeeper::GetValue(const std::string& key,
                                        Value** result) const {
  Value* stored_value = NULL;
  if (!prefs_.GetValue(key, &stored_value))
    return PrefStore::READ_NO_VALUE;

  // Check whether there's a default value, which indicates READ_USE_DEFAULT
  // should be returned.
  if (stored_value->IsType(Value::TYPE_NULL))
    return PrefStore::READ_USE_DEFAULT;

  *result = stored_value;
  return PrefStore::READ_OK;
}

void ConfigurationPolicyPrefKeeper::GetDifferingPrefPaths(
    const ConfigurationPolicyPrefKeeper* other,
    std::vector<std::string>* differing_prefs) const {
  prefs_.GetDifferingKeys(&other->prefs_, differing_prefs);
}

void ConfigurationPolicyPrefKeeper::Apply(ConfigurationPolicyType policy,
                                          Value* value) {
  if (ApplyProxyPolicy(policy, value))
    return;

  if (ApplySyncPolicy(policy, value))
    return;

  if (ApplyAutoFillPolicy(policy, value))
    return;

  if (ApplyPolicyFromMap(policy, value, kDefaultSearchPolicyMap,
                         arraysize(kDefaultSearchPolicyMap)))
    return;

  if (ApplyPolicyFromMap(policy, value, kSimplePolicyMap,
                         arraysize(kSimplePolicyMap)))
    return;

  // Other policy implementations go here.
  NOTIMPLEMENTED();
  delete value;
}

const ConfigurationPolicyPrefKeeper::PolicyToPreferenceMapEntry*
ConfigurationPolicyPrefKeeper::FindPolicyInMap(
    ConfigurationPolicyType policy,
    const PolicyToPreferenceMapEntry* map,
    int table_size) const {
  for (int i = 0; i < table_size; ++i) {
    if (map[i].policy_type == policy)
      return map + i;
  }
  return NULL;
}

bool ConfigurationPolicyPrefKeeper::RemovePreferencesOfMap(
    const PolicyToPreferenceMapEntry* map, int table_size) {
  bool found_any = false;
  for (int i = 0; i < table_size; ++i) {
    if (prefs_.RemoveValue(map[i].preference_path))
      found_any = true;
  }
  return found_any;
}

bool ConfigurationPolicyPrefKeeper::ApplyPolicyFromMap(
    ConfigurationPolicyType policy,
    Value* value,
    const PolicyToPreferenceMapEntry* map,
    int size) {
  for (int current = 0; current < size; ++current) {
    if (map[current].policy_type == policy) {
      DCHECK_EQ(map[current].value_type, value->GetType())
          << "mismatch in provided and expected policy value for preferences"
          << map[current].preference_path << ". expected = "
          << map[current].value_type << ", actual = "<< value->GetType();
      prefs_.SetValue(map[current].preference_path, value);
      return true;
    }
  }
  return false;
}

bool ConfigurationPolicyPrefKeeper::ApplyProxyPolicy(
    ConfigurationPolicyType policy,
    Value* value) {
  // We only collect the values until we have sufficient information when
  // FinalizeProxyPolicySettings() is called to determine whether the presented
  // values were correct and apply them in that case.
  if (policy == kPolicyProxyMode ||
      policy == kPolicyProxyServer ||
      policy == kPolicyProxyPacUrl ||
      policy == kPolicyProxyBypassList) {
    delete proxy_policies_[policy];
    proxy_policies_[policy] = value;
    return true;
  }
  // We are not interested in this policy.
  return false;
}

bool ConfigurationPolicyPrefKeeper::ApplySyncPolicy(
    ConfigurationPolicyType policy, Value* value) {
  if (policy == kPolicySyncDisabled) {
    bool disable_sync;
    if (value->GetAsBoolean(&disable_sync) && disable_sync)
      prefs_.SetValue(prefs::kSyncManaged, value);
    else
      delete value;
    return true;
  }
  return false;
}

bool ConfigurationPolicyPrefKeeper::ApplyAutoFillPolicy(
    ConfigurationPolicyType policy, Value* value) {
  if (policy == kPolicyAutoFillEnabled) {
    bool auto_fill_enabled;
    if (value->GetAsBoolean(&auto_fill_enabled) && !auto_fill_enabled)
      prefs_.SetValue(prefs::kAutoFillEnabled,
                       Value::CreateBooleanValue(false));
    delete value;
    return true;
  }
  return false;
}

void ConfigurationPolicyPrefKeeper::EnsureStringPrefExists(
    const std::string& path) {
  std::string value;
  if (!prefs_.GetString(path, &value))
    prefs_.SetString(path, value);
}

namespace {

// Implementation of SearchTermsData just for validation.
class SearchTermsDataForValidation : public SearchTermsData {
 public:
  SearchTermsDataForValidation() {}

  // Implementation of SearchTermsData.
  virtual std::string GoogleBaseURLValue() const {
    return "http://www.google.com/";
  }
  virtual std::string GetApplicationLocale() const {
    return "en";
  }
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  virtual std::wstring GetRlzParameterValue() const {
    return std::wstring();
  }
#endif
 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsDataForValidation);
};

}  // namespace

void ConfigurationPolicyPrefKeeper::FinalizeDefaultSearchPolicySettings() {
  bool enabled = true;
  if (prefs_.GetBoolean(prefs::kDefaultSearchProviderEnabled, &enabled) &&
      !enabled) {
    // If default search is disabled, we ignore the other fields.
    prefs_.SetString(prefs::kDefaultSearchProviderName, std::string());
    prefs_.SetString(prefs::kDefaultSearchProviderSearchURL, std::string());
    prefs_.SetString(prefs::kDefaultSearchProviderSuggestURL, std::string());
    prefs_.SetString(prefs::kDefaultSearchProviderIconURL, std::string());
    prefs_.SetString(prefs::kDefaultSearchProviderEncodings, std::string());
    prefs_.SetString(prefs::kDefaultSearchProviderKeyword, std::string());
    return;
  }
  std::string search_url;
  // The search URL is required.
  if (prefs_.GetString(prefs::kDefaultSearchProviderSearchURL, &search_url) &&
      !search_url.empty()) {
    SearchTermsDataForValidation search_terms_data;
    const TemplateURLRef search_url_ref(search_url, 0, 0);
    // It must support replacement (which implies it is valid).
    if (search_url_ref.SupportsReplacementUsingTermsData(search_terms_data)) {
      // The other entries are optional.  Just make sure that they are all
      // specified via policy, so that we don't use regular prefs.
      EnsureStringPrefExists(prefs::kDefaultSearchProviderSuggestURL);
      EnsureStringPrefExists(prefs::kDefaultSearchProviderIconURL);
      EnsureStringPrefExists(prefs::kDefaultSearchProviderEncodings);
      EnsureStringPrefExists(prefs::kDefaultSearchProviderKeyword);

      // For the name, default to the host if not specified.
      std::string name;
      if (!prefs_.GetString(prefs::kDefaultSearchProviderName, &name) ||
          name.empty())
        prefs_.SetString(prefs::kDefaultSearchProviderName,
                          GURL(search_url).host());

      // And clear the IDs since these are not specified via policy.
      prefs_.SetString(prefs::kDefaultSearchProviderID, std::string());
      prefs_.SetString(prefs::kDefaultSearchProviderPrepopulateID,
                        std::string());
      return;
    }
  }
  // Required entries are not there.  Remove any related entries.
  RemovePreferencesOfMap(kDefaultSearchPolicyMap,
                         arraysize(kDefaultSearchPolicyMap));
}

void ConfigurationPolicyPrefKeeper::FinalizeProxyPolicySettings() {
  if (CheckProxySettings())
    ApplyProxySettings();

  STLDeleteContainerPairSecondPointers(proxy_policies_.begin(),
                                       proxy_policies_.end());
  proxy_policies_.clear();
}

bool ConfigurationPolicyPrefKeeper::CheckProxySettings() {
  bool mode = HasProxyPolicy(kPolicyProxyMode);
  bool server = HasProxyPolicy(kPolicyProxyServer);
  bool pac_url = HasProxyPolicy(kPolicyProxyPacUrl);
  bool bypass_list = HasProxyPolicy(kPolicyProxyBypassList);

  if ((server || pac_url || bypass_list) && !mode) {
    LOG(WARNING) << "A centrally-administered policy defines proxy setting"
                 << " details without setting a proxy mode.";
    return false;
  }

  if (!mode)
    return true;

  int mode_value;
  if (!proxy_policies_[kPolicyProxyMode]->GetAsInteger(&mode_value)) {
    LOG(WARNING) << "Invalid proxy mode value.";
    return false;
  }

  switch (mode_value) {
    case kPolicyNoProxyServerMode:
      if (server || pac_url || bypass_list) {
        LOG(WARNING) << "A centrally-administered policy disables the use of"
                     << " a proxy but also specifies an explicit proxy"
                     << " configuration.";
        return false;
      }
      break;
    case kPolicyAutoDetectProxyMode:
      if (server || bypass_list) {
        LOG(WARNING) << "A centrally-administered policy dictates that a proxy"
                     << " shall be auto configured but specifies fixed proxy"
                     << " servers or a by-pass list.";
        return false;
      }
      break;
    case kPolicyManuallyConfiguredProxyMode:
      if (!server) {
        LOG(WARNING) << "A centrally-administered policy dictates that the"
                     << " system proxy settings should use fixed proxy servers"
                     << " without specifying which ones.";
        return false;
      }
      if (pac_url) {
        LOG(WARNING) << "A centrally-administered policy dictates that the"
                     << " system proxy settings should use fixed proxy servers"
                     << " but also specifies a PAC script.";
        return false;
      }
      break;
    case kPolicyUseSystemProxyMode:
      if (server || pac_url || bypass_list) {
        LOG(WARNING) << "A centrally-administered policy dictates that the"
                     << " system proxy settings should be used but also "
                     << " specifies an explicit proxy configuration.";
        return false;
      }
      break;
    default:
      LOG(WARNING) << "Invalid proxy mode " << mode_value;
      return false;
  }
  return true;
}

void ConfigurationPolicyPrefKeeper::ApplyProxySettings() {
  if (!HasProxyPolicy(kPolicyProxyMode))
    return;

  int int_mode;
  CHECK(proxy_policies_[kPolicyProxyMode]->GetAsInteger(&int_mode));
  ProxyPrefs::ProxyMode mode;
  switch (int_mode) {
    case kPolicyNoProxyServerMode:
      mode = ProxyPrefs::MODE_DIRECT;
      break;
    case kPolicyAutoDetectProxyMode:
      mode = ProxyPrefs::MODE_AUTO_DETECT;
      if (HasProxyPolicy(kPolicyProxyPacUrl))
        mode = ProxyPrefs::MODE_PAC_SCRIPT;
      break;
    case kPolicyManuallyConfiguredProxyMode:
      mode = ProxyPrefs::MODE_FIXED_SERVERS;
      break;
    case kPolicyUseSystemProxyMode:
      mode = ProxyPrefs::MODE_SYSTEM;
      break;
    default:
      mode = ProxyPrefs::MODE_DIRECT;
      NOTREACHED();
  }
  prefs_.SetValue(prefs::kProxyMode, Value::CreateIntegerValue(mode));

  if (HasProxyPolicy(kPolicyProxyServer)) {
    prefs_.SetValue(prefs::kProxyServer, proxy_policies_[kPolicyProxyServer]);
    proxy_policies_[kPolicyProxyServer] = NULL;
  } else {
    prefs_.SetValue(prefs::kProxyServer, Value::CreateNullValue());
  }
  if (HasProxyPolicy(kPolicyProxyPacUrl)) {
    prefs_.SetValue(prefs::kProxyPacUrl, proxy_policies_[kPolicyProxyPacUrl]);
    proxy_policies_[kPolicyProxyPacUrl] = NULL;
  } else {
    prefs_.SetValue(prefs::kProxyPacUrl, Value::CreateNullValue());
  }
  if (HasProxyPolicy(kPolicyProxyBypassList)) {
    prefs_.SetValue(prefs::kProxyBypassList,
                proxy_policies_[kPolicyProxyBypassList]);
    proxy_policies_[kPolicyProxyBypassList] = NULL;
  } else {
    prefs_.SetValue(prefs::kProxyBypassList, Value::CreateNullValue());
  }
}

bool ConfigurationPolicyPrefKeeper::HasProxyPolicy(
    ConfigurationPolicyType policy) const {
  std::map<ConfigurationPolicyType, Value*>::const_iterator iter;
  iter = proxy_policies_.find(policy);
  return iter != proxy_policies_.end() &&
         iter->second && !iter->second->IsType(Value::TYPE_NULL);
}

namespace {

// Manages the lifecycle of the shared platform-specific policy providers for
// managed platform, device management and recommended policy. Instantiated as a
// Singleton.
class ConfigurationPolicyProviderKeeper {
 public:
  ConfigurationPolicyProviderKeeper()
      : managed_platform_provider_(CreateManagedPlatformProvider()),
        device_management_provider_(CreateDeviceManagementProvider()),
        recommended_provider_(CreateRecommendedProvider()) {}
  virtual ~ConfigurationPolicyProviderKeeper() {}

  ConfigurationPolicyProvider* managed_platform_provider() const {
    return managed_platform_provider_.get();
  }

  ConfigurationPolicyProvider* device_management_provider() const {
    return device_management_provider_.get();
  }

  ConfigurationPolicyProvider* recommended_provider() const {
    return recommended_provider_.get();
  }

 private:
  scoped_ptr<ConfigurationPolicyProvider> managed_platform_provider_;
  scoped_ptr<ConfigurationPolicyProvider> device_management_provider_;
  scoped_ptr<ConfigurationPolicyProvider> recommended_provider_;

  static ConfigurationPolicyProvider* CreateManagedPlatformProvider();
  static ConfigurationPolicyProvider* CreateDeviceManagementProvider();
  static ConfigurationPolicyProvider* CreateRecommendedProvider();

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderKeeper);
};

static base::LazyInstance<ConfigurationPolicyProviderKeeper>
    g_configuration_policy_provider_keeper(base::LINKER_INITIALIZED);

ConfigurationPolicyProvider*
ConfigurationPolicyProviderKeeper::CreateManagedPlatformProvider() {
  const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list =
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList();
#if defined(OS_WIN)
  return new ConfigurationPolicyProviderWin(policy_list);
#elif defined(OS_MACOSX)
  return new ConfigurationPolicyProviderMac(policy_list);
#elif defined(OS_POSIX)
  FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    return new ConfigDirPolicyProvider(
        policy_list,
        config_dir_path.Append(FILE_PATH_LITERAL("managed")));
  } else {
    return new DummyConfigurationPolicyProvider(policy_list);
  }
#else
  return new DummyConfigurationPolicyProvider(policy_list);
#endif
}

ConfigurationPolicyProvider*
ConfigurationPolicyProviderKeeper::CreateDeviceManagementProvider() {
  return new DummyConfigurationPolicyProvider(
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList());
}

ConfigurationPolicyProvider*
ConfigurationPolicyProviderKeeper::CreateRecommendedProvider() {
  const ConfigurationPolicyProvider::PolicyDefinitionList* policy_list =
      ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList();
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  FilePath config_dir_path;
  if (PathService::Get(chrome::DIR_POLICY_FILES, &config_dir_path)) {
    return new ConfigDirPolicyProvider(
        policy_list,
        config_dir_path.Append(FILE_PATH_LITERAL("recommended")));
  } else {
    return new DummyConfigurationPolicyProvider(policy_list);
  }
#else
  return new DummyConfigurationPolicyProvider(policy_list);
#endif
}

}  // namespace

ConfigurationPolicyPrefStore::ConfigurationPolicyPrefStore(
    ConfigurationPolicyProvider* provider)
    : provider_(provider),
      initialization_complete_(provider->IsInitializationComplete()) {
  // Read initial policy.
  policy_keeper_.reset(new ConfigurationPolicyPrefKeeper(provider));

  registrar_.Init(provider_);
  registrar_.AddObserver(this);
}

ConfigurationPolicyPrefStore::~ConfigurationPolicyPrefStore() {
}

void ConfigurationPolicyPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void ConfigurationPolicyPrefStore::RemoveObserver(
    PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ConfigurationPolicyPrefStore::IsInitializationComplete() const {
  return initialization_complete_;
}

PrefStore::ReadResult
ConfigurationPolicyPrefStore::GetValue(const std::string& key,
                                       Value** value) const {
  return policy_keeper_->GetValue(key, value);
}

void ConfigurationPolicyPrefStore::OnUpdatePolicy() {
  Refresh();
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateManagedPlatformPolicyPrefStore() {
  return new ConfigurationPolicyPrefStore(
      g_configuration_policy_provider_keeper.Get().managed_platform_provider());
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateDeviceManagementPolicyPrefStore(
    Profile* profile) {
  ConfigurationPolicyProviderKeeper* keeper =
      g_configuration_policy_provider_keeper.Pointer();
  ConfigurationPolicyProvider* provider = NULL;
  if (profile)
    provider = profile->GetPolicyContext()->GetDeviceManagementPolicyProvider();
  if (!provider)
    provider = keeper->device_management_provider();
  return new ConfigurationPolicyPrefStore(provider);
}

// static
ConfigurationPolicyPrefStore*
ConfigurationPolicyPrefStore::CreateRecommendedPolicyPrefStore() {
  return new ConfigurationPolicyPrefStore(
      g_configuration_policy_provider_keeper.Get().recommended_provider());
}

/* static */
const ConfigurationPolicyProvider::PolicyDefinitionList*
ConfigurationPolicyPrefStore::GetChromePolicyDefinitionList() {
  static ConfigurationPolicyProvider::PolicyDefinitionList::Entry entries[] = {
    { kPolicyHomePage, Value::TYPE_STRING, key::kHomepageLocation },
    { kPolicyHomepageIsNewTabPage, Value::TYPE_BOOLEAN,
      key::kHomepageIsNewTabPage },
    { kPolicyRestoreOnStartup, Value::TYPE_INTEGER, key::kRestoreOnStartup },
    { kPolicyURLsToRestoreOnStartup, Value::TYPE_LIST,
      key::kURLsToRestoreOnStartup },
    { kPolicyDefaultSearchProviderEnabled, Value::TYPE_BOOLEAN,
      key::kDefaultSearchProviderEnabled },
    { kPolicyDefaultSearchProviderName, Value::TYPE_STRING,
      key::kDefaultSearchProviderName },
    { kPolicyDefaultSearchProviderKeyword, Value::TYPE_STRING,
      key::kDefaultSearchProviderKeyword },
    { kPolicyDefaultSearchProviderSearchURL, Value::TYPE_STRING,
      key::kDefaultSearchProviderSearchURL },
    { kPolicyDefaultSearchProviderSuggestURL, Value::TYPE_STRING,
      key::kDefaultSearchProviderSuggestURL },
    { kPolicyDefaultSearchProviderIconURL, Value::TYPE_STRING,
      key::kDefaultSearchProviderIconURL },
    { kPolicyDefaultSearchProviderEncodings, Value::TYPE_STRING,
      key::kDefaultSearchProviderEncodings },
    { kPolicyProxyMode, Value::TYPE_INTEGER, key::kProxyMode },
    { kPolicyProxyServer, Value::TYPE_STRING, key::kProxyServer },
    { kPolicyProxyPacUrl, Value::TYPE_STRING, key::kProxyPacUrl },
    { kPolicyProxyBypassList, Value::TYPE_STRING, key::kProxyBypassList },
    { kPolicyAlternateErrorPagesEnabled, Value::TYPE_BOOLEAN,
      key::kAlternateErrorPagesEnabled },
    { kPolicySearchSuggestEnabled, Value::TYPE_BOOLEAN,
      key::kSearchSuggestEnabled },
    { kPolicyDnsPrefetchingEnabled, Value::TYPE_BOOLEAN,
      key::kDnsPrefetchingEnabled },
    { kPolicyDisableSpdy, Value::TYPE_BOOLEAN, key::kDisableSpdy },
    { kPolicySafeBrowsingEnabled, Value::TYPE_BOOLEAN,
      key::kSafeBrowsingEnabled },
    { kPolicyMetricsReportingEnabled, Value::TYPE_BOOLEAN,
      key::kMetricsReportingEnabled },
    { kPolicyPasswordManagerEnabled, Value::TYPE_BOOLEAN,
      key::kPasswordManagerEnabled },
    { kPolicyPasswordManagerAllowShowPasswords, Value::TYPE_BOOLEAN,
      key::kPasswordManagerAllowShowPasswords },
    { kPolicyAutoFillEnabled, Value::TYPE_BOOLEAN, key::kAutoFillEnabled },
    { kPolicyDisabledPlugins, Value::TYPE_LIST, key::kDisabledPlugins },
    { kPolicyApplicationLocale, Value::TYPE_STRING,
      key::kApplicationLocaleValue },
    { kPolicySyncDisabled, Value::TYPE_BOOLEAN, key::kSyncDisabled },
    { kPolicyExtensionInstallAllowList, Value::TYPE_LIST,
      key::kExtensionInstallAllowList },
    { kPolicyExtensionInstallDenyList, Value::TYPE_LIST,
      key::kExtensionInstallDenyList },
    { kPolicyExtensionInstallForceList, Value::TYPE_LIST,
      key::kExtensionInstallForceList },
    { kPolicyShowHomeButton, Value::TYPE_BOOLEAN, key::kShowHomeButton },
    { kPolicyPrintingEnabled, Value::TYPE_BOOLEAN, key::kPrintingEnabled },
    { kPolicyJavascriptEnabled, Value::TYPE_BOOLEAN, key::kJavascriptEnabled },
    { kPolicySavingBrowserHistoryDisabled, Value::TYPE_BOOLEAN,
      key::kSavingBrowserHistoryDisabled },
    { kPolicyDeveloperToolsDisabled, Value::TYPE_BOOLEAN,
      key::kDeveloperToolsDisabled },
    { kPolicyBlockThirdPartyCookies, Value::TYPE_BOOLEAN,
      key::kBlockThirdPartyCookies },
    { kPolicyDefaultCookiesSetting, Value::TYPE_INTEGER,
      key::kDefaultCookiesSetting },
    { kPolicyDefaultImagesSetting, Value::TYPE_INTEGER,
      key::kDefaultImagesSetting },
    { kPolicyDefaultJavaScriptSetting, Value::TYPE_INTEGER,
      key::kDefaultJavaScriptSetting },
    { kPolicyDefaultPluginsSetting, Value::TYPE_INTEGER,
      key::kDefaultPluginsSetting },
    { kPolicyDefaultPopupsSetting, Value::TYPE_INTEGER,
      key::kDefaultPopupsSetting },
    { kPolicyDefaultNotificationSetting, Value::TYPE_INTEGER,
      key::kDefaultNotificationSetting },
    { kPolicyDefaultGeolocationSetting, Value::TYPE_INTEGER,
      key::kDefaultGeolocationSetting },
    { kPolicyAuthSchemes, Value::TYPE_STRING, key::kAuthSchemes },
    { kPolicyDisableAuthNegotiateCnameLookup, Value::TYPE_BOOLEAN,
      key::kDisableAuthNegotiateCnameLookup },
    { kPolicyEnableAuthNegotiatePort, Value::TYPE_BOOLEAN,
      key::kEnableAuthNegotiatePort },
    { kPolicyAuthServerWhitelist, Value::TYPE_STRING,
      key::kAuthServerWhitelist },
    { kPolicyAuthNegotiateDelegateWhitelist, Value::TYPE_STRING,
      key::kAuthNegotiateDelegateWhitelist },
    { kPolicyGSSAPILibraryName, Value::TYPE_STRING,
      key::kGSSAPILibraryName },
    { kPolicyDisable3DAPIs, Value::TYPE_BOOLEAN,
      key::kDisable3DAPIs },

#if defined(OS_CHROMEOS)
    { kPolicyChromeOsLockOnIdleSuspend, Value::TYPE_BOOLEAN,
      key::kChromeOsLockOnIdleSuspend },
#endif
  };

  static ConfigurationPolicyProvider::PolicyDefinitionList policy_list = {
    entries,
    entries + arraysize(entries),
  };
  return &policy_list;
}

void ConfigurationPolicyPrefStore::Refresh() {
  // Construct a new keeper, determine what changed and swap the keeper in.
  scoped_ptr<ConfigurationPolicyPrefKeeper> new_keeper(
      new ConfigurationPolicyPrefKeeper(provider_));
  std::vector<std::string> changed_prefs;
  new_keeper->GetDifferingPrefPaths(policy_keeper_.get(), &changed_prefs);
  policy_keeper_.reset(new_keeper.release());

  // Send out change notifications.
  for (std::vector<std::string>::const_iterator pref(changed_prefs.begin());
       pref != changed_prefs.end();
       ++pref) {
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_,
                      OnPrefValueChanged(*pref));
  }

  // Update the initialization flag.
  if (!initialization_complete_ &&
      provider_->IsInitializationComplete()) {
    initialization_complete_ = true;
    FOR_EACH_OBSERVER(PrefStore::Observer, observers_,
                      OnInitializationCompleted());
  }
}

}  // namespace policy
