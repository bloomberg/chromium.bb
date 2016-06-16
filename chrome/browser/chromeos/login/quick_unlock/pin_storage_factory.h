// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/core/account_id/account_id.h"

class Profile;

namespace user_manager {
class User;
}

namespace chromeos {

class PinStorage;

// Singleton that owns all PinStorage instances and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated PinStorage.
class PinStorageFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PinStorage instance for |profile|.
  static PinStorage* GetForProfile(Profile* profile);

  // Helper method that finds the PinStorage instance for |user|. This returns
  // GetForProfile with the profile associated with |user|.
  static PinStorage* GetForUser(const user_manager::User* user);

  // Helper method that returns the PinStorage instance for |account_id|. This
  // returns GetForProfile with the profile associated with |account_id|.
  static PinStorage* GetForAccountId(const AccountId& account_id);

  static PinStorageFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PinStorageFactory>;

  PinStorageFactory();
  ~PinStorageFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  DISALLOW_COPY_AND_ASSIGN(PinStorageFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_FACTORY_H_
