// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These are functions to access various profile-management flags but with
// possible overrides from Experiements.  This is done inside chrome/common
// because it is accessed by files through the chrome/ directory tree.

#ifndef COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
#define COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_

namespace base {
class CommandLine;
}

namespace switches {

// Checks whether account consistency is enabled. If enabled, the account
// management UI is available in the avatar bubble.
bool IsEnableAccountConsistency();

// Whether the chrome.identity API should be multi-account.
bool IsExtensionsMultiAccount();

// Called in tests to force enable account consistency.
void EnableAccountConsistencyForTesting(base::CommandLine* command_line);

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_CORE_COMMON_PROFILE_MANAGEMENT_SWITCHES_H_
