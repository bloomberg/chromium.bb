// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_RESTORE_SERVICE_FACTORY_H_
#define APPS_APP_RESTORE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class AppRestoreService;

// Singleton that owns all AppRestoreServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AppRestoreService.
class AppRestoreServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppRestoreService* GetForProfile(Profile* profile);

  static AppRestoreServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AppRestoreServiceFactory>;

  AppRestoreServiceFactory();
  virtual ~AppRestoreServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_APP_RESTORE_SERVICE_FACTORY_H_
