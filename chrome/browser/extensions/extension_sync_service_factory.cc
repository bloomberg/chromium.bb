// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/extensions/extension_prefs_factory.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

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

BrowserContextKeyedService*
    ExtensionSyncServiceFactory::BuildServiceInstanceFor(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new ExtensionSyncService(
      profile,
      extensions::ExtensionPrefsFactory::GetForProfile(profile),
      extensions::ExtensionSystemFactory::GetForProfile(profile)->
          extension_service());
}

content::BrowserContext* ExtensionSyncServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
