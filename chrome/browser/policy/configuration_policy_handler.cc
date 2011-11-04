// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "policy/configuration_policy_type.h"
#include "policy/policy_constants.h"

namespace policy {

namespace {

// Implementations of ConfigurationPolicyHandler -------------------------------

// Abstract class derived from ConfigurationPolicyHandler that should be
// subclassed to handle a single policy (not a combination of policies).
class TypeCheckingPolicyHandler : public ConfigurationPolicyHandler {
 public:
  TypeCheckingPolicyHandler(ConfigurationPolicyType policy_type,
                            base::Value::Type value_type);

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;

 protected:
  virtual ~TypeCheckingPolicyHandler();

  ConfigurationPolicyType policy_type() const;

 private:
  // The ConfigurationPolicyType of the policy.
  ConfigurationPolicyType policy_type_;

  // The type the value of the policy should have.
  base::Value::Type value_type_;

  DISALLOW_COPY_AND_ASSIGN(TypeCheckingPolicyHandler);
};

// ConfigurationPolicyHandler for policies that map directly to a preference.
class SimplePolicyHandler : public TypeCheckingPolicyHandler {
 public:
  SimplePolicyHandler(ConfigurationPolicyType policy_type,
                      base::Value::Type value_type,
                      const char* pref_path);
  virtual ~SimplePolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // The DictionaryValue path of the preference the policy maps to.
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(SimplePolicyHandler);
};

// ConfigurationPolicyHandler for the SyncDisabled policy.
class SyncPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  SyncPolicyHandler();
  virtual ~SyncPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncPolicyHandler);
};

// ConfigurationPolicyHandler for the AutofillEnabled policy.
class AutofillPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  AutofillPolicyHandler();
  virtual ~AutofillPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillPolicyHandler);
};

// ConfigurationPolicyHandler for the DownloadDirectory policy.
class DownloadDirPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  DownloadDirPolicyHandler();
  virtual ~DownloadDirPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadDirPolicyHandler);
};

// ConfigurationPolicyHandler for the DiskCacheDir policy.
class DiskCacheDirPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  explicit DiskCacheDirPolicyHandler();
  virtual ~DiskCacheDirPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiskCacheDirPolicyHandler);
};

// ConfigurationPolicyHandler for the FileSelectionDialogsHandler policy.
class FileSelectionDialogsHandler : public TypeCheckingPolicyHandler {
 public:
  FileSelectionDialogsHandler();
  virtual ~FileSelectionDialogsHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSelectionDialogsHandler);
};

// ConfigurationPolicyHandler for the incognito mode policies.
class IncognitoModePolicyHandler : public ConfigurationPolicyHandler {
 public:
  IncognitoModePolicyHandler();
  virtual ~IncognitoModePolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  IncognitoModePrefs::Availability GetAvailabilityValueAsEnum(
      const Value* availability);

  DISALLOW_COPY_AND_ASSIGN(IncognitoModePolicyHandler);
};

// ConfigurationPolicyHandler for the DefaultSearchEncodings policy.
class DefaultSearchEncodingsPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  DefaultSearchEncodingsPolicyHandler();
  virtual ~DefaultSearchEncodingsPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultSearchEncodingsPolicyHandler);
};

// ConfigurationPolicyHandler for the default search policies.
class DefaultSearchPolicyHandler : public ConfigurationPolicyHandler {
 public:
  DefaultSearchPolicyHandler();
  virtual ~DefaultSearchPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // Calls |CheckPolicySettings()| on each of the handlers in |handlers_|
  // and returns true if all of the calls return true and false otherwise.
  bool CheckIndividualPolicies(const PolicyMap& policies,
                               PolicyErrorMap* errors);

  // Returns true if there is a value for |policy_type| in |policies| and false
  // otherwise.
  bool HasDefaultSearchPolicy(const PolicyMap& policies,
                              ConfigurationPolicyType policy_type);

  // Returns true if any default search policies are specified in |policies| and
  // false otherwise.
  bool AnyDefaultSearchPoliciesSpecified(const PolicyMap& policies);

  // Returns true if the default search provider is disabled and false
  // otherwise.
  bool DefaultSearchProviderIsDisabled(const PolicyMap& policies);

  // Returns true if the default search URL was set and is valid and false
  // otherwise.
  bool DefaultSearchURLIsValid(const PolicyMap& policies);

  // Make sure that the |path| if present in |prefs_|.  If not, set it to
  // a blank string.
  void EnsureStringPrefExists(PrefValueMap* prefs, const std::string& path);

  // The ConfigurationPolicyHandler handlers for each default search policy.
  HandlerList handlers_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSearchPolicyHandler);
};

// ConfigurationPolicyHandler for the proxy policies.
class ProxyPolicyHandler : public ConfigurationPolicyHandler {
 public:
  ProxyPolicyHandler();
  virtual ~ProxyPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const Value* GetProxyPolicyValue(const PolicyMap& policies,
                                   ConfigurationPolicyType policy);

  // Converts the deprecated ProxyServerMode policy value to a ProxyMode value
  // and places the result in |mode_value|. Returns true if the conversion
  // succeeded and false otherwise.
  bool CheckProxyModeAndServerMode(const PolicyMap& policies,
                                   PolicyErrorMap* errors,
                                   std::string* mode_value);

  DISALLOW_COPY_AND_ASSIGN(ProxyPolicyHandler);
};

//
class JavascriptPolicyHandler : public ConfigurationPolicyHandler {
 public:
  JavascriptPolicyHandler();
  virtual ~JavascriptPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const PolicyMap& policies,
                                   PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(JavascriptPolicyHandler);
};


