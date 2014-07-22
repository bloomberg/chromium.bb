// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_REGISTRY_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_set.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
class ExtensionRegistryObserver;

// ExtensionRegistry holds sets of the installed extensions for a given
// BrowserContext. An incognito browser context and its master browser context
// share a single registry.
class ExtensionRegistry : public KeyedService {
 public:
  // Flags to pass to GetExtensionById() to select which sets to look in.
  enum IncludeFlag {
    NONE        = 0,
    ENABLED     = 1 << 0,
    DISABLED    = 1 << 1,
    TERMINATED  = 1 << 2,
    BLACKLISTED = 1 << 3,
    EVERYTHING = (1 << 4) - 1,
  };

  explicit ExtensionRegistry(content::BrowserContext* browser_context);
  virtual ~ExtensionRegistry();

  // Returns the instance for the given |browser_context|.
  static ExtensionRegistry* Get(content::BrowserContext* browser_context);

  content::BrowserContext* browser_context() const { return browser_context_; }

  // NOTE: These sets are *eventually* mututally exclusive, but an extension can
  // appear in two sets for short periods of time.
  const ExtensionSet& enabled_extensions() const {
    return enabled_extensions_;
  }
  const ExtensionSet& disabled_extensions() const {
    return disabled_extensions_;
  }
  const ExtensionSet& terminated_extensions() const {
    return terminated_extensions_;
  }
  const ExtensionSet& blacklisted_extensions() const {
    return blacklisted_extensions_;
  }

  // Returns a set of all installed, disabled, blacklisted, and terminated
  // extensions.
  scoped_ptr<ExtensionSet> GenerateInstalledExtensionsSet() const;

  // The usual observer interface.
  void AddObserver(ExtensionRegistryObserver* observer);
  void RemoveObserver(ExtensionRegistryObserver* observer);

  // Invokes the observer method OnExtensionLoaded(). The extension must be
  // enabled at the time of the call.
  void TriggerOnLoaded(const Extension* extension);

  // Invokes the observer method OnExtensionUnloaded(). The extension must not
  // be enabled at the time of the call.
  void TriggerOnUnloaded(const Extension* extension,
                         UnloadedExtensionInfo::Reason reason);

  // If this is a fresh install then |is_update| is false and there must not be
  // any installed extension with |extension|'s ID. If this is an update then
  // |is_update| is true and must be an installed extension with |extension|'s
  // ID, and |old_name| must be non-empty.
  // If true, |from_ephemeral| indicates that the extension was previously
  // installed ephemerally and has been promoted to a regular installed
  // extension. |is_update| should also be true.
  void TriggerOnWillBeInstalled(const Extension* extension,
                                bool is_update,
                                bool from_ephemeral,
                                const std::string& old_name);

  // Invokes the observer method OnExtensionInstalled(). The extension must be
  // contained in one of the registry's extension sets.
  void TriggerOnInstalled(const Extension* extension,
                          bool is_update);

  // Invokes the observer method OnExtensionUninstalled(). The extension must
  // not be any installed extension with |extension|'s ID.
  void TriggerOnUninstalled(const Extension* extension, UninstallReason reason);

  // Find an extension by ID using |include_mask| to pick the sets to search:
  //  * enabled_extensions()     --> ExtensionRegistry::ENABLED
  //  * disabled_extensions()    --> ExtensionRegistry::DISABLED
  //  * terminated_extensions()  --> ExtensionRegistry::TERMINATED
  //  * blacklisted_extensions() --> ExtensionRegistry::BLACKLISTED
  // Returns NULL if the extension is not found in the selected sets.
  const Extension* GetExtensionById(const std::string& id,
                                    int include_mask) const;

  // Adds the specified extension to the enabled set. The registry becomes an
  // owner. Any previous extension with the same ID is removed.
  // Returns true if there is no previous extension.
  // NOTE: You probably want to use ExtensionService instead of calling this
  // method directly.
  bool AddEnabled(const scoped_refptr<const Extension>& extension);

  // Removes the specified extension from the enabled set.
  // Returns true if the set contained the specified extension.
  // NOTE: You probably want to use ExtensionService instead of calling this
  // method directly.
  bool RemoveEnabled(const std::string& id);

  // As above, but for the disabled set.
  bool AddDisabled(const scoped_refptr<const Extension>& extension);
  bool RemoveDisabled(const std::string& id);

  // As above, but for the terminated set.
  bool AddTerminated(const scoped_refptr<const Extension>& extension);
  bool RemoveTerminated(const std::string& id);

  // As above, but for the blacklisted set.
  bool AddBlacklisted(const scoped_refptr<const Extension>& extension);
  bool RemoveBlacklisted(const std::string& id);

  // Removes all extensions from all sets.
  void ClearAll();

  // Sets a callback to run when the disabled extension set is modified.
  // TODO(jamescook): This is too specific for a generic registry; find some
  // other way to do this.
  void SetDisabledModificationCallback(
      const ExtensionSet::ModificationCallback& callback);

  // KeyedService implementation:
  virtual void Shutdown() OVERRIDE;

 private:
  // Extensions that are installed, enabled and not terminated.
  ExtensionSet enabled_extensions_;

  // Extensions that are installed and disabled.
  ExtensionSet disabled_extensions_;

  // Extensions that are installed and terminated.
  ExtensionSet terminated_extensions_;

  // Extensions that are installed and blacklisted. Generally these shouldn't be
  // considered as installed by the extension platform: we only keep them around
  // so that if extensions are blacklisted by mistake they can easily be
  // un-blacklisted.
  ExtensionSet blacklisted_extensions_;

  ObserverList<ExtensionRegistryObserver> observers_;

  content::BrowserContext* const browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRegistry);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_REGISTRY_H_
