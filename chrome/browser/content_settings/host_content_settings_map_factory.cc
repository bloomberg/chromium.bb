// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/off_the_record_profile_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/content_settings/content_settings_supervised_provider.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/notifications/notification_channels_provider_android.h"
#endif  // OS_ANDROID

HostContentSettingsMapFactory::HostContentSettingsMapFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
        "HostContentSettingsMap",
        BrowserContextDependencyManager::GetInstance()) {
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  DependsOn(SupervisedUserSettingsServiceFactory::GetInstance());
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
#endif
}

HostContentSettingsMapFactory::~HostContentSettingsMapFactory() {
}

// static
HostContentSettingsMap* HostContentSettingsMapFactory::GetForProfile(
    Profile* profile) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  return static_cast<HostContentSettingsMap*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
HostContentSettingsMapFactory* HostContentSettingsMapFactory::GetInstance() {
  return base::Singleton<HostContentSettingsMapFactory>::get();
}

scoped_refptr<RefcountedKeyedService>
    HostContentSettingsMapFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = static_cast<Profile*>(context);

  // If off the record, retrieve the host content settings map of the parent
  // profile in order to ensure the preferences have been migrated.
  // TODO(crbug.com/277296): Remove check that profile does not equal the
  // original profile once TestingProfile::ForceIncognito is gone.
  if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE &&
      profile != profile->GetOriginalProfile()) {
    GetForProfile(profile->GetOriginalProfile());
  }

  bool store_last_modified = base::FeatureList::IsEnabled(features::kTabsInCbd);

  scoped_refptr<HostContentSettingsMap> settings_map(new HostContentSettingsMap(
      profile->GetPrefs(),
      profile->GetProfileType() == Profile::INCOGNITO_PROFILE,
      profile->GetProfileType() == Profile::GUEST_PROFILE,
      store_last_modified));

  sync_preferences::PrefServiceSyncable* pref_service =
      PrefServiceSyncableFromProfile(profile);
  if (pref_service) {
    pref_service->RegisterMergeDataFinishedCallback(
        base::Bind(&HostContentSettingsMap::MigrateDomainScopedSettings,
                   settings_map->GetWeakPtr(), true /* after_sync */));
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // These must be registered before before the HostSettings are passed over to
  // the IOThread.  Simplest to do this on construction.
  ExtensionService::RegisterContentSettings(settings_map.get(), profile);
#endif // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  SupervisedUserSettingsService* supervised_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(profile);
  // This may be null in testing.
  if (supervised_service) {
    std::unique_ptr<content_settings::SupervisedProvider> supervised_provider(
        new content_settings::SupervisedProvider(supervised_service));
    settings_map->RegisterProvider(HostContentSettingsMap::SUPERVISED_PROVIDER,
                                   std::move(supervised_provider));
  }
#endif // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if defined(OS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kSiteNotificationChannels)) {
    auto channels_provider =
        base::MakeUnique<NotificationChannelsProviderAndroid>();
    settings_map->RegisterProvider(
        HostContentSettingsMap::NOTIFICATION_ANDROID_PROVIDER,
        std::move(channels_provider));
  }
#endif  // defined (OS_ANDROID)
  return settings_map;
}

content::BrowserContext* HostContentSettingsMapFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
