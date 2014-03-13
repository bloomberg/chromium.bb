// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/chromeos/managed_user_password_service_factory.h"

#include "chrome/browser/chromeos/login/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/managed_mode/chromeos/managed_user_password_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ManagedUserPasswordService*
ManagedUserPasswordServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagedUserPasswordService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagedUserPasswordServiceFactory*
ManagedUserPasswordServiceFactory::GetInstance() {
  return Singleton<ManagedUserPasswordServiceFactory>::get();
}

ManagedUserPasswordServiceFactory::ManagedUserPasswordServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ManagedUserPasswordService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ManagedUserSharedSettingsServiceFactory::GetInstance());
}

ManagedUserPasswordServiceFactory::
    ~ManagedUserPasswordServiceFactory() {}

KeyedService* ManagedUserPasswordServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile= static_cast<Profile*>(context);
  chromeos::User* user = chromeos::UserManager::Get()->
      GetUserByProfile(profile);
  if (user->GetType() != chromeos::User::USER_TYPE_LOCALLY_MANAGED)
    return NULL;
  ManagedUserPasswordService* result = new ManagedUserPasswordService();
  result->Init(
      user->email(),
      ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(profile));
  return result;
}

content::BrowserContext*
ManagedUserPasswordServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
