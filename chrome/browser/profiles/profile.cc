// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

Profile::Profile()
    : restored_last_session_(false),
      sent_destroyed_notification_(false),
      accessibility_pause_level_(0) {
}

// static
Profile* Profile::FromBrowserContext(content::BrowserContext* browser_context) {
  // This is safe; this is the only implementation of the browser context.
  return static_cast<Profile*>(browser_context);
}

// static
Profile* Profile::FromWebUI(content::WebUI* web_ui) {
  return FromBrowserContext(web_ui->GetWebContents()->GetBrowserContext());
}

TestingProfile* Profile::AsTestingProfile() {
  return NULL;
}

// static
const char* const Profile::kProfileKey = "__PROFILE__";

// static
void Profile::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  prefs->RegisterBooleanPref(prefs::kSearchSuggestEnabled,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSessionExitedCleanly,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kSessionExitType,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingEnabled,
                             true,
                             PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingReportingEnabled,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingProceedAnywayDisabled,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDisableExtensions,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kExtensionAlertsInitializedPref,
                             false, PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kSelectFileLastDirectory,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kDefaultZoomLevel,
                            0.0,
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kPerHostZoomLevels,
                                PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultApps,
                            "install",
                            PrefServiceSyncable::UNSYNCABLE_PREF);
#if defined(OS_CHROMEOS)
  // TODO(dilmah): For OS_CHROMEOS we maintain kApplicationLocale in both
  // local state and user's profile.  For other platforms we maintain
  // kApplicationLocale only in local state.
  // In the future we may want to maintain kApplicationLocale
  // in user's profile for other platforms as well.
  prefs->RegisterStringPref(prefs::kApplicationLocale,
                            std::string(),
                            PrefServiceSyncable::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kApplicationLocaleBackup,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kApplicationLocaleAccepted,
                            std::string(),
                            PrefServiceSyncable::UNSYNCABLE_PREF);
#endif

#if defined(OS_ANDROID)
  prefs->RegisterBooleanPref(prefs::kDevToolsRemoteEnabled,
                             false,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSpdyProxyEnabled,
                             true,
                             PrefServiceSyncable::UNSYNCABLE_PREF);
#endif

}


std::string Profile::GetDebugName() {
  std::string name = GetPath().BaseName().MaybeAsASCII();
  if (name.empty()) {
    name = "UnknownProfile";
  }
  return name;
}

bool Profile::IsGuestSession() const {
#if defined(OS_CHROMEOS)
  static bool is_guest_session =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession);
  return is_guest_session;
#else
  return false;
#endif
}

bool Profile::IsSyncAccessible() {
  browser_sync::SyncPrefs prefs(GetPrefs());
  return ProfileSyncService::IsSyncEnabled() && !prefs.IsManaged();
}

void Profile::MaybeSendDestroyedNotification() {
  if (!sent_destroyed_notification_) {
    sent_destroyed_notification_ = true;
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_PROFILE_DESTROYED,
        content::Source<Profile>(this),
        content::NotificationService::NoDetails());
  }
}
