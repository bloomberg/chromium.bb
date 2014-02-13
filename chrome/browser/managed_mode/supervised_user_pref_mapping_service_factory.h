// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_SUPERVISED_USER_PREF_MAPPING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MANAGED_MODE_SUPERVISED_USER_PREF_MAPPING_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class SupervisedUserPrefMappingService;

class SupervisedUserPrefMappingServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static SupervisedUserPrefMappingService* GetForBrowserContext(
      content::BrowserContext* profile);

  static SupervisedUserPrefMappingServiceFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SupervisedUserPrefMappingServiceFactory>;

  SupervisedUserPrefMappingServiceFactory();
  virtual ~SupervisedUserPrefMappingServiceFactory();

  // BrowserContextKeyedServiceFactory:
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_SUPERVISED_USER_PREF_MAPPING_SERVICE_FACTORY_H_