// Helper classes --------------------------------------------------------------

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
  virtual string16 GetRlzParameterValue() const {
    return string16();
  }
#endif
 private:
  DISALLOW_COPY_AND_ASSIGN(SearchTermsDataForValidation);
};

// This is used to check whether for a given ProxyMode value, the ProxyPacUrl,
// the ProxyBypassList and the ProxyServer policies are allowed to be specified.
// |error_message_id| is the message id of the localized error message to show
// when the policies are not specified as allowed. Each value of ProxyMode
// has a ProxyModeValidationEntry in the |kProxyModeValidationMap| below.
struct ProxyModeValidationEntry {
  const char* mode_value;
  bool pac_url_allowed;
  bool bypass_list_allowed;
  bool server_allowed;
  int error_message_id;
};

// Maps a policy type to a preference path, and to the expected value type.
// This is the entry type of |kSimplePolicyMap| below.
struct PolicyToPreferenceMapEntry {
  base::Value::Type value_type;
  ConfigurationPolicyType policy_type;
  const char* preference_path;
};


// Static data -----------------------------------------------------------------

// List of policy types to preference names. This is used for simple policies
// that directly map to a single preference.
const PolicyToPreferenceMapEntry kSimplePolicyMap[] = {
  { Value::TYPE_STRING, kPolicyHomepageLocation,  prefs::kHomePage },
  { Value::TYPE_BOOLEAN, kPolicyHomepageIsNewTabPage,
    prefs::kHomePageIsNewTabPage },
  { Value::TYPE_INTEGER, kPolicyRestoreOnStartup,
    prefs::kRestoreOnStartup},
  { Value::TYPE_LIST, kPolicyRestoreOnStartupURLs,
    prefs::kURLsToRestoreOnStartup },
  { Value::TYPE_BOOLEAN, kPolicyAlternateErrorPagesEnabled,
    prefs::kAlternateErrorPagesEnabled },
  { Value::TYPE_BOOLEAN, kPolicySearchSuggestEnabled,
    prefs::kSearchSuggestEnabled },
  { Value::TYPE_BOOLEAN, kPolicyDnsPrefetchingEnabled,
    prefs::kNetworkPredictionEnabled },
  { Value::TYPE_BOOLEAN, kPolicyDisableSpdy,
    prefs::kDisableSpdy },
  { Value::TYPE_LIST, kPolicyDisabledSchemes,
    prefs::kDisabledSchemes },
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
  { Value::TYPE_STRING, kPolicyApplicationLocaleValue,
    prefs::kApplicationLocale},
  { Value::TYPE_LIST, kPolicyExtensionInstallWhitelist,
    prefs::kExtensionInstallAllowList},
  { Value::TYPE_LIST, kPolicyExtensionInstallBlacklist,
    prefs::kExtensionInstallDenyList},
  { Value::TYPE_LIST, kPolicyExtensionInstallForcelist,
    prefs::kExtensionInstallForceList},
  { Value::TYPE_LIST, kPolicyDisabledPlugins,
    prefs::kPluginsDisabledPlugins},
  { Value::TYPE_LIST, kPolicyDisabledPluginsExceptions,
    prefs::kPluginsDisabledPluginsExceptions},
  { Value::TYPE_LIST, kPolicyEnabledPlugins,
    prefs::kPluginsEnabledPlugins},
  { Value::TYPE_BOOLEAN, kPolicyShowHomeButton,
    prefs::kShowHomeButton },
  { Value::TYPE_BOOLEAN, kPolicySavingBrowserHistoryDisabled,
    prefs::kSavingBrowserHistoryDisabled },
  { Value::TYPE_BOOLEAN, kPolicyClearSiteDataOnExit,
    prefs::kClearSiteDataOnExit },
  { Value::TYPE_BOOLEAN, kPolicyDeveloperToolsDisabled,
    prefs::kDevToolsDisabled },
  { Value::TYPE_BOOLEAN, kPolicyBlockThirdPartyCookies,
    prefs::kBlockThirdPartyCookies },
  { Value::TYPE_INTEGER, kPolicyDefaultCookiesSetting,
    prefs::kManagedDefaultCookiesSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultImagesSetting,
    prefs::kManagedDefaultImagesSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultPluginsSetting,
    prefs::kManagedDefaultPluginsSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultPopupsSetting,
    prefs::kManagedDefaultPopupsSetting },
  { Value::TYPE_LIST, kPolicyAutoSelectCertificateForUrls,
    prefs::kManagedAutoSelectCertificateForUrls},
  { Value::TYPE_LIST, kPolicyCookiesAllowedForUrls,
    prefs::kManagedCookiesAllowedForUrls },
  { Value::TYPE_LIST, kPolicyCookiesBlockedForUrls,
    prefs::kManagedCookiesBlockedForUrls },
  { Value::TYPE_LIST, kPolicyCookiesSessionOnlyForUrls,
    prefs::kManagedCookiesSessionOnlyForUrls },
  { Value::TYPE_LIST, kPolicyImagesAllowedForUrls,
    prefs::kManagedImagesAllowedForUrls },
  { Value::TYPE_LIST, kPolicyImagesBlockedForUrls,
    prefs::kManagedImagesBlockedForUrls },
  { Value::TYPE_LIST, kPolicyJavaScriptAllowedForUrls,
    prefs::kManagedJavaScriptAllowedForUrls },
  { Value::TYPE_LIST, kPolicyJavaScriptBlockedForUrls,
    prefs::kManagedJavaScriptBlockedForUrls },
  { Value::TYPE_LIST, kPolicyPluginsAllowedForUrls,
    prefs::kManagedPluginsAllowedForUrls },
  { Value::TYPE_LIST, kPolicyPluginsBlockedForUrls,
    prefs::kManagedPluginsBlockedForUrls },
  { Value::TYPE_LIST, kPolicyPopupsAllowedForUrls,
    prefs::kManagedPopupsAllowedForUrls },
  { Value::TYPE_LIST, kPolicyPopupsBlockedForUrls,
    prefs::kManagedPopupsBlockedForUrls },
  { Value::TYPE_LIST, kPolicyNotificationsAllowedForUrls,
    prefs::kManagedNotificationsAllowedForUrls },
  { Value::TYPE_LIST, kPolicyNotificationsBlockedForUrls,
    prefs::kManagedNotificationsBlockedForUrls },
  { Value::TYPE_INTEGER, kPolicyDefaultNotificationsSetting,
    prefs::kManagedDefaultNotificationsSetting },
  { Value::TYPE_INTEGER, kPolicyDefaultGeolocationSetting,
    prefs::kManagedDefaultGeolocationSetting },
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
  { Value::TYPE_BOOLEAN, kPolicyAllowCrossOriginAuthPrompt,
    prefs::kAllowCrossOriginAuthPrompt },
  { Value::TYPE_BOOLEAN, kPolicyDisable3DAPIs,
    prefs::kDisable3DAPIs },
  { Value::TYPE_BOOLEAN, kPolicyDisablePluginFinder,
    prefs::kDisablePluginFinder },
  { Value::TYPE_INTEGER, kPolicyPolicyRefreshRate,
    prefs::kUserPolicyRefreshRate },
  { Value::TYPE_INTEGER, kPolicyDevicePolicyRefreshRate,
    prefs::kDevicePolicyRefreshRate },
  { Value::TYPE_BOOLEAN, kPolicyInstantEnabled, prefs::kInstantEnabled },
  { Value::TYPE_BOOLEAN, kPolicyDefaultBrowserSettingEnabled,
    prefs::kDefaultBrowserSettingEnabled },
  { Value::TYPE_BOOLEAN, kPolicyRemoteAccessHostFirewallTraversal,
    prefs::kRemoteAccessHostFirewallTraversal },
  { Value::TYPE_BOOLEAN, kPolicyCloudPrintProxyEnabled,
    prefs::kCloudPrintProxyEnabled },
  { Value::TYPE_BOOLEAN, kPolicyCloudPrintSubmitEnabled,
    prefs::kCloudPrintSubmitEnabled },
  { Value::TYPE_BOOLEAN, kPolicyTranslateEnabled, prefs::kEnableTranslate },
  { Value::TYPE_BOOLEAN, kPolicyAllowOutdatedPlugins,
    prefs::kPluginsAllowOutdated },
  { Value::TYPE_BOOLEAN, kPolicyAlwaysAuthorizePlugins,
    prefs::kPluginsAlwaysAuthorize },
  { Value::TYPE_BOOLEAN, kPolicyBookmarkBarEnabled,
    prefs::kShowBookmarkBar },
  { Value::TYPE_BOOLEAN, kPolicyEditBookmarksEnabled,
    prefs::kEditBookmarksEnabled },
  { Value::TYPE_BOOLEAN, kPolicyAllowFileSelectionDialogs,
    prefs::kAllowFileSelectionDialogs },
  { Value::TYPE_BOOLEAN, kPolicyImportBookmarks,
    prefs::kImportBookmarks},
  { Value::TYPE_BOOLEAN, kPolicyImportHistory,
    prefs::kImportHistory},
  { Value::TYPE_BOOLEAN, kPolicyImportHomepage,
    prefs::kImportHomepage},
  { Value::TYPE_BOOLEAN, kPolicyImportSearchEngine,
    prefs::kImportSearchEngine },
  { Value::TYPE_BOOLEAN, kPolicyImportSavedPasswords,
    prefs::kImportSavedPasswords },
  { Value::TYPE_INTEGER, kPolicyMaxConnectionsPerProxy,
    prefs::kMaxConnectionsPerProxy },
  { Value::TYPE_BOOLEAN, kPolicyHideWebStorePromo,
    prefs::kNTPHideWebStorePromo },
  { Value::TYPE_LIST, kPolicyURLBlacklist,
    prefs::kUrlBlacklist },
  { Value::TYPE_LIST, kPolicyURLWhitelist,
    prefs::kUrlWhitelist },

#if defined(OS_CHROMEOS)
  { Value::TYPE_BOOLEAN, kPolicyChromeOsLockOnIdleSuspend,
    prefs::kEnableScreenLock },
  { Value::TYPE_STRING, kPolicyChromeOsReleaseChannel,
    prefs::kChromeOsReleaseChannel },
#endif
};

