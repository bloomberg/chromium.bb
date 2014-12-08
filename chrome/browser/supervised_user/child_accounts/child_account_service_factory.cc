// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/child_account_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/supervised_user/child_accounts/child_account_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ChildAccountService* ChildAccountServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChildAccountService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChildAccountServiceFactory* ChildAccountServiceFactory::GetInstance() {
  return Singleton<ChildAccountServiceFactory>::get();
}

ChildAccountServiceFactory::ChildAccountServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "ChildAccountService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SupervisedUserServiceFactory::GetInstance());
  // Indirect dependency via AccountServiceFlagFetcher.
  DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  // TODO(treib): Do we have more dependencies here?
}

ChildAccountServiceFactory::~ChildAccountServiceFactory() {}

KeyedService* ChildAccountServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ChildAccountService(static_cast<Profile*>(profile));
}
