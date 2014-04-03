// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

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

// static
KeyedService* AppListSyncableServiceFactory::BuildInstanceFor(
    content::BrowserContext* browser_context) {
  Profile* profile = static_cast<Profile*>(browser_context);
  VLOG(1) << "BuildServiceInstanceFor: " << profile->GetDebugName();
  return new AppListSyncableService(profile,
                                    extensions::ExtensionSystem::Get(profile));
}

AppListSyncableServiceFactory::AppListSyncableServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppListSyncableService",
        BrowserContextDependencyManager::GetInstance()) {
  VLOG(1) << "AppListSyncableServiceFactory()";
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

AppListSyncableServiceFactory::~AppListSyncableServiceFactory() {
}

KeyedService* AppListSyncableServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return BuildInstanceFor(static_cast<Profile*>(browser_context));
}

void AppListSyncableServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
}

content::BrowserContext* AppListSyncableServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // In Guest session, off the record profile should not be redirected to the
  // original one.
  Profile* profile = static_cast<Profile*>(context);
  if (profile->IsGuestSession())
    return chrome::GetBrowserContextOwnInstanceInIncognito(context);
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool AppListSyncableServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Start AppListSyncableService early so that the app list positions are
  // available before the app list is opened.
  return true;
}

bool AppListSyncableServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace app_list
