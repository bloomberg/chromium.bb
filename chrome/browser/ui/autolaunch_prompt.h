// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOLAUNCH_PROMPT_H_
#define CHROME_BROWSER_UI_AUTOLAUNCH_PROMPT_H_
#pragma once

class PrefService;
class Profile;

namespace browser {

// Determines whether or not the auto-launch prompt should be shown, and shows
// it as needed. Returns true if it was shown, false otherwise.
bool ShowAutolaunchPrompt(Profile* profile);

// Registers auto-launch specific prefs.
void RegisterAutolaunchPrefs(PrefService* prefs);

}  // namespace browser


#endif  // CHROME_BROWSER_UI_AUTOLAUNCH_PROMPT_H_
