// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
#define IOS_CHROME_BROWSER_CHROME_SWITCHES_H_

// Defines all the command-line switches used by iOS Chrome.

namespace switches {

extern const char kDisableAllBookmarksView[];
extern const char kDisableContextualSearch[];
extern const char kDisableIOSFastWebScrollViewInsets[];
extern const char kDisableIOSFeatures[];
extern const char kDisableIOSPasswordGeneration[];
extern const char kDisableIOSPasswordSuggestions[];
extern const char kDisableIOSWKWebView[];
extern const char kDisableLRUSnapshotCache[];
extern const char kDisableNTPFavicons[];
extern const char kDisableOfflineAutoReload[];
extern const char kDisableTabEviction[];
extern const char kDisableTabSwitcher[];
extern const char kDisableIOSPhysicalWeb[];

extern const char kEnableAllBookmarksView[];
extern const char kEnableContextualSearch[];
extern const char kEnableCredentialManagerAPI[];
extern const char kEnableIOSFastWebScrollViewInsets[];
extern const char kEnableIOSFeatures[];
extern const char kEnableIOSHandoffToOtherDevices[];
extern const char kEnableIOSPasswordGeneration[];
extern const char kEnableIOSWKWebView[];
extern const char kEnableLRUSnapshotCache[];
extern const char kEnableNTPFavicons[];
extern const char kEnableOfflineAutoReload[];
extern const char kEnableReaderModeToolbarIcon[];
extern const char kEnableTabEviction[];
extern const char kEnableTabSwitcher[];
extern const char kEnableIOSPhysicalWeb[];

extern const char kIOSForceVariationIds[];
extern const char kIOSMetricsRecordingOnly[];
extern const char kUserAgent[];

extern const char kIOSHostResolverRules[];
extern const char kIOSIgnoreCertificateErrors[];
extern const char kIOSTestingFixedHttpPort[];
extern const char kIOSTestingFixedHttpsPort[];

// TODO(crbug.com/567136): those switches are duplicated between desktop
// and iOS. Once the corresponding code has been componentized or is no longer
// used by iOS, remove the duplicate definition.
extern const char kHistoryEnableGroupByDomain[];
extern const char kMarkNonSecureAs[];
extern const char kMarkNonSecureAsNeutral[];
extern const char kMarkNonSecureAsNonSecure[];

}  // namespace switches

#endif  // IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
