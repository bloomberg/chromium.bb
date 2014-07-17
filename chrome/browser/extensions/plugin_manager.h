// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_

#include <set>
#include <string>

#include "base/scoped_observer.h"
#include "chrome/common/extensions/manifest_handlers/nacl_modules_handler.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry_observer.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionRegistry;

class PluginManager : public BrowserContextKeyedAPI,
                      public ExtensionRegistryObserver {
 public:
  explicit PluginManager(content::BrowserContext* context);
  virtual ~PluginManager();

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<PluginManager>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<PluginManager>;

#if !defined(DISABLE_NACL)

  // We implement some Pepper plug-ins using NaCl to take advantage of NaCl's
  // strong sandbox. Typically, these NaCl modules are stored in extensions
  // and registered here. Not all NaCl modules need to register for a MIME
  // type, just the ones that are responsible for rendering a particular MIME
  // type, like application/pdf. Note: We only register NaCl modules in the
  // browser process.
  void RegisterNaClModule(const NaClModuleInfo& info);
  void UnregisterNaClModule(const NaClModuleInfo& info);

  // Call UpdatePluginListWithNaClModules() after registering or unregistering
  // a NaCl module to see those changes reflected in the PluginList.
  void UpdatePluginListWithNaClModules();

  extensions::NaClModuleInfo::List::iterator FindNaClModule(const GURL& url);

#endif  // !defined(DISABLE_NACL)

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionLoaded(content::BrowserContext* browser_context,
                                 const Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "PluginManager"; }
  static const bool kServiceIsNULLWhileTesting = true;

  extensions::NaClModuleInfo::List nacl_module_list_;

  Profile* profile_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_PLUGIN_MANAGER_H_
