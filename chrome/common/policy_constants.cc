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
const char kShowHomeButton[] = "ShowHomeButton";

}  // namespace key

}  // namespace policy
