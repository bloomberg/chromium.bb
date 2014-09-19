// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SAVED_DEVICES_SERVICE_FACTORY_H_
#define APPS_SAVED_DEVICES_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class SavedDevicesService;

// BrowserContextKeyedServiceFactory for SavedDevicesService.
class SavedDevicesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SavedDevicesService* GetForProfile(Profile* profile);

  static SavedDevicesServiceFactory* GetInstance();

 private:
  SavedDevicesServiceFactory();
  virtual ~SavedDevicesServiceFactory();
  friend struct DefaultSingletonTraits<SavedDevicesServiceFactory>;

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_SAVED_DEVICES_SERVICE_FACTORY_H_
