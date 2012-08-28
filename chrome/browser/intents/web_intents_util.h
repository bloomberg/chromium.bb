// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
#define CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_

#include "base/string16.h"

class Browser;
class Profile;
class PrefService;

namespace web_intents {

namespace action {

// "Recognized" action strings. These are basically the
// actions we're reporting via UMA.
const char kEdit[] = "http://webintents.org/edit";
const char kPick[] = "http://webintents.org/pick";
const char kSave[] = "http://webintents.org/save";
const char kShare[] = "http://webintents.org/share";
const char kSubscribe[] = "http://webintents.org/subscribe";
const char kView[] = "http://webintents.org/view";

}

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

// Returns the recognized action (the one described at
// webintents.org) or an empty string if the action is not recognized.
bool IsRecognizedAction(const string16& action);

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
