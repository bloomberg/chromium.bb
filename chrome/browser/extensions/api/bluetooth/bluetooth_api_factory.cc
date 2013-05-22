// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_factory.h"

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace extensions {

// static
BluetoothAPI* BluetoothAPIFactory::GetForProfile(Profile* profile) {
  return static_cast<BluetoothAPI*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BluetoothAPIFactory* BluetoothAPIFactory::GetInstance() {
  return Singleton<BluetoothAPIFactory>::get();
}

BluetoothAPIFactory::BluetoothAPIFactory()
    : BrowserContextKeyedServiceFactory(
        "BluetoothAPI",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

BluetoothAPIFactory::~BluetoothAPIFactory() {
}

BrowserContextKeyedService* BluetoothAPIFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new BluetoothAPI(static_cast<Profile*>(profile));
}

content::BrowserContext* BluetoothAPIFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool BluetoothAPIFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool BluetoothAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
