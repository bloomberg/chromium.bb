// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ManagedUserService;
class Profile;

class ManagedUserServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ManagedUserService* GetForProfile(Profile* profile);

  static ManagedUserServiceFactory* GetInstance();

  // Used to create instances for testing.
  static KeyedService* BuildInstanceFor(Profile* profile);

 private:
  friend struct DefaultSingletonTraits<ManagedUserServiceFactory>;

  ManagedUserServiceFactory();
  virtual ~ManagedUserServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SERVICE_FACTORY_H_
