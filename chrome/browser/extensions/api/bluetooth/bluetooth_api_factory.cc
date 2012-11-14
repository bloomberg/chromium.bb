// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_factory.h"

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
BluetoothAPI* BluetoothAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<BluetoothAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
BluetoothAPIFactory* BluetoothAPIFactory::GetInstance() {
  return Singleton<BluetoothAPIFactory>::get();
}

BluetoothAPIFactory::BluetoothAPIFactory()
    : ProfileKeyedServiceFactory("BluetoothAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

BluetoothAPIFactory::~BluetoothAPIFactory() {
}

ProfileKeyedService* BluetoothAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new BluetoothAPI(profile);
}

bool BluetoothAPIFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool BluetoothAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool BluetoothAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
