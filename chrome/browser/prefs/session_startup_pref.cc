// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"

#include <string>

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/url_fixer/url_fixer.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/window_restore_utils.h"
#endif

namespace {

enum StartupURLsMigrationMetrics {
  STARTUP_URLS_MIGRATION_METRICS_PERFORMED,
  STARTUP_URLS_MIGRATION_METRICS_NOT_PRESENT,
  STARTUP_URLS_MIGRATION_METRICS_RESET,
  STARTUP_URLS_MIGRATION_METRICS_MAX,
};

// Converts a SessionStartupPref::Type to an integer written to prefs.
int TypeToPrefValue(SessionStartupPref::Type type) {
  switch (type) {
    case SessionStartupPref::LAST: return SessionStartupPref::kPrefValueLast;
    case SessionStartupPref::URLS: return SessionStartupPref::kPrefValueURLs;
    default:                       return SessionStartupPref::kPrefValueNewTab;
  }
}

void SetNewURLList(PrefService* prefs) {
  if (prefs->IsUserModifiablePreference(prefs::kURLsToRestoreOnStartup)) {
    base::ListValue new_url_pref_list;
    base::StringValue* home_page =
        new base::StringValue(prefs->GetString(prefs::kHomePage));
    new_url_pref_list.Append(home_page);
    prefs->Set(prefs::kURLsToRestoreOnStartup, new_url_pref_list);
  }
}

void URLListToPref(const base::ListValue* url_list, SessionStartupPref* pref) {
  pref->urls.clear();
  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    std::string url_text;
    if (url_list->GetString(i, &url_text)) {
      GURL fixed_url = url_fixer::FixupURL(url_text, std::string());
      pref->urls.push_back(fixed_url);
    }
  }
}

}  // namespace

