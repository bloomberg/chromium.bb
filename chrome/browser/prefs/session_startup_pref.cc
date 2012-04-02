// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/protected_prefs_watcher.h"
#include "chrome/browser/protector/protector_service.h"
#include "chrome/browser/protector/protector_service_factory.h"
#include "chrome/common/pref_names.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/window_restore_utils.h"
#endif

namespace {

// For historical reasons the enum and value registered in the prefs don't line
// up. These are the values registered in prefs.
const int kPrefValueHomePage = 0;  // Deprecated
const int kPrefValueLast = 1;
const int kPrefValueURLs = 4;
const int kPrefValueNewTab = 5;

// Converts a SessionStartupPref::Type to an integer written to prefs.
int TypeToPrefValue(SessionStartupPref::Type type) {
  switch (type) {
    case SessionStartupPref::LAST:     return kPrefValueLast;
    case SessionStartupPref::URLS:     return kPrefValueURLs;
    default:                           return kPrefValueNewTab;
  }
}

void SetNewURLList(PrefService* prefs) {
  ListValue new_url_pref_list;
  StringValue* home_page = new StringValue(prefs->GetString(prefs::kHomePage));
  new_url_pref_list.Append(home_page);
  prefs->Set(prefs::kURLsToRestoreOnStartup, new_url_pref_list);
}

void URLListToPref(const base::ListValue* url_list, SessionStartupPref* pref) {
  pref->urls.clear();
  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    Value* value = NULL;
    CHECK(url_list->Get(i, &value));
    std::string url_text;
    if (value->GetAsString(&url_text)) {
      GURL fixed_url = URLFixerUpper::FixupURL(url_text, "");
      pref->urls.push_back(fixed_url);
    }
  }
}

}  // namespace

// static
void SessionStartupPref::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kRestoreOnStartup,
                             TypeToPrefValue(GetDefaultStartupType()),
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterListPref(prefs::kURLsToRestoreOnStartup,
                          PrefService::SYNCABLE_PREF);
}

// static
SessionStartupPref::Type SessionStartupPref::GetDefaultStartupType() {
#if defined(OS_CHROMEOS)
  SessionStartupPref::Type type = SessionStartupPref::LAST;
#else
  SessionStartupPref::Type type = SessionStartupPref::DEFAULT;
#endif

#if defined(OS_MACOSX)
  // Use Lion's system preference, if it is set.
  if (restore_utils::IsWindowRestoreEnabled())
    type = SessionStartupPref::LAST;
#endif

  return type;
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
    ListPrefUpdate update(prefs, prefs::kURLsToRestoreOnStartup);
    ListValue* url_pref_list = update.Get();
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

  // Migrate from "Open the home page" to "Open the following URLs". If the user
  // had the "Open the homepage" option selected, then we need to switch them to
  // "Open the following URLs" and set the list of URLs to a list containing
  // just the user's homepage.
  if (pref.type == SessionStartupPref::HOMEPAGE) {
    prefs->SetInteger(prefs::kRestoreOnStartup, kPrefValueURLs);
    pref.type = SessionStartupPref::URLS;
    SetNewURLList(prefs);
  }

  // Always load the urls, even if the pref type isn't URLS. This way the
  // preferences panels can show the user their last choice.
  const ListValue* url_list = prefs->GetList(prefs::kURLsToRestoreOnStartup);
  URLListToPref(url_list, &pref);

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

// static
SessionStartupPref::Type SessionStartupPref::PrefValueToType(int pref_value) {
  switch (pref_value) {
    case kPrefValueLast:     return SessionStartupPref::LAST;
    case kPrefValueURLs:     return SessionStartupPref::URLS;
    case kPrefValueHomePage: return SessionStartupPref::HOMEPAGE;
    default:                 return SessionStartupPref::DEFAULT;
  }
}

// static
bool SessionStartupPref::DidStartupPrefChange(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  protector::ProtectedPrefsWatcher* prefs_watcher =
      protector::ProtectorServiceFactory::GetForProfile(profile)->
          GetPrefsWatcher();
  if (!TypeIsManaged(prefs) &&
#if defined(OS_MACOSX)
      // On Mac OS, default value for |kRestoreOnStartup| depends on system
      // settings and may be different from one run to another.
      !prefs->FindPreference(prefs::kRestoreOnStartup)->IsDefaultValue() &&
#endif
      prefs_watcher->DidPrefChange(prefs::kRestoreOnStartup)) {
    return true;
  }
  if (!URLsAreManaged(prefs) &&
      prefs_watcher->DidPrefChange(prefs::kURLsToRestoreOnStartup)) {
    return true;
  }
  return false;
}

// static
SessionStartupPref SessionStartupPref::GetStartupPrefBackup(Profile* profile) {
  protector::ProtectedPrefsWatcher* prefs_watcher =
      protector::ProtectorServiceFactory::GetForProfile(profile)->
          GetPrefsWatcher();

  int type;
  CHECK(prefs_watcher->GetBackupForPref(
            prefs::kRestoreOnStartup)->GetAsInteger(&type));
  SessionStartupPref backup_pref(PrefValueToType(type));

  const ListValue* url_list;
  CHECK(prefs_watcher->GetBackupForPref(
            prefs::kURLsToRestoreOnStartup)->GetAsList(&url_list));
  URLListToPref(url_list, &backup_pref);

  return backup_pref;
}

SessionStartupPref::SessionStartupPref(Type type) : type(type) {}

SessionStartupPref::~SessionStartupPref() {}
