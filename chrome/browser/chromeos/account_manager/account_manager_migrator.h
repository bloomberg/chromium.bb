// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_
#define CHROME_BROWSER_CHROMEOS_ACCOUNT_MANAGER_ACCOUNT_MANAGER_MIGRATOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/account_manager/account_migration_runner.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace chromeos {

class AccountManagerMigrator : public KeyedService {
 public:
  explicit AccountManagerMigrator(Profile* profile);
  ~AccountManagerMigrator() override;

  // Starts migrating accounts to Chrome OS Account Manager.
  void Start();

  // Gets the current status of migration.
  AccountMigrationRunner::Status GetStatus() const;

 private:
  void OnMigrationRunComplete(
      const AccountMigrationRunner::MigrationResult& result);

  // A non-owning pointer to |Profile|.
  Profile* const profile_;

  // Used for running migration steps.
  chromeos::AccountMigrationRunner migration_runner_;

  base::WeakPtrFactory<AccountManagerMigrator> weak_factory_;
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
