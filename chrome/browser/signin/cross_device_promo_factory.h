// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class CrossDevicePromo;
class Profile;

// Singleton that owns all CrossDevicePromos and associates them with
// Profiles.
class CrossDevicePromoFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of CrossDevicePromo associated with this profile,
  // creating one if none exists.
  static CrossDevicePromo* GetForProfile(Profile* profile);

  // Returns an instance of the CrossDevicePromoFactory singleton.
  static CrossDevicePromoFactory* GetInstance();

  // Implementation of BrowserContextKeyedServiceFactory.
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  friend struct base::DefaultSingletonTraits<CrossDevicePromoFactory>;

  CrossDevicePromoFactory();
  ~CrossDevicePromoFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CrossDevicePromoFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_CROSS_DEVICE_PROMO_FACTORY_H_
