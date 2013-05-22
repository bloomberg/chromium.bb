// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_system.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class BrowserContextKeyedService;
class Profile;

namespace extensions {
class ExtensionSystem;

// BrowserContextKeyedServiceFactory for ExtensionSystemImpl::Shared.
// Should not be used except by ExtensionSystem(Factory).
class ExtensionSystemSharedFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionSystemImpl::Shared* GetForProfile(
      Profile* profile);

  static ExtensionSystemSharedFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSystemSharedFactory>;

  ExtensionSystemSharedFactory();
  virtual ~ExtensionSystemSharedFactory();

  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
};

// BrowserContextKeyedServiceFactory for ExtensionSystem.
class ExtensionSystemFactory : public BrowserContextKeyedServiceFactory {
 public:
  // BrowserContextKeyedServiceFactory implementation:
  static ExtensionSystem* GetForProfile(Profile* profile);

  static ExtensionSystemFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSystemFactory>;

  ExtensionSystemFactory();
  virtual ~ExtensionSystemFactory();

  virtual BrowserContextKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_
