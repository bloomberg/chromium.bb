// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;
class ExtensionRegistry;
class ExtensionSystem;
class RendererStartupHelper;

// ExtensionRegistrar drives the stages of registering and unregistering
// extensions for a BrowserContext.
// TODO(michaelpg): Add functionality for reloading and terminating extensions.
class ExtensionRegistrar {
 public:
  explicit ExtensionRegistrar(content::BrowserContext* browser_context);
  virtual ~ExtensionRegistrar();

  // Marks |extension| enabled and notifies other components about it.
  // TODO(michaelpg): Move other steps and checks for enabling the extension
  // from ExtensionService::EnableExtension into here.
  void EnableExtension(scoped_refptr<const Extension> extension);

  // Marks |extension| disabled and notifies other components that it is
  // disabled. The ExtensionRegistry retains a reference to it in
  // disabled_extensions().
  // TODO(michaelpg): Move other steps for disabling the extension from
  // ExtensionService::DisableExtension into here.
  void DisableExtension(scoped_refptr<const Extension> extension);

  // Removes |extension| from the extension system by deactivating it if it is
  // enabled and removing references to it from the ExtensionRegistry's enabled
  // or disabled sets.
  // Note: Extensions will not be removed from other sets (terminated,
  // blacklisted or blocked). ExtensionService handles that, since it also adds
  // it to those sets. TODO(michaelpg): Make ExtensionRegistrar the sole mutator
  // of ExtensionRegsitry to simplify this usage.
  void RemoveExtension(const ExtensionId& extension_id,
                       UnloadedExtensionReason reason);

 private:
  // Triggers the unloaded notifications to deactivate an extension.
  void DeactivateExtension(const Extension* extension,
                           UnloadedExtensionReason reason);

  // Updates the ExtensionSystem when an extension is disabled or removed (even
  // if it was already disabled or terminated). Necessary because
  // ExtensionSystem tracks enabled and disabled extensions separately.
  void NotifyExtensionDisabledOrRemoved(const ExtensionId& extension_id,
                                        UnloadedExtensionReason reason);

  // Marks the extension ready after URLRequestContexts have been updated on
  // the IO thread.
  void OnExtensionRegisteredWithRequestContexts(
      scoped_refptr<const Extension> extension);

  // These objects should outlive the service that owns us (ExtensionService).
  ExtensionSystem* const extension_system_;
  ExtensionRegistry* const registry_;
  extensions::RendererStartupHelper* renderer_helper_;

  base::WeakPtrFactory<ExtensionRegistrar> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRegistrar);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_
