// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
#define COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_

#include "base/feature_list.h"

namespace signin {

// Account consistency feature. Only used on platforms where Mirror is not
// always enabled (ENABLE_MIRROR is false).
extern const base::Feature kAccountConsistencyFeature;

// The account consistency method parameter name.
extern const char kAccountConsistencyFeatureMethodParameter[];

// Account consistency method values.
extern const char kAccountConsistencyFeatureMethodMirror[];
extern const char kAccountConsistencyFeatureMethodDiceFixAuthErrors[];
extern const char kAccountConsistencyFeatureMethodDice[];

enum class AccountConsistencyMethod {
  kDisabled,           // No account consistency.
  kMirror,             // Account management UI in the avatar bubble.
  kDiceFixAuthErrors,  // No account consistency, but Dice fixes authentication
                       // errors.
  kDice                // Account management UI on Gaia webpages.
};

// Returns the account consistency method.
AccountConsistencyMethod GetAccountConsistencyMethod();

// Checks whether Mirror account consistency is enabled. If enabled, the account
// management UI is available in the avatar bubble.
bool IsAccountConsistencyMirrorEnabled();

// Checks whether Dice account consistency is enabled. If enabled, then account
// management UI is available on the Gaia webpages.
// Returns true when the account consistency method is kDice.
bool IsAccountConsistencyDiceEnabled();

// Whether the chrome.identity API should be multi-account.
bool IsExtensionsMultiAccount();

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