// List of policy types to preference names, for policies affecting the default
// search provider.
const PolicyToPreferenceMapEntry kDefaultSearchPolicyMap[] = {
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
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderInstantURL,
    prefs::kDefaultSearchProviderInstantURL },
  { Value::TYPE_STRING, kPolicyDefaultSearchProviderIconURL,
    prefs::kDefaultSearchProviderIconURL },
  { Value::TYPE_LIST, kPolicyDefaultSearchProviderEncodings,
    prefs::kDefaultSearchProviderEncodings },
};

// List of entries determining which proxy policies can be specified, depending
// on the ProxyMode.
const ProxyModeValidationEntry kProxyModeValidationMap[] = {
  { ProxyPrefs::kDirectProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_DISABLED_ERROR },
  { ProxyPrefs::kAutoDetectProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_AUTO_DETECT_ERROR },
  { ProxyPrefs::kPacScriptProxyModeName,
    true, false, false, IDS_POLICY_PROXY_MODE_PAC_URL_ERROR },
  { ProxyPrefs::kFixedServersProxyModeName,
    false, true, true, IDS_POLICY_PROXY_MODE_FIXED_SERVERS_ERROR },
  { ProxyPrefs::kSystemProxyModeName,
    false, false, false, IDS_POLICY_PROXY_MODE_SYSTEM_ERROR },
};


