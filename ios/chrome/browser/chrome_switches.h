// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
#define IOS_CHROME_BROWSER_CHROME_SWITCHES_H_

// Defines all the command-line switches used by iOS Chrome.

namespace switches {

extern const char kDisableContextualSearch[];
extern const char kDisableIOSFeatures[];
extern const char kDisableIOSPasswordSuggestions[];
extern const char kDisableNTPFavicons[];
extern const char kDisableThirdPartyKeyboardWorkaround[];

extern const char kEnableContextualSearch[];
extern const char kEnableIOSFeatures[];
extern const char kEnableIOSHandoffToOtherDevices[];
extern const char kEnableNTPFavicons[];
extern const char kEnableSpotlightActions[];
extern const char kEnableThirdPartyKeyboardWorkaround[];

extern const char kIOSForceVariationIds[];
extern const char kUserAgent[];

extern const char kIOSHostResolverRules[];
extern const char kIOSIgnoreCertificateErrors[];
extern const char kIOSTestingFixedHttpPort[];
extern const char kIOSTestingFixedHttpsPort[];

}  // namespace switches

#endif  // IOS_CHROME_BROWSER_CHROME_SWITCHES_H_
