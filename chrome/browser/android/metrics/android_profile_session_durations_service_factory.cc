// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_profile_session_durations_service_factory.h"

#include "chrome/browser/android/metrics/android_profile_session_durations_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
AndroidProfileSessionDurationsService*
AndroidProfileSessionDurationsServiceFactory::GetForActiveUserProfile() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  return AndroidProfileSessionDurationsServiceFactory::GetForProfile(profile);
}

// static
AndroidProfileSessionDurationsService*
AndroidProfileSessionDurationsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AndroidProfileSessionDurationsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AndroidProfileSessionDurationsServiceFactory*
AndroidProfileSessionDurationsServiceFactory::GetInstance() {
  return base::Singleton<AndroidProfileSessionDurationsServiceFactory>::get();
}

AndroidProfileSessionDurationsServiceFactory::
    AndroidProfileSessionDurationsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AndroidProfileSessionDurationsService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

AndroidProfileSessionDurationsServiceFactory::
    ~AndroidProfileSessionDurationsServiceFactory() = default;

KeyedService*
AndroidProfileSessionDurationsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(profile);
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return new AndroidProfileSessionDurationsService(sync_service,
                                                   identity_manager);
}

bool AndroidProfileSessionDurationsServiceFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}
