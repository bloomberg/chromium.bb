// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_util.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_switches.h"
#include "net/base/mime_util.h"

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

bool MimeTypesMatch(const string16& type1, const string16& type2) {
  // We don't have a MIME matcher that allows patterns on both sides
  // Instead, we do two comparisons, treating each type in turn as a
  // pattern. If either one matches, we consider this a MIME match.
  std::string t1 = UTF16ToUTF8(type1);
  std::string t2 = UTF16ToUTF8(type2);

  // If either side is _all_ wildcard, it's a match!
  if (t1 == "*" || t1 == "*/*" || t2 == "*" || t2 == "*/*")
    return true;

  StringToLowerASCII(&t1);
  StringToLowerASCII(&t2);
  return (net::MatchesMimeType(t1, t2)) || net::MatchesMimeType(t2, t1);
}

}  // namespace web_intents
