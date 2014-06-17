// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SHARED_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SHARED_SETTINGS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class SupervisedUserSharedSettingsService;

class SupervisedUserSharedSettingsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SupervisedUserSharedSettingsService* GetForBrowserContext(
      content::BrowserContext* profile);

  static SupervisedUserSharedSettingsServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<
      SupervisedUserSharedSettingsServiceFactory>;

  SupervisedUserSharedSettingsServiceFactory();
  virtual ~SupervisedUserSharedSettingsServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SHARED_SETTINGS_SERVICE_FACTORY_H_
