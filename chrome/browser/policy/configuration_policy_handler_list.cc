// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/configuration_policy_handler_list.h"

#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/policy/configuration_policy_handler.h"
#include "chrome/browser/policy/policy_error_map.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_value_map.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/policy/configuration_policy_handler_chromeos.h"
#endif

namespace policy {

namespace {

// Maps a policy type to a preference path, and to the expected value type.
// This is the entry type of |kSimplePolicyMap| below.
struct PolicyToPreferenceMapEntry {
  base::Value::Type value_type;
  ConfigurationPolicyType policy_type;
  const char* preference_path;
};

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
  { Value::TYPE_INTEGER, kPolicyDiskCacheSize,
    prefs::kDiskCacheSize },
  { Value::TYPE_INTEGER, kPolicyMediaCacheSize,
    prefs::kMediaCacheSize },
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

}  // namespace

ConfigurationPolicyHandlerList::ConfigurationPolicyHandlerList() {
  for (size_t i = 0; i < arraysize(kSimplePolicyMap); ++i) {
    handlers_.push_back(
        new SimplePolicyHandler(kSimplePolicyMap[i].policy_type,
                                kSimplePolicyMap[i].value_type,
                                kSimplePolicyMap[i].preference_path));
  }

  handlers_.push_back(new AutofillPolicyHandler());
  handlers_.push_back(new DefaultSearchPolicyHandler());
  handlers_.push_back(new DiskCacheDirPolicyHandler());
  handlers_.push_back(new FileSelectionDialogsHandler());
  handlers_.push_back(new IncognitoModePolicyHandler());
  handlers_.push_back(new JavascriptPolicyHandler());
  handlers_.push_back(new ProxyPolicyHandler());
  handlers_.push_back(new SyncPolicyHandler());

#if !defined(OS_CHROMEOS)
  handlers_.push_back(new DownloadDirPolicyHandler());
#endif  // !defined(OS_CHROME0S)

#if defined(OS_CHROMEOS)
  handlers_.push_back(
      new NetworkConfigurationPolicyHandler(
          kPolicyDeviceOpenNetworkConfiguration));
  handlers_.push_back(
      new NetworkConfigurationPolicyHandler(
          kPolicyOpenNetworkConfiguration));
#endif
}

ConfigurationPolicyHandlerList::~ConfigurationPolicyHandlerList() {
  STLDeleteElements(&handlers_);
}

void ConfigurationPolicyHandlerList::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs,
    PolicyErrorMap* errors) const {
  PolicyErrorMap scoped_errors;
  if (!errors)
    errors = &scoped_errors;

  std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
  for (handler = handlers_.begin(); handler != handlers_.end(); ++handler) {
    if ((*handler)->CheckPolicySettings(policies, errors) && prefs)
      (*handler)->ApplyPolicySettings(policies, prefs);
  }

  for (PolicyMap::const_iterator it = policies.begin();
       it != policies.end();
       ++it) {
    if (IsDeprecatedPolicy(it->first))
      errors->AddError(it->first, IDS_POLICY_DEPRECATED);
  }
}

void ConfigurationPolicyHandlerList::PrepareForDisplaying(
    PolicyMap* policies) const {
  std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
  for (handler = handlers_.begin(); handler != handlers_.end(); ++handler)
    (*handler)->PrepareForDisplaying(policies);
}

}  // namespace policy
