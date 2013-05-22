// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_WHITELIST_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_WHITELIST_MANAGER_FACTORY_H_

#include "base/compiler_specific.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

template <typename T> struct DefaultSingletonTraits;
class Profile;

namespace autofill {
namespace autocheckout {

class WhitelistManager;

// A wrapper of WhitelistManager so we can use it as a profiled
// keyed service. Exposed in header file only for tests.
class WhitelistManagerService : public BrowserContextKeyedService {
 public:
  virtual WhitelistManager* GetWhitelistManager() = 0;
};

// Singleton that owns all WhitelistManager and associates them
// with Profiles.
// Listens for the Profile's destruction notification and cleans up the
// associated WhitelistManager.
class WhitelistManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the WhitelistManager for |profile|, creating it if
  // it is not yet created.
  static WhitelistManager* GetForProfile(Profile* profile);

  static WhitelistManagerFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<WhitelistManagerFactory>;

  WhitelistManagerFactory();
  virtual ~WhitelistManagerFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace autocheckout
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOCHECKOUT_WHITELIST_MANAGER_FACTORY_H_
