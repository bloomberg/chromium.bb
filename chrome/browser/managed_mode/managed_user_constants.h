// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_CONSTANTS_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_CONSTANTS_H_

namespace managed_users {

// Keys for managed user settings. These are configured remotely and mapped to
// preferences by the SupervisedUserPrefStore.
extern const char kContentPackDefaultFilteringBehavior[];
extern const char kContentPackManualBehaviorHosts[];
extern const char kContentPackManualBehaviorURLs[];
extern const char kForceSafeSearch[];
extern const char kSigninAllowed[];
extern const char kUserName[];

extern const char kUserPasswordRecord[];

// A pseudo-email address for systems that expect well-formed email addresses
// (like Sync), even though we're not signed in.
extern const char kManagedUserPseudoEmail[];

// Keys for managed user shared settings. These can be configured remotely or
// locally, and are mapped to preferences by the
// SupervisedUserPrefMappingService.
extern const char kChromeAvatarIndex[];
extern const char kChromeOSAvatarIndex[];
extern const char kChromeOSPasswordData[];

}  // namespace managed_users

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_CONSTANTS_H_
