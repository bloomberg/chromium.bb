// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/policy_constants.h"

namespace policy {

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kRegistrySubKey[] = L"SOFTWARE\\Policies\\Google\\Chrome";
#else
const wchar_t kRegistrySubKey[] = L"SOFTWARE\\Policies\\Chromium";
#endif
#endif

namespace key {

const char kHomepageLocation[] = "HomepageLocation";
const char kHomepageIsNewTabPage[] = "HomepageIsNewTabPage";
const char kRestoreOnStartup[] = "RestoreOnStartup";
const char kURLsToRestoreOnStartup[] = "RestoreOnStartupURLs";
const char kDefaultSearchProviderEnabled[] = "DefaultSearchProviderEnabled";
const char kDefaultSearchProviderName[] = "DefaultSearchProviderName";
const char kDefaultSearchProviderKeyword[] = "DefaultSearchProviderKeyword";
const char kDefaultSearchProviderSearchURL[] =
    "DefaultSearchProviderSearchURL";
const char kDefaultSearchProviderSuggestURL[] =
    "DefaultSearchProviderSuggestURL";
const char kDefaultSearchProviderInstantURL[] =
    "DefaultSearchProviderInstantURL";
const char kDefaultSearchProviderIconURL[] =
    "DefaultSearchProviderIconURL";
const char kDefaultSearchProviderEncodings[] =
    "DefaultSearchProviderEncodings";
const char kDisableSpdy[] = "DisableSpdy";
const char kProxyMode[] = "ProxyMode";
// Deprecated name of policy to set proxy server mode
const char kProxyServerMode[] = "ProxyServerMode";
const char kProxyServer[] = "ProxyServer";
const char kProxyPacUrl[] = "ProxyPacUrl";
const char kProxyBypassList[] = "ProxyBypassList";
const char kAlternateErrorPagesEnabled[] = "AlternateErrorPagesEnabled";
const char kSearchSuggestEnabled[] = "SearchSuggestEnabled";
const char kDnsPrefetchingEnabled[] = "DnsPrefetchingEnabled";
const char kSafeBrowsingEnabled[] = "SafeBrowsingEnabled";
const char kMetricsReportingEnabled[] = "MetricsReportingEnabled";
const char kPasswordManagerEnabled[] = "PasswordManagerEnabled";
const char kPasswordManagerAllowShowPasswords[] =
    "PasswordManagerAllowShowPasswords";
const char kDisabledPlugins[] = "DisabledPlugins";
const char kAutoFillEnabled[] = "AutoFillEnabled";
const char kApplicationLocaleValue[] = "ApplicationLocaleValue";
const char kSyncDisabled[] = "SyncDisabled";
const char kExtensionInstallAllowList[] = "ExtensionInstallWhitelist";
const char kExtensionInstallDenyList[] = "ExtensionInstallBlacklist";
const char kExtensionInstallForceList[] = "ExtensionInstallForcelist";
const char kShowHomeButton[] = "ShowHomeButton";
const char kPrintingEnabled[] = "PrintingEnabled";
const char kJavascriptEnabled[] = "JavascriptEnabled";
const char kSavingBrowserHistoryDisabled[] = "SavingBrowserHistoryDisabled";
const char kDeveloperToolsDisabled[] = "DeveloperToolsDisabled";
const char kBlockThirdPartyCookies[] = "BlockThirdPartyCookies";
const char kDefaultCookiesSetting[] = "DefaultCookiesSetting";
const char kDefaultImagesSetting[] = "DefaultImagesSetting";
const char kDefaultJavaScriptSetting[] = "DefaultJavaScriptSetting";
const char kDefaultPluginsSetting[] = "DefaultPluginsSetting";
const char kDefaultPopupsSetting[] = "DefaultPopupsSetting";
const char kDefaultNotificationSetting[] = "DefaultNotificationSetting";
const char kDefaultGeolocationSetting[] = "DefaultGeolocationSetting";
const char kAuthSchemes[] = "AuthSchemes";
const char kDisableAuthNegotiateCnameLookup[] =
    "DisableAuthNegotiateCnameLookup";
const char kEnableAuthNegotiatePort[] = "EnableAuthNegotiatePort";
const char kAuthServerWhitelist[] = "AuthServerWhitelist";
const char kAuthNegotiateDelegateWhitelist[] = "AuthNegotiateDelegateWhitelist";
const char kGSSAPILibraryName[] = "GSSAPILibraryName";
const char kDisable3DAPIs[] = "Disable3DAPIs";
const char kPolicyRefreshRate[] = "PolicyRefreshRate";

// Chrome Frame specific policy constants
const char kChromeFrameRendererSettings[] = "ChromeFrameRendererSettings";
const char kRenderInChromeFrameList[] = "RenderInChromeFrameList";
const char kRenderInHostList[] = "RenderInHostList";
const char kChromeFrameContentTypes[] = "ChromeFrameContentTypes";

#if defined(OS_CHROMEOS)
// ChromeOS policy constants
const char kChromeOsLockOnIdleSuspend[] = "ChromeOsLockOnIdleSuspend";
#endif

}  // namespace key

}  // namespace policy
