// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/manager_password_service_factory.h"

#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/chromeos/manager_password_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"

namespace chromeos {

// static
ManagerPasswordService*
ManagerPasswordServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ManagerPasswordService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ManagerPasswordServiceFactory*
ManagerPasswordServiceFactory::GetInstance() {
  return base::Singleton<ManagerPasswordServiceFactory>::get();
}

ManagerPasswordServiceFactory::ManagerPasswordServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ManagerPasswordService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SupervisedUserSharedSettingsServiceFactory::GetInstance());
  DependsOn(SupervisedUserSyncServiceFactory::GetInstance());
}

ManagerPasswordServiceFactory::
    ~ManagerPasswordServiceFactory() {}

KeyedService* ManagerPasswordServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (ChromeUserManager::Get()->GetSupervisedUserManager()->HasSupervisedUsers(
          user->GetAccountId().GetUserEmail())) {
    ManagerPasswordService* result = new ManagerPasswordService();
    result->Init(
        user->GetAccountId().GetUserEmail(),
        SupervisedUserSyncServiceFactory::GetForProfile(profile),
        SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
            profile));
    return result;
  }
  return NULL;
}

content::BrowserContext*
ManagerPasswordServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace chromeos
