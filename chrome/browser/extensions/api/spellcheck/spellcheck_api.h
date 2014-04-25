// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_

#include "base/scoped_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {
class ExtensionRegistry;

class SpellcheckAPI : public BrowserContextKeyedAPI,
                      public ExtensionRegistryObserver {
 public:
  explicit SpellcheckAPI(content::BrowserContext* context);
  virtual ~SpellcheckAPI();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SpellcheckAPI>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<SpellcheckAPI>;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "SpellcheckAPI";
  }

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(SpellcheckAPI);
};

template <>
void BrowserContextKeyedAPIFactory<SpellcheckAPI>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SPELLCHECK_SPELLCHECK_API_H_
