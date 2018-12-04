// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_manager_migrator.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromeos/chromeos_switches.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace chromeos {

AccountManagerMigrator::AccountManagerMigrator() = default;

AccountManagerMigrator::~AccountManagerMigrator() = default;

void AccountManagerMigrator::Start() {
  DVLOG(1) << "AccountManagerMigrator::Start";

  if (!chromeos::switches::IsAccountManagerEnabled())
    return;

  // TODO(sinhak): Migrate Device Account.
  // TODO(sinhak): Migrate Secondary Accounts in Chrome content area.
  // TODO(sinhak): Migrate Secondary Accounts in ARC.
  // TODO(sinhak): Store success state in Preferences.
  // TODO(sinhak): Gather UMA stats.
  // TODO(sinhak): Verify Device Account LST state.
  // TODO(sinhak): Enable account reconciliation.
}

// static
AccountManagerMigrator* AccountManagerMigratorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccountManagerMigrator*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AccountManagerMigratorFactory* AccountManagerMigratorFactory::GetInstance() {
  static base::NoDestructor<AccountManagerMigratorFactory> instance;
  return instance.get();
}

AccountManagerMigratorFactory::AccountManagerMigratorFactory()
    : BrowserContextKeyedServiceFactory(
          "AccountManagerMigrator",
          BrowserContextDependencyManager::GetInstance()) {}

AccountManagerMigratorFactory::~AccountManagerMigratorFactory() = default;

KeyedService* AccountManagerMigratorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AccountManagerMigrator();
}

}  // namespace chromeos
