// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace chromeos {

class AccountManagerMigrator : public KeyedService {
 public:
  AccountManagerMigrator();
  ~AccountManagerMigrator() override;

  // Starts migrating accounts to Chrome OS Account Manager.
  void Start();

 private:
  DISALLOW_COPY_AND_ASSIGN(AccountManagerMigrator);
};

class AccountManagerMigratorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AccountManagerMigrator* GetForBrowserContext(
      content::BrowserContext* context);
  static AccountManagerMigratorFactory* GetInstance();

 private:
  friend class base::NoDestructor<AccountManagerMigratorFactory>;

  AccountManagerMigratorFactory();
  ~AccountManagerMigratorFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AccountManagerMigratorFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_
