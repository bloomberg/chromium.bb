// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SYNC_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class ManagedUserSyncService;
class Profile;

class ManagedUserSyncServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ManagedUserSyncService* GetForProfile(Profile* profile);

  static ManagedUserSyncServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ManagedUserSyncServiceFactory>;

  ManagedUserSyncServiceFactory();
  virtual ~ManagedUserSyncServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SYNC_SERVICE_FACTORY_H_
