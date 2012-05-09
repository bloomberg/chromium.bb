// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NETWORK_PROFILE_BUBBLE_PREFS_H_
#define CHROME_BROWSER_UI_NETWORK_PROFILE_BUBBLE_PREFS_H_
#pragma once

class PrefService;

namespace browser {

// The number of warnings to be shown on consequtive starts of Chrome before the
// silent period starts.
extern const int kMaxWarnings;

// Register the pref that controls whether the bubble should be shown anymore.
void RegisterNetworkProfileBubblePrefs(PrefService* prefs);

}  // namespace

#endif  // CHROME_BROWSER_UI_NETWORK_PROFILE_BUBBLE_PREFS_H_
