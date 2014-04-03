// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/signin/core/browser/account_reconcilor.h"

class AccountReconcilor;
class Profile;

// Singleton that owns all AccountReconcilors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up.
class AccountReconcilorFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the instance of AccountReconcilor associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have an
  // AccountReconcilor (for example, if |profile| is incognito).
  static AccountReconcilor* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static AccountReconcilorFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AccountReconcilorFactory>;

  AccountReconcilorFactory();
  virtual ~AccountReconcilorFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_FACTORY_H_
