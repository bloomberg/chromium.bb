// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_system_impl.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {
class ExtensionSystem;

// BrowserContextKeyedServiceFactory for ExtensionSystemImpl::Shared.
// Should not be used except by ExtensionSystem(Factory).
class ExtensionSystemSharedFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionSystemImpl::Shared* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionSystemSharedFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSystemSharedFactory>;

  ExtensionSystemSharedFactory();
  virtual ~ExtensionSystemSharedFactory();

  // BrowserContextKeyedServiceFactory implementation:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSystemSharedFactory);
};

// BrowserContextKeyedServiceFactory for ExtensionSystemImpl.
// TODO(yoz): Rename to ExtensionSystemImplFactory.
class ExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  // ExtensionSystem provider implementation:
  virtual ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) OVERRIDE;

  static ExtensionSystemFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ExtensionSystemFactory>;

  ExtensionSystemFactory();
  virtual ~ExtensionSystemFactory();

  // BrowserContextKeyedServiceFactory implementation:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSystemFactory);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_
