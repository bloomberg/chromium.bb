// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"

// static
ExtensionSyncService* ExtensionSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ExtensionSyncServiceFactory* ExtensionSyncServiceFactory::GetInstance() {
  return Singleton<ExtensionSyncServiceFactory>::get();
}

ExtensionSyncServiceFactory::ExtensionSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ExtensionSyncService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

ExtensionSyncServiceFactory::~ExtensionSyncServiceFactory() {}

KeyedService* ExtensionSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ExtensionSyncService(
      profile,
      extensions::ExtensionPrefsFactory::GetForBrowserContext(context),
      extensions::ExtensionSystem::Get(profile)->extension_service());
}

content::BrowserContext* ExtensionSyncServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()->
      GetOriginalContext(context);
}
