// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/credential_cache_service_factory_win.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/credential_cache_service_win.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"


namespace syncer {

// static
CredentialCacheServiceFactory* CredentialCacheServiceFactory::GetInstance() {
  return Singleton<CredentialCacheServiceFactory>::get();
}

// static
syncer::CredentialCacheService* CredentialCacheServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<syncer::CredentialCacheService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

CredentialCacheServiceFactory::CredentialCacheServiceFactory()
    : ProfileKeyedServiceFactory("CredentialCacheService",
                                 ProfileDependencyManager::GetInstance()) {
  // TODO(rsimha): Uncomment this once it exists.
  // DependsOn(PrefServiceFactory::GetInstance());
  DependsOn(ProfileSyncServiceFactory::GetInstance());
  DependsOn(SigninManagerFactory::GetInstance());
  DependsOn(TokenServiceFactory::GetInstance());
}

CredentialCacheServiceFactory::~CredentialCacheServiceFactory() {
}

// static
bool CredentialCacheServiceFactory::IsDefaultProfile(Profile* profile) {
  DCHECK(profile);
  FilePath default_user_data_dir;
  chrome::GetDefaultUserDataDirectory(&default_user_data_dir);
  return profile->GetPath() ==
         ProfileManager::GetDefaultProfileDir(default_user_data_dir);
}

bool CredentialCacheServiceFactory::ServiceIsCreatedWithProfile() {
  return true;
}

ProfileKeyedService* CredentialCacheServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  // Only instantiate a CredentialCacheService object if we are running in the
  // default profile on Windows 8, and if credential caching is enabled.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      IsDefaultProfile(profile) &&
      command_line->HasSwitch(switches::kEnableSyncCredentialCaching)) {
    return new syncer::CredentialCacheService(profile);
  }
  return NULL;
}

}  // namespace syncer
