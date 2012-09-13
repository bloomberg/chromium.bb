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

enum ActionId {
  ACTION_ID_CUSTOM = 1,  // for all unrecognized types
  ACTION_ID_EDIT,
  ACTION_ID_PICK,
  ACTION_ID_SAVE,
  ACTION_ID_SHARE,
  ACTION_ID_SUBSCRIBE,
  ACTION_ID_VIEW,
};

// "Recognized" action strings. These are basically the
// actions we're reporting via UMA.
const char kActionEdit[] = "http://webintents.org/edit";
const char kActionPick[] = "http://webintents.org/pick";
const char kActionSave[] = "http://webintents.org/save";
const char kActionShare[] = "http://webintents.org/share";
const char kActionSubscribe[] = "http://webintents.org/subscribe";
const char kActionView[] = "http://webintents.org/view";

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

// Returns the action::Id corresponding to |action| or ACTION_ID_CUSTOM
// if |action| is not recognized.
ActionId ToActionId(const string16& action);

// Returns true if |type1| and |type2| "match". Supports wild cards in both
// |type1| and |type2|. Wild cards are of the form '<type>/*', '*/*', and '*'.
bool MimeTypesMatch(const string16& type1, const string16& type2);

}  // namespace web_intents

#endif  // CHROME_BROWSER_INTENTS_WEB_INTENTS_UTIL_H_
