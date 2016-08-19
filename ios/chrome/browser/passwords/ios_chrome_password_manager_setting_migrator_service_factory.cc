// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/passwords/ios_chrome_password_manager_setting_migrator_service_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/password_manager/sync/browser/password_manager_setting_migrator_service.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

// static
password_manager::PasswordManagerSettingMigratorService*
IOSChromePasswordManagerSettingMigratorServiceFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<password_manager::PasswordManagerSettingMigratorService*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
IOSChromePasswordManagerSettingMigratorServiceFactory*
IOSChromePasswordManagerSettingMigratorServiceFactory::GetInstance() {
  return base::Singleton<
      IOSChromePasswordManagerSettingMigratorServiceFactory>::get();
}

IOSChromePasswordManagerSettingMigratorServiceFactory::
    IOSChromePasswordManagerSettingMigratorServiceFactory()
    : BrowserStateKeyedServiceFactory(
          "PasswordManagerSettingMigratorService",
          BrowserStateDependencyManager::GetInstance()) {}

IOSChromePasswordManagerSettingMigratorServiceFactory::
    ~IOSChromePasswordManagerSettingMigratorServiceFactory() {}

std::unique_ptr<KeyedService>
IOSChromePasswordManagerSettingMigratorServiceFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return base::MakeUnique<
      password_manager::PasswordManagerSettingMigratorService>(
      browser_state->GetSyncablePrefs());
}

web::BrowserState*
IOSChromePasswordManagerSettingMigratorServiceFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
