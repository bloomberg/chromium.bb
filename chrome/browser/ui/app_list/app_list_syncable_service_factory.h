// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_FACTORY_H_

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace app_list {

class AppListSyncableService;

// Singleton that owns all AppListSyncableServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AppListSyncableService.
class AppListSyncableServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppListSyncableService* GetForProfile(Profile* profile);

  static AppListSyncableServiceFactory* GetInstance();

  static KeyedService* BuildInstanceFor(
      content::BrowserContext* browser_context);

 private:
  friend struct DefaultSingletonTraits<AppListSyncableServiceFactory>;

  AppListSyncableServiceFactory();
  virtual ~AppListSyncableServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableServiceFactory);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SYNCABLE_SERVICE_FACTORY_H_
