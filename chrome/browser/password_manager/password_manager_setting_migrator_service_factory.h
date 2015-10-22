// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace password_manager {
class PasswordManagerSettingMigratorService;
}

class PasswordManagerSettingMigratorServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PasswordManagerSettingMigratorServiceFactory* GetInstance();
  static password_manager::PasswordManagerSettingMigratorService* GetForProfile(
      Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<
      PasswordManagerSettingMigratorServiceFactory>;

  PasswordManagerSettingMigratorServiceFactory();
  ~PasswordManagerSettingMigratorServiceFactory() override;

  // BrowserContextKeyedServiceFactory
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_FACTORY_H_
