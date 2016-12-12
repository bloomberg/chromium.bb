// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

enum class ServiceAccessType;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace ios {
class ChromeBrowserState;
}

namespace password_manager {
class PasswordManagerSettingMigratorService;
}

class IOSChromePasswordManagerSettingMigratorServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static password_manager::PasswordManagerSettingMigratorService*
  GetForBrowserState(ios::ChromeBrowserState* browser_state);

  static IOSChromePasswordManagerSettingMigratorServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      IOSChromePasswordManagerSettingMigratorServiceFactory>;

  IOSChromePasswordManagerSettingMigratorServiceFactory();
  ~IOSChromePasswordManagerSettingMigratorServiceFactory() override;

  // BrowserStateKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_IOS_CHROME_PASSWORD_MANAGER_SETTING_MIGRATOR_SERVICE_FACTORY_H_
