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

}  // namespace managed_users

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_CONSTANTS_H_
