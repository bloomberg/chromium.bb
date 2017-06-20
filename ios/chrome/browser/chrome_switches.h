// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
#define IOS_CHROME_BROWSER_CHROME_SWITCHES_H_

// Defines all the command-line switches used by iOS Chrome.

namespace switches {

extern const char kDisableContextualSearch[];
extern const char kDisableIOSFastWebScrollViewInsets[];
extern const char kDisableIOSFeatures[];
extern const char kDisableIOSPasswordGeneration[];
extern const char kDisableIOSPasswordSuggestions[];
extern const char kDisableLRUSnapshotCache[];
extern const char kDisableNTPFavicons[];
extern const char kDisableOfflineAutoReload[];
extern const char kDisablePaymentRequest[];
extern const char kDisableTabStripAutoScrollNewTabs[];
extern const char kDisableIOSPhysicalWeb[];
extern const char kDisableRequestMobileSite[];
extern const char kDisableSuggestionsUI[];
extern const char kDisableBookmarkReordering[];
extern const char kDisableSlimNavigationManager[];

extern const char kEnableContextualSearch[];
extern const char kEnableIOSFastWebScrollViewInsets[];
extern const char kEnableIOSFeatures[];
extern const char kEnableIOSHandoffToOtherDevices[];
extern const char kEnableIOSPasswordGeneration[];
extern const char kEnableLRUSnapshotCache[];
extern const char kEnableNTPFavicons[];
extern const char kEnableOfflineAutoReload[];
extern const char kEnablePaymentRequest[];
extern const char kEnableReaderModeToolbarIcon[];
extern const char kEnableSpotlightActions[];
extern const char kEnableIOSPhysicalWeb[];
extern const char kEnableSuggestionsUI[];
extern const char kEnableBookmarkReordering[];
extern const char kEnableSlimNavigationManager[];

extern const char kIOSForceVariationIds[];
extern const char kUserAgent[];

extern const char kIOSHostResolverRules[];
extern const char kIOSIgnoreCertificateErrors[];
extern const char kIOSTestingFixedHttpPort[];
extern const char kIOSTestingFixedHttpsPort[];

}  // namespace switches

#endif  // IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