// static
void SessionStartupPref::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kRestoreOnStartup,
      TypeToPrefValue(GetDefaultStartupType()),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kURLsToRestoreOnStartup,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterListPref(prefs::kURLsToRestoreOnStartupOld,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kRestoreOnStartupMigrated,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterInt64Pref(
      prefs::kRestoreStartupURLsMigrationTime,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
SessionStartupPref::Type SessionStartupPref::GetDefaultStartupType() {
#if defined(OS_CHROMEOS)
  return SessionStartupPref::LAST;
#else
  return SessionStartupPref::DEFAULT;
#endif
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
    base::ListValue* url_pref_list = update.Get();
    DCHECK(url_pref_list);
    url_pref_list->Clear();
    for (size_t i = 0; i < pref.urls.size(); ++i) {
      url_pref_list->Set(static_cast<int>(i),
                         new base::StringValue(pref.urls[i].spec()));
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

  MigrateIfNecessary(prefs);
  MigrateMacDefaultPrefIfNecessary(prefs);

  SessionStartupPref pref(
      PrefValueToType(prefs->GetInteger(prefs::kRestoreOnStartup)));

  // Always load the urls, even if the pref type isn't URLS. This way the
  // preferences panels can show the user their last choice.
  const base::ListValue* url_list =
      prefs->GetList(prefs::kURLsToRestoreOnStartup);
  URLListToPref(url_list, &pref);

  return pref;
}

// static
void SessionStartupPref::MigrateIfNecessary(PrefService* prefs) {
  DCHECK(prefs);

  // Check if we need to migrate the old version of the startup URLs preference
  // to the new name, and also send metrics about the migration.
  StartupURLsMigrationMetrics metrics_result =
      STARTUP_URLS_MIGRATION_METRICS_MAX;
  const base::ListValue* old_startup_urls =
      prefs->GetList(prefs::kURLsToRestoreOnStartupOld);
  if (!prefs->GetUserPrefValue(prefs::kRestoreStartupURLsMigrationTime)) {
    // Record the absence of the migration timestamp, this will get overwritten
    // below if migration occurs now.
    metrics_result = STARTUP_URLS_MIGRATION_METRICS_NOT_PRESENT;

    // Seems like we never migrated, do it if necessary.
    if (!prefs->GetUserPrefValue(prefs::kURLsToRestoreOnStartup)) {
      if (old_startup_urls && !old_startup_urls->empty()) {
        prefs->Set(prefs::kURLsToRestoreOnStartup, *old_startup_urls);
        prefs->ClearPref(prefs::kURLsToRestoreOnStartupOld);
      }
      metrics_result = STARTUP_URLS_MIGRATION_METRICS_PERFORMED;
    }

    prefs->SetInt64(prefs::kRestoreStartupURLsMigrationTime,
                    base::Time::Now().ToInternalValue());
  } else if (old_startup_urls && !old_startup_urls->empty()) {
    // Migration needs to be reset.
    prefs->ClearPref(prefs::kURLsToRestoreOnStartupOld);
    base::Time last_migration_time = base::Time::FromInternalValue(
        prefs->GetInt64(prefs::kRestoreStartupURLsMigrationTime));
    base::Time now = base::Time::Now();
    prefs->SetInt64(prefs::kRestoreStartupURLsMigrationTime,
                    now.ToInternalValue());
    if (now < last_migration_time)
      last_migration_time = now;
    UMA_HISTOGRAM_CUSTOM_TIMES("Settings.StartupURLsResetTime",
                               now - last_migration_time,
                               base::TimeDelta::FromDays(0),
                               base::TimeDelta::FromDays(7),
                               50);
    metrics_result = STARTUP_URLS_MIGRATION_METRICS_RESET;
  }

  // Record a metric migration event if something interesting happened.
  if (metrics_result != STARTUP_URLS_MIGRATION_METRICS_MAX) {
    UMA_HISTOGRAM_ENUMERATION(
          "Settings.StartupURLsMigration",
          metrics_result,
          STARTUP_URLS_MIGRATION_METRICS_MAX);
  }

  if (!prefs->GetBoolean(prefs::kRestoreOnStartupMigrated)) {
    // Read existing values.
    const base::Value* homepage_is_new_tab_page_value =
        prefs->GetUserPrefValue(prefs::kHomePageIsNewTabPage);
    bool homepage_is_new_tab_page = true;
    if (homepage_is_new_tab_page_value) {
      if (!homepage_is_new_tab_page_value->GetAsBoolean(
              &homepage_is_new_tab_page))
        NOTREACHED();
    }

    const base::Value* restore_on_startup_value =
        prefs->GetUserPrefValue(prefs::kRestoreOnStartup);
    int restore_on_startup = -1;
    if (restore_on_startup_value) {
      if (!restore_on_startup_value->GetAsInteger(&restore_on_startup))
        NOTREACHED();
    }

    // If restore_on_startup has the deprecated value kPrefValueHomePage,
    // migrate it to open the homepage on startup. If 'homepage is NTP' is set,
    // that means just opening the NTP. If not, it means opening a one-item URL
    // list containing the homepage.
    if (restore_on_startup == kPrefValueHomePage) {
      if (homepage_is_new_tab_page) {
        prefs->SetInteger(prefs::kRestoreOnStartup, kPrefValueNewTab);
      } else {
        prefs->SetInteger(prefs::kRestoreOnStartup, kPrefValueURLs);
        SetNewURLList(prefs);
      }
    } else if (!restore_on_startup_value && !homepage_is_new_tab_page &&
               GetDefaultStartupType() == DEFAULT) {
      // kRestoreOnStartup was never set by the user, but the homepage was set.
      // Migrate to the list of URLs. (If restore_on_startup was never set,
      // and homepage_is_new_tab_page is true, no action is needed. The new
      // default value is "open the new tab page" which is what we want.)
      prefs->SetInteger(prefs::kRestoreOnStartup, kPrefValueURLs);
      SetNewURLList(prefs);
    }

    prefs->SetBoolean(prefs::kRestoreOnStartupMigrated, true);
  }
}

// static
void SessionStartupPref::MigrateMacDefaultPrefIfNecessary(PrefService* prefs) {
#if defined(OS_MACOSX)
  DCHECK(prefs);
  if (!restore_utils::IsWindowRestoreEnabled())
    return;
  // The default startup pref used to be LAST, now it is DEFAULT. Don't change
  // the setting for existing profiles (even if the user has never changed it),
  // but make new profiles default to DEFAULT.
  bool old_profile_version =
      !prefs->FindPreference(
          prefs::kProfileCreatedByVersion)->IsDefaultValue() &&
      Version(prefs->GetString(prefs::kProfileCreatedByVersion)).IsOlderThan(
          "21.0.1180.0");
  if (old_profile_version && TypeIsDefault(prefs))
    prefs->SetInteger(prefs::kRestoreOnStartup, kPrefValueLast);
#endif
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
bool SessionStartupPref::TypeIsDefault(PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_restore =
      prefs->FindPreference(prefs::kRestoreOnStartup);
  DCHECK(pref_restore);
  return pref_restore->IsDefaultValue();
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

SessionStartupPref::SessionStartupPref(Type type) : type(type) {}

SessionStartupPref::~SessionStartupPref() {}
