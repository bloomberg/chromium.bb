// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_TEST_UTIL_H_

#include <memory>

class TestingProfile;

// Builds a profile in pre-Dice account consistency mode.
// The profile is in Dice migration but does not migrate automatically because
// it was created before Dice was launched.
std::unique_ptr<TestingProfile> BuildPreDiceProfile();

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_TEST_UTIL_H_
