// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_

namespace chromeos {

// Returns true if quick unlock is allowed by policy and the feature flag is
// present.
bool IsQuickUnlockEnabled();

// Forcibly enable quick-unlock for testing.
void EnableQuickUnlockForTesting();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_UTILS_H_