// Helper functions ------------------------------------------------------------

std::string ValueTypeToString(Value::Type type) {
  static const char* strings[] = {
    "null",
    "boolean",
    "integer",
    "double",
    "string",
    "binary",
    "dictionary",
    "list"
  };
  DCHECK(static_cast<size_t>(type) < arraysize(strings));
  return std::string(strings[type]);
}


// TypeCheckingPolicyHandler implementation ------------------------------------

TypeCheckingPolicyHandler::TypeCheckingPolicyHandler(
    ConfigurationPolicyType policy_type,
    Value::Type value_type)
    : policy_type_(policy_type),
      value_type_(value_type) {
}

TypeCheckingPolicyHandler::~TypeCheckingPolicyHandler() {
}

ConfigurationPolicyType TypeCheckingPolicyHandler::policy_type() const {
  return policy_type_;
}

bool TypeCheckingPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  const Value* value = policies.Get(policy_type_);
  if (value && value_type_ != value->GetType()) {
    errors->AddError(policy_type_,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(value_type_));
    return false;
  }
  return true;
}


// SimplePolicyHandler implementation ------------------------------------------

SimplePolicyHandler::SimplePolicyHandler(
    ConfigurationPolicyType policy_type,
    Value::Type value_type,
    const char* pref_path)
    : TypeCheckingPolicyHandler(policy_type, value_type),
      pref_path_(pref_path) {
}

SimplePolicyHandler::~SimplePolicyHandler() {
}

void SimplePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                              PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  if (value)
    prefs->SetValue(pref_path_, value->DeepCopy());
}


// SyncPolicyHandler implementation --------------------------------------------

SyncPolicyHandler::SyncPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicySyncDisabled,
                                Value::TYPE_BOOLEAN) {
}

SyncPolicyHandler::~SyncPolicyHandler() {
}

void SyncPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                            PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  bool disable_sync;
  if (value && value->GetAsBoolean(&disable_sync) && disable_sync)
    prefs->SetValue(prefs::kSyncManaged, value->DeepCopy());
}


// AutofillPolicyHandler implementation ----------------------------------------

AutofillPolicyHandler::AutofillPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicyAutoFillEnabled,
                                Value::TYPE_BOOLEAN) {
}

AutofillPolicyHandler::~AutofillPolicyHandler() {
}

void AutofillPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  bool auto_fill_enabled;
  if (value && value->GetAsBoolean(&auto_fill_enabled) && !auto_fill_enabled) {
    prefs->SetValue(prefs::kAutofillEnabled,
                    Value::CreateBooleanValue(false));
  }
}


// DownloadDirPolicyHandler implementation -------------------------------------

DownloadDirPolicyHandler::DownloadDirPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicyDownloadDirectory,
                                Value::TYPE_STRING) {
}

DownloadDirPolicyHandler::~DownloadDirPolicyHandler() {
}

void DownloadDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                   PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  FilePath::StringType string_value;
  if (!value || !value->GetAsString(&string_value))
    return;

  FilePath::StringType expanded_value =
      policy::path_parser::ExpandPathVariables(string_value);
  // Make sure the path isn't empty, since that will point to an undefined
  // location; the default location is used instead in that case.
  // This is checked after path expansion because a non-empty policy value can
  // lead to an empty path value after expansion (e.g. "\"\"").
  if (expanded_value.empty())
    expanded_value = download_util::GetDefaultDownloadDirectory().value();
  prefs->SetValue(prefs::kDownloadDefaultDirectory,
                  Value::CreateStringValue(expanded_value));
  prefs->SetValue(prefs::kPromptForDownload,
                  Value::CreateBooleanValue(false));
}


// DiskCacheDirPolicyHandler implementation ------------------------------------

DiskCacheDirPolicyHandler::DiskCacheDirPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicyDiskCacheDir,
                                Value::TYPE_STRING) {
}

DiskCacheDirPolicyHandler::~DiskCacheDirPolicyHandler() {
}

void DiskCacheDirPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
  const Value* value = policies.Get(policy_type());
  FilePath::StringType string_value;
  if (value && value->GetAsString(&string_value)) {
    FilePath::StringType expanded_value =
        policy::path_parser::ExpandPathVariables(string_value);
    prefs->SetValue(prefs::kDiskCacheDir,
                    Value::CreateStringValue(expanded_value));
  }
}


// FileSelectionDialogsHandler implementation ----------------------------------

FileSelectionDialogsHandler::FileSelectionDialogsHandler()
    : TypeCheckingPolicyHandler(kPolicyAllowFileSelectionDialogs,
                                Value::TYPE_BOOLEAN) {
}

FileSelectionDialogsHandler::~FileSelectionDialogsHandler() {
}

void FileSelectionDialogsHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  bool allow_dialogs;
  const Value* value = policies.Get(policy_type());
  if (value && value->GetAsBoolean(&allow_dialogs)) {
    prefs->SetValue(prefs::kAllowFileSelectionDialogs,
                    Value::CreateBooleanValue(allow_dialogs));
    // Disallow selecting the download location if file dialogs are disabled.
    if (!allow_dialogs) {
      prefs->SetValue(prefs::kPromptForDownload,
                      Value::CreateBooleanValue(false));
    }
  }
}


// IncognitoModePolicyHandler implementation -----------------------------------

IncognitoModePolicyHandler::IncognitoModePolicyHandler() {
}

IncognitoModePolicyHandler::~IncognitoModePolicyHandler() {
}

bool IncognitoModePolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  int int_value = IncognitoModePrefs::ENABLED;
  const Value* availability = policies.Get(kPolicyIncognitoModeAvailability);

  if (availability) {
    if (availability->GetAsInteger(&int_value)) {
      IncognitoModePrefs::Availability availability_enum_value;
      if (!IncognitoModePrefs::IntToAvailability(int_value,
                                                 &availability_enum_value)) {
        errors->AddError(kPolicyIncognitoModeAvailability,
                         IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::IntToString(int_value));
        return false;
      }
    } else {
      errors->AddError(kPolicyIncognitoModeAvailability,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_INTEGER));
      return false;
    }
  } else {
    const Value* deprecated_enabled = policies.Get(kPolicyIncognitoEnabled);
    if (deprecated_enabled &&
        !deprecated_enabled->IsType(Value::TYPE_BOOLEAN)) {
      errors->AddError(kPolicyIncognitoEnabled,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_BOOLEAN));
      return false;
    }
  }
  return true;
}

void IncognitoModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  const Value* availability = policies.Get(kPolicyIncognitoModeAvailability);
  const Value* deprecated_enabled = policies.Get(kPolicyIncognitoEnabled);
  if (availability) {
    int int_value = IncognitoModePrefs::ENABLED;
    IncognitoModePrefs::Availability availability_enum_value;
    if (availability->GetAsInteger(&int_value) &&
        IncognitoModePrefs::IntToAvailability(int_value,
                                              &availability_enum_value)) {
      prefs->SetValue(prefs::kIncognitoModeAvailability,
                      Value::CreateIntegerValue(availability_enum_value));
    } else {
      NOTREACHED();
    }
  } else if (deprecated_enabled) {
    // If kPolicyIncognitoModeAvailability is not specified, check the obsolete
    // kPolicyIncognitoEnabled.
    bool enabled = true;
    if (deprecated_enabled->GetAsBoolean(&enabled)) {
      prefs->SetInteger(prefs::kIncognitoModeAvailability,
                        enabled ? IncognitoModePrefs::ENABLED :
                                  IncognitoModePrefs::DISABLED);
    } else {
      NOTREACHED();
    }
  }
}


// DefaultSearchEncodingsPolicyHandler implementation --------------------------

DefaultSearchEncodingsPolicyHandler::DefaultSearchEncodingsPolicyHandler()
    : TypeCheckingPolicyHandler(kPolicyDefaultSearchProviderEncodings,
                                Value::TYPE_LIST) {
}

DefaultSearchEncodingsPolicyHandler::~DefaultSearchEncodingsPolicyHandler() {
}

void DefaultSearchEncodingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies, PrefValueMap* prefs) {
  // The DefaultSearchProviderEncodings policy has type list, but the related
  // preference has type string. Convert one into the other here, using
  // ';' as a separator.
  const Value* value = policies.Get(policy_type());
  const ListValue* list;
  if (!value || !value->GetAsList(&list))
    return;

  ListValue::const_iterator iter(list->begin());
  ListValue::const_iterator end(list->end());
  std::vector<std::string> string_parts;
  for (; iter != end; ++iter) {
    std::string s;
    if ((*iter)->GetAsString(&s)) {
      string_parts.push_back(s);
    }
  }
  std::string encodings = JoinString(string_parts, ';');
  prefs->SetValue(prefs::kDefaultSearchProviderEncodings,
                  Value::CreateStringValue(encodings));
}


// DefaultSearchPolicyHandler implementation -----------------------------------

DefaultSearchPolicyHandler::DefaultSearchPolicyHandler() {
  for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
    ConfigurationPolicyType policy_type =
        kDefaultSearchPolicyMap[i].policy_type;
    if (policy_type == kPolicyDefaultSearchProviderEncodings) {
      handlers_.push_back(new DefaultSearchEncodingsPolicyHandler());
    } else {
      handlers_.push_back(
          new SimplePolicyHandler(policy_type,
                                  kDefaultSearchPolicyMap[i].value_type,
                                  kDefaultSearchPolicyMap[i].preference_path));
    }
  }
}

DefaultSearchPolicyHandler::~DefaultSearchPolicyHandler() {
  STLDeleteElements(&handlers_);
}

