// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/core/browser/signin_buildflags.h"

// static
AccountConsistencyModeManagerFactory*
AccountConsistencyModeManagerFactory::GetInstance() {
  return base::Singleton<AccountConsistencyModeManagerFactory>::get();
}

// static
AccountConsistencyModeManager*
AccountConsistencyModeManagerFactory::GetForProfile(Profile* profile) {
  DCHECK(profile);
  return static_cast<AccountConsistencyModeManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AccountConsistencyModeManagerFactory::AccountConsistencyModeManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "AccountConsistencyModeManager",
          BrowserContextDependencyManager::GetInstance()) {}

AccountConsistencyModeManagerFactory::~AccountConsistencyModeManagerFactory() =
    default;

KeyedService* AccountConsistencyModeManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());
  Profile* profile = Profile::FromBrowserContext(context);

  // New profiles are consistent at creation and can be migrated immediately
  // without going through full migration (which requires restarting Chrome).
  bool auto_migrate_to_dice = profile->WasCreatedByVersionOrLater("75.0.0.0");
  return AccountConsistencyModeManager::ShouldBuildServiceForProfile(profile)
             ? new AccountConsistencyModeManager(profile, auto_migrate_to_dice)
             : nullptr;
}

void AccountConsistencyModeManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountConsistencyModeManager::RegisterProfilePrefs(registry);
}

bool AccountConsistencyModeManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
