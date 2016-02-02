// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <string>

#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/sync_driver/sync_prefs.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/chromeos_switches.h"
#endif

#if defined(ENABLE_EXTENSIONS)
#include "extensions/browser/pref_names.h"
#endif

Profile::Profile()
    : restored_last_session_(false),
      sent_destroyed_notification_(false),
      accessibility_pause_level_(0),
      is_guest_profile_(false),
      is_system_profile_(false) {
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

ChromeZoomLevelPrefs* Profile::GetZoomLevelPrefs() {
  return NULL;
}

Profile::Delegate::~Delegate() {
}

// static
const char Profile::kProfileKey[] = "__PROFILE__";
// This must be a string which can never be a valid domain.
const char Profile::kNoHostedDomainFound[] = "NO_HOSTED_DOMAIN";

// static
void Profile::RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kSearchSuggestEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if BUILDFLAG(ANDROID_JAVA_UI)
  registry->RegisterStringPref(
      prefs::kContextualSearchEnabled,
      std::string(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
  registry->RegisterBooleanPref(prefs::kSessionExitedCleanly, true);
  registry->RegisterStringPref(prefs::kSessionExitType, std::string());
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingExtendedReportingEnabled,
                                false);
  registry->RegisterBooleanPref(prefs::kSafeBrowsingProceedAnywayDisabled,
                                false);
  registry->RegisterBooleanPref(prefs::kSSLErrorOverrideAllowed, true);
  registry->RegisterDictionaryPref(prefs::kSafeBrowsingIncidentsSent);
  registry->RegisterBooleanPref(
      prefs::kSafeBrowsingExtendedReportingOptInAllowed, true);
#if BUILDFLAG(ENABLE_GOOGLE_NOW)
  registry->RegisterBooleanPref(prefs::kGoogleGeolocationAccessEnabled, false);
#endif
  // This pref is intentionally outside the above #if. That flag corresponds
  // to the Notifier extension and does not gate the launcher page.
  // TODO(skare): Remove or rename ENABLE_GOOGLE_NOW: http://crbug.com/459827.
  registry->RegisterBooleanPref(prefs::kGoogleNowLauncherEnabled, true);
  registry->RegisterBooleanPref(prefs::kDisableExtensions, false);
#if defined(ENABLE_EXTENSIONS)
  registry->RegisterBooleanPref(extensions::pref_names::kAlertsInitialized,
                                false);
#endif
  registry->RegisterStringPref(prefs::kSelectFileLastDirectory, std::string());
  registry->RegisterDictionaryPref(prefs::kPartitionDefaultZoomLevel);
  registry->RegisterDictionaryPref(prefs::kPartitionPerHostZoomLevels);
  registry->RegisterStringPref(prefs::kDefaultApps, "install");
  registry->RegisterBooleanPref(prefs::kSpeechRecognitionFilterProfanities,
                                true);
  registry->RegisterIntegerPref(prefs::kProfileIconVersion, 0);
  registry->RegisterBooleanPref(prefs::kAllowDinosaurEasterEgg, true);
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
  registry->RegisterStringPref(prefs::kApplicationLocaleBackup, std::string());
  registry->RegisterStringPref(prefs::kApplicationLocaleAccepted,
                               std::string());
  registry->RegisterStringPref(prefs::kCurrentWallpaperAppName, std::string());
#endif

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kDevToolsRemoteEnabled, false);
#endif

  registry->RegisterBooleanPref(prefs::kDataSaverEnabled, false);
  data_reduction_proxy::RegisterSyncableProfilePrefs(registry);

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS) && !defined(OS_IOS)
  // Preferences related to the avatar bubble and user manager tutorials.
  registry->RegisterIntegerPref(prefs::kProfileAvatarTutorialShown, 0);
#endif

#if defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kClickedUpdateMenuItem, false);
#endif

#if defined(ENABLE_MEDIA_ROUTER)
#if defined(GOOGLE_CHROME_BUILD)
  registry->RegisterBooleanPref(
      prefs::kMediaRouterCloudServicesPrefSet,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kMediaRouterEnableCloudServices,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // defined(GOOGLE_CHROME_BUILD)
  registry->RegisterBooleanPref(
      prefs::kMediaRouterFirstRunFlowAcknowledged,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
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
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kGuestSession);
  return is_guest_session;
#else
  return is_guest_profile_;
#endif
}

bool Profile::IsSystemProfile() const {
  return is_system_profile_;
}

bool Profile::IsNewProfile() {
  // The profile has been shut down if the prefs were loaded from disk, unless
  // first-run autoimport wrote them and reloaded the pref service.
  // TODO(dconnelly): revisit this when crbug.com/22142 (unifying the profile
  // import code) is fixed.
  return GetOriginalProfile()->GetPrefs()->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_CREATED_NEW_PREF_STORE;
}

bool Profile::IsSyncAllowed() {
  if (ProfileSyncServiceFactory::HasProfileSyncService(this)) {
    return ProfileSyncServiceFactory::GetForProfile(this)->IsSyncAllowed();
  }

  // No ProfileSyncService created yet - we don't want to create one, so just
  // infer the accessible state by looking at prefs/command line flags.
  sync_driver::SyncPrefs prefs(GetPrefs());
  return ProfileSyncService::IsSyncAllowedByFlag() && !prefs.IsManaged();
}

void Profile::MaybeSendDestroyedNotification() {
  if (!sent_destroyed_notification_) {
    sent_destroyed_notification_ = true;

    NotifyWillBeDestroyed(this);
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

double Profile::GetDefaultZoomLevelForProfile() {
  return GetDefaultStoragePartition(this)
      ->GetHostZoomMap()
      ->GetDefaultZoomLevel();
}