bool DefaultSearchPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                     PolicyErrorMap* errors) {
  if (!CheckIndividualPolicies(policies, errors))
    return false;

  if (DefaultSearchProviderIsDisabled(policies)) {
    // Add an error for all specified default search policies except
    // DefaultSearchProviderEnabled.
    for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
      ConfigurationPolicyType policy_type =
          kDefaultSearchPolicyMap[i].policy_type;
      if (policy_type != kPolicyDefaultSearchProviderEnabled &&
          HasDefaultSearchPolicy(policies, policy_type)) {
        errors->AddError(policy_type, IDS_POLICY_DEFAULT_SEARCH_DISABLED);
      }
    }
    return true;
  }

  const Value* search_url =
      policies.Get(kPolicyDefaultSearchProviderSearchURL);
  if (!search_url && AnyDefaultSearchPoliciesSpecified(policies)) {
    errors->AddError(kPolicyDefaultSearchProviderSearchURL,
                     IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  }

  if (search_url && !DefaultSearchURLIsValid(policies)) {
    errors->AddError(kPolicyDefaultSearchProviderSearchURL,
                     IDS_POLICY_INVALID_SEARCH_URL_ERROR);
    return false;
  }
  return true;
}

void DefaultSearchPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                     PrefValueMap* prefs) {
  if (DefaultSearchProviderIsDisabled(policies)) {
    // If default search is disabled, the other fields are ignored.
    prefs->SetString(prefs::kDefaultSearchProviderName, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderSearchURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderSuggestURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderIconURL, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderEncodings, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderKeyword, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderInstantURL, std::string());
    return;
  }

  const Value* search_url =
      policies.Get(kPolicyDefaultSearchProviderSearchURL);
  // The search URL is required.
  if (!search_url)
    return;

  // The other entries are optional.  Just make sure that they are all
  // specified via policy, so that the regular prefs aren't used.
  if (DefaultSearchURLIsValid(policies)) {
    HandlerList::const_iterator handler = handlers_.begin();
    for ( ; handler != handlers_.end(); ++handler)
      (*handler)->ApplyPolicySettings(policies, prefs);

    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderSuggestURL);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderIconURL);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderEncodings);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderKeyword);
    EnsureStringPrefExists(prefs, prefs::kDefaultSearchProviderInstantURL);

    // For the name, default to the host if not specified.
    std::string name;
    if (!prefs->GetString(prefs::kDefaultSearchProviderName, &name) ||
        name.empty()) {
      std::string search_url_string;
      if (search_url->GetAsString(&search_url_string)) {
        prefs->SetString(prefs::kDefaultSearchProviderName,
                         GURL(search_url_string).host());
      }
    }

    // And clear the IDs since these are not specified via policy.
    prefs->SetString(prefs::kDefaultSearchProviderID, std::string());
    prefs->SetString(prefs::kDefaultSearchProviderPrepopulateID,
                     std::string());
  }
}

bool DefaultSearchPolicyHandler::CheckIndividualPolicies(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  HandlerList::const_iterator handler = handlers_.begin();
  for ( ; handler != handlers_.end(); ++handler) {
    if (!(*handler)->CheckPolicySettings(policies, errors))
      return false;
  }
  return true;
}

bool DefaultSearchPolicyHandler::HasDefaultSearchPolicy(
    const PolicyMap& policies,
    ConfigurationPolicyType policy_type) {
  return policies.Get(policy_type) != NULL;
}

bool DefaultSearchPolicyHandler::AnyDefaultSearchPoliciesSpecified(
    const PolicyMap& policies) {
  for (size_t i = 0; i < arraysize(kDefaultSearchPolicyMap); ++i) {
    if (policies.Get(kDefaultSearchPolicyMap[i].policy_type))
      return true;
  }
  return false;
}

bool DefaultSearchPolicyHandler::DefaultSearchProviderIsDisabled(
    const PolicyMap& policies) {
  const Value* provider_enabled =
      policies.Get(kPolicyDefaultSearchProviderEnabled);
  bool enabled = true;
  return provider_enabled &&
         provider_enabled->GetAsBoolean(&enabled) &&
         !enabled;
}

bool DefaultSearchPolicyHandler::DefaultSearchURLIsValid(
    const PolicyMap& policies) {
  const Value* search_url =
      policies.Get(kPolicyDefaultSearchProviderSearchURL);
  if (!search_url)
    return false;

  std::string search_url_string;
  if (search_url->GetAsString(&search_url_string)) {
    SearchTermsDataForValidation search_terms_data;
    const TemplateURLRef search_url_ref(search_url_string, 0, 0);
    // It must support replacement (which implies it is valid).
    return search_url_ref.SupportsReplacementUsingTermsData(search_terms_data);
  }
  return false;
}

void DefaultSearchPolicyHandler::EnsureStringPrefExists(
    PrefValueMap* prefs,
    const std::string& path) {
  std::string value;
  if (!prefs->GetString(path, &value))
    prefs->SetString(path, value);
}


// ProxyPolicyHandler implementation -------------------------------------------

ProxyPolicyHandler::ProxyPolicyHandler() {
}

ProxyPolicyHandler::~ProxyPolicyHandler() {
}

