// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_

#include "base/basictypes.h"
#include "base/scoped_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionRegistry;

class ExtensionWebUIOverrideRegistrar : public BrowserContextKeyedAPI,
                                        public ExtensionRegistryObserver {
 public:
  explicit ExtensionWebUIOverrideRegistrar(content::BrowserContext* context);
  virtual ~ExtensionWebUIOverrideRegistrar();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionWebUIOverrideRegistrar>;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "ExtensionWebUIOverrideRegistrar";
  }

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebUIOverrideRegistrar);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEB_UI_OVERRIDE_REGISTRAR_H_
