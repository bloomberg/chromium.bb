// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_PIN_STORAGE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {

class PinStorage;

// Singleton that owns all PinStorage instances and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated PinStorage.
class PinStorageFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PinStorage instance for |profile|.
  static PinStorage* GetForProfile(Profile* profile);

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
