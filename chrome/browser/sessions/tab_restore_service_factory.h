// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class TabRestoreService;
class Profile;

// Singleton that owns all TabRestoreServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated TabRestoreService.
class TabRestoreServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static TabRestoreService* GetForProfile(Profile* profile);

  // Variant of GetForProfile() that returns NULL if TabRestoreService does not
  // exist.
  static TabRestoreService* GetForProfileIfExisting(Profile* profile);

  static void ResetForProfile(Profile* profile);

  static TabRestoreServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<TabRestoreServiceFactory>;

  TabRestoreServiceFactory();
  virtual ~TabRestoreServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

#endif  // CHROME_BROWSER_SESSIONS_TAB_RESTORE_SERVICE_FACTORY_H_
