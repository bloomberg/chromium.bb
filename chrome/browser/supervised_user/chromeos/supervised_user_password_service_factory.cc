// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chromeos/supervised_user_password_service_factory.h"

#include "chrome/browser/chromeos/login/users/supervised_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/chromeos/supervised_user_password_service.h"
#include "chrome/browser/supervised_user/supervised_user_shared_settings_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace chromeos {

// static
SupervisedUserPasswordService*
SupervisedUserPasswordServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SupervisedUserPasswordService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SupervisedUserPasswordServiceFactory*
SupervisedUserPasswordServiceFactory::GetInstance() {
  return Singleton<SupervisedUserPasswordServiceFactory>::get();
}

SupervisedUserPasswordServiceFactory::SupervisedUserPasswordServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SupervisedUserPasswordService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SupervisedUserSharedSettingsServiceFactory::GetInstance());
}

SupervisedUserPasswordServiceFactory::
    ~SupervisedUserPasswordServiceFactory() {}

KeyedService* SupervisedUserPasswordServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile= static_cast<Profile*>(context);
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(profile);
  if (user->GetType() != user_manager::USER_TYPE_SUPERVISED)
    return NULL;
  SupervisedUserPasswordService* result = new SupervisedUserPasswordService();
  result->Init(
      user->email(),
      SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
          profile));
  return result;
}

content::BrowserContext*
SupervisedUserPasswordServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace chromeos
