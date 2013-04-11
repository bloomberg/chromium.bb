// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SYNC_PROFILE_SIGNIN_CONFIRMATION_HELPER_H_
#define CHROME_BROWSER_UI_SYNC_PROFILE_SIGNIN_CONFIRMATION_HELPER_H_

#include "base/callback.h"

class Profile;

namespace ui {

// Determines whether the browser has ever been shutdown since the
// profile was created.
bool HasBeenShutdown(Profile* profile);

// Determines whether the user should be prompted to create a new
// profile before signin.
void CheckShouldPromptForNewProfile(
    Profile* profile,
    const base::Callback<void(bool)>& cb);

}  // namespace ui

#endif  // CHROME_BROWSER_UI_SYNC_PROFILE_SIGNIN_CONFIRMATION_HELPER_H_
