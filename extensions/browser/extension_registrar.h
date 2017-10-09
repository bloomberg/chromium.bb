// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class Extension;
class ExtensionPrefs;
class ExtensionRegistry;
class ExtensionSystem;
class RendererStartupHelper;

// ExtensionRegistrar drives the stages of registering and unregistering
// extensions for a BrowserContext. It uses the ExtensionRegistry to track
// extension states. Other classes may query the ExtensionRegistry directly,
// but eventually only ExtensionRegistrar will be able to make changes to it.
class ExtensionRegistrar {
 public:
  // Delegate for embedder-specific functionality like policy and permissions.
  class Delegate {
   public:
    Delegate() = default;
    virtual ~Delegate() = default;

    // Handles updating the browser context when an extension is activated
    // (becomes enabled).
    // |is_newly_added| specifies if the extension has just been added.
    // Otherwise, the extension was already added; it may have been activated
    // before and then disabled/reloaded.
    virtual void PostActivateExtension(scoped_refptr<const Extension> extension,
                                       bool is_newly_added) = 0;

    // Handles updating the browser context when an enabled extension is
    // deactivated (whether disabled or removed).
    virtual void PostDeactivateExtension(
        scoped_refptr<const Extension> extension) = 0;

    // Returns true if the extension is allowed to be enabled or disabled,
    // respectively.
    virtual bool CanEnableExtension(const Extension* extension) = 0;
    virtual bool CanDisableExtension(const Extension* extension) = 0;

    // Returns true if the extension should be blocked.
    virtual bool ShouldBlockExtension(const Extension* extension) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // The provided Delegate should outlive this object.
  ExtensionRegistrar(content::BrowserContext* browser_context,
                     Delegate* delegate);
  virtual ~ExtensionRegistrar();

  // Adds the extension to the ExtensionRegistry. The extension will be added to
  // the enabled, disabled, blacklisted or blocked set. If the extension is
  // added as enabled, it will be activated.
  void AddExtension(scoped_refptr<const Extension> extension);

  // Removes |extension| from the extension system by deactivating it if it is
  // enabled and removing references to it from the ExtensionRegistry's
  // enabled or disabled sets.
  // Note: Extensions will not be removed from other sets (terminated,
  // blacklisted or blocked). ExtensionService handles that, since it also adds
  // it to those sets. TODO(michaelpg): Make ExtensionRegistrar the sole mutator
  // of ExtensionRegsitry to simplify this usage.
  void RemoveExtension(const ExtensionId& extension_id,
                       UnloadedExtensionReason reason);

  // If the extension is disabled, marks it as enabled and activates it for use.
  // Otherwise, simply updates the ExtensionPrefs. (Blacklisted or blocked
  // extensions cannot be enabled.)
  void EnableExtension(const ExtensionId& extension_id);

  // Marks |extension| as disabled and deactivates it. The ExtensionRegistry
  // retains a reference to it, so it can be enabled later.
  void DisableExtension(const ExtensionId& extension_id, int disable_reasons);

  // Given an extension that was disabled for reloading, completes the reload
  // by replacing the old extension with the new version and enabling it.
  // Returns true on success.
  bool ReplaceReloadedExtension(scoped_refptr<const Extension> extension);

  // TODO(michaelpg): Add methods for blacklisting and blocking extensions.

  // Returns true if the extension is enabled (including terminated), or if it
  // is not loaded but isn't explicitly disabled in preferences.
  bool IsExtensionEnabled(const ExtensionId& extension_id) const;

 private:
  // Activates |extension| by marking it enabled and notifying other components
  // about it.
  void ActivateExtension(const Extension* extension, bool is_newly_added);

  // Triggers the unloaded notifications to deactivate an extension.
  void DeactivateExtension(const Extension* extension,
                           UnloadedExtensionReason reason);

  // Marks the extension ready after URLRequestContexts have been updated on
  // the IO thread.
  void OnExtensionRegisteredWithRequestContexts(
      scoped_refptr<const Extension> extension);

  content::BrowserContext* const browser_context_;

  // Delegate provided in the constructor. Should outlive this object.
  Delegate* const delegate_;

  // Keyed services we depend on. Cached here for repeated access.
  ExtensionSystem* const extension_system_;
  ExtensionPrefs* const extension_prefs_;
  ExtensionRegistry* const registry_;
  RendererStartupHelper* const renderer_helper_;

  base::WeakPtrFactory<ExtensionRegistrar> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRegistrar);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRAR_H_
