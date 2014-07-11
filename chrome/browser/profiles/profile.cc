// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "base/prefs/pref_service.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/pref_names.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#endif

#if defined(OS_ANDROID) && defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#endif

Profile::Profile()
    : restored_last_session_(false),
      sent_destroyed_notification_(false),
      accessibility_pause_level_(0) {
}

Profile::~Profile() {
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

Profile::Delegate::~Delegate() {
}

// static
const char Profile::kProfileKey[] = "__PROFILE__";

// static
void Profile::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if defined(OS_ANDROID)
  registry->RegisterStringPref(
      prefs::kContextualSearchEnabled,
      std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(
      prefs::kSessionExitedCleanly,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSessionExitType,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_ANDROID) && defined(FULL_SAFE_BROWSING)
  // During Finch trail, safe browsing should be turned off
  // by default, and not sync'ed with desktop.
  // If we want to enable safe browsing on Android, we will
  // need to remove this Android-specific code.
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingEnabled,
      SafeBrowsingService::IsEnabledByFieldTrial(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#else
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingExtendedReportingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingDownloadFeedbackEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingReportingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingProceedAnywayDisabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingIncidentReportSent,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(ENABLE_GOOGLE_NOW)
  registry->RegisterBooleanPref(
      prefs::kGoogleGeolocationAccessEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(
      prefs::kDisableExtensions,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      extensions::pref_names::kAlertsInitialized,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kSelectFileLastDirectory,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kDefaultZoomLevel,
      0.0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kPerHostZoomLevels,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDefaultApps,
      "install",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kSpeechRecognitionFilterProfanities,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kProfileIconVersion,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_CHROMEOS)
  // TODO(dilmah): For OS_CHROMEOS we maintain kApplicationLocale in both
  // local state and user's profile.  For other platforms we maintain
  // kApplicationLocale only in local state.
  // In the future we may want to maintain kApplicationLocale
  // in user's profile for other platforms as well.
  registry->RegisterStringPref(
      prefs::kApplicationLocale,
      std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterStringPref(
      prefs::kApplicationLocaleBackup,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kApplicationLocaleAccepted,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kCurrentWallpaperAppName,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kDevToolsRemoteEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
#if defined(OS_ANDROID) || defined(OS_IOS)
  data_reduction_proxy::RegisterSyncableProfilePrefs(registry);
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS) && !defined(OS_IOS)
  // Preferences related to the avatar bubble and user manager tutorials.
  registry->RegisterIntegerPref(
      prefs::kProfileAvatarTutorialShown,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kProfileUserManagerTutorialShown,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
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
  static bool is_guest_session = CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kGuestSession);
  return is_guest_session;
#else
  return GetPath() == ProfileManager::GetGuestProfilePath();
#endif
}

bool Profile::IsNewProfile() {
  // The profile has been shut down if the prefs were loaded from disk, unless
  // first-run autoimport wrote them and reloaded the pref service.
  // TODO(dconnelly): revisit this when crbug.com/22142 (unifying the profile
  // import code) is fixed.
  return GetOriginalProfile()->GetPrefs()->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}

bool Profile::IsSyncAccessible() {
  if (ProfileSyncServiceFactory::HasProfileSyncService(this))
    return !ProfileSyncServiceFactory::GetForProfile(this)->IsManaged();

  // No ProfileSyncService created yet - we don't want to create one, so just
  // infer the accessible state by looking at prefs/command line flags.
  sync_driver::SyncPrefs prefs(GetPrefs());
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

bool ProfileCompare::operator()(Profile* a, Profile* b) const {
  DCHECK(a && b);
  if (a->IsSameProfile(b))
    return false;
  return a->GetOriginalProfile() < b->GetOriginalProfile();
}
