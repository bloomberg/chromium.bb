// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MANAGER_PASSWORD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MANAGER_PASSWORD_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace chromeos {

class ManagerPasswordService;

class ManagerPasswordServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ManagerPasswordService* GetForProfile(Profile* profile);

  static ManagerPasswordServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ManagerPasswordServiceFactory>;

  ManagerPasswordServiceFactory();
  ~ManagerPasswordServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_MANAGER_PASSWORD_SERVICE_FACTORY_H_
