// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_util.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_switches.h"

namespace web_intents {
namespace {

struct ActionMapping {
  const char* name;
  const ActionId id;
};

const ActionMapping kActionMap[] = {
  { kActionEdit, ACTION_ID_EDIT },
  { kActionPick, ACTION_ID_PICK },
  { kActionSave, ACTION_ID_SAVE },
  { kActionShare, ACTION_ID_SHARE},
  { kActionSubscribe, ACTION_ID_SUBSCRIBE },
  { kActionView, ACTION_ID_VIEW },
};

// Returns the ActionMapping for |action| if one exists, or NULL.
const ActionMapping* FindActionMapping(const string16& action) {
  for (size_t i = 0; i < arraysize(kActionMap); ++i) {
    if (EqualsASCII(action, kActionMap[i].name)) {
      return &kActionMap[i];
    }
  }
  return NULL;
}

}  // namespace

void RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kWebIntentsEnabled, true,
                                  PrefService::SYNCABLE_PREF);
}

bool IsWebIntentsEnabled(PrefService* prefs) {
  return prefs->GetBoolean(prefs::kWebIntentsEnabled);
}

bool IsWebIntentsEnabledForProfile(Profile* profile) {
  return IsWebIntentsEnabled(profile->GetPrefs());
}

Browser* GetBrowserForBackgroundWebIntentDelivery(Profile* profile) {
#if defined(OS_ANDROID)
  return NULL;
#else
  Browser* browser = BrowserList::GetLastActive();
  if (browser && profile && browser->profile() != profile)
    return NULL;
  return browser;
#endif  // defined(OS_ANDROID)
}

bool IsRecognizedAction(const string16& action) {
  const ActionMapping* mapping = FindActionMapping(action);
  return mapping != NULL;
}

ActionId ToActionId(const string16& action) {
  const ActionMapping* mapping = FindActionMapping(action);
  return mapping != NULL ? mapping->id : ACTION_ID_CUSTOM;
}

}  // namespace web_intents
