// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"

#include <string>

#include "base/string_piece.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace {

// For historical reasons the enum and value registered in the prefs don't line
// up. These are the values registered in prefs.
const int kPrefValueDefault = 0;
const int kPrefValueLast = 1;
const int kPrefValueURLs = 4;

// Converts a SessionStartupPref::Type to an integer written to prefs.
int TypeToPrefValue(SessionStartupPref::Type type) {
  switch (type) {
    case SessionStartupPref::LAST:   return kPrefValueLast;
    case SessionStartupPref::URLS:   return kPrefValueURLs;
    default:                         return kPrefValueDefault;
  }
}

// Converts an integer pref value to a SessionStartupPref::Type.
SessionStartupPref::Type PrefValueToType(int pref_value) {
  switch (pref_value) {
    case kPrefValueLast:  return SessionStartupPref::LAST;
    case kPrefValueURLs:  return SessionStartupPref::URLS;
    default:              return SessionStartupPref::DEFAULT;
  }
}

}  // namespace

// static
void SessionStartupPref::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kRestoreOnStartup,
      TypeToPrefValue(browser_defaults::kDefaultSessionStartupType));
  prefs->RegisterListPref(prefs::kURLsToRestoreOnStartup);
}

// static
void SessionStartupPref::SetStartupPref(
    Profile* profile,
    const SessionStartupPref& pref) {
  DCHECK(profile);
  SetStartupPref(profile->GetPrefs(), pref);
}

// static
void SessionStartupPref::SetStartupPref(PrefService* prefs,
                                        const SessionStartupPref& pref) {
  DCHECK(prefs);

  if (!SessionStartupPref::TypeIsManaged(prefs))
    prefs->SetInteger(prefs::kRestoreOnStartup, TypeToPrefValue(pref.type));

  if (!SessionStartupPref::URLsAreManaged(prefs)) {
    // Always save the URLs, that way the UI can remain consistent even if the
    // user changes the startup type pref.
    // Ownership of the ListValue retains with the pref service.
    ScopedUserPrefUpdate update(prefs, prefs::kURLsToRestoreOnStartup);
    ListValue* url_pref_list =
        prefs->GetMutableList(prefs::kURLsToRestoreOnStartup);
    DCHECK(url_pref_list);
    url_pref_list->Clear();
    for (size_t i = 0; i < pref.urls.size(); ++i) {
      url_pref_list->Set(static_cast<int>(i),
                         new StringValue(pref.urls[i].spec()));
    }
  }
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(Profile* profile) {
  DCHECK(profile);
  return GetStartupPref(profile->GetPrefs());
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(PrefService* prefs) {
  DCHECK(prefs);
  SessionStartupPref pref(
      PrefValueToType(prefs->GetInteger(prefs::kRestoreOnStartup)));

  // Always load the urls, even if the pref type isn't URLS. This way the
  // preferences panels can show the user their last choice.
  const ListValue* url_pref_list = prefs->GetList(
      prefs::kURLsToRestoreOnStartup);
  if (url_pref_list) {
    for (size_t i = 0; i < url_pref_list->GetSize(); ++i) {
      Value* value = NULL;
      if (url_pref_list->Get(i, &value)) {
        std::string url_text;
        if (value->GetAsString(&url_text)) {
          GURL fixed_url = URLFixerUpper::FixupURL(url_text, "");
          pref.urls.push_back(fixed_url);
        }
      }
    }
  }
  return pref;
}

// static
bool SessionStartupPref::TypeIsManaged(PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_restore =
      prefs->FindPreference(prefs::kRestoreOnStartup);
  DCHECK(pref_restore);
  return pref_restore->IsManaged();
}

// static
bool SessionStartupPref::URLsAreManaged(PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_urls =
      prefs->FindPreference(prefs::kURLsToRestoreOnStartup);
  DCHECK(pref_urls);
  return pref_urls->IsManaged();
}

SessionStartupPref::SessionStartupPref() : type(DEFAULT) {}

SessionStartupPref::SessionStartupPref(Type type) : type(type) {}

SessionStartupPref::~SessionStartupPref() {}
