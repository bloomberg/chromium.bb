// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace app_list {

// static
AppListSyncableService* AppListSyncableServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AppListSyncableService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
AppListSyncableServiceFactory* AppListSyncableServiceFactory::GetInstance() {
  return Singleton<AppListSyncableServiceFactory>::get();
}

AppListSyncableServiceFactory::AppListSyncableServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppListSyncableService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

AppListSyncableServiceFactory::~AppListSyncableServiceFactory() {
}

BrowserContextKeyedService*
AppListSyncableServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = static_cast<Profile*>(browser_context);
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return new AppListSyncableService(profile, extension_service);
}

void AppListSyncableServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
}

content::BrowserContext* AppListSyncableServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // In guest session, off the record profile should be used instead of the
  // original one, as the off the record profile is the active profile.
  // TODO(tbarzic): Add a helper function to incognito_helpers.h that properly
  //     handles the guest mode.
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsGuestSession())
    return profile->GetOffTheRecordProfile();
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AppListSyncableServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace app_list
