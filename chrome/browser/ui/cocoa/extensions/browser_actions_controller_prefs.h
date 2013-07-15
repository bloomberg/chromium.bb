// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_PREFS_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

// Registers the user preferences used by BrowserActionsController.
void RegisterBrowserActionsControllerProfilePrefs(
    user_prefs::PrefRegistrySyncable* prefs);

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTIONS_CONTROLLER_PREFS_H_
