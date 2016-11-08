// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MULTI_USER_USER_SWITCH_UTIL_H_
#define CHROME_BROWSER_UI_ASH_MULTI_USER_USER_SWITCH_UTIL_H_

#include "base/callback.h"

// Tries to switch to a new user by first checking if desktop casting / sharing
// is going on, and letting the user decide if they want to terminate it or not.
// After terminating any desktop sharing operations, the |switch_user| function
// will be called.
void TrySwitchingActiveUser(const base::Callback<void()> switch_user);

#endif  // CHROME_BROWSER_UI_ASH_MULTI_USER_USER_SWITCH_UTIL_H_
