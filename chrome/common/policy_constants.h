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
extern const char kDefaultSearchProviderName[];
extern const char kDefaultSearchProviderKeyword[];
extern const char kDefaultSearchProviderSearchURL[];
extern const char kDefaultSearchProviderSuggestURL[];
extern const char kDefaultSearchProviderIconURL[];
extern const char kDefaultSearchProviderEncodings[];
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
extern const char kDisabledPlugins[];
extern const char kAutoFillEnabled[];
extern const char kApplicationLocaleValue[];
extern const char kSyncDisabled[];
extern const char kExtensionInstallAllowList[];
extern const char kExtensionInstallDenyList[];
extern const char kShowHomeButton[];

}  // namespace key

}  // namespace policy

#endif  // CHROME_COMMON_POLICY_CONSTANTS_H_
