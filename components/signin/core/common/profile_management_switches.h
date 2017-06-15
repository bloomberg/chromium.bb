// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
#define COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_

#include "build/build_config.h"

namespace base {
class CommandLine;
}

namespace switches {

enum class AccountConsistencyMethod {
  kDisabled,  // No account consistency.
  kMirror,    // Account management UI in the avatar bubble.
  kDice       // Account management UI on Gaia webpages.
};

// Returns the account consistency method.
AccountConsistencyMethod GetAccountConsistencyMethod();

// Checks whether Mirror account consistency is enabled. If enabled, the account
// management UI is available in the avatar bubble.
bool IsAccountConsistencyMirrorEnabled();

// Checks whether Dice account consistency is enabled. If enabled, then account
// management UI is available on the Gaia webpages.
bool IsAccountConsistencyDiceEnabled();

// Whether the chrome.identity API should be multi-account.
bool IsExtensionsMultiAccount();

// Called in tests to force enable Mirror account consistency.
void EnableAccountConsistencyMirrorForTesting(base::CommandLine* command_line);

// Dice is only supported on desktop.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
// Called in tests to force enable Dice account consistency.
void EnableAccountConsistencyDiceForTesting(base::CommandLine* command_line);
#endif

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
