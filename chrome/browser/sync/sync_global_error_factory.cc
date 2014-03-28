// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_global_error_factory.h"

#include "ash/shell.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

SyncGlobalErrorFactory::SyncGlobalErrorFactory()
    : BrowserContextKeyedServiceFactory(
        "SyncGlobalError",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(GlobalErrorServiceFactory::GetInstance());
}

SyncGlobalErrorFactory::~SyncGlobalErrorFactory() {}

// static
SyncGlobalError* SyncGlobalErrorFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SyncGlobalError*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SyncGlobalErrorFactory* SyncGlobalErrorFactory::GetInstance() {
  return Singleton<SyncGlobalErrorFactory>::get();
}

KeyedService* SyncGlobalErrorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(USE_ASH)
  if (ash::Shell::HasInstance())
    return NULL;
#endif

  Profile* profile = static_cast<Profile*>(context);
  ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  if (!profile_sync_service)
    return NULL;

  SyncErrorController* sync_error_controller =
      profile_sync_service->sync_error_controller();
  if (!sync_error_controller)
    return NULL;

  return new SyncGlobalError(sync_error_controller,
                             profile_sync_service);
}
