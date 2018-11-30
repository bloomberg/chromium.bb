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
  return new AccountConsistencyModeManager(
      Profile::FromBrowserContext(context));
}

void AccountConsistencyModeManagerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AccountConsistencyModeManager::RegisterProfilePrefs(registry);
}
