// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
#define EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_

#include "base/memory/shared_memory.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/user_script_loader.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionRegistry;

// UserScriptLoader for extensions.
class ExtensionUserScriptLoader : public UserScriptLoader,
                                  public ExtensionRegistryObserver {
 public:
  // The listen_for_extension_system_loaded is only set true when initilizing
  // the Extension System, e.g, when constructs SharedUserScriptMaster in
  // ExtensionSystemImpl.
  ExtensionUserScriptLoader(content::BrowserContext* browser_context,
                            const HostID& host_id,
                            bool listen_for_extension_system_loaded);
  ~ExtensionUserScriptLoader() override;

 private:
  // UserScriptLoader:
  void UpdateHostsInfo(const std::set<HostID>& changed_hosts) override;
  LoadUserScriptsContentFunction GetLoadUserScriptsFunction() override;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;

  // Initiates script load when we have been waiting for the extension system
  // to be ready.
  void OnExtensionSystemReady();

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  base::WeakPtrFactory<ExtensionUserScriptLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionUserScriptLoader);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_USER_SCRIPT_LOADER_H_
