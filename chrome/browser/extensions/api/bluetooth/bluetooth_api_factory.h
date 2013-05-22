// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class Profile;

namespace extensions {

class BluetoothAPI;

class BluetoothAPIFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BluetoothAPI* GetForProfile(Profile* profile);

  static BluetoothAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<BluetoothAPIFactory>;

  BluetoothAPIFactory();
  virtual ~BluetoothAPIFactory();

  // BrowserContextKeyedServiceFactory implementation.
  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_FACTORY_H_
