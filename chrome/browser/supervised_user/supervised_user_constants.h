// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_CONSTANTS_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_CONSTANTS_H_

namespace supervised_users {

// Keys for supervised user settings. These are configured remotely and mapped
// to preferences by the SupervisedUserPrefStore.
extern const char kContentPackDefaultFilteringBehavior[];
extern const char kContentPackManualBehaviorHosts[];
extern const char kContentPackManualBehaviorURLs[];
extern const char kForceSafeSearch[];
extern const char kSigninAllowed[];
extern const char kUserName[];

extern const char kUserPasswordRecord[];

// A pseudo-email address for systems that expect well-formed email addresses
// (like Sync), even though we're not signed in.
extern const char kSupervisedUserPseudoEmail[];

// Keys for supervised user shared settings. These can be configured remotely or
// locally, and are mapped to preferences by the
// SupervisedUserPrefMappingService.
extern const char kChromeAvatarIndex[];
extern const char kChromeOSAvatarIndex[];
extern const char kChromeOSPasswordData[];

}  // namespace supervised_users

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_CONSTANTS_H_
