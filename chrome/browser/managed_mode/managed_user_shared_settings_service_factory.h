// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class ManagedUserSharedSettingsService;

class ManagedUserSharedSettingsServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ManagedUserSharedSettingsService* GetForBrowserContext(
      content::BrowserContext* profile);

  static ManagedUserSharedSettingsServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ManagedUserSharedSettingsServiceFactory>;

  ManagedUserSharedSettingsServiceFactory();
  virtual ~ManagedUserSharedSettingsServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_SERVICE_FACTORY_H_
