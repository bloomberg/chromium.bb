// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
#define CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_

class Browser;
class Profile;
class PrefService;

namespace web_intents {

// Registers the preferences related to Web Intents.
void RegisterUserPrefs(PrefService* user_prefs);

// Returns true if WebIntents are enabled in preferences.
bool IsWebIntentsEnabled(PrefService* prefs);

// Returns true if WebIntents are enabled due to various factors. |profile| is
// the Profile to check that WebIntents are enabled for.
bool IsWebIntentsEnabledForProfile(Profile* profile);

// In a context where we are generating a web intent based on internal events,
// or from an extension background page, get the browser in which to show the
// intent picker to the user.
Browser* GetBrowserForBackgroundWebIntentDelivery(Profile* profile);

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
