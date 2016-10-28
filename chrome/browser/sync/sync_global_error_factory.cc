// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_global_error_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

SyncGlobalErrorFactory::SyncGlobalErrorFactory()
    : BrowserContextKeyedServiceFactory(
        "SyncGlobalError",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(LoginUIServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
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
  return base::Singleton<SyncGlobalErrorFactory>::get();
}

KeyedService* SyncGlobalErrorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if defined(USE_ASH)
  if (ash::Shell::HasInstance())
    return nullptr;
#endif

  Profile* profile = static_cast<Profile*>(context);
  browser_sync::ProfileSyncService* profile_sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);

  if (!profile_sync_service)
    return nullptr;

  syncer::SyncErrorController* sync_error_controller =
      profile_sync_service->sync_error_controller();
  if (!sync_error_controller)
    return nullptr;

  return new SyncGlobalError(GlobalErrorServiceFactory::GetForProfile(profile),
                             LoginUIServiceFactory::GetForProfile(profile),
                             sync_error_controller, profile_sync_service);
}