bool ProxyPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                             PolicyErrorMap* errors) {
  const Value* mode = GetProxyPolicyValue(policies, kPolicyProxyMode);
  const Value* server = GetProxyPolicyValue(policies, kPolicyProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, kPolicyProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, kPolicyProxyPacUrl);
  const Value* bypass_list =
      GetProxyPolicyValue(policies, kPolicyProxyBypassList);

  if ((server || pac_url || bypass_list) && !(mode || server_mode)) {
    errors->AddError(kPolicyProxyMode,
                     IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  }

  std::string mode_value;
  if (!CheckProxyModeAndServerMode(policies, errors, &mode_value))
    return false;

  // If neither ProxyMode nor ProxyServerMode are specified, mode_value will be
  // empty and the proxy shouldn't be configured at all.
  if (mode_value.empty())
    return true;

  bool is_valid_mode = false;
  for (size_t i = 0; i != arraysize(kProxyModeValidationMap); ++i) {
    const ProxyModeValidationEntry& entry = kProxyModeValidationMap[i];
    if (entry.mode_value != mode_value)
      continue;

    is_valid_mode = true;

    if (!entry.pac_url_allowed && pac_url)
      errors->AddError(kPolicyProxyPacUrl, entry.error_message_id);
    if (!entry.bypass_list_allowed && bypass_list)
      errors->AddError(kPolicyProxyBypassList, entry.error_message_id);
    if (!entry.server_allowed && server)
      errors->AddError(kPolicyProxyServer, entry.error_message_id);

    if ((!entry.pac_url_allowed && pac_url) ||
        (!entry.bypass_list_allowed && bypass_list) ||
        (!entry.server_allowed && server)) {
      return false;
    }
  }

  if (!is_valid_mode) {
    errors->AddError(mode ? kPolicyProxyMode : kPolicyProxyServerMode,
                     IDS_POLICY_OUT_OF_RANGE_ERROR, mode_value);
    return false;
  }
  return true;
}

void ProxyPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                             PrefValueMap* prefs) {
  const Value* mode = GetProxyPolicyValue(policies, kPolicyProxyMode);
  const Value* server = GetProxyPolicyValue(policies, kPolicyProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, kPolicyProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, kPolicyProxyPacUrl);
  const Value* bypass_list =
      GetProxyPolicyValue(policies, kPolicyProxyBypassList);

  ProxyPrefs::ProxyMode proxy_mode;
  if (mode) {
    std::string string_mode;
    CHECK(mode->GetAsString(&string_mode));
    CHECK(ProxyPrefs::StringToProxyMode(string_mode, &proxy_mode));
  } else if (server_mode) {
    int int_mode = 0;
    CHECK(server_mode->GetAsInteger(&int_mode));

    switch (int_mode) {
      case kPolicyNoProxyServerMode:
        proxy_mode = ProxyPrefs::MODE_DIRECT;
        break;
      case kPolicyAutoDetectProxyServerMode:
        proxy_mode = ProxyPrefs::MODE_AUTO_DETECT;
        break;
      case kPolicyManuallyConfiguredProxyServerMode:
        proxy_mode = ProxyPrefs::MODE_FIXED_SERVERS;
        if (pac_url)
          proxy_mode = ProxyPrefs::MODE_PAC_SCRIPT;
        break;
      case kPolicyUseSystemProxyServerMode:
        proxy_mode = ProxyPrefs::MODE_SYSTEM;
        break;
      default:
        proxy_mode = ProxyPrefs::MODE_DIRECT;
        NOTREACHED();
    }
  } else {
    return;
  }

  switch (proxy_mode) {
    case ProxyPrefs::MODE_DIRECT:
      prefs->SetValue(prefs::kProxy, ProxyConfigDictionary::CreateDirect());
      break;
    case ProxyPrefs::MODE_AUTO_DETECT:
      prefs->SetValue(prefs::kProxy, ProxyConfigDictionary::CreateAutoDetect());
      break;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string pac_url_string;
      if (pac_url->GetAsString(&pac_url_string)) {
        prefs->SetValue(prefs::kProxy,
            ProxyConfigDictionary::CreatePacScript(pac_url_string, false));
      } else {
        NOTREACHED();
      }
      break;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      std::string bypass_list_string;
      if (server->GetAsString(&proxy_server)) {
        if (bypass_list)
          bypass_list->GetAsString(&bypass_list_string);
        prefs->SetValue(prefs::kProxy,
                        ProxyConfigDictionary::CreateFixedServers(
                            proxy_server, bypass_list_string));
      }
      break;
    }
    case ProxyPrefs::MODE_SYSTEM:
      prefs->SetValue(prefs::kProxy,
                      ProxyConfigDictionary::CreateSystem());
      break;
    case ProxyPrefs::kModeCount:
      NOTREACHED();
  }
}

const Value* ProxyPolicyHandler::GetProxyPolicyValue(
    const PolicyMap& policies, ConfigurationPolicyType policy) {
  const Value* value = policies.Get(policy);
  std::string tmp;
  if (!value ||
      value->IsType(Value::TYPE_NULL) ||
      (value->IsType(Value::TYPE_STRING) &&
      value->GetAsString(&tmp) &&
      tmp.empty())) {
    return NULL;
  }
  return value;
}

