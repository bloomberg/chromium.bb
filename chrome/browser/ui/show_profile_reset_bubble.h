// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHOW_PROFILE_RESET_BUBBLE_H_
#define CHROME_BROWSER_UI_SHOW_PROFILE_RESET_BUBBLE_H_

#include "chrome/browser/profile_resetter/profile_reset_callback.h"

class Browser;

// Shows the prefs reset bubble on the platforms that support it. The callback
// is run when the user chooses to reset preferences.
void ShowProfileResetBubble(Browser* browser,
                            const ProfileResetCallback& reset_callback);

#endif  // CHROME_BROWSER_UI_SHOW_PROFILE_RESET_BUBBLE_H_
