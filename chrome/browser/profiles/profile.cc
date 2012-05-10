// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#endif

// A pointer to the request context for the default profile.  See comments on
// Profile::GetDefaultRequestContext.
net::URLRequestContextGetter* Profile::default_request_context_;

Profile::Profile()
    : restored_last_session_(false),
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
void Profile::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kSearchSuggestEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSessionExitedCleanly,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kSafeBrowsingReportingEnabled,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  // TODO(erg): kSpeechRecognitionFilterProfanities should also be moved to
  // speech_input_extension_manager.cc, but is more involved because of
  // BrowserContext/Profile confusion because of ChromeSpeechInputPreferences.
  prefs->RegisterBooleanPref(prefs::kSpeechRecognitionFilterProfanities,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDisableExtensions,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kExtensionAlertsInitializedPref,
                             false, PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kSelectFileLastDirectory,
                            "",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDoublePref(prefs::kDefaultZoomLevel,
                            0.0,
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterDictionaryPref(prefs::kPerHostZoomLevels,
                                PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kDefaultApps,
                            "install",
                            PrefService::UNSYNCABLE_PREF);
#if defined(OS_CHROMEOS)
  // TODO(dilmah): For OS_CHROMEOS we maintain kApplicationLocale in both
  // local state and user's profile.  For other platforms we maintain
  // kApplicationLocale only in local state.
  // In the future we may want to maintain kApplicationLocale
  // in user's profile for other platforms as well.
  prefs->RegisterStringPref(prefs::kApplicationLocale,
                            "",
                            PrefService::SYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kApplicationLocaleBackup,
                            "",
                            PrefService::UNSYNCABLE_PREF);
  prefs->RegisterStringPref(prefs::kApplicationLocaleAccepted,
                            "",
                            PrefService::UNSYNCABLE_PREF);
#endif
}

// static
net::URLRequestContextGetter* Profile::GetDefaultRequestContext() {
  return default_request_context_;
}

std::string Profile::GetDebugName() {
  std::string name = GetPath().BaseName().MaybeAsASCII();
  if (name.empty()) {
    name = "UnknownProfile";
  }
  return name;
}

// static
bool Profile::IsGuestSession() {
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
