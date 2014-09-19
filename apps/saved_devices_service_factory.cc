// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_devices_service_factory.h"

#include "apps/saved_devices_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
SavedDevicesService* SavedDevicesServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SavedDevicesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SavedDevicesServiceFactory* SavedDevicesServiceFactory::GetInstance() {
  return Singleton<SavedDevicesServiceFactory>::get();
}

SavedDevicesServiceFactory::SavedDevicesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SavedDevicesService",
          BrowserContextDependencyManager::GetInstance()) {
}

SavedDevicesServiceFactory::~SavedDevicesServiceFactory() {
}

KeyedService* SavedDevicesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SavedDevicesService(static_cast<Profile*>(profile));
}

}  // namespace apps
