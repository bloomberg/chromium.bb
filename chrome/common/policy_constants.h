// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_POLICY_CONSTANTS_H_
#define CHROME_COMMON_POLICY_CONSTANTS_H_
#pragma once

#include "build/build_config.h"

namespace policy {

#if defined(OS_WIN)
// The windows registry path we read the policy configuration from.
extern const wchar_t kRegistrySubKey[];
#endif

// Key names for the policy settings.
namespace key {

extern const char kHomepageLocation[];
extern const char kHomepageIsNewTabPage[];
extern const char kRestoreOnStartup[];
extern const char kURLsToRestoreOnStartup[];
extern const char kDefaultSearchProviderEnabled[];
extern const char kDefaultSearchProviderName[];
extern const char kDefaultSearchProviderKeyword[];
extern const char kDefaultSearchProviderSearchURL[];
extern const char kDefaultSearchProviderSuggestURL[];
extern const char kDefaultSearchProviderInstantURL[];
extern const char kDefaultSearchProviderIconURL[];
extern const char kDefaultSearchProviderEncodings[];
extern const char kDisableSpdy[];
extern const char kProxyMode[];
extern const char kProxyServerMode[];
extern const char kProxyServer[];
extern const char kProxyPacUrl[];
extern const char kProxyBypassList[];
extern const char kAlternateErrorPagesEnabled[];
extern const char kSearchSuggestEnabled[];
extern const char kDnsPrefetchingEnabled[];
extern const char kSafeBrowsingEnabled[];
extern const char kMetricsReportingEnabled[];
extern const char kPasswordManagerEnabled[];
extern const char kPasswordManagerAllowShowPasswords[];
extern const char kDisabledPlugins[];
extern const char kAutoFillEnabled[];
extern const char kApplicationLocaleValue[];
extern const char kSyncDisabled[];
extern const char kExtensionInstallAllowList[];
extern const char kExtensionInstallDenyList[];
extern const char kExtensionInstallForceList[];
extern const char kShowHomeButton[];
extern const char kPrintingEnabled[];
extern const char kJavascriptEnabled[];
extern const char kSavingBrowserHistoryDisabled[];
extern const char kDeveloperToolsDisabled[];
extern const char kBlockThirdPartyCookies[];
extern const char kDefaultCookiesSetting[];
extern const char kDefaultImagesSetting[];
extern const char kDefaultJavaScriptSetting[];
extern const char kDefaultPluginsSetting[];
extern const char kDefaultPopupsSetting[];
extern const char kDefaultNotificationSetting[];
extern const char kDefaultGeolocationSetting[];
extern const char kAuthSchemes[];
extern const char kDisableAuthNegotiateCnameLookup[];
extern const char kEnableAuthNegotiatePort[];
extern const char kAuthServerWhitelist[];
extern const char kAuthNegotiateDelegateWhitelist[];
extern const char kGSSAPILibraryName[];
extern const char kDisable3DAPIs[];
extern const char kPolicyRefreshRate[];

// Chrome Frame specific policy constants
extern const char kChromeFrameRendererSettings[];
extern const char kRenderInChromeFrameList[];
extern const char kRenderInHostList[];
extern const char kChromeFrameContentTypes[];

#if defined(OS_CHROMEOS)
// ChromeOS policy constants
extern const char kChromeOsLockOnIdleSuspend[];
#endif

}  // namespace key

}  // namespace policy

#endif  // CHROME_COMMON_POLICY_CONSTANTS_H_
