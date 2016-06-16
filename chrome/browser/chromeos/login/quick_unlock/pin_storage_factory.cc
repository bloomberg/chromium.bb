// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_factory.h"

#include "chrome/browser/chromeos/login/quick_unlock/pin_storage.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

// static
PinStorage* PinStorageFactory::GetForProfile(Profile* profile) {
  return static_cast<PinStorage*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PinStorage* PinStorageFactory::GetForUser(const user_manager::User* user) {
  Profile* profile = ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return nullptr;

  return GetForProfile(profile);
}

// static
PinStorage* PinStorageFactory::GetForAccountId(const AccountId& account_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user)
    return nullptr;

  return GetForUser(user);
}

// static
PinStorageFactory* PinStorageFactory::GetInstance() {
  return base::Singleton<PinStorageFactory>::get();
}

PinStorageFactory::PinStorageFactory()
    : BrowserContextKeyedServiceFactory(
          "PinStorageFactory",
          BrowserContextDependencyManager::GetInstance()) {}

PinStorageFactory::~PinStorageFactory() {}

KeyedService* PinStorageFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PinStorage(Profile::FromBrowserContext(context)->GetPrefs());
}

}  // namespace chromeos
