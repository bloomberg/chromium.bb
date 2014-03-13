// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"

#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ManagedUserSyncService* ManagedUserSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ManagedUserSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedUserSyncServiceFactory* ManagedUserSyncServiceFactory::GetInstance() {
  return Singleton<ManagedUserSyncServiceFactory>::get();
}

ManagedUserSyncServiceFactory::ManagedUserSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ManagedUserSyncService",
          BrowserContextDependencyManager::GetInstance()) {
}

ManagedUserSyncServiceFactory::~ManagedUserSyncServiceFactory() {}

KeyedService* ManagedUserSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ManagedUserSyncService(static_cast<Profile*>(profile)->GetPrefs());
}
