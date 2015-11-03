// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_setting_migrator_service_factory.h"

#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/sync/browser/password_manager_setting_migrator_service.h"

// static
PasswordManagerSettingMigratorServiceFactory*
PasswordManagerSettingMigratorServiceFactory::GetInstance() {
  return base::Singleton<PasswordManagerSettingMigratorServiceFactory>::get();
}

// static
password_manager::PasswordManagerSettingMigratorService*
PasswordManagerSettingMigratorServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::PasswordManagerSettingMigratorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PasswordManagerSettingMigratorServiceFactory::
    PasswordManagerSettingMigratorServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PasswordManagerSettingMigratorService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ProfileSyncServiceFactory::GetInstance());
}

PasswordManagerSettingMigratorServiceFactory::
    ~PasswordManagerSettingMigratorServiceFactory() {}

KeyedService*
PasswordManagerSettingMigratorServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new password_manager::PasswordManagerSettingMigratorService(
      PrefServiceSyncableFromProfile(profile));
}

bool PasswordManagerSettingMigratorServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}
