// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_FACTORY_H_
#define APPS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_FACTORY_H_

#include "base/memory/singleton.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

// A factory that provides ShellExtensionSystem for app_shell.
class ShellExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  // ExtensionSystemProvider implementation:
  virtual ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) OVERRIDE;

  static ShellExtensionSystemFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ShellExtensionSystemFactory>;

  ShellExtensionSystemFactory();
  virtual ~ShellExtensionSystemFactory();

  // BrowserContextKeyedServiceFactory implementation:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ShellExtensionSystemFactory);
};

}  // namespace extensions

#endif  // APPS_SHELL_BROWSER_SHELL_EXTENSION_SYSTEM_FACTORY_H_