bool ProxyPolicyHandler::CheckProxyModeAndServerMode(const PolicyMap& policies,
                                                     PolicyErrorMap* errors,
                                                     std::string* mode_value) {
  const Value* mode = GetProxyPolicyValue(policies, kPolicyProxyMode);
  const Value* server = GetProxyPolicyValue(policies, kPolicyProxyServer);
  const Value* server_mode =
      GetProxyPolicyValue(policies, kPolicyProxyServerMode);
  const Value* pac_url = GetProxyPolicyValue(policies, kPolicyProxyPacUrl);

  // If there's a server mode, convert it into a mode.
  // When both are specified, the mode takes precedence.
  if (mode) {
    if (server_mode) {
      errors->AddError(kPolicyProxyServerMode,
                       IDS_POLICY_OVERRIDDEN,
                       GetPolicyName(kPolicyProxyMode));
    }
    if (!mode->GetAsString(mode_value)) {
      errors->AddError(kPolicyProxyMode,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_BOOLEAN));
      return false;
    }

    ProxyPrefs::ProxyMode mode;
    if (!ProxyPrefs::StringToProxyMode(*mode_value, &mode)) {
      errors->AddError(kPolicyProxyMode, IDS_POLICY_INVALID_PROXY_MODE_ERROR);
      return false;
    }

    if (mode == ProxyPrefs::MODE_PAC_SCRIPT && !pac_url) {
      errors->AddError(kPolicyProxyPacUrl, IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    } else if (mode == ProxyPrefs::MODE_FIXED_SERVERS && !server) {
      errors->AddError(kPolicyProxyServer, IDS_POLICY_NOT_SPECIFIED_ERROR);
      return false;
    }
  } else if (server_mode) {
    int server_mode_value;
    if (!server_mode->GetAsInteger(&server_mode_value)) {
      errors->AddError(kPolicyProxyServerMode,
                       IDS_POLICY_TYPE_ERROR,
                       ValueTypeToString(Value::TYPE_INTEGER));
      return false;
    }

    switch (server_mode_value) {
      case kPolicyNoProxyServerMode:
        *mode_value = ProxyPrefs::kDirectProxyModeName;
        break;
      case kPolicyAutoDetectProxyServerMode:
        *mode_value = ProxyPrefs::kAutoDetectProxyModeName;
        break;
      case kPolicyManuallyConfiguredProxyServerMode:
        if (server && pac_url) {
          int message_id = IDS_POLICY_PROXY_BOTH_SPECIFIED_ERROR;
          errors->AddError(kPolicyProxyServer, message_id);
          errors->AddError(kPolicyProxyPacUrl, message_id);
          return false;
        }
        if (!server && !pac_url) {
          int message_id = IDS_POLICY_PROXY_NEITHER_SPECIFIED_ERROR;
          errors->AddError(kPolicyProxyServer, message_id);
          errors->AddError(kPolicyProxyPacUrl, message_id);
          return false;
        }
        if (pac_url)
          *mode_value = ProxyPrefs::kPacScriptProxyModeName;
        else
          *mode_value = ProxyPrefs::kFixedServersProxyModeName;
        break;
      case kPolicyUseSystemProxyServerMode:
        *mode_value = ProxyPrefs::kSystemProxyModeName;
        break;
      default:
        errors->AddError(kPolicyProxyServerMode,
                         IDS_POLICY_OUT_OF_RANGE_ERROR,
                         base::IntToString(server_mode_value));
        return false;
    }
  }
  return true;
}


// JavascriptPolicyHandler implementation --------------------------------------

JavascriptPolicyHandler::JavascriptPolicyHandler() {
}

JavascriptPolicyHandler::~JavascriptPolicyHandler() {
}

bool JavascriptPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                  PolicyErrorMap* errors) {
  const Value* javascript_enabled = policies.Get(kPolicyJavascriptEnabled);
  const Value* default_setting = policies.Get(kPolicyDefaultJavaScriptSetting);

  if (javascript_enabled && !javascript_enabled->IsType(Value::TYPE_BOOLEAN)) {
    errors->AddError(kPolicyJavascriptEnabled,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(Value::TYPE_BOOLEAN));
  }

  if (default_setting && !default_setting->IsType(Value::TYPE_INTEGER)) {
    errors->AddError(kPolicyDefaultJavaScriptSetting,
                     IDS_POLICY_TYPE_ERROR,
                     ValueTypeToString(Value::TYPE_INTEGER));
  }

  if (javascript_enabled && default_setting) {
    errors->AddError(kPolicyJavascriptEnabled,
                     IDS_POLICY_OVERRIDDEN,
                     GetPolicyName(kPolicyDefaultJavaScriptSetting));
  }

  return true;
}

void JavascriptPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                  PrefValueMap* prefs) {
  int setting = CONTENT_SETTING_DEFAULT;
  const Value* default_setting = policies.Get(kPolicyDefaultJavaScriptSetting);

  if (default_setting) {
    default_setting->GetAsInteger(&setting);
  } else {
    const Value* javascript_enabled = policies.Get(kPolicyJavascriptEnabled);
    bool enabled = true;
    if (javascript_enabled &&
        javascript_enabled->GetAsBoolean(&enabled) &&
        !enabled) {
      setting = CONTENT_SETTING_BLOCK;
    }
  }

  if (setting != CONTENT_SETTING_DEFAULT) {
    prefs->SetValue(prefs::kManagedDefaultJavaScriptSetting,
                    Value::CreateIntegerValue(setting));
  }
}


}  // namespace


// ConfigurationPolicyHandler implementation -----------------------------------

// static
ConfigurationPolicyHandler::HandlerList*
    ConfigurationPolicyHandler::CreateHandlerList() {
  HandlerList* list = new HandlerList;

  for (size_t i = 0; i < arraysize(kSimplePolicyMap); ++i) {
    list->push_back(
        new SimplePolicyHandler(kSimplePolicyMap[i].policy_type,
                                kSimplePolicyMap[i].value_type,
                                kSimplePolicyMap[i].preference_path));
  }

  list->push_back(new AutofillPolicyHandler());
  list->push_back(new DefaultSearchPolicyHandler());
  list->push_back(new DiskCacheDirPolicyHandler());
  list->push_back(new FileSelectionDialogsHandler());
  list->push_back(new IncognitoModePolicyHandler());
  list->push_back(new JavascriptPolicyHandler());
  list->push_back(new ProxyPolicyHandler());
  list->push_back(new SyncPolicyHandler());

#if !defined(OS_CHROMEOS)
  list->push_back(new DownloadDirPolicyHandler());
#endif  // !defined(OS_CHROME0S)

  return list;
}

}  // namespace policy
